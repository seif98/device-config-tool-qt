// Minimal dependency-free test runner (no gtest needed).
#include "../core/device_sim.h"
#include "../core/params.h"
#include "../core/protocol.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

static int g_failed = 0, g_run = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_run;                                                                 \
        if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

using namespace dev;

static std::vector<uint8_t> setReq(uint8_t id, int32_t v) {
    Frame f; f.cmd = Set; f.payload = {id}; putI32(f.payload, v); return encode(f);
}
static std::vector<uint8_t> getReq(uint8_t id) {
    Frame f; f.cmd = Get; f.payload = {id}; return encode(f);
}
static Frame parseOne(const std::vector<uint8_t>& bytes) {
    FrameParser p; p.feed(bytes.data(), bytes.size()); Frame f; p.next(f); return f;
}

static void test_crc() {
    const char* s = "123456789";
    CHECK(crc16(reinterpret_cast<const uint8_t*>(s), std::strlen(s)) == 0x29B1);  // CCITT-FALSE check value
}

static void test_encode_decode() {
    Frame f; f.cmd = Value; f.payload = {1, 0x10, 0x27, 0, 0};
    auto bytes = encode(f);
    CHECK(bytes.size() == 10);
    CHECK(bytes[0] == kSof && bytes[1] == 5 && bytes[2] == Value);
    Frame g = parseOne(bytes);
    CHECK(g.cmd == Value && g.payload == f.payload);
}

static void test_payload_limit() {
    Frame f; f.cmd = Value; f.payload.assign(kMaxPayload + 1, 0);
    bool thrown = false;
    try { encode(f); } catch (const std::length_error&) { thrown = true; }
    CHECK(thrown);
    f.payload.assign(kMaxPayload, 0);
    CHECK(encode(f).size() == 5 + kMaxPayload);
}

static void test_parser_byte_by_byte() {
    auto bytes = getReq(3);
    FrameParser p; Frame f; int got = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        p.feed(&bytes[i], 1);
        if (p.next(f)) ++got;
    }
    CHECK(got == 1 && f.cmd == Get && f.payload.size() == 1 && f.payload[0] == 3);
}

static void test_parser_garbage_and_two_frames() {
    std::vector<uint8_t> in = {0x00, 0xFF, 0x13};          // noise
    auto a = getReq(1), b = getReq(2);
    in.insert(in.end(), a.begin(), a.end());
    in.push_back(0x77);                                    // noise between frames
    in.insert(in.end(), b.begin(), b.end());
    FrameParser p; p.feed(in.data(), in.size());
    Frame f;
    CHECK(p.next(f) && f.payload[0] == 1);
    CHECK(p.next(f) && f.payload[0] == 2);
    CHECK(!p.next(f));
    CHECK(p.crcErrors() == 0);
}

static void test_parser_crc_error_and_resync() {
    auto bad = getReq(1);
    bad[3] ^= 0x01;                                        // corrupt payload
    auto good = getReq(2);
    std::vector<uint8_t> in = bad;
    in.insert(in.end(), good.begin(), good.end());
    FrameParser p; p.feed(in.data(), in.size());
    Frame f;
    CHECK(p.next(f) && f.payload[0] == 2);                 // good frame still recovered
    CHECK(p.crcErrors() >= 1);
}

static void test_false_sof_in_payload() {
    // A payload containing 0xA5 must not confuse the parser.
    Frame f; f.cmd = Value; f.payload = {kSof, kSof, 0, 0, 0};
    auto bytes = encode(f);
    Frame g = parseOne(bytes);
    CHECK(g.payload == f.payload);
}

static void test_sim_get_set() {
    DeviceSimulator sim;
    auto r = sim.receive(getReq(1).data(), getReq(1).size());
    Frame f = parseOne(r);
    CHECK(f.cmd == Value && f.payload[0] == 1 && getI32(&f.payload[1]) == 100);   // default

    auto s = setReq(1, 250);
    f = parseOne(sim.receive(s.data(), s.size()));
    CHECK(f.cmd == Value && getI32(&f.payload[1]) == 250);
    CHECK(sim.value(1) == 250);
}

static void test_sim_errors() {
    DeviceSimulator sim;
    auto s = setReq(2, 9999);                              // Gain max is 64
    Frame f = parseOne(sim.receive(s.data(), s.size()));
    CHECK(f.cmd == Error && f.payload[0] == 2 && f.payload[1] == OutOfRange);
    CHECK(sim.value(2) == 1);                              // unchanged

    auto g = getReq(99);
    f = parseOne(sim.receive(g.data(), g.size()));
    CHECK(f.cmd == Error && f.payload[1] == UnknownParam);

    Frame bad; bad.cmd = Set; bad.payload = {1};           // too short
    auto b = encode(bad);
    f = parseOne(sim.receive(b.data(), b.size()));
    CHECK(f.cmd == Error && f.payload[1] == BadPayload);

    Frame unk; unk.cmd = 0x55;
    auto u = encode(unk);
    f = parseOne(sim.receive(u.data(), u.size()));
    CHECK(f.cmd == Error && f.payload[1] == UnknownCommand);
}

static void test_sim_multiple_requests_one_chunk() {
    DeviceSimulator sim;
    auto a = getReq(1), b = getReq(2);
    a.insert(a.end(), b.begin(), b.end());
    auto r = sim.receive(a.data(), a.size());
    FrameParser p; p.feed(r.data(), r.size());
    Frame f; int n = 0;
    while (p.next(f)) ++n;
    CHECK(n == 2);
}

static void test_sim_telemetry() {
    DeviceSimulator sim;
    Frame f = parseOne(sim.makeTelemetry(1.25));           // idle -> zero
    CHECK(f.cmd == Telemetry && f.payload.size() == 8);
    CHECK(getU32(&f.payload[0]) == 0 && getI32(&f.payload[4]) == 0);

    auto s = setReq(3, 1);                                 // Mode = run
    sim.receive(s.data(), s.size());
    f = parseOne(sim.makeTelemetry(1.25));                 // sin(2*pi*0.2*1.25) = 1
    CHECK(getU32(&f.payload[0]) == 1);
    CHECK(getI32(&f.payload[4]) == 1000);                  // amp = 1000 * gain(1)
}

static void test_param_table() {
    CHECK(findParam(1) && findParam(5) && !findParam(0));
    for (const auto& p : kParams) CHECK(p.def >= p.min && p.def <= p.max);
}

int main() {
    test_crc(); test_encode_decode(); test_payload_limit();
    test_parser_byte_by_byte(); test_parser_garbage_and_two_frames();
    test_parser_crc_error_and_resync(); test_false_sof_in_payload();
    test_sim_get_set(); test_sim_errors(); test_sim_multiple_requests_one_chunk();
    test_sim_telemetry(); test_param_table();
    std::printf("%d checks, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
