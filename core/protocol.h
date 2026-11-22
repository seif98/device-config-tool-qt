#pragma once
// Framed binary protocol: [SOF 0xA5][LEN][CMD][PAYLOAD x LEN][CRC16 lo][CRC16 hi]
// CRC-16/CCITT-FALSE is computed over LEN, CMD and PAYLOAD. Pure C++17, no Qt.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dev {

constexpr uint8_t kSof = 0xA5;
constexpr size_t kMaxPayload = 32;

enum Cmd : uint8_t {
    Get       = 0x01,  // payload: id(1)
    Set       = 0x02,  // payload: id(1) value(i32 LE)
    Value     = 0x81,  // payload: id(1) value(i32 LE)   (reply to Get/Set)
    Error     = 0x7F,  // payload: id(1) code(1)
    Telemetry = 0x90,  // payload: seq(u32 LE) reading(i32 LE, milli-units)
};

enum ErrCode : uint8_t {
    UnknownParam   = 1,
    OutOfRange     = 2,
    BadPayload     = 3,
    UnknownCommand = 4,
};

struct Frame {
    uint8_t cmd = 0;
    std::vector<uint8_t> payload;
};

uint16_t crc16(const uint8_t* data, size_t len);

// Throws std::length_error if payload exceeds kMaxPayload.
std::vector<uint8_t> encode(const Frame& frame);

// Incremental parser: feed it raw bytes in any chunking, pull complete frames out.
// Resynchronises automatically after garbage or corrupted frames.
class FrameParser {
public:
    void feed(const uint8_t* data, size_t len);
    bool next(Frame& out);
    size_t crcErrors() const { return crcErrors_; }

private:
    std::vector<uint8_t> buf_;
    size_t crcErrors_ = 0;
};

void putI32(std::vector<uint8_t>& out, int32_t v);
void putU32(std::vector<uint8_t>& out, uint32_t v);
int32_t getI32(const uint8_t* p);
uint32_t getU32(const uint8_t* p);

}  // namespace dev
