# Audio Engine Architecture

## Class Interaction Diagram

```mermaid
classDiagram
    %% Main Engine Coordinator
    AudioEngine *-- "1" AsioManager : manages
    AudioEngine *-- "1" OscController : manages
    AudioEngine *-- "many" AudioNode : owns
    AudioEngine *-- "many" Connection : contains
    AudioEngine --> Configuration : configured by

    %% Node Hierarchy
    AudioNode <|-- AsioSourceNode
    AudioNode <|-- AsioSinkNode
    AudioNode <|-- FileSourceNode
    AudioNode <|-- FileSinkNode
    AudioNode <|-- FfmpegProcessorNode

    %% Buffer Handling
    AudioNode --> AudioBuffer : produces/consumes
    AsioSourceNode --> AudioBuffer : produces
    AsioSinkNode --> AudioBuffer : consumes
    FileSourceNode --> AudioBuffer : produces
    FileSinkNode --> AudioBuffer : consumes
    FfmpegProcessorNode --> AudioBuffer : transforms

    %% Hardware Interfaces
    AsioSourceNode --> AsioManager : receives data from
    AsioSinkNode --> AsioManager : sends data to
    FfmpegProcessorNode --> FfmpegFilter : uses
    OscController --> "liblo" : uses
    FileSourceNode --> "FFmpegSource" : reads from
    FileSinkNode --> "FFmpegSource" : writes to

    %% Configuration
    ConfigurationParser --> Configuration : creates
    DeviceStateManager --> Configuration : generates
    DeviceStateManager --> OscController : queries

    %% Connection Between Nodes
    Connection --> AudioNode : connects

    %% Reference Counting
    AudioBuffer --> "shared_ptr" : uses for reference counting

    %% Class Definitions
    class AudioEngine {
        -Configuration m_config
        -AsioManager* m_asioManager
        -OscController* m_oscController
        -vector~AudioNode*~ m_nodes
        -vector~Connection~ m_connections
        -map~string, AudioNode*~ m_nodeMap
        -vector~AudioNode*~ m_processOrder
        +initialize(Configuration)
        +run()
        +stop()
        +processAsioBlock(long, bool)
        +getNodeByName(string)
        +getOscController()
    }

    class AudioNode {
        <<abstract>>
        #string m_name
        #NodeType m_type
        #AudioEngine* m_engine
        #double m_sampleRate
        #long m_bufferSize
        #AVSampleFormat m_format
        #AVChannelLayout m_channelLayout
        #bool m_configured
        #bool m_running
        #string m_lastError
        +configure(params, sampleRate, bufferSize, format, layout)
        +start()
        +process()
        +stop()
        +getOutputBuffer(padIndex)
        +setInputBuffer(buffer, padIndex)
        +getInputPadCount()
        +getOutputPadCount()
        +updateParameter(paramName, paramValue)
        +isConfigured()
        +isRunning()
        +isBufferFormatCompatible(buffer)
        +getLastError()
    }

    class AudioBuffer {
        -vector~vector~uint8_t~~ m_data
        -long m_frames
        -double m_sampleRate
        -AVSampleFormat m_format
        -AVChannelLayout m_channelLayout
        -atomic~int~ m_refCount
        +allocate(frames, sampleRate, format, layout)
        +free()
        +copyFrom(buffer)
        +getPlaneData(plane)
        +createReference()
        +isValid()
    }

    class AsioManager {
        -long m_bufferSize
        -double m_sampleRate
        -ASIOSampleType m_sampleType
        +loadDriver(deviceName)
        +initDevice(sampleRate, bufferSize)
        +createBuffers(inputChannels, outputChannels)
        +start()
        +stop()
        +setCallback(callback)
    }

    class OscController {
        -string m_targetIp
        -int m_targetPort
        -lo_server_thread m_oscServer
        +sendCommand(address, args)
        +startReceiver(port)
        +setMatrixCrosspointGain(input, output, gain)
        +setChannelVolume(channelType, channel, volumeDb)
        +setChannelMute(channelType, channel, mute)
        +queryChannelVolume(channelType, channel, volumeDb)
        +applyConfiguration(config)
        +getTargetIp()
        +getTargetPort()
    }

    class AsioSourceNode {
        -AsioManager* m_asioMgr
        -vector~long~ m_asioChannelIndices
        -shared_ptr~AudioBuffer~ m_outputBuffer
        -mutex m_bufferMutex
        +receiveAsioData(doubleBufferIndex, asioBuffers)
        +getOutputBuffer(padIndex)
    }

    class AsioSinkNode {
        -AsioManager* m_asioMgr
        -vector~long~ m_asioChannelIndices
        -shared_ptr~AudioBuffer~ m_inputBuffer
        -mutex m_bufferMutex
        +setInputBuffer(buffer, padIndex)
        +provideAsioData(doubleBufferIndex, asioBuffers)
    }

    class FileSourceNode {
        -string m_filePath
        -thread m_readerThread
        -queue~shared_ptr~AudioBuffer~~ m_outputQueue
        -mutex m_queueMutex
        -condition_variable m_queueCondVar
        -AVFormatContext* m_formatCtx
        -AVCodecContext* m_codecCtx
        +start()
        +stop()
        +getOutputBuffer(padIndex)
        +seekTo(position)
        +getCurrentPosition()
        +getDuration()
    }

    class FileSinkNode {
        -string m_filePath
        -thread m_writerThread
        -queue~shared_ptr~AudioBuffer~~ m_inputQueue
        -mutex m_queueMutex
        -condition_variable m_queueCondVar
        -AVFormatContext* m_formatCtx
        -AVCodecContext* m_codecCtx
        +start()
        +stop()
        +setInputBuffer(buffer, padIndex)
        +flush()
    }

    class FfmpegProcessorNode {
        -FfmpegFilter m_ffmpegFilter
        -string m_filterDescription
        -AVFrame* m_inputFrame
        -AVFrame* m_outputFrame
        -shared_ptr~AudioBuffer~ m_inputBuffer
        -shared_ptr~AudioBuffer~ m_outputBuffer
        -mutex m_processMutex
        +process()
        +updateParameter(paramName, paramValue)
        +updateParameter(filterName, paramName, value)
    }

    class FfmpegFilter {
        -AVFilterGraph* m_graph
        -AVFilterContext* m_srcContext
        -AVFilterContext* m_sinkContext
        -map~string, AVFilterContext*~ m_namedFilters
        +initGraph(filterDescription, sampleRate, format, layout)
        +process(inputFrame, outputFrame)
        +updateParameter(filterName, paramName, value)
    }

    class Configuration {
        +string asioDeviceName
        +string rmeOscIp
        +int rmeOscPort
        +double sampleRate
        +long bufferSize
        +vector~NodeConfig~ nodes
        +vector~ConnectionConfig~ connections
        +vector~OscCommandConfig~ rmeCommands
        +loadFromFile(filePath)
        +saveToFile(filePath)
        +toJsonString()
        +setConnectionParams(ip, port, receivePort)
        +getTargetIp()
        +getTargetPort()
        +getReceivePort()
        +setMatrixCrosspointGain(input, output, gainDb)
        +setChannelMute(channelType, channel, mute)
        +setChannelVolume(channelType, channel, volumeDb)
        +getCommands()
        +addCommand(address, args)
        +clearCommands()
    }

    class ConfigurationParser {
        +parse(argc, argv, config)
        +parseFromFile(filePath, config)
        +parseJson(content, config)
    }

    class Connection {
        +AudioNode* sourceNode
        +int sourcePad
        +AudioNode* sinkNode
        +int sinkPad
        +transfer()
        +toString()
    }

    class DeviceStateManager {
        +DeviceStateManager(OscController*)
        +queryDeviceState(callback, channelCount)
        +queryParameter(address, callback)
        +queryParameters(addresses, callback)
    }
```

