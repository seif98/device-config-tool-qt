#include "device_sim.h"
#include "params.h"
#include <cmath>

namespace dev {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void DeviceSimulator::reset() {
    values_.clear();
    for (const auto& p : kParams) values_[p.id] = p.def;
    seq_ = 0;
}

int32_t DeviceSimulator::value(uint8_t id) const {
    auto it = values_.find(id);
    return it == values_.end() ? 0 : it->second;
}

std::vector<uint8_t> DeviceSimulator::reply(uint8_t cmd, std::vector<uint8_t> payload) {
    Frame f;
    f.cmd = cmd;
    f.payload = std::move(payload);
    return encode(f);
}

std::vector<uint8_t> DeviceSimulator::error(uint8_t id, uint8_t code) {
    return reply(Error, {id, code});
}

std::vector<uint8_t> DeviceSimulator::handle(const Frame& f) {
    switch (f.cmd) {
    case Get: {
        if (f.payload.size() != 1) return error(0, BadPayload);
        const uint8_t id = f.payload[0];
        if (!findParam(id)) return error(id, UnknownParam);
        std::vector<uint8_t> p{id};
        putI32(p, values_[id]);
        return reply(Value, std::move(p));
    }
    case Set: {
        if (f.payload.size() != 5) return error(0, BadPayload);
        const uint8_t id = f.payload[0];
        const ParamDef* def = findParam(id);
        if (!def) return error(id, UnknownParam);
        const int32_t v = getI32(&f.payload[1]);
        if (v < def->min || v > def->max) return error(id, OutOfRange);
        values_[id] = v;
        std::vector<uint8_t> p{id};
        putI32(p, v);
        return reply(Value, std::move(p));
    }
    default:
        return error(0, UnknownCommand);
    }
}

std::vector<uint8_t> DeviceSimulator::receive(const uint8_t* data, size_t len) {
    parser_.feed(data, len);
    std::vector<uint8_t> out;
    Frame f;
    while (parser_.next(f)) {
        auto r = handle(f);
        out.insert(out.end(), r.begin(), r.end());
    }
    return out;
}

std::vector<uint8_t> DeviceSimulator::makeTelemetry(double t) {
    // A sine wave whose amplitude follows the Gain setting; Mode 0 (idle) reads zero.
    const double amp = 1000.0 * value(2);
    const int32_t reading = value(3) == 0 ? 0
                                          : static_cast<int32_t>(std::lround(amp * std::sin(2 * kPi * 0.2 * t)));
    std::vector<uint8_t> p;
    putU32(p, seq_++);
    putI32(p, reading);
    return reply(Telemetry, std::move(p));
}

}  // namespace dev
