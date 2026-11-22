#include "protocol.h"
#include <stdexcept>

namespace dev {

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

std::vector<uint8_t> encode(const Frame& f) {
    if (f.payload.size() > kMaxPayload)
        throw std::length_error("payload too long");
    std::vector<uint8_t> out;
    out.reserve(5 + f.payload.size());
    out.push_back(kSof);
    out.push_back(static_cast<uint8_t>(f.payload.size()));
    out.push_back(f.cmd);
    out.insert(out.end(), f.payload.begin(), f.payload.end());
    const uint16_t crc = crc16(out.data() + 1, 2 + f.payload.size());
    out.push_back(static_cast<uint8_t>(crc & 0xFF));
    out.push_back(static_cast<uint8_t>(crc >> 8));
    return out;
}

void FrameParser::feed(const uint8_t* data, size_t len) {
    buf_.insert(buf_.end(), data, data + len);
}

bool FrameParser::next(Frame& out) {
    while (!buf_.empty()) {
        if (buf_[0] != kSof) {                 // discard noise before a frame start
            buf_.erase(buf_.begin());
            continue;
        }
        if (buf_.size() < 2) return false;     // need LEN
        const size_t len = buf_[1];
        if (len > kMaxPayload) {               // impossible length: false start byte
            buf_.erase(buf_.begin());
            continue;
        }
        const size_t total = 5 + len;
        if (buf_.size() < total) return false; // wait for the rest
        const uint16_t calc = crc16(buf_.data() + 1, 2 + len);
        const uint16_t rx = static_cast<uint16_t>(buf_[3 + len] | (buf_[4 + len] << 8));
        if (calc != rx) {                      // corrupted: skip this SOF and resync
            ++crcErrors_;
            buf_.erase(buf_.begin());
            continue;
        }
        out.cmd = buf_[2];
        out.payload.assign(buf_.begin() + 3, buf_.begin() + 3 + len);
        buf_.erase(buf_.begin(), buf_.begin() + total);
        return true;
    }
    return false;
}

void putI32(std::vector<uint8_t>& out, int32_t v) { putU32(out, static_cast<uint32_t>(v)); }

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

uint32_t getU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

int32_t getI32(const uint8_t* p) { return static_cast<int32_t>(getU32(p)); }

}  // namespace dev
