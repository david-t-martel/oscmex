graph TD
    subgraph "LibLO Package Structure"
        A[lo.h] --> B[lo_types.h]
        A --> C[lo_endian.h]
        A --> D[lo_osc_types.h]
        A --> E[lo_errors.h]
        A --> F[lo_lowlevel.h]

        G[lo_types_internal.h] --> B

        H[server_thread.c] --> A
        H --> G

        I[address.c] --> A
        I --> G

        J[message.c] --> A
        J --> G

        K[server.c] --> A
        K --> G

        L[method.c] --> A
        L --> G

        M[blob.c] --> A
        M --> G

        N[bundle.c] --> A
        N --> G

        O[timetag.c] --> A
        O --> G

        P[lo_cpp.h] --> A

        subgraph "C API Implementation"
            I
            J
            K
            L
            M
            N
            O
            H
        end

        subgraph "C++ Wrapper"
            P --> Q[Address class]
            P --> R[Message class]
            P --> S[ServerThread class]
            P --> T[Method class]
            P --> U[Exception class]

            Q --- R
            R --- S
            S --- T
        end
    end

    subgraph "Core Data Structures"
        V[lo_address] --- I
        W[lo_message] --- J
        X[lo_server] --- K
        Y[lo_method] --- L
        Z[lo_blob] --- M
        AA[lo_bundle] --- N
        BB[lo_timetag] --- O
        CC[lo_server_thread] --- H

        V --> Q
        W --> R
        CC --> S
        Y --> T
    end

    subgraph "Main Flow"
        direction LR
        Client[Client Application] --> |"1. Create Address"| V
        Client --> |"2. Create Message"| W
        Client --> |"3. Send Message"| SendFunc[lo_send_message]

        Server[Server Application] --> |"1. Create ServerThread"| CC
        Server --> |"2. Add Method Handler"| Y
        Server --> |"3. Start Server"| StartFunc[lo_server_thread_start]

        SendFunc --> Network[Network]
        Network --> ReceiveFunc[lo_server_recv]
        ReceiveFunc --> DispatchFunc[Dispatch to handlers]
        DispatchFunc --> Y
    end

    style A fill:#f9f,stroke:#333,stroke-width:2px
    style P fill:#bbf,stroke:#333,stroke-width:2px
    style Client fill:#bfb,stroke:#333,stroke-width:2px
    style Server fill:#bfb,stroke:#333,stroke-width:2px
