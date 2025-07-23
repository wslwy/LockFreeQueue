#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>
#include <cassert>

#include <lfq_array_based.h>

// 包含队列实现（略）
// #include "lfq_array_based.hpp" 

using namespace std;

// 单线程基本功能测试
void test_basic_functionality() {
    cout << "===== Basic Functionality Test =====" << endl;
    lfq_array_based<int> queue(5);

    assert(queue.empty());

    // 基础入队/出队测试
    assert(queue.enqueue(1));
    assert(queue.enqueue(2));
    assert(!queue.empty());

    int val;
    assert(queue.dequeue(val) && val == 1);
    assert(queue.dequeue(val) && val == 2);
    assert(queue.empty());
    assert(!queue.dequeue(val));

    // 满队列测试
    for (int i = 0; i < 4; ++i)  // 容量5实际可用4个位置
        assert(queue.enqueue(i));
    assert(!queue.enqueue(10));  // 应失败

    cout << "Basic tests passed!\n" << endl;
}

// 多生产者单消费者测试 (MPSC)
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

    // 创建生产者
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i] {
            // 等待开始信号
            while (!start_flag.load(memory_order_acquire))
                this_thread::yield();

            // 每个生产者写入特定范围的数字
            for (size_t j = 0; j < items_per_producer; ++j) {
                int item = static_cast<int>(i * items_per_producer + j);
                while (!queue.enqueue(item))
                    this_thread::yield(); // 队列满时等待
            }
            });
    }

    // 创建消费者
    thread consumer([&] {
        size_t total_items = num_producers * items_per_producer;
        for (size_t i = 0; i < total_items; ++i) {
            int val;
            while (!queue.dequeue(val))  // 队列空时等待
                this_thread::yield();

            lock_guard<mutex> lock(consumer_mtx);
            consumer_items.push_back(val);
        }
        });

    // 启动测试
    start_flag.store(true, memory_order_release);

    // 等待线程结束
    for (auto& p : producers) p.join();
    consumer.join();

    // 验证结果
    assert(consumer_items.size() == num_producers * items_per_producer);
    assert(queue.empty());

    // 验证无数据丢失/重复
    vector<bool> items_present(num_producers * items_per_producer, false);
    for (int item : consumer_items) {
        assert(!items_present[item]);  // 检测重复
        /*if (items_present[item])
            cout << item << endl;*/
        items_present[item] = true;
    }
    assert(find(items_present.begin(), items_present.end(), false) == items_present.end());

    cout << "MPSC test passed! Items: " << consumer_items.size() << "\n" << endl;
}

// 队列满时生产者竞争测试
void test_full_queue_contention() {
    cout << "===== Full Queue Contention Test =====" << endl;
    const size_t capacity = 10;  // 小容量制造竞争
    lfq_array_based<int> queue(capacity);

    const size_t num_threads = 4;
    atomic<int> failed_enqueues{ 0 };
    vector<thread> threads;

    // 填充部分队列（剩1空位）（队列实际容量为 capacity-1）
    for (int i = 0; i < capacity - 2; ++i)
        queue.enqueue(i);

    // 创建竞争线程
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&] {
            int v = 100;  // 所有线程都尝试入队这个值
            if (!queue.enqueue(v))
                failed_enqueues.fetch_add(1, memory_order_relaxed);
            });
    }

    // 等待线程结束
    for (auto& t : threads) t.join();

    // 验证只有一个线程成功
    assert(failed_enqueues.load() == num_threads - 1);

    cout << "Full queue contention test passed!\n" << endl;
}

// 移动语义和异常安全测试
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

		MoveTracker() = default;  // 默认构造
        MoveTracker(const MoveTracker&) = delete;
        MoveTracker& operator=(const MoveTracker&) = delete;
    };

    lfq_array_based<MoveTracker> queue(5);

    // 测试移动构造
    MoveTracker obj1(100);
    queue.enqueue(std::move(obj1));
    assert(obj1.moved_from);

    // 测试移动赋值
    MoveTracker obj2(200);
    queue.enqueue(static_cast<MoveTracker&&>(obj2));
    assert(obj2.moved_from);

    // 测试出队移动
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