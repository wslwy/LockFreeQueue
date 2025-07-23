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
	std::unique_ptr<T[]> buffer_ptr_;  // ӵ�л�����
	std::atomic<T*> buffer_;           // ָ�򻺳�����ԭ��ָ�룬���ڷ���
	const size_t capacity_;		// ����������
	//std::atomic<size_t> head_;	// ��������
	//std::atomic<size_t> tail_;	// ��β����
	alignas(64) std::atomic<size_t> head_;
	alignas(64) std::atomic<size_t> tail_;
};

template <typename T>
lfq_array_based<T>::lfq_array_based(size_t capacity)
	: buffer_ptr_(static_cast<T*>(::operator new(sizeof(T)* capacity))), // ����ԭʼ�ڴ�
	buffer_(buffer_ptr_.get()),
	capacity_(capacity),
	head_(0),
	tail_(0) {
	if (capacity == 0) {
		throw std::invalid_argument("Capacity must be greater than zero.");
	}

	// ��ʼ���ڴ�Ϊԭʼ�ֽ� ������Ĭ�Ϲ��죩
	std::memset(buffer_ptr_.get(), 0, sizeof(T) * capacity);
}

template <typename T>
bool lfq_array_based<T>::enqueue(const T& value) {
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

	return true;
}

template <typename T>
bool lfq_array_based<T>::enqueue(T&& value) {
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
	if (head == cur_tail) {
		return false; // ����Ϊ��
	}

	// ȷ��������ȷ�Ļ���������
	T* current_slot = &buffer_[head];
	value = std::move(*current_slot);

	// ����head��ʹ��release��֤˳��
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