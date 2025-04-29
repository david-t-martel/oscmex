# Open Sound Control (OSC) Protocol Summary

## Introduction

Open Sound Control (OSC) is a content format and protocol designed for communication among computers, sound synthesizers, and other multimedia devices. It is optimized for modern networking technology and provides high-resolution, flexible, and time-sensitive control. Originally developed at CNMAT (Center for New Music and Audio Technologies, UC Berkeley), it offers advantages over MIDI in terms of networkability, data types, and namespace flexibility.

## OSCPP Library Conformance Note

This document summarizes the OSC 1.0 and 1.1 specifications. The `oscpp` library aims to implement these specifications fully.

* **Current Status:** Please refer to the project's [`README.md`](README.md) and [`TODO.md`](TODO.md) for the specific features and data types currently implemented and those that are pending (e.g., full pattern matching, TCP framing, array support).

## Core Concepts (OSC 1.0 & 1.1)

OSC communication is based on sending **OSC Packets** over a network (or other transport layers).

### 1. OSC Packets

There are two fundamental types of OSC Packets:

* **OSC Message:** An atomic unit of data transfer. It consists of:
  * An **OSC Address Pattern**: A null-terminated, OSC-String starting with `/`, indicating the target parameter(s) (e.g., `/mixer/channel/1/fader`).
  * An **OSC Type Tag String**: An OSC-String starting with a comma `,`, followed by characters representing the data types of the arguments (e.g., `,ifs` for int32, float32, string). If no arguments are present, a string containing only the comma `,` should be sent.
  * **OSC Arguments**: A sequence of binary data representing the values, corresponding to the type tags. Data must be 4-byte aligned (padded with null bytes if necessary).

* **OSC Bundle:** A collection of OSC Messages and/or other OSC Bundles, intended to be processed atomically based on a timestamp. It consists of:
  * The OSC-String `"#bundle"`.
  * An **OSC Time Tag**: A 64-bit fixed-point timestamp indicating when the bundle's contents should ideally take effect. It uses the NTP format (Network Time Protocol): the first 32 bits represent seconds since midnight Jan 1, 1900, and the last 32 bits represent fractional parts of a second. A value of `0x00000000 00000001` (1) represents immediate execution.
  * A sequence of **OSC Packet Elements**: Each element is preceded by its size (int32, indicating the number of bytes in the element) and contains either a complete OSC Message or another complete OSC Bundle.

### 2. OSC Address Space & Pattern Matching

* **Address Space:** OSC uses a hierarchical, symbolic naming scheme similar to URLs or file paths (e.g., `/synthesizers/granular/grain_rate`). Parts are separated by `/`. The receiving entity maintains a set of OSC Addresses corresponding to its controllable methods or parameters.
* **Pattern Matching:** Receiving applications match incoming OSC Address Patterns against the OSC Addresses of their controllable elements. OSC defines a powerful pattern matching syntax:
  * `?`: Matches any single character except `/`.
  * `*`: Matches any sequence of zero or more characters except `/`.
  * `[]`: Matches any single character listed within the brackets (e.g., `[abc]`). Ranges are allowed (e.g., `[0-9]`). `!` or `^` as the first character negates the set (e.g., `[!abc]`).
  * `{}`: Matches any string from a comma-separated list within the braces (e.g., `{foo,bar}`).
* **Dispatching:** When an OSC Message arrives, the receiving server attempts to match the Address Pattern against all known OSC Addresses in its address space. For every matching address, the corresponding OSC Method (function/procedure) is invoked with the arguments from the OSC Message. If multiple addresses match, the corresponding methods should ideally be invoked concurrently or in an undefined order.

### 3. OSC Data Types (OSC 1.0 Core)

OSC 1.0 defined a basic set of argument types, each identified by a character in the Type Tag String:

| Tag  | Type        | Description                                                                                            | Alignment |
| :--- | :---------- | :----------------------------------------------------------------------------------------------------- | :-------- |
| `i`  | int32       | 32-bit big-endian two's complement integer                                                             | 4 bytes   |
| `f`  | float32     | 32-bit big-endian IEEE 754 floating point number                                                       | 4 bytes   |
| `s`  | OSC-String  | Sequence of non-null ASCII chars, followed by a null, followed by 0-3 null padding bytes               | 4 bytes   |
| `b`  | OSC-Blob    | Binary Large Object: int32 size count, followed by that many bytes, followed by 0-3 null padding bytes | 4 bytes   |
| `t`  | OSC-TimeTag | 64-bit NTP format timestamp (used in bundles, can technically be an argument)                          | 8 bytes   |

