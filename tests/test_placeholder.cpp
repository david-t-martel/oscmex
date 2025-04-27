#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "osc/OSC.h"

// Simple test framework for OSC library

// Function to test TimeTag functionality
void test_time_tag()
{
    std::cout << "Testing TimeTag..." << std::endl;

    // Test immediate time tag
    osc::TimeTag immediate = osc::TimeTag::immediate();
    assert(immediate.isImmediate());

    // Test current time
    osc::TimeTag now = osc::TimeTag::now();
    assert(!now.isImmediate());

    // Test comparisons
    assert(immediate < now);
    assert(now > immediate);
    assert(immediate != now);

    // Test conversion to/from NTP format
    uint64_t ntp = now.toNTP();
    osc::TimeTag fromNTP(ntp);
    assert(fromNTP == now);

    // Test conversion to/from time point
    auto tp = now.toTimePoint();
    osc::TimeTag fromTP(tp);
    assert(fromTP == now);

    std::cout << "TimeTag tests passed." << std::endl;
}

// Function to test Value functionality
void test_value()
{
    std::cout << "Testing Value..." << std::endl;

    // Test Int32
    osc::Value int32Val(42);
    assert(int32Val.isInt32());
    assert(int32Val.asInt32() == 42);
    assert(int32Val.typeTag() == 'i');

    // Test Float
    osc::Value floatVal(3.14159f);
    assert(floatVal.isFloat());
    assert(floatVal.asFloat() == 3.14159f);
    assert(floatVal.typeTag() == 'f');

    // Test String
    osc::Value stringVal("test");
    assert(stringVal.isString());
    assert(stringVal.asString() == "test");
    assert(stringVal.typeTag() == 's');

    // Test serialization/deserialization (placeholder)
    auto serialized = int32Val.serialize();
    assert(!serialized.empty());

    std::cout << "Value tests passed." << std::endl;
}

// Function to test Message functionality
void test_message()
{
    std::cout << "Testing Message..." << std::endl;

    // Create a message
    osc::Message msg("/test/path");

    // Add arguments
    msg.addInt32(42);
    msg.addFloat(3.14159f);
    msg.addString("test");

    // Check properties
    assert(msg.getPath() == "/test/path");
    assert(msg.getArgumentCount() == 3);

    // Check arguments
    assert(msg.getArgument(0).asInt32() == 42);
    assert(msg.getArgument(1).asFloat() == 3.14159f);
    assert(msg.getArgument(2).asString() == "test");

    // Test serialization (placeholder)
    auto serialized = msg.serialize();
    assert(!serialized.empty());

    // TODO: Test deserialization once implemented

    std::cout << "Message tests passed." << std::endl;
}

// Function to test Bundle functionality
void test_bundle()
{
    std::cout << "Testing Bundle..." << std::endl;

    // Create a bundle
    osc::Bundle bundle(osc::TimeTag::immediate());

    // Create and add messages
    osc::Message msg1("/test/1");
    msg1.addInt32(1);

    osc::Message msg2("/test/2");
    msg2.addString("test");

    bundle.addMessage(msg1);
    bundle.addMessage(msg2);

    // Check properties
    assert(bundle.time().isImmediate());
    assert(!bundle.isEmpty());
    assert(bundle.size() == 2);

    // Test forEach
    int count = 0;
    bundle.forEach([&count](const osc::Message &msg)
                   {
        count++;
        if (msg.getPath() == "/test/1") {
            assert(msg.getArgument(0).asInt32() == 1);
        } else if (msg.getPath() == "/test/2") {
            assert(msg.getArgument(0).asString() == "test");
        } else {
            assert(false); // Should not happen
        } });
    assert(count == 2);

    // Test serialization (placeholder)
    auto serialized = bundle.serialize();
    assert(!serialized.empty());

    // TODO: Test deserialization once implemented

    std::cout << "Bundle tests passed." << std::endl;
}

// Function to test Address functionality (may not work in all environments)
void test_address()
{
    std::cout << "Testing Address..." << std::endl;

    // Create an address
    osc::Address addr("localhost", "8000", osc::Protocol::UDP);

    // Check properties
    assert(addr.host() == "localhost");
    assert(addr.port() == "8000");
    assert(addr.protocol() == osc::Protocol::UDP);

    // Test URL
    std::string url = addr.url();
    assert(url.find("osc.udp://") == 0);
    assert(url.find("localhost:8000") != std::string::npos);

    // Test from URL
    osc::Address fromUrl = osc::Address::fromUrl("osc.udp://localhost:9000/");
    assert(fromUrl.host() == "localhost");
    assert(fromUrl.port() == "9000");
    assert(fromUrl.protocol() == osc::Protocol::UDP);

    std::cout << "Address tests passed." << std::endl;
}

// Main test function
int main()
{
    std::cout << "Running OSC library tests..." << std::endl;

    try
    {
        // Run individual test functions
        test_time_tag();
        test_value();
        test_message();
        test_bundle();
        test_address();

        std::cout << "All tests passed!" << std::endl;
        return 0;
    }
    catch (const osc::OSCException &e)
    {
        std::cerr << "OSC Exception: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 1;
}
