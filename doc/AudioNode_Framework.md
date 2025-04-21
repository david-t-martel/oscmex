# AudioNode Framework Documentation

## Overview

The AudioNode framework is the core architecture of the AudioEngine project, providing a modular, flexible system for audio processing. It uses a node-based design pattern where discrete processing units (nodes) are connected to form an audio processing graph. This document explains the framework's components and how they interact.

## Core Components

### AudioNode (Base Class)

`AudioNode` is the abstract base class that defines the common interface for all audio processing nodes:

```cpp
class AudioNode {
public:
    enum class NodeType {
        SOURCE,
        SINK,
        PROCESSOR
    };

    // Core interface methods
    virtual bool configure(const std::map<std::string, std::string> &params,
                          double sampleRate, long bufferSize,
                          AVSampleFormat format, AVChannelLayout layout) = 0;
    virtual bool start() = 0;
    virtual void process() = 0;
    virtual void stop() = 0;
    virtual void cleanup() = 0;

    // Buffer I/O methods
    virtual std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) = 0;
    virtual bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) = 0;

    // State and utility methods
    virtual bool isConfigured() const;
    virtual bool isRunning() const;
    virtual std::string getLastError() const;
    // ...
};
```

Key responsibilities:

- **Configuration**: Each node can be configured with parameters, sample rate, buffer size, and format information
- **Lifecycle Management**: Provides consistent start/stop/cleanup operations
- **Buffer I/O**: Standardizes how nodes exchange audio data through input/output "pads"
- **State Reporting**: Offers methods to check node status and error conditions

### AudioBuffer

`AudioBuffer` is a reference-counted container for audio data that handles various sample formats and channel layouts:

```cpp
class AudioBuffer {
private:
    std::vector<std::vector<uint8_t>> m_data; // Planar data storage
    long m_frames;
    double m_sampleRate;
    AVSampleFormat m_format;
    AVChannelLayout m_channelLayout;
    std::atomic<int> m_refCount;

public:
    static std::shared_ptr<AudioBuffer> allocate(long frames, double sampleRate,
                                               AVSampleFormat format,
                                               AVChannelLayout layout);
    std::shared_ptr<AudioBuffer> createReference();
    void free();

    // Data access methods
    uint8_t* getPlaneData(int plane);
    const uint8_t* getPlaneData(int plane) const;
    // ...
};
```

Key features:

- **Reference Counting**: Allows buffers to be shared between nodes without unnecessary copying
- **Format Flexibility**: Supports various audio formats using FFmpeg's AVSampleFormat and AVChannelLayout
- **Data Access**: Provides safe methods to access the underlying audio data
- **Memory Management**: Efficiently handles allocation and freeing of audio data

### Connection

The `Connection` class represents a link between two nodes' pads, allowing audio to flow from one node to another:

```cpp
class Connection {
private:
    AudioNode* m_sourceNode;
    int m_sourcePad;
    AudioNode* m_sinkNode;
    int m_sinkPad;

public:
    Connection(AudioNode* source, int sourcePad, AudioNode* sink, int sinkPad);

    bool transfer(); // Moves data from source to sink
    // ...
};
```

Key responsibilities:

- **Topology Definition**: Defines how nodes are connected in the processing graph
- **Data Transfer**: Facilitates the movement of `AudioBuffer` instances between nodes
- **Format Compatibility**: Ensures connected nodes can exchange data correctly

### Node Types

The system includes several specialized node types derived from `AudioNode`:

#### AsioSourceNode

Captures audio from ASIO hardware inputs:

```cpp
class AsioSourceNode : public AudioNode {
private:
    AsioManager* m_asioManager;
    std::vector<long> m_channelIndices;
    std::shared_ptr<AudioBuffer> m_outputBuffer;
    // ...

public:
    bool receiveAsioData(long doubleBufferIndex, void** bufferPtrs);
    // ... AudioNode interface implementation
};
```

Key features:

- **ASIO Integration**: Captures audio from specific ASIO hardware channels
- **Format Conversion**: Converts from ASIO's native format to the engine's internal format
- **Buffer Management**: Creates `AudioBuffer` instances from raw ASIO data

