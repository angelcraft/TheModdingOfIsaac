#include "isaacnx/cm_multi.hpp"
#include "isaacnx/cm_wire.hpp"
#include <zlib.h>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace isaacnx;

static void varint(std::vector<uint8_t>& buf, uint64_t n) {
    while (n > 127) { buf.push_back(static_cast<uint8_t>(n) | 0x80); n >>= 7; }
    buf.push_back(static_cast<uint8_t>(n));
}

static std::vector<uint8_t> multi(const std::vector<uint8_t>& children,
                                  bool gzip, std::string& error) {
    std::vector<uint8_t> payload = children;
    if (gzip) {
        z_stream zs {};
        assert(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                             15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK);
        payload.resize(deflateBound(&zs, static_cast<uLong>(children.size())));
        zs.next_in = const_cast<Bytef*>(children.data());
        zs.avail_in = static_cast<uInt>(children.size());
        zs.next_out = payload.data();
        zs.avail_out = static_cast<uInt>(payload.size());
        assert(deflate(&zs, Z_FINISH) == Z_STREAM_END);
        payload.resize(zs.total_out);
        deflateEnd(&zs);
    }
    std::vector<uint8_t> body;
    varint(body, 0x08); varint(body, gzip ? children.size() : 0);
    varint(body, 0x12); varint(body, payload.size());
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<uint8_t> frame;
    assert(encode_cm_proto_frame(1, {}, body, frame, error));
    return frame;
}

int main() {
    std::string error;
    std::vector<uint8_t> single;
    assert(encode_cm_proto_frame(751, {}, {0x08,0x01}, single, error));
    std::vector<std::vector<uint8_t>> messages;
    assert(cm_unpack_messages(single.data(), single.size(), messages, error));
    assert(messages.size() == 1 && messages[0] == single);
    std::vector<uint8_t> children;
    for (int i = 0; i < 3; ++i) {
        const uint32_t n = static_cast<uint32_t>(single.size());
        for (int byte = 0; byte < 4; ++byte)
            children.push_back(static_cast<uint8_t>(n >> (8 * byte)));
        children.insert(children.end(), single.begin(), single.end());
    }
    for (bool compressed : {false, true}) {
        const auto wrapped = multi(children, compressed, error);
        assert(cm_unpack_messages(wrapped.data(), wrapped.size(), messages, error));
        assert(messages.size() == 3);
        for (const auto& m : messages) assert(m == single);
        auto damaged = wrapped;
        damaged.back() ^= 0xff;
        if (compressed)
            assert(!cm_unpack_messages(damaged.data(), damaged.size(), messages, error));
    }
    // Length prefix must never let content escape its enclosing payload.
    children[0] = 0xff;
    const auto invalid = multi(children, false, error);
    assert(!cm_unpack_messages(invalid.data(), invalid.size(), messages, error));

    std::mt19937 rng(0x434D4D55);
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> bytes(rng() % 200);
        for (auto& b : bytes) b = static_cast<uint8_t>(rng());
        (void)cm_unpack_messages(bytes.data(), bytes.size(), messages, error);
    }
    std::cout << "CM Multi/gzip + 20000 malformed frames OK\n";
}
