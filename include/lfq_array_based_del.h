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

	~lfq_array_based() noexcept;

private:
	void clear_remaining() noexcept;

	bool dequeue_impl(T& value) noexcept;

	bool is_buffer_empty() const noexcept

	std::unique_ptr<T[]> buffer_ptr_;  // ӵ�л�����
	std::atomic<T*> buffer_;           // ָ�򻺳�����ԭ��ָ�룬���ڷ���
	const size_t capacity_;		// ����������
	//std::atomic<size_t> head_;	// ��������
	//std::atomic<size_t> tail_;	// ��β����
	alignas(64) std::atomic<size_t> head_;
	alignas(64) std::atomic<size_t> tail_;

	std::atomic<bool> shutdown_{ false }; // ���ڰ�ȫ�رն���
	std::atomic<size_t> active_operations_{ 0 };    // ���������
};

template <typename T>
lfq_array_based<T>::lfq_array_based(size_t capacity)
	: buffer_ptr_(std::make_unique<T[]>(capacity)),
	buffer_(buffer_ptr_.get()),
	capacity_(capacity),
	head_(0),
	tail_(0) {
	if (capacity == 0) {
		throw std::invalid_argument("Capacity must be greater than zero.");
	}
};

template <typename T>
bool lfq_array_based<T>::enqueue(const T& value) {
	if (shutdown_.load(std::memory_order_acquire)) {
		return false; // �ѹرվܾ��²���
	}

	active_operations_.fetch_add(1, std::memory_order_acq_rel);

	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;
	T* current_slot;  // ���ڰ�ȫ���ʻ�����

	do {
		next_tail = (tail + 1) % capacity_;

		// ��ѭ�������¼���head��ȷ������״̬
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // ��������
		}

		// �ؼ������ڴ˴�д������!
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // �ɹ�ʱʹ�ø�ǿ���ڴ���
		std::memory_order_relaxed));

	// CAS�ɹ��󣺵�ǰ�̶߳�ռ��ӵ��tail��λ
	// ��ȡ��λָ�루��ԭ�ӷ��ʣ�
	current_slot = &buffer_[tail];

	// д�����ݣ���ʱ���Ὰ����
	*current_slot = value;

	// ȷ�����ݶ������߿ɼ�
	std::atomic_thread_fence(std::memory_order_release);

	active_operations_.fetch_sub(1, std::memory_order_release);

	return true;
}

template <typename T>
bool lfq_array_based<T>::enqueue(T&& value) {
	if (shutdown_.load(std::memory_order_acquire)) {
		return false; // �ѹرվܾ��²���
	}

	active_operations_.fetch_add(1, std::memory_order_acq_rel);

	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;
	T* current_slot;  // ���ڰ�ȫ���ʻ�����

	do {
		next_tail = (tail + 1) % capacity_;

		// ��ѭ�������¼���head��ȷ������״̬
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // ��������
		}

		// �ؼ������ڴ˴�д������!
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // �ɹ�ʱʹ�ø�ǿ���ڴ���
		std::memory_order_relaxed));

	// CAS�ɹ��󣺵�ǰ�̶߳�ռ��ӵ��tail��λ
	// ��ȡ��λָ�루��ԭ�ӷ��ʣ�
	current_slot = &buffer_[tail];

	// д�����ݣ���ʱ���Ὰ����
	*current_slot = move(value);

	// ȷ�����ݶ������߿ɼ�
	std::atomic_thread_fence(std::memory_order_release);

	active_operations_.fetch_sub(1, std::memory_order_release);

	return true;
}

// ��������ʱʵ��
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
//		// �������Ƿ�Ϊ��
//		if (head == cur_tail) {
//			return false; // ����Ϊ��
//		}
//
//		// ��¼��һ��Ҫ��ȡ��λ��
//		next_head = (head + 1) % capacity_;
//		current_slot = &buffer_[head];
//
//		// ����CAS����headָ��
//		// ����ɹ�����ǰ�����߻�ø�Ԫ�صĶ�ȡȨ
//	} while (!head_.compare_exchange_weak(
//		head,
//		next_head,
//		std::memory_order_acq_rel,  // �ɹ�ʱʹ�û�ȡ-�ͷ��ڴ���
//		std::memory_order_relaxed)); // ʧ��ʱʹ�ÿ����ڴ���
//
//	// CAS�ɹ��󣺵�ǰ�̶߳�ռ��ӵ��head��λ
//	value = std::move(*current_slot);
//
//	// ȷ�����ݼ������
//	std::atomic_thread_fence(std::memory_order_release);
//
//	return true;
//}


// ��������ʱʵ��
template <typename T>
bool lfq_array_based<T>::dequeue(T& value) {
	if (shutdown_.load(std::memory_order_acquire)) {
		return false; // �ѹرվܾ��²���
	}

	active_operations_.fetch_add(1, std::memory_order_acq_rel);

	size_t head = head_.load(std::memory_order_relaxed);

	// ����ʹ��acquire��ȡtail
	size_t const cur_tail = tail_.load(std::memory_order_acquire);
	if (head == cur_tail) {
		return false; // ����Ϊ��
	}

	// ȷ��������ȷ�Ļ���������
	T* current_slot = &buffer_[head];
	value = std::move(*current_slot);

	// ����head��ʹ��release��֤˳��
	head_.store((head + 1) % capacity_, std::memory_order_release);

	active_operations_.fetch_sub(1, std::memory_order_release);
	return true;
}


template <typename T>
bool lfq_array_based<T>::empty() const {
	// ʹ��relaxed���أ���Ϊ����ֻ����������ֵ����������������ͬ����������
	// ��ɢ�пգ�����֤��ȷ��
	size_t head = head_.load(std::memory_order_relaxed);
	size_t tail = tail_.load(std::memory_order_relaxed);
	return head == tail;
}


template <typename T>
lfq_array_based<T>::~lfq_array_based() noexcept {
	// ��ȫ���٣�����ֱ�����в������
	shutdown_.store(true, std::memory_order_release);

	// �ȴ�����������/�������˳�
	while (active_operations_.load(std::memory_order_acquire) > 0) {
		std::this_thread::yield();
	}

	// ǿ�����ʣ��Ԫ��
	clear_remaining();
}


// private func

template <typename T>
void lfq_array_based<T>::clear_remaining() noexcept {
	T temp;
	// ֻ��ն�������д�������
	while (!is_buffer_empty()) {
		if (dequeue_impl(temp)) {
			// Ԫ������ʱ�������Զ�����
		}
	}
}

template <typename T>
bool lfq_array_based<T>::dequeue_impl(T& value) noexcept {
	// �ڲ�����ʵ�֣������shutdown_
	// (ʹ������ԭdequeue�߼����������߳�ͬ��)
	size_t head = head_.load(std::memory_order_relaxed);
	if (head == tail_.load(std::memory_order_relaxed)) {
		return false;
	}

	value = std::move(buffer_[head]);
	head_.store((head + 1) % capacity_, std::memory_order_release);
	return true;
}

template <typename T>
bool lfq_array_based<T>::is_buffer_empty() const noexcept {
	// ֱ�ӱȽ������Ƿ����
	return head_.load(std::memory_order_relaxed) ==
		tail_.load(std::memory_order_relaxed);
}