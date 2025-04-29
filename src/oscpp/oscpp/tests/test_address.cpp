#include <cassert>
#include <iostream>

#include "osc/Address.h"

void test_address_creation() {
    osc::Address addr("localhost", "8000", osc::Protocol::UDP);
    assert(addr.host() == "localhost");
    assert(addr.port() == "8000");
    assert(addr.protocol() == osc::Protocol::UDP);
}

void test_address_url() {
    osc::Address addr("localhost", "8000", osc::Protocol::UDP);
    std::string expectedUrl = "osc.udp://localhost:8000/";
    assert(addr.url() == expectedUrl);
}

void test_address_invalid() {
    osc::Address addr("invalid_host", "8000", osc::Protocol::UDP);
    assert(!addr.isValid());
}

int main() {
    std::cout << "Running Address tests..." << std::endl;

    test_address_creation();
    test_address_url();
    test_address_invalid();

    std::cout << "All Address tests passed!" << std::endl;
    return 0;
}
