AudioNode Network Initialization Process
This document outlines the initialization process for constructing a network of AudioNodes in the AudioEngine project.

Overview
The initialization process establishes a modular audio processing network by:

Discovering hardware capabilities
Creating appropriate audio nodes
Configuring nodes with optimal settings
Establishing connections between nodes
Initializing external hardware via OSC
Detailed Initialization Flow
1. Engine Initialization
The process begins when the application initializes the AudioEngine with a configuration:

```cpp
Configuration config = loadConfiguration("config.json");
AudioEngine engine;
if (!engine.initialize(config)) {
    // Handle initialization failure
}
```

2. Hardware Discovery
The engine first loads and initializes the specified ASIO driver:

```cpp
// Inside AudioEngine::initialize()
m_asioManager = std::make_unique<AsioManager>();
if (!m_asioManager->loadDriver(config.audioDevice)) {
    logError("Failed to load ASIO driver");
    return false;
}

if (!m_asioManager->initDevice()) {
    logError("Failed to initialize ASIO device");
    return false;
}
```

During this phase, the hardware capabilities are discovered:

Supported sample rates
Optimal buffer size
Available channels
Hardware latency
3. Auto-Configuration
The engine adapts its processing settings based on hardware capabilities:

```cpp
// Inside AudioEngine::initialize()
long bufferSize;
double sampleRate;
m_asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate);

// Use these values to configure internal processing
m_bufferSize = bufferSize;
m_sampleRate = sampleRate;
m_internalFormat = AV_SAMPLE_FMT_FLTP; // Float planar format for internal processing
```

4. ASIO Buffer Creation
Once settings are determined, ASIO buffers are created for the required channels:

```cpp
// Inside AudioEngine::initialize()
std::vector<long> inputChannels = mapChannelsFromConfig(config.inputChannelMap);
std::vector<long> outputChannels = mapChannelsFromConfig(config.outputChannelMap);

if (!m_asioManager->createBuffers(inputChannels, outputChannels)) {
    logError("Failed to create ASIO buffers");
    return false;
}
```

5. Node Creation
Based on the configuration, the engine creates the required AudioNode instances:

```cpp
// Inside AudioEngine::initialize()
bool success = createAndConfigureNodes();
if (!success) {
    logError("Failed to create and configure nodes");
    return false;
}
```

This includes:

Creating ASIO source nodes for hardware inputs
Creating ASIO sink nodes for hardware outputs
Creating file source nodes for audio file playback
Creating file sink nodes for recording
Creating processor nodes for effects and transformations
6. Node Configuration
Each node is configured with the appropriate settings:

```cpp
// Inside AudioEngine::createAndConfigureNodes()
for (auto& nodeConfig : m_config.nodes) {
    std::shared_ptr<AudioNode> node = createNode(nodeConfig.type);
    if (!node) {
        logError("Failed to create node of type: " + nodeConfig.type);
        return false;
    }

    if (!node->configure(nodeConfig.params, m_sampleRate, m_bufferSize,
                        m_internalFormat, m_internalLayout)) {
        logError("Failed to configure node: " + nodeConfig.name);
        return false;
    }

    m_nodes.push_back(node);
    m_nodeMap[nodeConfig.name] = node.get();
}
```

7. Connection Establishment
The engine establishes connections between nodes according to the configuration:

```cpp
// Inside AudioEngine::setupConnections()
for (auto& connectionConfig : m_config.connections) {
    AudioNode* sourceNode = m_nodeMap[connectionConfig.sourceName];
    AudioNode* sinkNode = m_nodeMap[connectionConfig.sinkName];

    if (!sourceNode || !sinkNode) {
        logError("Invalid connection - node not found");
        return false;
    }

    m_connections.emplace_back(sourceNode, connectionConfig.sourcePad,
                             sinkNode, connectionConfig.sinkPad);
}
```

8. Process Order Calculation
The engine determines the optimal order for processing nodes:

