#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace game
{
    // Minimal multicast callback used for gameplay events (target hit, objective
    // completed, drill finished, ...). Subscribers are plain std::function.
    //
    // There is no per-connection handle: connect only callbacks that are
    // guaranteed to outlive the Signal. In this game every emitter (a Target, a
    // Drill) is destroyed together with, or before, the objects that listen to
    // it, so a bare vector of slots is sufficient and keeps the wiring readable.
    template <typename... Args>
    class Signal
    {
    public:
        using Slot = std::function<void(Args...)>;

        void Connect(Slot slot)
        {
            if (slot)
            {
                m_slots.push_back(std::move(slot));
            }
        }

        void Clear()
        {
            m_slots.clear();
        }

        bool Empty() const
        {
            return m_slots.empty();
        }

        void Emit(Args... args) const
        {
            // Iterate a copy so a slot may safely mutate this Signal.
            const auto slots = m_slots;
            for (const auto& slot : slots)
            {
                slot(args...);
            }
        }

    private:
        std::vector<Slot> m_slots;
    };
}