#### AsioSinkNode

Sends audio to ASIO hardware outputs:

```cpp
class AsioSinkNode : public AudioNode {
private:
    AsioManager* m_asioManager;
    std::vector<long> m_channelIndices;
    std::shared_ptr<AudioBuffer> m_inputBuffer;
    // ...

public:
    bool provideAsioData(long doubleBufferIndex, void** bufferPtrs);
    // ... AudioNode interface implementation
};
```

Key features:

- **ASIO Integration**: Outputs audio to specific ASIO hardware channels
- **Format Conversion**: Converts from the engine's internal format to ASIO's native format
- **Buffer Consumption**: Uses `AudioBuffer` instances to feed data to ASIO

#### FileSourceNode

Reads and decodes audio from files:

```cpp
class FileSourceNode : public AudioNode {
private:
    std::string m_filePath;
    std::thread m_readerThread;
    std::queue<std::shared_ptr<AudioBuffer>> m_outputQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondVar;
    // FFmpeg structures for decoding
    AVFormatContext* m_formatCtx;
    AVCodecContext* m_codecCtx;
    // ...

public:
    // ... AudioNode interface implementation
    bool seekTo(double position);
    double getCurrentPosition();
    double getDuration();
};
```

Key features:

- **File Decoding**: Uses FFmpeg to read and decode audio files in various formats
- **Threaded Operation**: Runs in a dedicated thread to avoid blocking the audio processing chain
- **Buffer Queueing**: Maintains a thread-safe queue of decoded audio buffers
- **Transport Control**: Provides seeking and position reporting

#### FileSinkNode

Encodes and writes audio to files:

```cpp
class FileSinkNode : public AudioNode {
private:
    std::string m_filePath;
    std::thread m_writerThread;
    std::queue<std::shared_ptr<AudioBuffer>> m_inputQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondVar;
    // FFmpeg structures for encoding
    AVFormatContext* m_formatCtx;
    AVCodecContext* m_codecCtx;
    // ...

public:
    // ... AudioNode interface implementation
    void flush();
};
```

Key features:

- **File Encoding**: Uses FFmpeg to encode and write audio to files in various formats
- **Threaded Operation**: Runs in a dedicated thread to avoid blocking the audio processing chain
- **Buffer Queueing**: Maintains a thread-safe queue of buffers to be encoded
- **Finalization**: Handles proper file finalization with flush and trailer writing

#### FfmpegProcessorNode

Processes audio using FFmpeg's filter capabilities:

```cpp
class FfmpegProcessorNode : public AudioNode {
private:
    FfmpegFilter m_filter;
    std::string m_filterDescription;
    std::shared_ptr<AudioBuffer> m_inputBuffer;
    std::shared_ptr<AudioBuffer> m_outputBuffer;
    std::mutex m_processMutex;
    // ...

public:
    // ... AudioNode interface implementation
    bool updateParameter(const std::string& filterName,
                        const std::string& paramName,
                        float value);
};
```

Key features:

- **DSP Processing**: Applies audio effects and transformations using FFmpeg's filter graph
- **Dynamic Parameter Control**: Allows real-time updates to filter parameters
- **Complex Chaining**: Supports complex chains of multiple filters
- **Thread Safety**: Ensures safe parameter updates during processing

### FfmpegFilter

The `FfmpegFilter` class wraps FFmpeg's filter graph system for audio processing:

```cpp
class FfmpegFilter {
private:
    AVFilterGraph* m_graph;
    AVFilterContext* m_srcContext;
    AVFilterContext* m_sinkContext;
    std::map<std::string, AVFilterContext*> m_namedFilters;
    // ...

public:
    bool initGraph(const std::string& filterDescription,
                  double sampleRate, AVSampleFormat format,
                  AVChannelLayout layout);
    bool process(AVFrame* inputFrame, AVFrame* outputFrame);
    bool updateParameter(const std::string& filterName,
                        const std::string& paramName,
                        float value);
    // ...
};
```

Key features:

