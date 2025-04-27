// filepath: c:\codedev\auricleinc\oscmex\src\oscpp\tests\test_message.cpp
#include <iostream>
#include <cassert>
#include "osc/Message.h"
#include "osc/Value.h"

void test_message_creation() {
    osc::Message msg("/test/path");
    assert(msg.getPath() == "/test/path");
    assert(msg.getArgumentCount() == 0);
}

void test_message_arguments() {
    osc::Message msg("/test/path");
    msg.addInt32(42);
    msg.addFloat(3.14f);
    msg.addString("Hello");

    assert(msg.getArgumentCount() == 3);
    assert(msg.getArgument(0).asInt32() == 42);
    assert(msg.getArgument(1).asFloat() == 3.14f);
    assert(msg.getArgument(2).asString() == "Hello");
}

void test_message_serialization() {
    osc::Message msg("/test/path");
    msg.addInt32(42);
    msg.addFloat(3.14f);

    auto serialized = msg.serialize();
    assert(!serialized.empty());
}

int main() {
    std::cout << "Running Message tests..." << std::endl;

    test_message_creation();
    test_message_arguments();
    test_message_serialization();

    std::cout << "All Message tests passed!" << std::endl;
    return 0;
}