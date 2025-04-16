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

    %% Configuration
    ConfigurationParser --> Configuration : creates

    %% Connection Between Nodes
    Connection --> AudioNode : connects

    %% Class Definitions
    class AudioEngine {
        -Configuration m_config
        -AsioManager* m_asioManager
        -RmeOscController* m_rmeController
        -vector~AudioNode*~ m_nodes
        -vector~Connection~ m_connections
        +initialize(Configuration)
        +run()
        +stop()
        +processAsioBlock(long)
    }

    class AudioNode {
        <<abstract>>
        #string m_name
        #NodeType m_type
        #AudioEngine* m_engine
        +configure(params, sampleRate, bufferSize, format, layout)
        +start()
        +process()
        +stop()
        +getOutputBuffer(padIndex)
        +setInputBuffer(buffer, padIndex)
    }

    class AudioBuffer {
        -vector~uint8_t*~ data
        -long frames
        -double sampleRate
        -AVSampleFormat format
        -AVChannelLayout channelLayout
        +allocate(frames, sampleRate, format, layout)
        +free()
        +copyFrom(buffer)
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
        +configure(ip, port)
        +sendCommand(address, args)
        +setMatrixCrosspointGain(input, output, gain)
    }

    class AsioSourceNode {
        -AsioManager* m_asioMgr
        -vector~long~ m_asioChannelIndices
        -AudioBuffer m_outputBuffer
        +receiveAsioData(doubleBufferIndex, asioBuffers)
        +getOutputBuffer(padIndex)
    }

    class AsioSinkNode {
        -AsioManager* m_asioMgr
        -vector~long~ m_asioChannelIndices
        -AudioBuffer m_inputBuffer
        +setInputBuffer(buffer, padIndex)
        +provideAsioData(doubleBufferIndex, asioBuffers)
    }

    class FileSourceNode {
        -string m_filePath
        -thread m_readerThread
        -queue~AudioBuffer~ m_outputQueue
        +start()
        +stop()
        +getOutputBuffer(padIndex)
    }

    class FileSinkNode {
        -string m_filePath
        -thread m_writerThread
        -queue~AudioBuffer~ m_inputQueue
        +start()
        +stop()
        +setInputBuffer(buffer, padIndex)
    }

    class FfmpegProcessorNode {
        -FfmpegFilter* m_ffmpegFilter
        -string m_filterDesc
        -AudioBuffer m_inputBuffer
        -AudioBuffer m_outputBuffer
        +process()
        +updateParameter(filterName, paramName, value)
    }

    class FfmpegFilter {
        -AVFilterGraph* m_graph
        -AVFilterContext* m_srcContext
        -AVFilterContext* m_sinkContext
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

        subgraph Nodes
            ASIO_SRC[AsioSourceNode]
            ASIO_SINK[AsioSinkNode]
            FILE_SRC[FileSourceNode]
            FILE_SINK[FileSinkNode]
            PROC[FfmpegProcessorNode]
        end

        BUFFER[AudioBuffer Pool]
        CONFIG[Configuration]
    end

    subgraph Storage
        FILES[Audio Files]
    end

    %% Hardware connections
    ASIO_HW <--> ASIO_MGR
    RME_HW <-- OSC --> RME_OSC

    %% ASIO data flow
    ASIO_MGR --> ASIO_SRC
    ASIO_SRC --> BUFFER
    BUFFER --> ASIO_SINK
    ASIO_SINK --> ASIO_MGR

    %% File data flow
    FILES --> FILE_SRC
    FILE_SRC --> BUFFER
    BUFFER --> FILE_SINK
    FILE_SINK --> FILES

    %% Processing data flow
    BUFFER --> PROC
    PROC --> BUFFER

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

    Engine->>RME: configure(ip, port)
    RME-->>Engine: configured

    Engine->>ASIO: loadDriver(deviceName)
    ASIO-->>Engine: driver loaded

    Engine->>ASIO: initDevice(sampleRate, bufferSize)
    ASIO-->>Engine: device initialized

    Engine->>Engine: setupConnections()

    Engine->>ASIO: createBuffers(inputChannels, outputChannels)
    ASIO-->>Engine: buffers created

    Engine->>RME: sendCommands()
    RME-->>Engine: commands sent

    Engine-->>Main: initialization complete

    Main->>Engine: run()
    Engine->>Nodes: start()
    Engine->>ASIO: start()

    Note over ASIO: ASIO callbacks begin

    ASIO->>Engine: processAsioBlock()
    Engine->>Nodes: process()
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

        state ProcessingBlock {
            [*] --> ReceiveAsioInput
            ReceiveAsioInput --> ProcessGraph: source nodes get data
            ProcessGraph --> SendAsioOutput: process through connections
            SendAsioOutput --> [*]: sink nodes provide data
        }
    }
```
