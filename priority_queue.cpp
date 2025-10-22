#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <atomic>


template <typename T>
class ThreadSafePriorityQueue {
private:
    std::priority_queue<T> pq;
    mutable std::mutex mtx;

public:
    ThreadSafePriorityQueue() = default;

    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        pq.push(value);
    }

    void push(T&& value) {
        std::lock_guard<std::mutex> lock(mtx);
        pq.push(std::move(value));
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (pq.empty()) return false;
        value = pq.top();
        pq.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return pq.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return pq.size();
    }
};

void priorityQueueTest() {
    ThreadSafePriorityQueue<int> pq;
    std::vector<std::thread> threads;
    const int NUM_THREADS = 4;
    const int PUSHES_PER_THREAD = 5;
    const int TOTAL_PUSHES = NUM_THREADS * PUSHES_PER_THREAD;

    std::atomic<int> pushed_count{0};
    std::atomic<int> popped_count{0};

    // RNG per thread
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 99);

    auto pusher = [&pq, &pushed_count, &gen, &dist](int id) {
        // each thread uses same generator here for simplicity
        for (int i = 0; i < PUSHES_PER_THREAD; ++i) {
            int priority = dist(gen);
            pq.push(priority);
            ++pushed_count;
            std::cout << "Pusher " << id << " pushed: " << priority << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };

    auto popper = [&pq, &popped_count]() {
        while (popped_count < TOTAL_PUSHES) {
            int value;
            if (pq.pop(value)) {
                ++popped_count;
                std::cout << "Popper popped: " << value << "\n";
            } else {
                // queue empty for now, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    };

    // start pushers
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(pusher, i);
    }

    // start single popper
    threads.emplace_back(popper);

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "All done. final size: " << pq.size() << "\n";
}

int main() {
    priorityQueueTest();
    return 0;
}
