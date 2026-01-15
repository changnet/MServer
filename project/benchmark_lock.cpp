#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>
#include <atomic>

// Window specific for high precision timing if needed, though std::chrono is usually fine
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const int ITERATIONS = 1000000;
const int DATA_SIZE = 256; // 256 Bytes log
char SRC[DATA_SIZE];
char DST[DATA_SIZE];

std::mutex g_mutex;
int g_counter = 0;

void benchmark_memcpy() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        memcpy(DST, SRC, DATA_SIZE);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Memcpy 256B (" << ITERATIONS << " ops): " << duration / 1000000.0 << " ms. Avg: " << duration / ITERATIONS << " ns/op" << std::endl;
}

void benchmark_mutex_uncontended() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_counter++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Mutex Uncontended (" << ITERATIONS << " ops): " << duration / 1000000.0 << " ms. Avg: " << duration / ITERATIONS << " ns/op" << std::endl;
}

void worker_thread(int id, std::atomic<bool>& start_flag) {
    while (!start_flag) {
        std::this_thread::yield();
    }
    for (int i = 0; i < ITERATIONS / 4; ++i) { // Split iterations among threads
        std::lock_guard<std::mutex> lock(g_mutex);
        g_counter++;
    }
}

void benchmark_mutex_contended() {
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag = false;
    
    // 4 Threads for contention
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker_thread, i, std::ref(start_flag));
    }

    auto start = std::chrono::high_resolution_clock::now();
    start_flag = true;
    for (auto& t : threads) {
        t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Mutex Contended 4-Threads (" << ITERATIONS << " total ops): " << duration / 1000000.0 << " ms. Avg: " << duration / ITERATIONS << " ns/op (Total Time / Total Ops)" << std::endl;
}

int main() {
    // Warmup
    memcpy(DST, SRC, DATA_SIZE);
    
    std::cout << "--- Benchmarking ---" << std::endl;
    benchmark_memcpy();
    benchmark_mutex_uncontended();
    benchmark_mutex_contended();
    
    return 0;
}

// g++ -O2 -o benchmark_lock benchmark_lock.cpp && benchmark_lock
// ./benchmark_lock
// --- Benchmarking ---
// Memcpy 256B (1000000 ops): 3.29655 ms. Avg: 3 ns/op
// Mutex Uncontended (1000000 ops): 8.55687 ms. Avg: 8 ns/op
// Mutex Contended 4-Threads (1000000 total ops): 55.4425 ms. Avg: 55 ns/op (Total Time / Total Ops)
