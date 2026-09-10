#include "course/GameEvent.h"

namespace game
{
    const char* ToString(TargetKind kind)
    {
        switch (kind)
        {
        case TargetKind::Generic:      return "Generic";
        case TargetKind::Cube:         return "Cube";
        case TargetKind::RedTarget:    return "RedTarget";
        case TargetKind::MovingTarget: return "MovingTarget";
        }
        return "Generic";
    }

    TargetKind TargetKindFromString(const std::string& value)
    {
        if (value == "cube")         return TargetKind::Cube;
        if (value == "red")          return TargetKind::RedTarget;
        if (value == "redTarget")    return TargetKind::RedTarget;
        if (value == "moving")       return TargetKind::MovingTarget;
        if (value == "movingTarget") return TargetKind::MovingTarget;
        return TargetKind::Generic;
    }
}
