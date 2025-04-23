# OSC Protocol Implementation Requirements

This document outlines key requirements for implementing systems based on the Open Sound Control (OSC) 1.0 and 1.1 specifications.

## I. Packet Structure & Encoding

* **Requirement 1. Packet Types:** Systems shall support the reception and/or transmission of the two fundamental OSC Packet types: OSC Messages and OSC Bundles.
* **Requirement 2. Message Format:** An OSC Message implementation shall consist of:
  * 2.1. An OSC Address Pattern (OSC-String format, starting with '/').
  * 2.2. An OSC Type Tag String (OSC-String format, starting with ',').
  * 2.3. Zero or more OSC Arguments, matching the types declared in the Type Tag String.
* **Requirement 3. Bundle Format:** An OSC Bundle implementation shall consist of:
  * 3.1. The OSC-String `"#bundle"`.
  * 3.2. An OSC Time Tag (64-bit NTP format).
  * 3.3. Zero or more OSC Packet Elements, where each element is preceded by its size (int32) and contains a valid OSC Message or OSC Bundle.
* **Requirement 4. Data Alignment:** All components within an OSC Packet (Address Pattern, Type Tag String, Arguments, Bundle Elements) shall adhere to 4-byte alignment boundaries. Padding shall be achieved using null bytes (`\0`). Specific data types (e.g., int64, float64, Time Tag) require 8-byte alignment relative to the start of the packet, which is naturally handled if all preceding elements adhere to 4-byte alignment.
* **Requirement 5. Endianness:** All numeric data types (int32, float32, int64, float64, Time Tag, size counts) shall be encoded in big-endian byte order.
* **Requirement 6. OSC-String Format:** OSC-Strings shall consist of a sequence of non-null ASCII characters, followed by a single null terminator character, followed by zero to three additional null padding characters to ensure 4-byte alignment.
* **Requirement 7. OSC-Blob Format:** OSC-Blobs shall consist of a 32-bit big-endian integer specifying the number of data bytes, followed by the raw data bytes, followed by zero to three null padding characters to ensure 4-byte alignment for the entire blob structure.

## II. Data Types

* **Requirement 8. Core Data Types (OSC 1.0):** Implementations shall support encoding and decoding of the following core data types: `i` (int32), `f` (float32), `s` (OSC-String), `b` (OSC-Blob). Support for `t` (OSC-TimeTag) as an argument type is optional but recommended.
* **Requirement 9. Extended Data Types (OSC 1.1):** For OSC 1.1 compliance, implementations shall additionally support encoding and decoding of: `h` (int64), `d` (float64), `S` (Symbol), `c` (char), `r` (RGBA color), `m` (MIDI message), `T` (True), `F` (False), `N` (Nil), `I` (Infinitum), `[` (Array Begin), `]` (Array End).
* **Requirement 10. Typeless Types:** The types `T`, `F`, `N`, `I`, `[`, `]` do not have corresponding argument data bytes but must be correctly represented in the Type Tag String.

## III. Addressing and Dispatching

* **Requirement 11. Address Pattern Syntax:** Address Patterns shall start with `/` and use `/` as a separator for hierarchical parts.
* **Requirement 12. Pattern Matching:** Receiving systems shall implement pattern matching logic supporting at least the following wildcard characters in incoming Address Patterns: `?`, `*`, `[]`, `{}` (as defined in OSC 1.0/1.1).
* **Requirement 13. Message Dispatch:** Upon receiving an OSC Message, the system shall attempt to match the Address Pattern against its internal OSC Address Space. For each match found, the corresponding OSC Method (or callback function) shall be invoked with the arguments provided in the message.
* **Requirement 14. Bundle Processing:** Upon receiving an OSC Bundle:
  * 14.1. If the Time Tag is 1 (`0x00000000 00000001`), the contained OSC Packets shall be processed immediately and atomically.
  * 14.2. If the Time Tag represents a future time, the contained OSC Packets shall be processed atomically at the specified time. The system must provide a mechanism for scheduling bundle execution.
  * 14.3. The atomicity requirement implies that all messages within a bundle should be executed without any other OSC message execution interleaving them.

## IV. Transport Layer

* **Requirement 15. Transport Independence:** The OSC content format shall be implemented independently of the transport mechanism.
* **Requirement 16. UDP Transport:** If UDP is used, the implementation must be aware of potential packet loss, duplication, and reordering. Packet size limitations (MTU) must be considered.
* **Requirement 17. TCP Transport:** If TCP is used, a mechanism for delineating OSC Packet boundaries within the stream must be implemented (e.g., SLIP encoding, length prefixing).
* **Requirement 18. SLIP Framing (Optional):** If SLIP is used (e.g., over serial), the implementation shall correctly frame OSC packets using SLIP END (0xC0) and ESC (0xDB) characters, including proper escaping of data bytes matching END or ESC.

## V. Error Handling

* **Requirement 19. Malformed Packets:** Receiving systems should detect malformed OSC packets (e.g., incorrect alignment, invalid type tags, size mismatches) and should discard them, preferably logging an error.
* **Requirement 20. Unmatched Messages:** Receiving systems should handle OSC Messages with Address Patterns that do not match any known OSC Address (e.g., ignore them silently or log a warning).
* **Requirement 21. Type Mismatches:** Receiving systems should define a clear behavior for handling messages where the received argument types (per the Type Tag String) do not match the types expected by the matched OSC Method (e.g., attempt type coercion, ignore the argument, ignore the message, log an error).
