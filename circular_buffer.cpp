#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <atomic>

#define BUFFER_SIZE 5

class ThreadSafeCircularBuffer {
private:
    int buffer[BUFFER_SIZE];
    int in = 0, out = 0, count = 0;
    bool closed = false;
    mutable std::mutex mtx;
    std::condition_variable not_full, not_empty;

public:
    // push blocks until space is available or buffer is closed.
    // returns false if buffer was closed.
    bool push(int value) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [this] { return count < BUFFER_SIZE || closed; });
        if (closed) return false;
        buffer[in] = value;
        in = (in + 1) % BUFFER_SIZE;
        ++count;
        lock.unlock();
        not_empty.notify_one();
        return true;
    }

    // pop blocks until an item is available or buffer is closed and empty.
    // returns false if buffer closed and no items remain.
    bool pop(int& value) {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this] { return count > 0 || closed; });
        if (count == 0) return false; // closed && empty
        value = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        --count;
        lock.unlock();
        not_full.notify_one();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mtx);
        return count == BUFFER_SIZE;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            closed = true;
        }
        not_empty.notify_all();
        not_full.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return static_cast<size_t>(count);
    }
};

void circularBufferTest() {
    ThreadSafeCircularBuffer cb;
    std::vector<std::thread> producers, consumers;

    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 2;
    const int ITEMS_PER_PRODUCER = 10;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> pushed_count{0};
    std::atomic<int> popped_count{0};

    // producers
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([p, &cb, &pushed_count]() {
            // thread-local RNG
            std::mt19937 gen((std::random_device())());
            std::uniform_int_distribution<int> dist(0, 999);
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * 1000 + dist(gen);
                if (!cb.push(value)) {
                    std::cout << "Producer " << p << " failed to push (closed)\n";
                    return;
                }
                ++pushed_count;
                std::cout << "Producer " << p << " pushed: " << value << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
            std::cout << "Producer " << p << " done\n";
        });
    }

    // consumers
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([c, &cb, &popped_count, TOTAL_ITEMS]() {
            while (true) {
                int value;
                if (!cb.pop(value)) break; // closed and empty
                int prev = popped_count.fetch_add(1) + 1;
                std::cout << "Consumer " << c << " popped: " << value
                          << " (total " << prev << ")\n";
                // optional processing delay
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                if (prev >= TOTAL_ITEMS) {
                    // we've consumed all expected items; remaining consumers will exit
                    // once buffer is drained and close() is called by main
                }
            }
            std::cout << "Consumer " << c << " exiting\n";
        });
    }

    for (auto& t : producers) t.join();
    // signal consumers to stop once producers are done and buffer drained
    cb.close();
    for (auto& t : consumers) t.join();

    std::cout << "Test complete. pushed=" << pushed_count.load()
              << " popped=" << popped_count.load()
              << " final_size=" << cb.size() << "\n";
}

int main() {
    circularBufferTest();
    return 0;
}