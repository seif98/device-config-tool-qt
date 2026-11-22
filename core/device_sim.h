#pragma once
#include "protocol.h"
#include <map>
#include <vector>

namespace dev {

// A software model of the device: answers Get/Set requests and produces telemetry.
// Lets the GUI (and the tests) run with no hardware attached.
class DeviceSimulator {
public:
    DeviceSimulator() { reset(); }

    void reset();

    // Feed bytes received from the host. Returns the encoded reply frames (may be empty).
    std::vector<uint8_t> receive(const uint8_t* data, size_t len);

    // One telemetry frame for the given time in seconds.
    std::vector<uint8_t> makeTelemetry(double tSeconds);

    int32_t value(uint8_t id) const;

private:
    std::vector<uint8_t> handle(const Frame& f);
    static std::vector<uint8_t> reply(uint8_t cmd, std::vector<uint8_t> payload);
    static std::vector<uint8_t> error(uint8_t id, uint8_t code);

    FrameParser parser_;
    std::map<uint8_t, int32_t> values_;
    uint32_t seq_ = 0;
};

}  // namespace dev
