#pragma once

#include <chrono>
#include <cstdint>

namespace osc
{
    /**
     * TimeTag class for handling OSC time tags.
     * OSC time tags are represented in NTP format with seconds since 1900
     * and fractional seconds.
     */
    class TimeTag
    {
    public:
        // Default constructor (immediate time tag)
        TimeTag();

        // Constructor from NTP format
        explicit TimeTag(uint64_t ntp);

        // Constructor from seconds and fraction
        TimeTag(uint32_t seconds, uint32_t fraction);

        // Constructor from std::chrono::system_clock::time_point
        explicit TimeTag(std::chrono::system_clock::time_point tp);

        // Static method for current time
        static TimeTag now();

        // Static method for immediate execution time
        static TimeTag immediate();

        // Convert to NTP format
        uint64_t toNTP() const;

        // Convert to std::chrono::system_clock::time_point
        std::chrono::system_clock::time_point toTimePoint() const;

        // Get seconds part
        uint32_t seconds() const;

        // Get fraction part
        uint32_t fraction() const;

        // Check if immediate
        bool isImmediate() const;

        // Comparison operators
        bool operator==(const TimeTag &other) const;
        bool operator!=(const TimeTag &other) const;
        bool operator<(const TimeTag &other) const;
        bool operator>(const TimeTag &other) const;
        bool operator<=(const TimeTag &other) const;
        bool operator>=(const TimeTag &other) const;

    private:
        uint32_t seconds_;
        uint32_t fraction_;
    };

} // namespace osc
