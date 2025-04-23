sequenceDiagram
    participant App as Application
    participant Engine as AudioEngine
    participant AsioMgr as AsioManager
    participant Nodes as AudioNodes
    participant OscCtrl as OscController

    App->>Engine: initialize(config)
    Engine->>Engine: validateConfiguration()

    Engine->>AsioMgr: loadDriver(deviceName)
    AsioMgr-->>Engine: driver loaded

    Engine->>AsioMgr: initDevice()
    AsioMgr-->>Engine: hardware capabilities (sample rate, buffer size)

    Engine->>Engine: autoConfigureNodes(hwCapabilities)

    Engine->>AsioMgr: createBuffers(inputChannels, outputChannels)
    AsioMgr-->>Engine: buffers created

    Engine->>Nodes: createNodes(config, hwCapabilities)
    Note over Engine,Nodes: Create and configure all AudioNodes

    Engine->>Nodes: configure(params, sampleRate, bufferSize, format, layout)
    Nodes-->>Engine: configuration status

    Engine->>Engine: establishConnections(config)
    Note over Engine: Connect nodes according to processing graph

    Engine->>Engine: calculateProcessOrder()
    Note over Engine: Determine optimal node processing sequence

    Engine->>OscCtrl: sendInitialConfiguration(config)
    Note over OscCtrl: Configure external hardware via OSC

    Engine->>AsioMgr: setCallback(processAsioBlock)
    Engine->>AsioMgr: start()
    AsioMgr-->>Engine: processing started

    App->>Engine: run()
    Note over App,Engine: System now running, processing audio
