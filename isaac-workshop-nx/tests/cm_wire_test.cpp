#include "isaacnx/cm_wire.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

int main() {
    using namespace isaacnx;
    const std::vector<uint8_t> header{0x0a, 0x01, 0x01}, body{0x10, 0x2a};
    std::vector<uint8_t> packet;
    std::string error;
    assert(encode_cm_proto_frame(123, header, body, packet, error));
    assert((packet == std::vector<uint8_t>{0x7b, 0, 0, 0x80, 3, 0, 0, 0,
                                           0x0a, 1, 1, 0x10, 0x2a}));
    CmProtoFrame parsed;
    assert(decode_cm_proto_frame(packet.data(), packet.size(), parsed, error));
    assert(parsed.emsg == 123 && parsed.header == header && parsed.body == body);
    assert(!encode_cm_proto_frame(0, header, body, packet, error));
    assert(!encode_cm_proto_frame(0x80000001u, header, body, packet, error));
    assert(!encode_cm_proto_frame(123, std::vector<uint8_t>(kMaxCmHeader + 1), body, packet, error));
    assert(!encode_cm_proto_frame(123, header, std::vector<uint8_t>(kMaxCmFrame), packet, error));
    assert(encode_cm_proto_frame(123, header, body, packet, error));
    for (size_t i = 0; i < 8; ++i) assert(!decode_cm_proto_frame(packet.data(), i, parsed, error));
    assert(!decode_cm_proto_frame(nullptr, packet.size(), parsed, error));
    auto wrong = packet;
    wrong[3] = 0;
    assert(!decode_cm_proto_frame(wrong.data(), wrong.size(), parsed, error));
    wrong = packet;
    wrong[4] = 0xff; wrong[5] = 0xff;
    assert(!decode_cm_proto_frame(wrong.data(), wrong.size(), parsed, error));
    wrong = packet;
    wrong[0] = 0; wrong[1] = 0; wrong[2] = 0;
    assert(!decode_cm_proto_frame(wrong.data(), wrong.size(), parsed, error));
    std::mt19937 rng(20261116);
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> fuzz(static_cast<size_t>(rng() % 60));
        for (auto& v : fuzz) v = static_cast<uint8_t>(rng());
        decode_cm_proto_frame(fuzz.data(), fuzz.size(), parsed, error);
    }
    std::puts("Steam CM protobuf frame tests passed");
}
