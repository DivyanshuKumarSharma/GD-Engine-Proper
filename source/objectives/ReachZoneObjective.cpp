#include "objectives/ReachZoneObjective.h"

#include <utility>

namespace game
{
    ReachZoneObjective::ReachZoneObjective(std::string description, std::string zoneTag)
        : Objective(ObjectiveKind::ReachZone, std::move(description), 1)
        , m_zoneTag(std::move(zoneTag))
    {
    }

    bool ReachZoneObjective::Accepts(const GameEvent& event) const
    {
        return event.type == GameEventType::ZoneReached
            && (m_zoneTag.empty() || event.tag == m_zoneTag);
    }
}