```cpp
// Inside AudioEngine::calculateProcessOrder()
// This is a simplified topological sort
m_processOrder.clear();
std::set<AudioNode*> processed;

// Add source nodes first (no inputs)
for (auto& node : m_nodes) {
    if (node->getType() == AudioNode::NodeType::SOURCE) {
        m_processOrder.push_back(node.get());
        processed.insert(node.get());
    }
}

// Add remaining nodes in dependency order
bool progress = true;
while (progress) {
    progress = false;
    for (auto& node : m_nodes) {
        if (processed.find(node.get()) != processed.end()) {
            continue; // Already processed
        }

        bool allInputsProcessed = true;
        // Check if all input nodes are already in the process order
        for (auto& conn : m_connections) {
            if (conn.getSinkNode() == node.get() &&
                processed.find(conn.getSourceNode()) == processed.end()) {
                allInputsProcessed = false;
                break;
            }
        }

        if (allInputsProcessed) {
            m_processOrder.push_back(node.get());
            processed.insert(node.get());
            progress = true;
        }
    }
}
```

9. External Hardware Configuration via OSC
If OSC control is enabled, the engine configures external hardware:

```cpp
// Inside AudioEngine::initialize()
if (m_config.useOsc) {
    m_oscController = std::make_unique<OscController>(m_config.oscPort);
    if (!m_oscController->initialize()) {
        logWarning("Failed to initialize OSC controller");
        // Continue anyway as this is not critical
    }

    // Send initial configuration commands
    bool oscSuccess = sendOscCommands();
    if (!oscSuccess) {
        logWarning("Some OSC commands failed to send");
        // Continue anyway as this is not critical
    }
}
```

10. Start Processing
Finally, the engine starts audio processing:

```cpp
// Inside AudioEngine::run()
// Set up ASIO callback
m_asioManager->setCallback([this](long doubleBufferIndex) {
    this->processAsioBlock(doubleBufferIndex, true);
});

// Start ASIO processing
if (!m_asioManager->start()) {
    logError("Failed to start ASIO processing");
    return false;
}

m_running = true;
return true;
```

Required Components to Implement
Based on the documentation, several components need to be developed:

AudioNode Base Class

Abstract interface for all audio processing nodes
Buffer management and configuration methods
AudioBuffer Class

Reference-counted audio data container
Support for various formats and channel layouts
Connection Class

Manages data flow between nodes
Handles buffer transfer between node pads
Node Implementations

AsioSourceNode for capturing hardware input
AsioSinkNode for hardware output
FileSourceNode for audio file playback
FileSinkNode for recording
FfmpegProcessorNode for audio processing
Configuration System

JSON-based configuration loading/saving
Structure to define nodes and connections
OscController

Interface to external hardware via OSC protocol
Command structure for device configuration

Example Configuration
Below is an example of a configuration file that defines a simple audio processing network:

```json
{
  "audioDevice": "ASIO Fireface UCX II",
  "nodes": [
    {
      "name": "mic_input",
      "type": "asio_source",
      "params": {
        "channels": "1,2"
      }
    },
    {
      "name": "eq_processor",
      "type": "ffmpeg_processor",
      "params": {
        "filter": "equalizer=f=1000:width_type=o:width=2:g=-10"
      }
    },
    {
      "name": "speaker_output",
      "type": "asio_sink",
      "params": {
        "channels": "7,8"
      }
    }
  ],
  "connections": [
    {
      "sourceName": "mic_input",
      "sourcePad": 0,
      "sinkName": "eq_processor",
      "sinkPad": 0
    },
    {
      "sourceName": "eq_processor",
      "sourcePad": 0,
      "sinkName": "speaker_output",
      "sinkPad": 0
    }
  ],
  "oscCommands": [
    {
      "address": "/1/volume",
      "arguments": [0.8]
    }
  ]
}
```

This configuration:
1. Uses the "ASIO Fireface UCX II" audio device
2. Creates three nodes:
   - An ASIO source node capturing from channels 1 and 2
   - An equalizer processor that reduces 1kHz frequencies by 10dB
   - An ASIO output node sending to channels 7 and 8
3. Connects the nodes in sequence: mic → equalizer → speakers
4. Sends an initial OSC command to set a volume control to 0.8

Conclusion
The initialization process creates a flexible, modular audio processing network based on available hardware capabilities and user configuration. By discovering hardware capabilities first, then creating and connecting audio nodes appropriately, the system ensures optimal audio quality and performance.
