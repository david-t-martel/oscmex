# Audio Engine Architecture

## Class Interaction Diagram

```mermaid
classDiagram
    %% Main Engine Coordinator
    AudioEngine *-- "1" AsioManager : manages
    AudioEngine *-- "1" RmeOscController : manages
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
    RmeOscController --> "liblo" : uses
    FileSourceNode --> "libavformat/libavcodec" : reads from
    FileSinkNode --> "libavformat/libavcodec" : writes to

    %% Configuration
    ConfigurationParser --> Configuration : creates

    %% Connection Between Nodes
    Connection --> AudioNode : connects

    %% Reference Counting
    AudioBuffer --> "shared_ptr" : uses for reference counting

    %% Class Definitions
    class AudioEngine {
        -Configuration m_config
        -AsioManager* m_asioManager
        -RmeOscController* m_rmeController
        -vector~AudioNode*~ m_nodes
        -vector~Connection~ m_connections
        -map~string, AudioNode*~ m_nodeMap
        -vector~AudioNode*~ m_processOrder
        +initialize(Configuration)
        +run()
        +stop()
        +processAsioBlock(long, bool)
        +getNodeByName(string)
        +getRmeController()
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
        +configure(params, sampleRate, bufferSize, format, layout)
        +start()
        +process()
        +stop()
        +getOutputBuffer(padIndex)
        +setInputBuffer(buffer, padIndex)
        +getInputPadCount()
        +getOutputPadCount()
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

    class RmeOscController {
        -string m_rmeIp
        -int m_rmePort
        -lo_address m_oscAddress
        -lo_server_thread m_oscServer
        +configure(ip, port)
        +sendCommand(address, args)
        +startReceiver(port)
        +stopReceiver()
        +setMessageCallback(callback)
        +setMatrixCrosspointGain(input, output, gain)
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
        +vector~RmeOscCommandConfig~ rmeCommands
    }

    class ConfigurationParser {
        +parse(argc, argv, config)
        +parseFromFile(filePath, config)
    }

    class Connection {
        +AudioNode* sourceNode
        +int sourcePad
        +AudioNode* sinkNode
        +int sinkPad
        +transfer()
        +toString()
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
        RME_OSC[RmeOscController]

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
    end

    subgraph Libraries
        LIBAV[FFmpeg Libraries]
        LIBLO[LibLO OSC]
    end

    %% Hardware connections
    ASIO_HW <--> ASIO_MGR
    RME_HW <-- OSC --> RME_OSC
    RME_OSC <--> LIBLO

    %% ASIO data flow
    ASIO_MGR --> ASIO_SRC
    ASIO_SRC --> BUFFER_POOL
    BUFFER_POOL --> ASIO_SINK
    ASIO_SINK --> ASIO_MGR

    %% File data flow
    FILES <--> LIBAV
    LIBAV <--> FILE_SRC
    FILE_SRC --> BUFFER_POOL
    BUFFER_POOL --> FILE_SINK
    FILE_SINK <--> LIBAV
    LIBAV --> FILES

    %% Processing data flow
    BUFFER_POOL --> PROC
    PROC <--> LIBAV
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
    CONFIG --> AudioEngine
```

## Initialization Sequence

```mermaid
sequenceDiagram
    participant Main
    participant Engine as AudioEngine
    participant Config as ConfigurationParser
    participant ASIO as AsioManager
    participant RME as RmeOscController
    participant Nodes as AudioNodes

    Main->>Config: parse(argc, argv)
    Config-->>Main: Configuration

    Main->>Engine: initialize(config)
    Engine->>Engine: createAndConfigureNodes()

    loop For each node in config
        Engine->>Engine: Create node of appropriate type
        Engine->>Nodes: configure(params, sampleRate, bufferSize, format, layout)
    end

    Engine->>RME: configure(ip, port)
    RME-->>Engine: configured

    Engine->>ASIO: loadDriver(deviceName)
    ASIO-->>Engine: driver loaded

    Engine->>ASIO: initDevice(sampleRate, bufferSize)
    ASIO-->>Engine: device initialized

    Engine->>Engine: setupConnections()

    loop For each connection in config
        Engine->>Engine: Create Connection(sourceNode, sourcePad, sinkNode, sinkPad)
    end

    Engine->>ASIO: createBuffers(inputChannels, outputChannels)
    ASIO-->>Engine: buffers created

    Engine->>RME: sendCommands(rmeCommands)
    RME-->>Engine: commands sent

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

## Processing Flow

```mermaid
stateDiagram-v2
    [*] --> Initialized
    Initialized --> Running: run()
    Running --> Stopped: stop()
    Stopped --> [*]: cleanup()

    state Running {
        [*] --> WaitForCallback
        WaitForCallback --> ProcessingBlock: ASIO callback
        ProcessingBlock --> WaitForCallback

        WaitForCallback --> ProcessFileNodes: File node threads
        ProcessFileNodes --> WaitForCallback

        state ProcessingBlock {
            [*] --> ReceiveAsioInput
            ReceiveAsioInput --> TransferBuffers: AsioSourceNode gets data
            TransferBuffers --> ProcessNodes: Transfer via connections
            ProcessNodes --> SendAsioOutput: Process in topological order
            SendAsioOutput --> [*]: AsioSinkNode provides data
        }
    }

    state ProcessFileNodes {
        [*] --> ReadAudioFile: FileSourceNode thread
        ReadAudioFile --> DecodeFrame
        DecodeFrame --> QueueBuffer
        QueueBuffer --> [*]

        [*] --> DequeueBuffer: FileSinkNode thread
        DequeueBuffer --> EncodeFrame
        EncodeFrame --> WriteFile
        WriteFile --> [*]
    }
```
