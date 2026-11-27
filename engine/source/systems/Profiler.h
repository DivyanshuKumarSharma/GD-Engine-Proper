#pragma once
#ifdef GD_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#define GD_ZONE(name) ZoneScopedN(name)
#define GD_FRAME() FrameMark
#else
#define GD_ZONE(name) ((void)0)
#define GD_FRAME() ((void)0)
#endif
