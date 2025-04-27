#include "osc/TimeTag.h"
#include <iostream>
#include <cassert>

void test_time_tag_now() {
    osc::TimeTag now = osc::TimeTag::now();
    assert(!now.isImmediate());
    std::cout << "Current TimeTag test passed." << std::endl;
}

void test_time_tag_immediate() {
    osc::TimeTag immediate = osc::TimeTag::immediate();
    assert(immediate.isImmediate());
    std::cout << "Immediate TimeTag test passed." << std::endl;
}

void test_time_tag_comparison() {
    osc::TimeTag immediate = osc::TimeTag::immediate();
    osc::TimeTag now = osc::TimeTag::now();
    assert(immediate < now);
    assert(now > immediate);
    assert(immediate != now);
    std::cout << "TimeTag comparison test passed." << std::endl;
}

void test_time_tag_conversion() {
    osc::TimeTag now = osc::TimeTag::now();
    uint64_t ntp = now.toNTP();
    osc::TimeTag fromNTP(ntp);
    assert(fromNTP == now);

    auto tp = now.toTimePoint();
    osc::TimeTag fromTP(tp);
    assert(fromTP == now);
    std::cout << "TimeTag conversion test passed." << std::endl;
}

int main() {
    std::cout << "Running TimeTag tests..." << std::endl;

    test_time_tag_now();
    test_time_tag_immediate();
    test_time_tag_comparison();
    test_time_tag_conversion();

    std::cout << "All TimeTag tests passed!" << std::endl;
    return 0;
}