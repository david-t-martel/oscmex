#include "osc/Address.h"
#include "osc/Message.h"
#include "osc/Bundle.h"
#include "osc/TimeTag.h"
#include <iostream>
#include <vector>
#include <cstddef>

int main()
{
    try
    {
        // Target address: UDP to localhost port 8000
        osc::Address address("127.0.0.1", "8000", osc::Protocol::UDP);
        if (!address.isValid())
        {
            std::cerr << "Failed to initialize address: " << address.getErrorMessage() << std::endl;
            return 1;
        }

        std::cout << "Created OSC address: " << address.url() << std::endl;

        // Create an OSC message with various argument types
        osc::Message msg("/test/path");
        msg.addInt32(42);
        msg.addFloat(3.14159f);
        msg.addString("Hello, OSC!");

        // Send the message
        std::cout << "Sending message to " << address.url() << "/test/path" << std::endl;
        if (!address.send(msg))
        {
            std::cerr << "Failed to send message: " << address.getErrorMessage() << std::endl;
        }
        else
        {
            std::cout << "Message sent successfully." << std::endl;
        }

        // Create a message with other argument types
        osc::Message msg2("/test/types");
        msg2.addInt64(1000000000000LL);
        msg2.addDouble(2.718281828459045);
        msg2.addBool(true);
        msg2.addChar('X');
        msg2.addTimeTag(osc::TimeTag::now());

        // Create a MIDI message (port ID, status, data1, data2)
        std::array<uint8_t, 4> midiMsg = {0, 0x90, 60, 100}; // Note on, middle C, velocity 100
        msg2.addMidi(midiMsg);

        // Send the second message
        std::cout << "Sending message to " << address.url() << "/test/types" << std::endl;
        if (!address.send(msg2))
        {
            std::cerr << "Failed to send message: " << address.getErrorMessage() << std::endl;
        }
        else
        {
            std::cout << "Message sent successfully." << std::endl;
        }

        // Create a bundle with multiple messages
        osc::Bundle bundle(osc::TimeTag::immediate()); // Immediate execution

        // Add the first message
        bundle.addMessage(msg);

        // Create and add another message directly
        osc::Message msg3("/test/another");
        msg3.addInt32(100);
        msg3.addString("Bundle message");
        bundle.addMessage(msg3);

        // Send the bundle
        std::cout << "Sending bundle to " << address.url() << std::endl;
        if (!address.send(bundle))
        {
            std::cerr << "Failed to send bundle: " << address.getErrorMessage() << std::endl;
        }
        else
        {
            std::cout << "Bundle sent successfully." << std::endl;
        }

        std::cout << "Client example completed." << std::endl;
    }
    catch (const osc::OSCException &e)
    {
        std::cerr << "OSC Error: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
