# OSC Implementation TODO List

This list tracks the remaining work, prioritizing the core features needed for a functional OSC library based on the current status.

## Core Implementation Priorities (Aligned with README Roadmap)

1. **Complete `osc::Value` Implementation:**
    * Implement constructors, type-checkers (`is...`), and accessors (`as...`) for *all* OSC 1.0/1.1 types (`i, h, f, d, s, S, b, t, c, r, m, T, F, N, I`).
    * Implement `Value::serialize()` to correctly format each type (big-endian, padding).
    * Implement `Value::typeTag()` to return the correct character.
    * Add support for OSC Arrays (`[` and `]`) within `osc::Value` (likely via `std::vector<osc::Value>`).

2. **Complete `osc::Message` and `osc::Bundle` Handling:**
    * Implement `Message::serialize()` using `Value::serialize()` for arguments, ensuring correct path, type tag string, and argument data formatting/alignment.
    * Implement static `Message::deserialize()` to parse binary data into a `Message`.
    * Add `Message::add...()` methods for all missing OSC 1.1 types (Int64, Double, Symbol, Char, Color, Midi, Bool, Nil, Infinitum, Array).
    * Implement `Bundle::serialize()` formatting (`#bundle`, time tag, size-prefixed elements).
    * Implement static `Bundle::deserialize()` to parse binary bundles, including nested elements.
    * Implement `Bundle::forEach` or similar iteration mechanism.

3. **Implement OSC Address Pattern Matching:**
    * Implement matching logic within `ServerImpl` to handle `?`, `*`, `[]`, `{}` according to OSC spec.
    * Integrate matching into the server's message dispatch routine to select the correct `MethodHandler`.
    * Consider optimizations (e.g., pattern tree) if performance becomes an issue later.

4. **Implement TCP Message Framing:**
    * Choose a framing method (e.g., SLIP encoding or length-prefixing).
    * Implement framing in `AddressImpl::send` for TCP connections.
    * Implement de-framing in `ServerImpl`'s TCP receiving logic to reconstruct complete OSC packets.

5. **Consolidate & Integrate Error Handling:**
    * Remove redundant exception definitions (`Exceptions.h`).
    * Standardize on `osc::OSCException` from `Types.h`.
    * Throw `OSCException` for critical errors (parsing, network setup, invalid arguments/types).
    * Ensure `Server::ErrorHandler` callback is invoked for relevant non-fatal runtime errors.
    * Review and refine error codes (`OSCException::ErrorCode`) and messages.

6. **Add Unit Tests:**
    * Create comprehensive unit tests for `Value` (all types, serialization/deserialization).
    * Test `Message` serialization/deserialization with various argument combinations.
    * Test `Bundle` serialization/deserialization, including nesting.
    * Test pattern matching logic thoroughly.
    * Test `Address` and `Server` with UDP, TCP (once framing is done), and UNIX sockets.
    * Test `TimeTag` conversions and comparisons.

7. **Expand Examples & Documentation:**
    * Update `README.md` examples to be fully functional once dependencies are met.
    * Add more examples covering different use cases (sending bundles, various data types, error handling).
    * Add Doxygen comments or similar for comprehensive API documentation.
    * Verify `SPECIFICATION.md` and `ARCHITECTURE.md` remain accurate.

## Secondary Improvements & Features (Post-Core Implementation)

* **API Consistency & Refinements:**
  * Review method naming across classes.
  * Consider adding a generic `Message::add(Value)` or template `add<T>()`.
  * Refine argument access in `Message` (e.g., `message.get<float>(0)`).
* **Performance Optimization:**
  * Profile serialization/deserialization.
  * Consider message/buffer pooling for high-frequency scenarios.
* **Features:**
  * Implement automatic type coercion in `Server` (optional).
  * Add support for OSC query namespace extension.
* **Cross-Platform Compatibility:**
  * Thoroughly test on Windows, Linux, macOS.
  * Address any discovered endianness or socket handling issues.
* **Code Quality:**
  * Add static analysis (e.g., Clang-Tidy) to CI.
  * Ensure C++17 best practices.
* **CI/CD:**
  * Set up a robust CI/CD pipeline (GitHub Actions, etc.) for building and testing.

## Long-term Goals

* Extended Protocol Support (WebSockets, MQTT)
* Tooling (Debugging, Visualization)
* Benchmarking
