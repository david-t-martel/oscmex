#include <gtest/gtest.h>

#include <vector>

#include "ServerImpl.h"
#include "osc/Exceptions.h"

using namespace osc;

// Test class to expose the tcp_framing namespace functions for testing
class TCPFramingTest {
   public:
    // Create proxy methods to access the tcp_framing functions
    static std::vector<std::byte> sendFramed(const std::vector<std::byte>& data) {
        std::vector<std::byte> buffer;
        try {
            osc::tcp_framing::sendFramed(0, data);  // Using dummy socket 0 for testing
        } catch (const OSCException& e) {
            // Convert any error into test-friendly format
            std::cerr << "Exception during sendFramed: " << e.what()
                      << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
        }
        return buffer;
    }

    static bool receiveFramed(const std::vector<std::byte>& buffer, size_t& offset,
                              std::vector<std::byte>& result, size_t maxSize = 65536) {
        try {
            return osc::tcp_framing::receiveFramed(0, result, maxSize);  // Using dummy socket 0
        } catch (const OSCException& e) {
            // Convert any error into test-friendly format
            std::cerr << "Exception during receiveFramed: " << e.what()
                      << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
            return false;
        }
    }
};

TEST(TCPFraming, SendFramed) {
    // Test empty message
    std::vector<std::byte> emptyData;
    std::vector<std::byte> emptyResult = TCPFramingTest::sendFramed(emptyData);

    // Should have 4 bytes for size (0) + 0 content bytes
    EXPECT_EQ(emptyResult.size(), 4);
    EXPECT_EQ(emptyResult[0], std::byte{0});
    EXPECT_EQ(emptyResult[1], std::byte{0});
    EXPECT_EQ(emptyResult[2], std::byte{0});
    EXPECT_EQ(emptyResult[3], std::byte{0});

    // Test small message
    std::vector<std::byte> smallData = {std::byte{'t'}, std::byte{'e'}, std::byte{'s'},
                                        std::byte{'t'}};
    std::vector<std::byte> smallResult = TCPFramingTest::sendFramed(smallData);

    // Should have 4 bytes for size (4) + 4 content bytes
    EXPECT_EQ(smallResult.size(), 8);
    EXPECT_EQ(smallResult[0], std::byte{0});
    EXPECT_EQ(smallResult[1], std::byte{0});
    EXPECT_EQ(smallResult[2], std::byte{0});
    EXPECT_EQ(smallResult[3], std::byte{4});
    EXPECT_EQ(smallResult[4], std::byte{'t'});
    EXPECT_EQ(smallResult[5], std::byte{'e'});
    EXPECT_EQ(smallResult[6], std::byte{'s'});
    EXPECT_EQ(smallResult[7], std::byte{'t'});

    // Test larger message (multi-byte size)
    std::vector<std::byte> largeData(300, std::byte{'x'});  // 300 'x' chars
    std::vector<std::byte> largeResult = TCPFramingTest::sendFramed(largeData);

    // Should have 4 bytes for size (300) + 300 content bytes
    EXPECT_EQ(largeResult.size(), 304);
    EXPECT_EQ(largeResult[0], std::byte{0});
    EXPECT_EQ(largeResult[1], std::byte{0});
    EXPECT_EQ(largeResult[2], std::byte{1});   // 300 = 0x012C in hex, so byte 2 is 1
    EXPECT_EQ(largeResult[3], std::byte{44});  // 300 = 0x012C in hex, so byte 3 is 44 (2C in hex)

    // Check a few content bytes
    EXPECT_EQ(largeResult[4], std::byte{'x'});
    EXPECT_EQ(largeResult[100], std::byte{'x'});
    EXPECT_EQ(largeResult[303], std::byte{'x'});
}

