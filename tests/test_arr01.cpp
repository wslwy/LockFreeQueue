#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>
#include <cassert>

#include <lfq_array_based.h>

// ��������ʵ�֣��ԣ�
// #include "lfq_array_based.hpp" 

using namespace std;

// ���̻߳������ܲ���
void test_basic_functionality() {
    cout << "===== Basic Functionality Test =====" << endl;
    lfq_array_based<int> queue(5);

    assert(queue.empty());

    // �������/���Ӳ���
    assert(queue.enqueue(1));
    assert(queue.enqueue(2));
    assert(!queue.empty());

    int val;
    assert(queue.dequeue(val) && val == 1);
    assert(queue.dequeue(val) && val == 2);
    assert(queue.empty());
    assert(!queue.dequeue(val));

    // �����в���
    for (int i = 0; i < 4; ++i)  // ����5ʵ�ʿ���4��λ��
        assert(queue.enqueue(i));
    assert(!queue.enqueue(10));  // Ӧʧ��

    cout << "Basic tests passed!\n" << endl;
}

// �������ߵ������߲��� (MPSC)
void test_mpsc() {
    cout << "===== MPSC Test =====" << endl;
    const size_t capacity = 100;
    const size_t num_producers = 4;
    const size_t items_per_producer = 5000;
    lfq_array_based<int> queue(capacity);

    atomic<bool> start_flag{ false };
    vector<thread> producers;
    vector<int> consumer_items;
    mutex consumer_mtx;

    // ����������
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i] {
            // �ȴ���ʼ�ź�
            while (!start_flag.load(memory_order_acquire))
                this_thread::yield();

            // ÿ��������д���ض���Χ������
            for (size_t j = 0; j < items_per_producer; ++j) {
                int item = static_cast<int>(i * items_per_producer + j);
                while (!queue.enqueue(item))
                    this_thread::yield(); // ������ʱ�ȴ�
            }
            });
    }

    // ����������
    thread consumer([&] {
        size_t total_items = num_producers * items_per_producer;
        for (size_t i = 0; i < total_items; ++i) {
            int val;
            while (!queue.dequeue(val))  // ���п�ʱ�ȴ�
                this_thread::yield();

            lock_guard<mutex> lock(consumer_mtx);
            consumer_items.push_back(val);
        }
        });

    // ��������
    start_flag.store(true, memory_order_release);

    // �ȴ��߳̽���
    for (auto& p : producers) p.join();
    consumer.join();

    // ��֤���
    assert(consumer_items.size() == num_producers * items_per_producer);
    assert(queue.empty());

    // ��֤�����ݶ�ʧ/�ظ�
    vector<bool> items_present(num_producers * items_per_producer, false);
    for (int item : consumer_items) {
        assert(!items_present[item]);  // ����ظ�
        /*if (items_present[item])
            cout << item << endl;*/
        items_present[item] = true;
    }
    assert(find(items_present.begin(), items_present.end(), false) == items_present.end());

    cout << "MPSC test passed! Items: " << consumer_items.size() << "\n" << endl;
}

// ������ʱ�����߾�������
void test_full_queue_contention() {
    cout << "===== Full Queue Contention Test =====" << endl;
    const size_t capacity = 10;  // С�������쾺��
    lfq_array_based<int> queue(capacity);

    const size_t num_threads = 4;
    atomic<int> failed_enqueues{ 0 };
    vector<thread> threads;

    // ��䲿�ֶ��У�ʣ1��λ��������ʵ������Ϊ capacity-1��
    for (int i = 0; i < capacity - 2; ++i)
        queue.enqueue(i);

    // ���������߳�
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&] {
            int v = 100;  // �����̶߳�����������ֵ
            if (!queue.enqueue(v))
                failed_enqueues.fetch_add(1, memory_order_relaxed);
            });
    }

    // �ȴ��߳̽���
    for (auto& t : threads) t.join();

    // ��ֻ֤��һ���̳߳ɹ�
    assert(failed_enqueues.load() == num_threads - 1);

    cout << "Full queue contention test passed!\n" << endl;
}

// �ƶ�������쳣��ȫ����
void test_move_semantics() {
    cout << "===== Move Semantics Test =====" << endl;
    struct MoveTracker {
        int id;
        bool moved_from = false;
        MoveTracker(int i) : id(i) {}
        MoveTracker(MoveTracker&& other) noexcept
            : id(other.id), moved_from(false) {
            other.moved_from = true;
        }
        MoveTracker& operator=(MoveTracker&& other) noexcept {
            if (this != &other) {
                id = other.id;
                moved_from = false;
                other.moved_from = true;
            }
            return *this;
        }

		MoveTracker() = default;  // Ĭ�Ϲ���
        MoveTracker(const MoveTracker&) = delete;
        MoveTracker& operator=(const MoveTracker&) = delete;
    };

    lfq_array_based<MoveTracker> queue(5);

    // �����ƶ�����
    MoveTracker obj1(100);
    queue.enqueue(std::move(obj1));
    assert(obj1.moved_from);

    // �����ƶ���ֵ
    MoveTracker obj2(200);
    queue.enqueue(static_cast<MoveTracker&&>(obj2));
    assert(obj2.moved_from);

    // ���Գ����ƶ�
    MoveTracker dest(0);
    assert(queue.dequeue(dest));
    assert(dest.id == 100 && !dest.moved_from);
    assert(queue.dequeue(dest));
    assert(dest.id == 200 && !dest.moved_from);

    cout << "Move semantics test passed!\n" << endl;
}

int main() {
    test_basic_functionality();
    test_mpsc();
    test_full_queue_contention();
    test_move_semantics();

    cout << "All tests passed successfully!" << endl;
    return 0;
}