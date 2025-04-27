#include "osc/Server.h"
#include "osc/Message.h"
#include "osc/Address.h"
#include <iostream>
#include <cassert>

void test_server_functionality() {
    // Create a server instance
    osc::Server server("8000", osc::Protocol::UDP);

    // Add a method to handle incoming messages
    server.addMethod("/test/path", "i", [](const osc::Message &message) {
        int32_t value = message.getArgument(0).asInt32();
        std::cout << "Received value: " << value << std::endl;
    });

    // Start the server
    assert(server.start());

    // Create a client address to send a test message
    osc::Address clientAddress("127.0.0.1", "8000", osc::Protocol::UDP);
    osc::Message msg("/test/path");
    msg.addInt32(42);

    // Send a test message
    assert(clientAddress.send(msg));

    // Stop the server
    server.stop();
}

int main() {
    std::cout << "Running Server tests..." << std::endl;

    try {
        test_server_functionality();
        std::cout << "All Server tests passed!" << std::endl;
        return 0;
    } catch (const osc::OSCException &e) {
        std::cerr << "OSC Exception: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 1;
}