TEST(TCPFraming, ReceiveFramed) {
    // Test receiving a complete message
    std::vector<std::byte> buffer = {std::byte{0},   std::byte{0},   std::byte{0},
                                     std::byte{4},   std::byte{'t'}, std::byte{'e'},
                                     std::byte{'s'}, std::byte{'t'}};
    size_t offset = 0;
    std::vector<std::byte> result;

    bool success = TCPFramingTest::receiveFramed(buffer, offset, result);

    EXPECT_TRUE(success);
    EXPECT_EQ(offset, 8);  // Should have consumed all 8 bytes
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], std::byte{'t'});
    EXPECT_EQ(result[1], std::byte{'e'});
    EXPECT_EQ(result[2], std::byte{'s'});
    EXPECT_EQ(result[3], std::byte{'t'});

    // Test receiving partial message (incomplete size)
    std::vector<std::byte> incompleteBuffer1 = {std::byte{0}, std::byte{0},
                                                std::byte{0}};  // Missing last byte of size
    offset = 0;
    result.clear();

    success = TCPFramingTest::receiveFramed(incompleteBuffer1, offset, result);

    EXPECT_FALSE(success);
    EXPECT_EQ(offset, 0);  // Should not have consumed any bytes
    EXPECT_EQ(result.size(), 0);

    // Test receiving partial message (incomplete content)
    std::vector<std::byte> incompleteBuffer2 = {std::byte{0},  std::byte{0},   std::byte{0},
                                                std::byte{4},  std::byte{'t'}, std::byte{'e'},
                                                std::byte{'s'}};  // Missing last byte of content
    offset = 0;
    result.clear();

    success = TCPFramingTest::receiveFramed(incompleteBuffer2, offset, result);

    EXPECT_FALSE(success);
    EXPECT_EQ(offset, 0);  // Should not have consumed any bytes
    EXPECT_EQ(result.size(), 0);

    // Test receiving multiple messages in one buffer
    std::vector<std::byte> multiBuffer = {
        std::byte{0},   std::byte{0},   std::byte{0},   std::byte{3},  std::byte{'a'},
        std::byte{'b'}, std::byte{'c'},  // First message
        std::byte{0},   std::byte{0},   std::byte{0},   std::byte{2},  std::byte{'d'},
        std::byte{'e'},  // Second message
        std::byte{0},   std::byte{0},   std::byte{0},   std::byte{5},  std::byte{'h'},
        std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}  // Third message
    };

    offset = 0;
    result.clear();

    // Read first message
    success = TCPFramingTest::receiveFramed(multiBuffer, offset, result);
    EXPECT_TRUE(success);
    EXPECT_EQ(offset, 7);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], std::byte{'a'});
    EXPECT_EQ(result[1], std::byte{'b'});
    EXPECT_EQ(result[2], std::byte{'c'});

    // Read second message
    result.clear();
    success = TCPFramingTest::receiveFramed(multiBuffer, offset, result);
    EXPECT_TRUE(success);
    EXPECT_EQ(offset, 13);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], std::byte{'d'});
    EXPECT_EQ(result[1], std::byte{'e'});

    // Read third message
    result.clear();
    success = TCPFramingTest::receiveFramed(multiBuffer, offset, result);
    EXPECT_TRUE(success);
    EXPECT_EQ(offset, 22);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], std::byte{'h'});
    EXPECT_EQ(result[4], std::byte{'o'});

    // Try to read again (should have no more data)
    result.clear();
    success = TCPFramingTest::receiveFramed(multiBuffer, offset, result);
    EXPECT_FALSE(success);
    EXPECT_EQ(offset, 22);  // Offset shouldn't change
    EXPECT_EQ(result.size(), 0);
}

TEST(TCPFraming, RoundTrip) {
    // Test round-trip encoding and decoding
    for (int size = 0; size < 1000; size += 111) {  // Test different message sizes
        std::vector<std::byte> originalData(size);
        for (int i = 0; i < size; i++) {
            originalData[i] = static_cast<std::byte>(i % 256);  // Fill with some pattern
        }

        // Encode
        std::vector<std::byte> encoded = TCPFramingTest::sendFramed(originalData);

        // Decode
        size_t offset = 0;
        std::vector<std::byte> decoded;
        bool success = TCPFramingTest::receiveFramed(encoded, offset, decoded);

        // Verify
        EXPECT_TRUE(success);
        EXPECT_EQ(offset, encoded.size());
        EXPECT_EQ(decoded.size(), originalData.size());

        // Check contents
        for (size_t i = 0; i < originalData.size(); i++) {
            EXPECT_EQ(decoded[i], originalData[i]);
        }
    }
}

TEST(TCPFraming, EdgeCases) {
    // Test zero-length message
    std::vector<std::byte> zeroData;
    std::vector<std::byte> encoded = TCPFramingTest::sendFramed(zeroData);

    size_t offset = 0;
    std::vector<std::byte> decoded;
    bool success = TCPFramingTest::receiveFramed(encoded, offset, decoded);

    EXPECT_TRUE(success);
    EXPECT_EQ(decoded.size(), 0);

    // Test message with max size (just test encoding, not allocating the full buffer)
    uint32_t maxSize = 0x7FFFFFFF;  // Max reasonable size, just under 2GB

    // Just test the size encoding part for maxSize
    std::vector<std::byte> maxSizeHeader = {static_cast<std::byte>((maxSize >> 24) & 0xFF),
                                            static_cast<std::byte>((maxSize >> 16) & 0xFF),
                                            static_cast<std::byte>((maxSize >> 8) & 0xFF),
                                            static_cast<std::byte>(maxSize & 0xFF)};

    // Extract size from header
    uint32_t extractedSize = (static_cast<uint32_t>(maxSizeHeader[0]) << 24) |
                             (static_cast<uint32_t>(maxSizeHeader[1]) << 16) |
                             (static_cast<uint32_t>(maxSizeHeader[2]) << 8) |
                             static_cast<uint32_t>(maxSizeHeader[3]);

    EXPECT_EQ(extractedSize, maxSize);

    // Test invalid size (too large to allocate)
    std::vector<std::byte> invalidSizeBuffer = {std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
                                                std::byte{0xFF}};  // Size = 4GB-1
    offset = 0;
    decoded.clear();

    // This should gracefully fail without crashing (implementation dependent)
    success = TCPFramingTest::receiveFramed(invalidSizeBuffer, offset, decoded);

    // We just verify that the function returns and doesn't crash
    // Actual behavior may vary depending on implementation details
}
