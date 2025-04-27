// filepath: c:\codedev\auricleinc\oscmex\src\TimeTag.cpp
/*
 * OSCPP - Open Sound Control C++ (OSCPP) Library.
 * This file contains the implementation of the TimeTag class,
 * which is responsible for handling OSC time tags.
 */

#include "Types.h"
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace osc
{

    // Default constructor (immediate time tag)
    TimeTag::TimeTag() : seconds_(0), fraction_(1) {}

    // Constructor from NTP format
    TimeTag::TimeTag(uint64_t ntp)
        : seconds_((ntp >> 32) & 0xFFFFFFFF),
          fraction_(ntp & 0xFFFFFFFF) {}

    // Constructor from seconds and fraction
    TimeTag::TimeTag(uint32_t seconds, uint32_t fraction)
        : seconds_(seconds), fraction_(fraction) {}

    // Constructor from std::chrono::system_clock::time_point
    TimeTag::TimeTag(std::chrono::system_clock::time_point tp)
    {
        auto duration = tp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        auto fractional = duration.count() % 1000000000;
        seconds_ = static_cast<uint32_t>(seconds + 2208988800U); // Adjust for NTP epoch
        fraction_ = static_cast<uint32_t>((fractional * 0xFFFFFFFF) / 1000000000);
    }

    // Static method for current time
    TimeTag TimeTag::now()
    {
        return TimeTag(std::chrono::system_clock::now());
    }

    // Static method for immediate execution time
    TimeTag TimeTag::immediate()
    {
        return TimeTag();
    }

    // Convert to NTP format
    uint64_t TimeTag::toNTP() const
    {
        return (static_cast<uint64_t>(seconds_) << 32) | fraction_;
    }

    // Convert to std::chrono::system_clock::time_point
    std::chrono::system_clock::time_point TimeTag::toTimePoint() const
    {
        auto seconds_since_epoch = static_cast<int64_t>(seconds_) - 2208988800LL; // Adjust for NTP epoch
        return std::chrono::system_clock::time_point(std::chrono::seconds(seconds_since_epoch)) +
               std::chrono::nanoseconds(static_cast<int64_t>(fraction_) * 1000000000 / 0xFFFFFFFF);
    }

    // Get seconds part
    uint32_t TimeTag::seconds() const
    {
        return seconds_;
    }

    // Get fraction part
    uint32_t TimeTag::fraction() const
    {
        return fraction_;
    }

    // Check if immediate
    bool TimeTag::isImmediate() const
    {
        return seconds_ == 0 && fraction_ == 1;
    }

    // Comparison operators
    bool TimeTag::operator==(const TimeTag &other) const
    {
        return seconds_ == other.seconds_ && fraction_ == other.fraction_;
    }

    bool TimeTag::operator!=(const TimeTag &other) const
    {
        return !(*this == other);
    }

    bool TimeTag::operator<(const TimeTag &other) const
    {
        return (seconds_ < other.seconds_) || (seconds_ == other.seconds_ && fraction_ < other.fraction_);
    }

    bool TimeTag::operator>(const TimeTag &other) const
    {
        return other < *this;
    }

    bool TimeTag::operator<=(const TimeTag &other) const
    {
        return !(*this > other);
    }

    bool TimeTag::operator>=(const TimeTag &other) const
    {
        return !(*this < other);
    }

} // namespace osc