- **Filter Graph Management**: Creates and manages FFmpeg filter graphs
- **Audio Processing**: Applies filters to audio data frames
- **Parameter Control**: Allows dynamic updates to filter parameters
- **Resource Management**: Handles proper cleanup of FFmpeg resources

### AsioManager

The `AsioManager` class handles interaction with ASIO audio hardware:

```cpp
class AsioManager {
private:
    bool m_driverLoaded;
    bool m_asioInitialized;
    bool m_buffersCreated;
    bool m_processing;
    long m_bufferSize;
    double m_sampleRate;
    ASIOSampleType m_sampleType;
    std::function<void(long)> m_callback;
    // ...

public:
    bool loadDriver(const std::string& deviceName);
    bool initDevice(double preferredSampleRate = 0, long preferredBufferSize = 0);
    bool createBuffers(const std::vector<long>& inputChannels,
                      const std::vector<long>& outputChannels);
    bool start();
    void stop();
    bool getBufferPointers(long doubleBufferIndex,
                          const std::vector<long>& channelIndices,
                          std::vector<void*>& bufferPtrs);
    void setCallback(std::function<void(long)> callback);
    // ...
};
```

Key features:

- **Driver Management**: Loads and initializes ASIO drivers
- **Buffer Management**: Creates and manages ASIO hardware buffers
- **Callback System**: Provides notification when audio data is available
- **Auto-configuration**: Can query optimal settings from the hardware
- **Channel Selection**: Activates only the specific channels needed by the nodes

## AudioEngine (Central Coordinator)

The `AudioEngine` class orchestrates the entire audio processing pipeline:

```cpp
class AudioEngine {
private:
    // Configuration
    Configuration m_config;
    std::unique_ptr<AsioManager> m_asioManager;
    std::unique_ptr<OscController> m_oscController;

    // Node management
    std::vector<std::shared_ptr<AudioNode>> m_nodes;
    std::map<std::string, AudioNode*> m_nodeMap;
    std::vector<Connection> m_connections;
    std::vector<AudioNode*> m_processOrder;

    // Processing state
    std::atomic<bool> m_running;
    std::thread m_processingThread;
    std::atomic<bool> m_stopProcessingThread;
    std::mutex m_processMutex;

    // Internal format and layout
    double m_sampleRate;
    long m_bufferSize;
    AVSampleFormat m_internalFormat;
    AVChannelLayout m_internalLayout;

    // Helper methods
    bool autoConfigureAsio();
    bool createAndConfigureNodes();
    bool setupConnections();
    bool sendOscCommands();
    bool calculateProcessOrder();
    void runFileProcessingLoop();
    // ...

public:
    bool initialize(Configuration config);
    bool run();
    void stop();
    void cleanup();
    bool processAsioBlock(long doubleBufferIndex, bool directProcess);
    // ...
};
```

Key responsibilities:

- **Initialization**: Sets up the hardware and creates nodes based on configuration
- **Node Management**: Creates, configures and connects audio processing nodes
- **Processing Control**: Manages the processing loop and data flow
- **Auto-configuration**: Can automatically configure based on hardware capabilities
- **Thread Management**: Handles threading for file-based and ASIO modes
- **OSC Integration**: Facilitates OSC-based control of audio hardware

## Data Flow Through the System

### Initialization Phase

1. The `AudioEngine` is initialized with a `Configuration` object
2. The `AsioManager` is created and initialized with hardware settings:
   - It first connects to the audio device and retrieves hardware capabilities
   - It then configures sample rate and buffer size based on hardware recommendations
   - It creates hardware buffers for the required channels
3. Audio processing nodes are created and configured based on the configuration
4. Connections between nodes are established
5. The `OscController` sends initial OSC commands to configure external hardware

### Processing Phase (ASIO Mode)

1. The ASIO hardware triggers a callback when audio data is available
2. The `AsioManager` calls the `AudioEngine::processAsioBlock` method
3. Each `AsioSourceNode` receives raw ASIO data and creates `AudioBuffer` instances
4. The `AudioEngine` processes each node in the calculated order
5. Each processing node:
   - Gets input buffers from its input pads
   - Processes the audio
   - Makes output buffers available on its output pads
