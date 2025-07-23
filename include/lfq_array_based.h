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
	struct Slot {
		T data;
		std::atomic<bool> ready = false; // ���ݾ�����־
	};
	std::unique_ptr<Slot[]> buffer_ptr_;		// ӵ�л�����
	Slot* const buffer_;           // ָ�򻺳�����ԭ��ָ�룬���ڷ���
	const size_t capacity_;		// ����������
	//std::atomic<size_t> head_;	// ��������
	//std::atomic<size_t> tail_;	// ��β����
	alignas(64) std::atomic<size_t> head_;
	alignas(64) std::atomic<size_t> tail_;

	// ���ò�λ״̬
	void set_slot_ready(size_t idx, bool ready);

	// ����λ״̬
	bool is_slot_ready(size_t idx) const;
};

template <typename T>
void lfq_array_based<T>::set_slot_ready(size_t idx, bool ready) {
	buffer_[idx].ready.store(ready, std::memory_order_release);
}

template <typename T>
bool lfq_array_based<T>::is_slot_ready(size_t idx) const {
	return buffer_[idx].ready.load(std::memory_order_acquire);
}

template <typename T>
lfq_array_based<T>::lfq_array_based(size_t capacity)
	: buffer_ptr_(static_cast<Slot*>(::operator new(sizeof(Slot)* capacity))), // ����ԭʼ�ڴ�
	buffer_(buffer_ptr_.get()),
	capacity_(capacity),
	head_(0),
	tail_(0) {
	if (capacity == 0) {
		throw std::invalid_argument("Capacity must be greater than zero.");
	}

	// ���ڴ��Ϲ���Slot����
	for (size_t i = 0; i < capacity; ++i) {
		new (&buffer_[i]) Slot();
	}
}

template <typename T>
bool lfq_array_based<T>::enqueue(const T& value) {
	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;

	// 1. Ԥ����λ
	do {
		next_tail = (tail + 1) % capacity_;

		// ��ѭ�������¼���head��ȷ������״̬
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // ��������
		}

		// �� ��CASǰ���ղ�
		if (is_slot_ready(tail))
			return false;
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // �ɹ�ʱʹ�ø�ǿ���ڴ���
		std::memory_order_relaxed));

	// CAS�ɹ��󣺵�ǰ�̶߳�ռ��ӵ��tail��λ
	// 2. ��ȫд������
	buffer_[tail].data = value;

	// 3. �������ݿ���״̬
	set_slot_ready(tail, true);

	return true;
}

template <typename T>
bool lfq_array_based<T>::enqueue(T&& value) {
	size_t tail = tail_.load(std::memory_order_relaxed);
	size_t next_tail;

	// 1. Ԥ����λ
	do {
		next_tail = (tail + 1) % capacity_;

		// ��ѭ�������¼���head��ȷ������״̬
		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false; // ��������
		}

		// �� ��CASǰ���ղ�
		if (is_slot_ready(tail))
			return false;
	} while (!tail_.compare_exchange_weak(
		tail,
		next_tail,
		std::memory_order_acq_rel,  // �ɹ�ʱʹ�ø�ǿ���ڴ���
		std::memory_order_relaxed));

	// CAS�ɹ��󣺵�ǰ�̶߳�ռ��ӵ��tail��λ
	// 2. ��ȫд������
	buffer_[tail].data = std::move(value);

	// 3. �������ݿ���״̬
	set_slot_ready(tail, true);

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
	size_t head = head_.load(std::memory_order_relaxed);

	// ����ʹ��acquire��ȡtail
	size_t const cur_tail = tail_.load(std::memory_order_acquire);
	// 1. ȷ��������׼����
	if (head == cur_tail || !is_slot_ready(head)) {
		return false;
	}

	// 2. ��ȡ����
	value = std::move(buffer_[head].data);

	// 3. ��ǲ�λΪ��
	set_slot_ready(head, false);

	// 4. ����ͷָ��
	head_.store((head + 1) % capacity_, std::memory_order_release);
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