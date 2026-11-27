#include "systems/JobSystem.h"
#include "systems/Profiler.h"
#include <algorithm>

namespace eng::systems {
thread_local JobSystem* JobSystem::s_pool = nullptr;
thread_local std::size_t JobSystem::s_index = 0;
JobSystem::JobSystem(std::size_t workers) {
    if (!workers) workers = std::min(8u, std::max(1u, std::thread::hardware_concurrency()));
    for (std::size_t i = 0; i < workers; ++i) m_queues.push_back(std::make_unique<Queue>());
    try {
        for (std::size_t i = 0; i < workers; ++i) m_workers.emplace_back([this, i] { Worker(i); });
    } catch (...) {
        { std::lock_guard<std::mutex> lock(m_state); m_stopping = true; }
        m_wake.notify_all();
        for (auto& worker : m_workers) worker.join();
        throw;
    }
}
JobSystem::~JobSystem() {
    { std::lock_guard<std::mutex> lock(m_state); m_stopping = true; }
    m_wake.notify_all();
    for (auto& worker : m_workers) worker.join();
}
void JobSystem::Enqueue(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> state(m_state);
        if (m_stopping) throw std::runtime_error("job system is shutting down");
        const auto index = s_pool == this ? s_index : m_next.fetch_add(1) % m_queues.size();
        auto& queue = *m_queues[index];
        std::lock_guard<std::mutex> lock(queue.mutex);
        queue.jobs.push_back(std::move(fn));
        ++m_outstanding;
        ++m_ready;
    }
    m_wake.notify_one();
}
bool JobSystem::TryRun() {
    const bool owner = s_pool == this;
    const auto start = owner ? s_index : m_next.fetch_add(1) % m_queues.size();
    for (std::size_t n = 0; n < m_queues.size(); ++n) {
        const auto i = (start + n) % m_queues.size();
        std::function<void()> job;
        {
            std::lock_guard<std::mutex> lock(m_queues[i]->mutex);
            auto& jobs = m_queues[i]->jobs;
            if (jobs.empty()) continue;
            if (owner && i == start) { job = std::move(jobs.back()); jobs.pop_back(); }
            else { job = std::move(jobs.front()); jobs.pop_front(); ++m_stolen; }
            --m_ready;
        }
        { GD_ZONE("Job execution"); job(); } // packaged_task captures user exceptions
        { std::lock_guard<std::mutex> lock(m_state); --m_outstanding; }
        m_wake.notify_all();
        return true;
    }
    return false;
}
void JobSystem::Worker(std::size_t index) {
    s_pool = this; s_index = index;
    for (;;) {
        if (TryRun()) continue;
        std::unique_lock<std::mutex> lock(m_state);
        m_wake.wait(lock, [this] { return m_ready != 0 || (m_stopping && m_outstanding == 0); });
        if (m_stopping && m_outstanding == 0) break;
    }
    s_pool = nullptr;
}
void JobSystem::WaitIdle() {
    while (m_outstanding != 0) {
        if (!TryRun()) {
            std::unique_lock<std::mutex> lock(m_state);
            m_wake.wait_for(lock, std::chrono::milliseconds(1), [this] { return m_outstanding == 0 || m_ready != 0; });
        }
    }
}
}
