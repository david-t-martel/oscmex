# OSCMix Architecture and Processing Flow

This document illustrates the core architecture and processing flow of OSCMix, which bridges OSC (Open Sound Control) messages with RME audio interface controls using MIDI SysEx commands.

```mermaid
flowchart TD
    A[Program Start] --> B[device_init(): Initialize device parameters]
    B --> C[Initialize channel structures: inputs_init(), playbacks_init(), outputs_init()]
    C --> D[osc_server_init(): Set up OSC message handling]
    D --> E[Main Loop: osc_server_poll() for incoming messages]
    E --> F{Incoming OSC Message?}
    F -- No --> E
    F -- Yes --> G[oscmsg_parse(): Parse OSC address pattern and arguments]
    G --> H{Message Command Type}

    %% Parameter Setting Flow
    H -- Set Parameter '/path value' --> I[dispatch_set(): Find parameter and validate value]
    I --> I1[setreg(): Format and send SysEx register command]
    I --> I2[setint(): Set integer parameter]
    I --> I3[setfixed(): Set fractional value parameter]
    I --> I4[setenum(): Set enumerated value parameter]
    I --> I5[setbool(): Set boolean parameter]
    I1 --> J[Update internal state tracking]
    I2 --> J
    I3 --> J
    I4 --> J
    I5 --> J
    J --> K[oscsend(): Send acknowledgment/notification back to client]
    K --> E

    %% Parameter Query Flow
    H -- Get Parameter '/path' --> L[dispatch_get(): Find parameter and retrieve current value]
    L --> L1[newint(): Format integer response]
    L --> L2[newfixed(): Format fractional value response]
    L --> L3[newenum(): Format enumerated value response]
    L --> L4[newbool(): Format boolean response]
    L1 --> M[oscsend(): Send parameter value to client]
    L2 --> M
    L3 --> M
    L4 --> M
    M --> E

    %% Special Commands Flow
    H -- Special Command --> N{Command Type}
    N -- "/refresh" --> O[setrefresh(): Request full device state refresh]
    O --> O1[setreg(REFRESH_ADDR, MAGIC_VALUE): Send refresh command to device]
    O1 --> O2[Set refreshing flag to true]
    O2 --> O3[Prepare to receive and process all parameter values]
    O3 --> E

    N -- "/dump" --> P[dump(): Debug function to display internal state]
    P --> E

    N -- "/sysex" --> Q[writesysex(): Send raw SysEx command to device]
    Q --> E

    N -- "/enum" --> R[oscsendenum(): List available enumeration values for parameter]
    R --> E

    %% SysEx Response Processing
    S[MIDI SysEx Response from Device] --> T[handlesysex(): Process device response]
    T --> U{SysEx Message Type}
    U -- Register Values --> V[handleregs(): Update internal state with register values]
    V --> W[Send OSC notifications for changed parameters]
    W --> E

    U -- Level Meters --> X[handlelevels(): Process level meter data]
    X --> Y[Format and send level meter OSC messages]
    Y --> E

    %% Error handling
    G -- Invalid/Unknown --> Z[osc_error(): Handle malformed message]
    Z --> E
```

## Core Component Descriptions

### Initialization

- **device_init()**: Detects the connected RME device and initializes the parameter mapping structure
- **inputs_init()**, **outputs_init()**, **playbacks_init()**: Set up state tracking structures for different channel types

### OSC Message Processing

- **oscmsg_parse()**: Extracts address pattern and arguments from incoming OSC message packets
- **dispatch_set()**, **dispatch_get()**: Find parameter in the parameter tree and call appropriate handlers
- **setreg()**: Core function that formats and sends MIDI SysEx commands to update device registers
- **setint()**, **setfixed()**, **setenum()**, **setbool()**: Type-specific parameter value setting and validation

### MIDI Response Handling

- **handlesysex()**: Processes SysEx responses received from the device
- **handleregs()**: Updates internal state based on register values received from the device
- **handlelevels()**: Processes level meter data and sends it as OSC messages

### Special Commands

- **setrefresh()**: Initiates a full device state refresh
- **oscsendenum()**: Returns the list of possible values for enumerated parameters
- **dump()**: Debugging tool that prints internal state information

### OSC Message Generation

- **oscsend()**: Formats and sends OSC messages to notify clients of state changes
- **newint()**, **newfixed()**, **newenum()**, **newbool()**: Type-specific response formatting

## Data Flow

1. OSC messages arrive from the network
2. Messages are parsed and dispatched to appropriate handlers
3. Parameters are validated and converted to device-specific values
4. MIDI SysEx commands are sent to the device
5. Device responses are processed and state is updated
6. Clients are notified of state changes via OSC messages

## Error Handling

- Invalid messages and parameters are rejected with appropriate error responses
- SysEx communication errors are detected and reported
- Device state is maintained consistently between client and hardware