*Note: All argument data must be padded with null bytes to ensure the total size of the argument data is a multiple of 4 bytes.*

## Updates and Additions (OSC 1.1)

OSC 1.1 builds upon OSC 1.0, adding new data types and clarifications without breaking backward compatibility for the core types.

### New Data Types in OSC 1.1

| Tag  | Type         | Description                                                                            | Argument Data | Alignment |
| :--- | :----------- | :------------------------------------------------------------------------------------- | :------------ | :-------- |
| `h`  | int64        | 64-bit big-endian two's complement integer                                             | Yes           | 8 bytes   |
| `d`  | float64      | 64-bit big-endian IEEE 754 double-precision float                                      | Yes           | 8 bytes   |
| `S`  | Symbol       | Alternate representation for OSC-string (semantics may differ by implementation)       | Yes           | 4 bytes   |
| `c`  | char         | 32-bit ASCII character (represented as an int32)                                       | Yes           | 4 bytes   |
| `r`  | RGBA color   | 32-bit RGBA color (8 bits per component: red, green, blue, alpha)                      | Yes           | 4 bytes   |
| `m`  | MIDI message | 4 bytes: Port ID (byte 0), Status byte (byte 1), Data1 (byte 2), Data2 (byte 3)        | Yes           | 4 bytes   |
| `T`  | True         | Represents the value True                                                              | No            | 0 bytes   |
| `F`  | False        | Represents the value False                                                             | No            | 0 bytes   |
| `N`  | Nil / Null   | Represents Null                                                                        | No            | 0 bytes   |
| `I`  | Infinitum    | Represents Infinity                                                                    | No            | 0 bytes   |
| `[`  | Array Begin  | Indicates the beginning of an array; type tags between `[` and `]` belong to the array | No            | 0 bytes   |
| `]`  | Array End    | Indicates the end of an array                                                          | No            | 0 bytes   |

*Note: Argument data alignment applies relative to the start of the argument data section.*

### Clarifications in OSC 1.1

OSC 1.1 also provides clearer definitions for certain aspects, improves error condition descriptions, and refines pattern matching details. Implementers should consult the OSC 1.1 specification for these nuances. Key clarifications include:

* More precise definition of OSC-String padding.
* Handling of malformed messages and bundles.
* Time tag semantics, especially regarding bundle processing order.

## Transport Layer

OSC itself only defines the *content format*. It is transport-independent. Common transport methods include:

* **UDP:** Most common due to low overhead and speed, suitable for real-time control. Packet loss and ordering are potential issues. Maximum packet size is limited by the underlying network MTU (Maximum Transmission Unit).
* **TCP:** Provides reliable, ordered transmission, useful when data integrity is paramount, but introduces latency and overhead. Requires a streaming format (e.g., SLIP encoding or length-prefixing) to delineate packet boundaries. *(Note: Framing required for oscpp)*
* **SLIP (Serial Line IP):** A simple protocol (RFC 1055) for sending IP datagrams over serial lines. OSC packets can be framed using SLIP's END (0xC0) and ESC (0xDB) characters.
* Other transports (like Files, Pipes, USB, Shared Memory) are also possible.

## Error Handling

The OSC specification does not mandate a specific error reporting mechanism back to the sender (especially difficult over UDP). Receiving applications should handle errors gracefully, such as:

* Ignoring malformed packets.
* Logging errors locally.
* Ignoring messages with address patterns that don't match any known address.
* Handling type mismatches (e.g., receiving an int where a float was expected) according to application logic (e.g., type coercion, ignoring the message, logging an error).

## Official Specification Documents

The authoritative sources for the OSC protocol are:

1. **OSC 1.0 Specification:**
    * URL: [http://opensoundcontrol.org/spec-1_0](http://opensoundcontrol.org/spec-1_0)
    * Defines the foundational structure, core data types, address patterns, and bundle format.

2. **OSC 1.1 Specification:**
    * URL: [http://opensoundcontrol.org/spec-1_1](http://opensoundcontrol.org/spec-1_1)
    * Extends OSC 1.0 with additional data types and provides clarifications. It is recommended to consult this for modern implementations.
