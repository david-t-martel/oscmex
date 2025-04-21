# Decoupling OscController from AudioEngine

## Architecture Updates

To make the `OscController` independent from the main `AudioEngine` while maintaining integration capabilities, the following architectural changes are required:

### 1. Core Design Changes

#### Independent Component Design

- **OscController as Standalone Component**: The `OscController` should be designed as a fully independent component with its own entry point, configuration system, and lifecycle management.
- **Optional Integration**: The `AudioEngine` should be designed to operate with or without an `OscController`.
- **Loose Coupling**: Communication between components should use interfaces and callbacks rather than direct dependencies.

#### Communication Patterns

1. **Interface-based Integration**:
   - Define interfaces for communication between components
   - Use dependency injection to provide OscController to AudioEngine when needed

2. **Event-Based Communication**:
   - Implement event/callback systems for two-way communication
   - Components raise events that others can subscribe to

3. **Message Queue**:
   - Add an optional intermediate message queue for asynchronous communication
   - Messages can be serialized for network distribution if needed

### 2. Component Structure

#### OscController (Independent)

```
OscController
├── Core Functionality
│   ├── OSC Message Handling
│   ├── Device Communication
│   └── Configuration Management
├── Standalone Operation
│   ├── Command-line Interface
│   ├── Configuration File Handling
│   └── Independent Lifecycle Management
└── Integration Points
    ├── Event System
    ├── Parameter Interface
    └── External Control Interface
```

#### AudioEngine (Independent)

```
AudioEngine
├── Core Audio Processing
│   ├── Audio Node Management
│   ├── Connection Handling
│   └── Processing Pipeline
├── Optional Control Interface
│   ├── Parameter Management
│   ├── Event System
│   └── External Control Adapter
└── Configuration Management
    ├── Audio-specific Configuration
    └── Optional Control-specific Configuration
```

## Implementation Strategy

### 1. Interface Definitions

Create interfaces to define the contract between components:

```cpp
// IExternalControl.h
class IExternalControl {
public:
    virtual ~IExternalControl() = default;

    // Parameter control
    virtual bool setParameter(const std::string& address, const std::vector<std::any>& args) = 0;
    virtual bool getParameter(const std::string& address, std::function<void(bool, const std::vector<std::any>&)> callback) = 0;

    // Batch operations
    virtual bool applyConfiguration(const Configuration& config) = 0;
    virtual bool queryDeviceState(std::function<void(bool, const Configuration&)> callback) = 0;

    // Event subscription
    virtual int addEventCallback(std::function<void(const std::string&, const std::vector<std::any>&)> callback) = 0;
    virtual void removeEventCallback(int callbackId) = 0;
};
```

### 2. OscController Updates

1. **Standalone Operation**:
   - Implement a main entry point for standalone operation
   - Add command-line argument parsing
   - Create an independent configuration loader

2. **Implementation of Interface**:
   - Make OscController implement the IExternalControl interface
   - Keep all OSC-specific logic encapsulated

```cpp
// OscController.h
class OscController : public IExternalControl {
public:
    // Constructor for standalone use
    OscController();

    // Configuration
    bool configure(const std::string& configFile);
    bool configure(const std::string& targetIp, int targetPort, int receivePort = 0);

    // IExternalControl implementation
    bool setParameter(const std::string& address, const std::vector<std::any>& args) override;
    bool getParameter(const std::string& address, std::function<void(bool, const std::vector<std::any>&)> callback) override;
    bool applyConfiguration(const Configuration& config) override;
    bool queryDeviceState(std::function<void(bool, const Configuration&)> callback) override;
    int addEventCallback(std::function<void(const std::string&, const std::vector<std::any>&)> callback) override;
    void removeEventCallback(int callbackId) override;

    // Standalone methods
    static int main(int argc, char* argv[]);

private:
    // ...existing code...
};
```

### 3. AudioEngine Updates

1. **Optional OscController Integration**:
   - Make OscController optional in initialization
   - Implement fallback behavior when OscController is not available
   - Add accessor for adding external control later

```cpp
// AudioEngine.h
class AudioEngine {
public:
    // Constructor without external control
    AudioEngine();

    // Initialize with optional external control
    bool initialize(Configuration config, std::shared_ptr<IExternalControl> externalControl = nullptr);

    // Add external control after initialization
    bool setExternalControl(std::shared_ptr<IExternalControl> externalControl);

    // Remove external control
    void clearExternalControl();

    // ...existing code...

private:
    // Optional external control component
    std::shared_ptr<IExternalControl> m_externalControl;

    // Helper method for OSC operations with fallback
    bool sendExternalCommand(const std::string& address, const std::vector<std::any>& args);

    // ...existing code...
};
```