6. `Connection` objects transfer data between connected nodes
7. `AsioSinkNode` instances provide data back to the ASIO hardware
8. The cycle repeats for each audio block

### Processing Phase (File Mode)

1. The `AudioEngine` runs a timed processing loop in a dedicated thread
2. `FileSourceNode` instances read and decode audio data in their own threads
3. The main processing loop coordinates the flow of data similar to ASIO mode
4. `FileSinkNode` instances encode and write output data in their own threads
5. Processing continues until all file sources reach the end of file or the engine is stopped

## Thread Model

The system employs multiple threads to ensure efficient audio processing:

1. **Main Thread**: Handles application logic and user interaction
2. **ASIO Callback Thread**: Driven by the ASIO hardware, processes audio in real time
3. **File Processing Thread**: For non-ASIO modes, runs a timed loop to process audio
4. **File Reader Threads**: Each `FileSourceNode` has its own thread for decoding
5. **File Writer Threads**: Each `FileSinkNode` has its own thread for encoding
6. **OSC Receiver Thread**: Listens for incoming OSC messages

Thread synchronization is handled through mutexes, atomic variables, and condition variables to ensure thread safety.

## Memory Management

The system uses reference counting to efficiently manage audio data:

1. `AudioBuffer` instances are reference-counted with std::shared_ptr
2. When a buffer is passed between nodes, only the reference count increases
3. Buffers are automatically freed when the last reference is released
4. Nodes can create new buffer references without copying the underlying data
5. This avoids unnecessary memory allocation and copying during audio processing

## Dual-Phase Device Configuration

The system implements a two-phase device configuration approach:

1. **Hardware Connection & Auto-Configuration Phase**:
   - When the engine starts, it first connects to the specified audio device via `AsioManager`
   - Once connected, `AsioManager` queries the device driver for optimal settings
   - This information is used to configure the engine's audio processing pipeline
   - The `AudioEngine::autoConfigureAsio()` method handles this auto-configuration

2. **Device-Specific Configuration Phase**:
   - After hardware connection and auto-configuration, device-specific settings are applied via OSC
   - The `OscController` sends commands defined in the `Configuration` to configure device parameters
   - These settings can be applied from a saved configuration file or built programmatically
   - The `AudioEngine::sendOscCommands()` method handles this phase

## Error Handling

The framework implements comprehensive error handling:

1. Configuration and initialization errors are reported with detailed messages
2. Runtime errors are logged but don't stop the entire processing chain
3. Resources are properly cleaned up even in error situations
4. Thread exceptions are caught and reported
5. Status callbacks notify the application of important events and errors

## Dynamic Parameter Control

The system supports real-time parameter updates:

1. The `FfmpegProcessorNode` can update filter parameters during processing
2. Parameter changes are queued and applied at safe points
3. The `OscController` can receive OSC messages for parameter control
4. Changes are propagated to the appropriate nodes through the `AudioEngine`

## Integration with External Systems

### OSC Integration

The `OscController` provides bidirectional OSC communication:

1. Sends commands to external hardware (e.g., RME TotalMix FX)
2. Receives OSC messages for parameter control
3. Queries device state for configuration persistence
4. Applies complete configurations to devices

### FFmpeg Integration

The system integrates FFmpeg for comprehensive audio processing:

1. File reading and decoding
2. File encoding and writing
3. Audio format conversion
4. Filter-based audio processing (EQ, dynamics, effects)
5. Resampling and channel mapping

## Conclusion

The AudioNode framework provides a flexible, modular system for audio processing with these key advantages:

1. **Modularity**: Components are encapsulated and follow single-responsibility principle
2. **Extensibility**: New node types can be added without changing the core framework
3. **Flexibility**: Processing chains can be dynamically constructed
4. **Efficiency**: Reference counting and careful thread management enable real-time performance
5. **Hardware Integration**: Seamless integration with ASIO and OSC-controlled hardware
6. **Powerful Processing**: FFmpeg integration provides comprehensive audio manipulation capabilities
