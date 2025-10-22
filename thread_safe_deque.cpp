#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <utility>
#include <atomic>

template <typename T>
class ThreadSafeDeque {
private:
    std::deque<T> data;
    mutable std::mutex mtx;

public:
    ThreadSafeDeque() = default;

    // push front/back (lvalue + rvalue overloads)
    void push_front(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_front(value);
    }
    void push_front(T&& value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_front(std::move(value));
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_back(value);
    }
    void push_back(T&& value) {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_back(std::move(value));
    }

    // pop front/back; return false if empty
    bool pop_front(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) return false;
        value = std::move(data.front());
        data.pop_front();
        return true;
    }

    bool pop_back(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) return false;
        value = std::move(data.back());
        data.pop_back();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }
};
// ...existing code...

void dequeTest() {
    ThreadSafeDeque<int> dq;
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 3;
    const int ITEMS_PER_PRODUCER = 10;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> pushed_count{0};
    std::atomic<int> popped_count{0};

    // producers: some push_front, others push_back
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([p, &dq, &pushed_count]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int v = p * 100 + i;
                if (p % 2 == 0) dq.push_back(v);
                else dq.push_front(v);
                ++pushed_count;
                std::cout << "Producer " << p << " pushed: " << v << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            std::cout << "Producer " << p << " done\n";
        });
    }

    // consumers: mixed pop_front/pop_back until TOTAL_ITEMS popped
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([c, &dq, &popped_count, TOTAL_ITEMS]() {
            while (popped_count.load() < TOTAL_ITEMS) {
                int v;
                bool ok = (c % 2 == 0) ? dq.pop_front(v) : dq.pop_back(v);
                if (ok) {
                    int prev = popped_count.fetch_add(1) + 1;
                    std::cout << "Consumer " << c << " popped: " << v
                              << " (total " << prev << ")\n";
                } else {
                    // empty for now, wait a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
            std::cout << "Consumer " << c << " exiting\n";
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::cout << "Test complete. pushed=" << pushed_count.load()
              << " popped=" << popped_count.load()
              << " final_size=" << dq.size() << "\n";
}

int main() {
    dequeTest();
    return 0;
}