## Data Flow Diagram

```mermaid
flowchart LR
    subgraph Hardware
        ASIO_HW[ASIO Hardware]
        RME_HW[RME TotalMix FX]
    end

    subgraph AudioEngine
        ASIO_MGR[AsioManager]
        OSC_CTRL[OscController]
        DEV_STATE[DeviceStateManager]

        subgraph NodeBuffers
            BUFFER_POOL[AudioBuffer Pool]
            REF_COUNT[Reference Counting]
        end

        subgraph Nodes
            ASIO_SRC[AsioSourceNode]
            ASIO_SINK[AsioSinkNode]
            FILE_SRC[FileSourceNode]
            FILE_SINK[FileSinkNode]
            PROC[FfmpegProcessorNode]
        end

        CONNECTIONS[Connections]
        CONFIG[Configuration]
    end

    subgraph Storage
        FILES[Audio Files]
        JSON_CONFIG[JSON Configuration]
    end

    subgraph Libraries
        FF_SOURCE[Integrated FFmpeg Source]
        LIBLO[LibLO OSC]
        ONEAPI[Intel oneAPI Libraries]
    end

    %% Hardware connections
    ASIO_HW <--> ASIO_MGR
    RME_HW <-- OSC --> OSC_CTRL
    OSC_CTRL <--> LIBLO

    %% ASIO data flow
    ASIO_MGR --> ASIO_SRC
    ASIO_SRC --> BUFFER_POOL
    BUFFER_POOL --> ASIO_SINK
    ASIO_SINK --> ASIO_MGR

    %% File data flow
    FILES <--> FF_SOURCE
    FF_SOURCE <--> FILE_SRC
    FILE_SRC --> BUFFER_POOL
    BUFFER_POOL --> FILE_SINK
    FILE_SINK <--> FF_SOURCE
    FF_SOURCE --> FILES

    %% Processing data flow
    BUFFER_POOL --> PROC
    PROC <--> FF_SOURCE
    PROC --> BUFFER_POOL

    %% Buffer reference counting
    BUFFER_POOL <--> REF_COUNT

    %% Node connections
    ASIO_SRC --> CONNECTIONS
    FILE_SRC --> CONNECTIONS
    CONNECTIONS --> PROC
    CONNECTIONS --> ASIO_SINK
    CONNECTIONS --> FILE_SINK

    %% Configuration
    JSON_CONFIG <--> CONFIG
    CONFIG --> AudioEngine

    %% Device state management
    DEV_STATE <--> OSC_CTRL
    DEV_STATE --> CONFIG
    CONFIG --> JSON_CONFIG

    %% Intel oneAPI integration
    ONEAPI <--> BUFFER_POOL
    ONEAPI <--> PROC
    ONEAPI <--> FF_SOURCE
```

