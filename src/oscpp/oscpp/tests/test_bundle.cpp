// filepath: c:\codedev\auricleinc\oscmex\src\oscpp\tests\test_bundle.cpp
#include <iostream>
#include <cassert>
#include "osc/Bundle.h"
#include "osc/Message.h"
#include "osc/TimeTag.h"

void test_bundle_creation() {
    osc::Bundle bundle(osc::TimeTag::immediate());
    assert(!bundle.isEmpty());
    assert(bundle.size() == 0);
}

void test_bundle_add_message() {
    osc::Bundle bundle(osc::TimeTag::immediate());
    osc::Message msg("/test/message");
    msg.addInt32(123);
    
    bundle.addMessage(msg);
    assert(bundle.size() == 1);
}

void test_bundle_dispatch() {
    osc::Bundle bundle(osc::TimeTag::immediate());
    osc::Message msg("/test/message");
    msg.addInt32(456);
    
    bundle.addMessage(msg);
    
    // Simulate dispatching the bundle
    bundle.forEach([](const osc::Message &message) {
        assert(message.getPath() == "/test/message");
        assert(message.getArgumentCount() == 1);
        assert(message.getArgument(0).asInt32() == 456);
    });
}

int main() {
    std::cout << "Running Bundle tests..." << std::endl;

    test_bundle_creation();
    test_bundle_add_message();
    test_bundle_dispatch();

    std::cout << "All Bundle tests passed!" << std::endl;
    return 0;
}