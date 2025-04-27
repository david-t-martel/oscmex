#include "osc/ServerThread.h"
#include "osc/Message.h"
#include "osc/Value.h"
#include <iostream>
#include <chrono>
#include <thread>

int main()
{
    try
    {
        // Create a server thread listening on UDP port 8000
        osc::ServerThread server("8000", osc::Protocol::UDP);

        // Add a message handler for "/test/path" expecting int, float, string
        server.addMethod("/test/path", "ifs", [](const osc::Message &message)
                         {
            try {
                // Access arguments
                int32_t intVal = message.getArgument(0).asInt32();
                float floatVal = message.getArgument(1).asFloat();
                std::string strVal = message.getArgument(2).asString();

                std::cout << "Received /test/path: "
                          << intVal << ", " << floatVal << ", \"" << strVal << "\"" << std::endl;
            } catch (const osc::OSCException& e) {
                 std::cerr << "Error processing message args: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                 std::cerr << "Standard exception in handler: " << e.what() << std::endl;
            } });

        // Add a handler for all messages under /test
        server.addMethod("/test/*", "", [](const osc::Message &message)
                         { std::cout << "Received message at " << message.getPath()
                                     << " with " << message.getArgumentCount() << " arguments" << std::endl; });

        // Add a default handler for other messages
        server.addDefaultMethod([](const osc::Message &message)
                                { std::cout << "Received unhandled message: " << message.getPath() << std::endl; });

        // Set an error handler
        server.setErrorHandler([](int code, const std::string &msg, const std::string &where)
                               { std::cerr << "Server Error " << code << " in " << where << ": " << msg << std::endl; });

        // Start the server thread
        if (server.start())
        {
            std::cout << "Server started on UDP port " << server.port() << std::endl;
            std::cout << "URL: " << server.url() << std::endl;

            // Keep the main thread alive (e.g., wait for user input)
            std::cout << "Press Enter to exit." << std::endl;
            std::cin.get();

            server.stop();
            std::cout << "Server stopped." << std::endl;
        }
        else
        {
            std::cerr << "Failed to start server." << std::endl;
            return 1;
        }
    }
    catch (const osc::OSCException &e)
    {
        std::cerr << "OSC Initialization Error: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