## Initialization Sequence

```mermaid
sequenceDiagram
    participant Main
    participant Engine as AudioEngine
    participant Config as ConfigurationParser
    participant ASIO as AsioManager
    participant OSC as OscController
    participant Nodes as AudioNodes
    participant FFmpeg as FFmpeg Libraries

    Main->>Config: parse(argc, argv)
    Config-->>Main: Configuration

    Main->>Engine: initialize(config)

    %% Phase 1: Hardware Connection & Auto-configuration
    rect rgb(230, 240, 255)
        Note over Engine,ASIO: Phase 1: Hardware Connection & Auto-configuration

        Engine->>FFmpeg: Initialize from local source
        FFmpeg-->>Engine: FFmpeg initialized

        Engine->>ASIO: loadDriver(deviceName)
        ASIO-->>Engine: driver loaded

        Engine->>ASIO: initDevice(preferredSampleRate, preferredBufferSize)

        ASIO->>ASIO: Query driver for optimal settings
        ASIO-->>Engine: Return actual sample rate, buffer size, and capabilities

        Engine->>Engine: Update configuration with device capabilities

        Engine->>ASIO: createBuffers(inputChannels, outputChannels)
        ASIO-->>Engine: buffers created
    end

    %% Phase 2: Create Engine Components and Apply Device-specific Settings
    rect rgb(240, 230, 255)
        Note over Engine,OSC: Phase 2: Create Engine Components and Apply Device-specific Settings

        Engine->>Engine: createAndConfigureNodes()

        loop For each node in config
            Engine->>Engine: Create node of appropriate type
            Engine->>Nodes: configure(params, sampleRate, bufferSize, format, layout)
        end

        Engine->>Engine: setupConnections()

        loop For each connection in config
            Engine->>Engine: Create Connection(sourceNode, sourcePad, sinkNode, sinkPad)
        end

        Engine->>OSC: configure(ip, port)
        OSC-->>Engine: configured

        Engine->>OSC: applyConfiguration(config)
        OSC->>OSC: sendBatch(commands)
        OSC-->>Engine: configuration applied
    end

    Engine-->>Main: initialization complete

    Main->>Engine: run()
    Engine->>Nodes: start()
    Engine->>Engine: calculateProcessOrder()
    Engine->>ASIO: start()

    Note over ASIO: ASIO callbacks begin

    ASIO->>Engine: processAsioBlock()

    loop For each node in process order
        Engine->>Nodes: process()
    end

    loop For each connection
        Engine->>Connection: transfer()
    end
```

## Device State Management Flow

```mermaid
sequenceDiagram
    participant User
    participant App as Application
    participant DSM as DeviceStateManager
    participant OSC as OscController
    participant Config as Configuration
    participant JSON as JSON File

    User->>App: Request read device state
    App->>DSM: queryDeviceState(callback)

    DSM->>DSM: Build parameter address list

    DSM->>OSC: queryParameters(addresses, callback)

    loop For each parameter
        OSC->>RME: Send query OSC message
        RME-->>OSC: Parameter value response
    end

    OSC-->>DSM: Parameter values map

    DSM->>Config: Create new Configuration
    DSM->>Config: Add connection params

    loop For each parameter value
        DSM->>DSM: Parse parameter address
        DSM->>Config: Add command based on parameter type
    end

    DSM-->>App: Callback with populated Configuration

    App->>Config: saveToFile(filePath)
    Config->>Config: Format JSON with structured sections
    Config->>JSON: Write formatted JSON

    App-->>User: Device state saved to file
```

