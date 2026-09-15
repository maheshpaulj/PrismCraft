#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <algorithm>

namespace prismcraft {

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = 0)
        : m_stop(false)
    {
        unsigned int hw = std::thread::hardware_concurrency();
        m_hardwareThreads = (hw > 0) ? hw : 2;
        if (numThreads == 0) {
            numThreads = (m_hardwareThreads > 1) ? (m_hardwareThreads - 1) : 1;
        }

        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->m_queueMutex);
                        this->m_cv.wait(lock, [this]() {
                            return this->m_stop.load() || !this->m_tasks.empty();
                        });

                        if (this->m_stop.load() && this->m_tasks.empty()) {
                            return;
                        }

                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }
                    try {
                        task();
                    } catch (...) {
                    }
                }
            });
        }
    }

    void shutdown() {
        bool expected = false;
        if (m_stop.compare_exchange_strong(expected, true)) {
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                std::queue<std::function<void()>> emptyQueue;
                std::swap(m_tasks, emptyQueue);
            }
            m_cv.notify_all();
            for (std::thread& worker : m_workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            m_workers.clear();
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop.load()) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_cv.notify_one();
        return res;
    }

    void enqueueTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop.load()) return;
            m_tasks.push(std::move(task));
        }
        m_cv.notify_one();
    }

    size_t pendingTasks() {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        return m_tasks.size();
    }

    size_t threadCount() const {
        return m_workers.size();
    }

    size_t getHardwareThreads() const {
        return m_hardwareThreads;
    }

private:
    size_t m_hardwareThreads = 0;
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop;
};

} // namespace prismcraft
