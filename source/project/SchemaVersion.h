#pragma once

namespace game
{
    // Bump when the on-disk project / drill schema changes in a way that needs a
    // migration. CourseProject::Load refuses a file whose schemaVersion is newer
    // than this and migrates older ones where it can.
    constexpr int kProjectSchemaVersion = 1;
    constexpr int kDrillSchemaVersion = 1;
}