## FFmpeg Integration Architecture

```mermaid
flowchart TD
    APP[Application] --> NODES[Audio Nodes]
    NODES --> FFPROC[FfmpegProcessorNode]
    FFPROC --> FILTER[FfmpegFilter]

    subgraph FFmpegSource["Integrated FFmpeg Source"]
        AVUTIL[libavutil]
        AVFORMAT[libavformat]
        AVCODEC[libavcodec]
        AVFILTER[libavfilter]
        SWRESAMPLE[libswresample]
        SWSCALE[libswscale]

        AVFORMAT --> AVCODEC
        AVFORMAT --> AVUTIL
        AVCODEC --> AVUTIL
        AVFILTER --> AVUTIL
        SWRESAMPLE --> AVUTIL
        SWSCALE --> AVUTIL
    end

    FILTER --> AVFILTER
    FILTER --> AVUTIL

    FILE_SRC[FileSourceNode] --> AVFORMAT
    FILE_SRC --> AVCODEC
    FILE_SRC --> SWRESAMPLE

    FILE_SINK[FileSinkNode] --> AVFORMAT
    FILE_SINK --> AVCODEC
    FILE_SINK --> SWRESAMPLE

    subgraph oneAPI["Intel oneAPI Components"]
        TBB[Threading Building Blocks]
        IPP[Integrated Performance Primitives]
        MKL[Math Kernel Library]
    end

    FFPROC --> TBB
    BUFFER[AudioBuffer] --> IPP
    AVFILTER --> MKL
```

## Decoupled Component Architecture

```mermaid
classDiagram
    %% Interface for external control
    class IExternalControl {
        <<interface>>
        +setParameter(address, args)
        +getParameter(address, callback)
        +applyConfiguration(config)
        +queryDeviceState(callback)
        +addEventCallback(callback)
        +removeEventCallback(id)
    }

    %% Main components with loose coupling
    AudioEngine o-- IExternalControl : uses optionally
    OscController ..|> IExternalControl : implements

    %% Configuration system
    Configuration <-- AudioEngine : uses
    Configuration <-- OscController : uses

    %% Hardware interfaces remain unchanged
    AsioManager <-- AudioEngine : manages
    AudioEngine o-- "many" AudioNode : owns

    %% OscController can operate independently
    OscController --> "liblo" : uses

    %% Class details
    class AudioEngine {
        -std::shared_ptr~IExternalControl~ m_externalControl
        +initialize(config, externalControl)
        +setExternalControl(control)
        +clearExternalControl()
        +sendExternalCommand(address, args)
    }

    class OscController {
        +configure(configFile)
        +configure(targetIp, targetPort, receivePort)
        +static main(argc, argv)
    }

    class Configuration {
        +loadAudioConfigFromFile(path)
        +loadControlConfigFromFile(path)
        +getAudioConfiguration()
        +getControlConfiguration()
    }
```

## Standalone vs Integrated Operation

```mermaid
flowchart TD
    subgraph "Standalone Operation"
        A[OscController Executable] --> B[OscController]
        B --> C[liblo OSC Library]
        B --> D[Device Communication]
    end

    subgraph "Integrated Operation"
        E[Main Application] --> F[AudioEngine]
        E --> G[OscController]
        F -.-> |Optional| G
        F --> H[AsioManager]
        F --> I[Audio Nodes]
        G --> J[liblo OSC Library]
        G --> K[Device Communication]
    end

    subgraph "Shared Components"
        L[Configuration System]
        M[IExternalControl Interface]
        G ..|> M
        F --> L
        G --> L
    end
```

## Component Initialization Sequence

```mermaid
sequenceDiagram
    participant Main
    participant Engine as AudioEngine
    participant IControl as IExternalControl
    participant OSC as OscController (Optional)
    participant Config as Configuration

    Main->>Config: loadFromFile()
    Config-->>Main: Configuration

    Main->>Engine: create()

    alt Use OSC Controller
        Main->>OSC: create()
        Main->>OSC: configure(ip, port)
        OSC-->>Main: configured
        Main->>Engine: initialize(config, oscController)
    else Without OSC Controller
        Main->>Engine: initialize(config, nullptr)
    end

    Engine->>Engine: setupAudio()
    Engine->>Engine: createNodes()

    opt If External Control Available
        Engine->>IControl: applyConfiguration(config)
        IControl-->>Engine: success/failure
    end

    Engine-->>Main: initialized

    Main->>Engine: run()

    Note over Engine, IControl: Engine operates with or without external control
```
