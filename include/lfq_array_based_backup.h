#pragma once

#include <vector>
#include <atomic>
#include <memory>

#include <stdexcept>
#include <cassert>
#include <iostream>

template <typename T>
class lfq_array_based {
public:
	explicit lfq_array_based(size_t capacity);

	bool enqueue(const T& value);

	bool enqueue(T&& value);

	bool dequeue(T& value);

	bool empty() const;

	~lfq_array_based() = default;

private:
	std::unique_ptr<T[]> buffer_ptr_;  // 拥有缓冲区
	std::atomic<T*> buffer_;           // 指向缓冲区的原子指针，用于访问
	const size_t capacity_;		// 缓冲区容量
	//std::atomic<size_t> head_;	// 队首索引
	//std::atomic<size_t> tail_;	// 队尾索引
	alignas(64) std::atomic<size_t> head_;
	alignas(64) std::atomic<size_t> tail_;
};

template <typename T>
lfq_array_based<T>::lfq_array_based(size_t capacity)
	: buffer_ptr_(static_cast<T*>(::operator new(sizeof(T)* capacity))), // 分配原始内存
	buffer_(buffer_ptr_.get()),
	capacity_(capacity),
	head_(0),
	tail_(0) {
	if (capacity == 0) {
		throw std::invalid_argument("Capacity must be greater than zero.");
	}

	// 初始化内存为原始字节 （避免默认构造）
	std::memset(buffer_ptr_.get(), 0, sizeof(T) * capacity);
}

template <typename T>
bool lfq_array_based<T>::enqueue(const T& value) {
	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;
	T* current_slot;  // 用于安全访问缓冲区

	do {
		next_tail = (tail + 1) % capacity_;

		// 在循环内重新加载head，确保最新状态
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // 队列已满
		}

		// 关键：不在此处写入数据!
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // 成功时使用更强的内存序
		std::memory_order_relaxed));

	// CAS成功后：当前线程独占地拥有tail槽位
	// 获取槽位指针（非原子访问）
	current_slot = &buffer_[tail];

	// 写入数据（此时不会竞争）
	*current_slot = value;

	// 确保数据对消费者可见
	std::atomic_thread_fence(std::memory_order_release);

	return true;
}

template <typename T>
bool lfq_array_based<T>::enqueue(T&& value) {
	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;
	T* current_slot;  // 用于安全访问缓冲区

	do {
		next_tail = (tail + 1) % capacity_;

		// 在循环内重新加载head，确保最新状态
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // 队列已满
		}

		// 关键：不在此处写入数据!
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // 成功时使用更强的内存序
		std::memory_order_relaxed));

	// CAS成功后：当前线程独占地拥有tail槽位
	// 获取槽位指针（非原子访问）
	current_slot = &buffer_[tail];

	// 写入数据（此时不会竞争）
	*current_slot = move(value);

	// 确保数据对消费者可见
	std::atomic_thread_fence(std::memory_order_release);

	return true;
}

// 多消费者时实现
//template <typename T>
//bool lfq_array_based<T>::dequeue(T& value) {
//	size_t head;
//	size_t next_head;
//	T* current_slot;
//
//	do {
//		head = head_.load(std::memory_order_relaxed);
//		size_t const cur_tail = tail_.load(std::memory_order_acquire);
//
//		// 检查队列是否为空
//		if (head == cur_tail) {
//			return false; // 队列为空
//		}
//
//		// 记录下一个要读取的位置
//		next_head = (head + 1) % capacity_;
//		current_slot = &buffer_[head];
//
//		// 尝试CAS更新head指针
//		// 如果成功，当前消费者获得该元素的读取权
//	} while (!head_.compare_exchange_weak(
//		head,
//		next_head,
//		std::memory_order_acq_rel,  // 成功时使用获取-释放内存序
//		std::memory_order_relaxed)); // 失败时使用宽松内存序
//
//	// CAS成功后：当前线程独占地拥有head槽位
//	value = std::move(*current_slot);
//
//	// 确保数据加载完成
//	std::atomic_thread_fence(std::memory_order_release);
//
//	return true;
//}


// 单消费者时实现
template <typename T>
bool lfq_array_based<T>::dequeue(T& value) {
	size_t head = head_.load(std::memory_order_relaxed);

	// 必须使用acquire读取tail
	size_t const cur_tail = tail_.load(std::memory_order_acquire);
	if (head == cur_tail) {
		return false; // 队列为空
	}

	// 确保看到正确的缓冲区内容
	T* current_slot = &buffer_[head];
	value = std::move(*current_slot);

	// 更新head（使用release保证顺序）
	head_.store((head + 1) % capacity_, std::memory_order_release);
	return true;
}


template <typename T>
bool lfq_array_based<T>::empty() const {
	// 使用relaxed加载，因为我们只关心索引的值，并不依赖它们来同步数据内容
	// 松散判空，不保证正确性
	size_t head = head_.load(std::memory_order_relaxed);
	size_t tail = tail_.load(std::memory_order_relaxed);
	return head == tail;
}