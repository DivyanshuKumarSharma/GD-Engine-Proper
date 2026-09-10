#pragma once

#include <string>

namespace game
{
    // "13080" -> "13,080". Small helper for score display.
    inline std::string FormatThousands(long long value)
    {
        const bool negative = value < 0;
        unsigned long long magnitude = negative
            ? static_cast<unsigned long long>(-(value + 1)) + 1ull
            : static_cast<unsigned long long>(value);

        std::string digits = std::to_string(magnitude);
        std::string out;
        int count = 0;
        for (auto it = digits.rbegin(); it != digits.rend(); ++it)
        {
            if (count != 0 && count % 3 == 0)
            {
                out.push_back(',');
            }
            out.push_back(*it);
            ++count;
        }
        if (negative)
        {
            out.push_back('-');
        }
        return std::string(out.rbegin(), out.rend());
    }
}
