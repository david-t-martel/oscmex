# OSCMix Detailed Flow (oscmix.c)

```mermaid
flowchart TD
    A[Program Start] --> B[device_init()]
    B --> C[inputs_init(), playbacks_init(), outputs_init()]
    C --> D[osc_server_init()]
    D --> E[Main Loop: osc_server_poll()]
    E --> F{OSC Message Received?}
    F -- No --> E
    F -- Yes --> G[oscmsg_parse()]
    G --> H{Message Type}

    H -- Set Parameter --> I[dispatch_set()]
    I --> I1[setreg()]
    I --> I2[setint()]
    I --> I3[setfixed()]
    I --> I4[setenum()]
    I --> I5[setbool()]
    I1 --> J[update_device_state()]
    I2 --> J
    I3 --> J
    I4 --> J
    I5 --> J
    J --> K[oscsend() - Acknowledge/Notify]
    K --> E

    H -- Get Parameter --> L[dispatch_get()]
    L --> L1[newint()]
    L --> L2[newfixed()]
    L --> L3[newenum()]
    L --> L4[newbool()]
    L1 --> M[oscsend() - Send Value]
    L2 --> M
    L3 --> M
    L4 --> M
    M --> E

    H -- Special Command --> N{Command Type}
    N -- "refresh" --> O[setrefresh()]
    O --> O1[setreg(0x3e04, 0x67cd)]
    O1 --> O2[refreshing = true]
    O2 --> O3[send_all_parameters()]
    O3 --> E

    N -- "dump" --> P[dump()]
    P --> E

    N -- "sysex" --> Q[writesysex()]
    Q --> E

    N -- "enum" --> R[oscsendenum()]
    R --> E

    %% Error handling
    G -- Invalid/Unknown --> S[osc_error()]
    S --> E
```

**Legend:**

- `device_init()`, `inputs_init()`, `outputs_init()`, `playbacks_init()`: Device and channel setup.
- `osc_server_init()`, `osc_server_poll()`: OSC server setup and polling for messages.
- `oscmsg_parse()`: Parses incoming OSC messages.
- `dispatch_set()`, `dispatch_get()`: Dispatches set/get parameter requests.
- `setreg()`, `setint()`, `setfixed()`, `setenum()`, `setbool()`: Set parameter handlers.
- `newint()`, `newfixed()`, `newenum()`, `newbool()`: Get/query parameter handlers.
- `oscsend()`, `oscsendenum()`: Sends OSC responses.
- `setrefresh()`, `send_all_parameters()`: Handles refresh command and sends all parameters.
- `dump()`, `writesysex()`: Special commands for debugging and SysEx.
- `osc_error()`: Handles errors or unknown messages.