### 4. Configuration System Updates

1. **Split Configuration**:
   - Separate audio-specific from control-specific configuration
   - Allow loading each part independently

```cpp
// Configuration.h
class Configuration {
public:
    // Load full configuration
    bool loadFromFile(const std::string& filePath);

    // Load only audio or control configuration
    bool loadAudioConfigFromFile(const std::string& filePath);
    bool loadControlConfigFromFile(const std::string& filePath);

    // Save full or partial configuration
    bool saveToFile(const std::string& filePath) const;
    bool saveAudioConfigToFile(const std::string& filePath) const;
    bool saveControlConfigToFile(const std::string& filePath) const;

    // Extract parts of configuration
    Configuration getAudioConfiguration() const;
    Configuration getControlConfiguration() const;

    // ...existing code...
};
```

## Standalone OscController Entry Point

Create a new entry point for standalone OscController operation:

```cpp
// osc_controller_main.cpp
#include "OscController.h"
#include <iostream>

int main(int argc, char* argv[]) {
    return OscController::main(argc, argv);
}
```

```cpp
// OscController.cpp (main implementation)
int OscController::main(int argc, char* argv[]) {
    std::cout << "RME OSC Controller Standalone Mode" << std::endl;

    // Parse command line arguments
    std::string configFile;
    std::string targetIp = "127.0.0.1";
    int targetPort = 7001;
    int receivePort = 9001;
    bool interactive = false;

    // Command line parsing...

    // Create and configure controller
    OscController controller;

    if (!configFile.empty()) {
        if (!controller.configure(configFile)) {
            std::cerr << "Failed to load configuration from " << configFile << std::endl;
            return 1;
        }
    } else {
        if (!controller.configure(targetIp, targetPort, receivePort)) {
            std::cerr << "Failed to configure OSC controller" << std::endl;
            return 1;
        }
    }

    if (interactive) {
        // Interactive command loop
        std::string command;
        std::cout << "Enter commands (type 'help' for available commands, 'quit' to exit):" << std::endl;

        while (true) {
            std::cout << "> ";
            std::getline(std::cin, command);

            if (command == "quit" || command == "exit") {
                break;
            }

            // Process commands...
        }
    } else {
        // Run until signal received
        std::cout << "Running in daemon mode. Press Ctrl+C to exit." << std::endl;

        // Set up signal handler
        // ...

        // Wait for signal
        // ...
    }

    return 0;
}
```

## Simplified Integration Example

```cpp
// Example integration in application code
#include "AudioEngine.h"
#include "OscController.h"
#include "Configuration.h"

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool useOsc = true; // Could be from command line
    std::string configFile = "config.json";

    // Load configuration
    Configuration config;
    if (!config.loadFromFile(configFile)) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }

    // Create AudioEngine
    AudioEngine engine;

    // Optionally create OscController
    std::shared_ptr<IExternalControl> externalControl;
    if (useOsc) {
        auto oscController = std::make_shared<OscController>();
        if (!oscController->configure(config.getTargetIp(), config.getTargetPort(), config.getReceivePort())) {
            std::cerr << "Warning: Failed to configure OSC controller, continuing without OSC" << std::endl;
        } else {
            externalControl = oscController;
        }
    }

    // Initialize engine with optional controller
    if (!engine.initialize(config, externalControl)) {
        std::cerr << "Failed to initialize audio engine" << std::endl;
        return 1;
    }

    // Run engine
    engine.run();

    // Main application loop
    // ...

    // Cleanup
    engine.stop();

    return 0;
}
```

## Benefits of Decoupled Design

1. **Independent Operation**: Both components can run independently for different use cases
2. **Simplified Testing**: Components can be tested in isolation
3. **Reduced Dependencies**: AudioEngine has fewer required dependencies
4. **Greater Flexibility**: Different control mechanisms can be swapped without changing AudioEngine
5. **Cleaner Architecture**: Better separation of concerns

## Suggested Code Updates

1. **Create Interface Files**:
   - `include/IExternalControl.h`: Define control interface
   - `include/IControlListener.h`: Define event listener interface

2. **Update OscController**:
   - Implement interfaces
   - Add standalone operation support
   - Enhance configuration handling

3. **Update AudioEngine**:
   - Make OscController dependency optional
   - Add fallback behavior
   - Use interface rather than concrete class

4. **Create New Executable**:
   - Build a standalone OscController application
   - Provide command-line interface
   - Support daemon mode operation
