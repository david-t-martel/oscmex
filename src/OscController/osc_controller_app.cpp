#include "AudioEngine/OscController.h"

/**
 * Standalone OSC Controller Application
 *
 * This demonstrates the decoupled architecture by providing a
 * standalone entry point for OscController without AudioEngine.
 */
int main(int argc, char *argv[])
{
    // Forward to OscController's static main method
    return AudioEngine::OscController::main(argc, argv);
}
