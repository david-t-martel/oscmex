# Audio Engine Runtime Sequence Diagram (Detailed Loop)

This diagram shows the typical sequence of events during initialization and real-time processing, using class names potentially found in the `oscmex` repository, with a more detailed audio processing loop.

```mermaid
sequenceDiagram
    participant Main as Main App (main.cpp)
    participant Cfg as Config
    participant AEC as AudioEngineController
    participant ADM as AudioDeviceManager (ASIO)
    participant OSC_S as OSCServer
    participant OSC_C as OSCClient
    participant RME as RME TotalMix FX (External)
    participant NodeGraph as Node Graph (Logical)
    participant BufPool as AudioBufferPool
    participant SrcNode as Source Node (e.g., AsioSourceNode)
    participant ProcNode as Processor Node (e.g., AudioProcessor/Ffmpeg)
    participant SinkNode as Sink Node (e.g., AsioSinkNode)
    participant ASIODrv as ASIO Driver (External)
    participant Remote as Remote OSC Control

    %% Initialization Phase (Simplified - see previous diagram for full detail) %%
    Main->>Cfg: parse(args/file)
    Cfg-->>Main: configData
    Main->>AEC: initialize(configData)
    activate AEC
    AEC->>ADM: create() & initDevice()
    AEC->>OSC_S: create() & configure() & startListening()
    AEC->>OSC_C: create() & configure()
    AEC->>NodeGraph: createNodes() & setupConnections()
    AEC->>OSC_C: sendCommand(Initial RME Commands)
    AEC->>ADM: createBuffers() & setCallback(processAudioBlock)
    AEC-->>Main: initializationComplete
    deactivate AEC

    %% Runtime Phase %%
    Main->>AEC: run()
    activate AEC
    AEC->>ADM: start() # Starts ASIO processing thread(s)
    Note right of ADM: ASIO thread now active

    loop Audio Processing Loop (Triggered by ASIO Callback)
        ASIODrv->>ADM: bufferSwitchCallback(doubleBufferIndex)
        activate ADM
        ADM->>AEC: processAudioBlock(doubleBufferIndex) # Forward callback to Engine
        activate AEC

        Note over AEC: Process Graph based on determined order

        %% 1. Handle Input Nodes %%
        AEC->>ADM: getBufferPointers(doubleBufferIndex, inputChannels)
        ADM-->>AEC: rawInputBufferPtrs
        AEC->>SrcNode: receiveAsioData(doubleBufferIndex, rawInputBufferPtrs) # Example for AsioSourceNode
        activate SrcNode
        SrcNode->>BufPool: acquireBuffer() # Get a buffer for output
        activate BufPool
        BufPool-->>SrcNode: outputAudioBuffer
        deactivate BufPool
        Note over SrcNode: Convert ASIO data to internal format<br/>(using SwrContext if needed)<br/>Fill outputAudioBuffer
        SrcNode-->>AEC: Input data ready
        deactivate SrcNode

        %% 2. Process Connections & Nodes %%
        Note over AEC: Iterate through processing order / connections
        AEC->>SrcNode: getOutputBuffer(padIndex) # Get buffer from source
        activate SrcNode
        SrcNode-->>AEC: sourceAudioBuffer (ownership moved)
        deactivate SrcNode

        AEC->>ProcNode: setInputBuffer(sourceAudioBuffer, padIndex) # Pass buffer to processor
        activate ProcNode
        Note over ProcNode: Stores input buffer
        ProcNode-->>AEC: Input set
        deactivate ProcNode

        AEC->>ProcNode: process() # Tell processor to process its input
        activate ProcNode
        ProcNode->>BufPool: acquireBuffer() # Get buffer for its output
        activate BufPool
        BufPool-->>ProcNode: processedAudioBuffer
        deactivate BufPool
        Note over ProcNode: Apply FFmpeg filter graph<br/>Input -> Output Buffer<br/>(Calls FfmpegFilter::process)
        ProcNode->>BufPool: releaseBuffer(inputAudioBuffer) # Release consumed input buffer
        ProcNode-->>AEC: Processing complete
        deactivate ProcNode

        AEC->>ProcNode: getOutputBuffer(padIndex) # Get processed buffer
        activate ProcNode
        ProcNode-->>AEC: processedAudioBuffer (ownership moved)
        deactivate ProcNode

        AEC->>SinkNode: setInputBuffer(processedAudioBuffer, padIndex) # Pass buffer to sink
        activate SinkNode
        Note over SinkNode: Stores input buffer
        SinkNode-->>AEC: Input set
        deactivate SinkNode

        %% 3. Handle Output Nodes %%
        AEC->>ADM: getBufferPointers(doubleBufferIndex, outputChannels)
        ADM-->>AEC: rawOutputBufferPtrs
        AEC->>SinkNode: provideAsioData(doubleBufferIndex, rawOutputBufferPtrs) # Example for AsioSinkNode
        activate SinkNode
        Note over SinkNode: Convert internal format data<br/>(from stored input buffer)<br/>to ASIO format (using SwrContext if needed)<br/>Copy data to rawOutputBufferPtrs
        SinkNode->>BufPool: releaseBuffer(consumedInputBuffer) # Release its input buffer
        SinkNode-->>AEC: Output data provided
        deactivate SinkNode

        %% 4. Signal ASIO Ready %%
        ADM->>ASIODrv: outputReady() # Signal ASIO buffers are ready
        deactivate ADM
        deactivate AEC
    end

    loop OSC Control Interaction (Async - Runs in OSCServer Thread)
        Remote->>OSC_S: Send OSC Control Message (e.g., /engine/param/update)
        activate OSC_S
        Note over OSC_S: Listener thread receives packet
        OSC_S->>OSC_S: parsePacket()
        OSC_S->>AEC: handleEngineCommand(address, args) # Callback registered during init
        activate AEC
        Note over AEC: Finds target node/parameter
        AEC->>ProcNode: updateParameter(filterName, param, value) # Thread-safe update queue/call
        deactivate AEC
        deactivate OSC_S
    end

    %% Shutdown Phase (Simplified) %%
    Main->>AEC: stop()
    activate AEC
    AEC->>ADM: stop()
    AEC->>OSC_S: stopListening()
    AEC->>NodeGraph: stopNodes()
    AEC-->>Main: stopped
    deactivate AEC

    Main->>AEC: cleanup()
    activate AEC
    AEC->>NodeGraph: cleanupNodes()
    AEC->>ADM: cleanup()
    AEC->>OSC_S: cleanup()
    AEC->>OSC_C: cleanup()
    deactivate AEC
    Note over Main: Application Exits

Diagram Key Point Updates:Audio Processing Loop Detail: Shows more granular steps:AEC interacts with specific node types (SrcNode, ProcNode, SinkNode) representing the logical flow.Explicit calls to acquireBuffer and releaseBuffer from/to the BufPool are shown.The sequence clarifies that AEC drives the process: getting output from one node, setting it as input to the next, and then triggering the process() method on processor nodes.Input nodes (AsioSourceNode) are triggered by AEC (receiveAsioData) to prepare their output buffer before the graph processing iteration.Output nodes (AsioSinkNode) are triggered by AEC (provideAsioData) to consume their input buffer and write to ASIO after the graph processing iteration.Buffer Ownership: Notes implicitly suggest buffer ownership transfer (e.g., using std::move with std::optional<AudioBuffer> or smart pointers) as buffers move between the engine and nodes. Input buffers are released after consumption.Node Graph Participant: Still represents the logical collection, but interactions within the loop are shown with specific node type examples for clarity.This more detailed sequence provides a clearer view of the real-time data flow, buffer management, and node interactions within the `AudioEngineController
