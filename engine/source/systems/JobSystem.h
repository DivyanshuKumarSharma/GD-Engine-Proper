#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace eng::systems {
// Mutex-protected local deques: owner pops LIFO, other workers steal FIFO.
// Wait() helps execute jobs, including nested jobs on a single-worker pool.
// Jobs own their input; scene/GL mutation stays on the calling thread.
class JobSystem {
public:
    explicit JobSystem(std::size_t workers = 0);
    ~JobSystem(); // Drains accepted work. Must be destroyed by its owner, not a job.
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    template<class F> auto Submit(F&& fn) -> std::future<std::invoke_result_t<F>> {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(fn));
        auto result = task->get_future();
        Enqueue([task] { (*task)(); });
        return result;
    }
    template<class T> T Wait(std::future<T>& result) {
        while (result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            if (!TryRun()) result.wait_for(std::chrono::milliseconds(1));
        }
        return result.get();
    }
    void WaitIdle(); // Owner only; not from inside a job.
    std::size_t WorkerCount() const { return m_workers.size(); }
    std::uint64_t StolenJobs() const { return m_stolen.load(); }
private:
    struct Queue { std::mutex mutex; std::deque<std::function<void()>> jobs; };
    void Enqueue(std::function<void()> fn);
    bool TryRun();
    void Worker(std::size_t index);
    std::vector<std::unique_ptr<Queue>> m_queues;
    std::vector<std::thread> m_workers;
    std::mutex m_state;
    std::condition_variable m_wake;
    bool m_stopping = false;
    std::atomic<std::size_t> m_ready{0}, m_outstanding{0}, m_next{0};
    std::atomic<std::uint64_t> m_stolen{0};
    static thread_local JobSystem* s_pool;
    static thread_local std::size_t s_index;
};
}
