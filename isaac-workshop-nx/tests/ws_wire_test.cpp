#include "isaacnx/ws_wire.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace isaacnx;

int main() {
    // RFC 6455, Section 4.2.2: SHA1 of client key + magic GUID.
    const uint8_t nonce[16] = {
        't','h','e',' ','s','a','m','p','l','e',' ','n','o','n','c','e'
    };
    const std::string key = ws_client_key(nonce);
    assert(key == "dGhlIHNhbXBsZSBub25jZQ==");
    assert(ws_expected_accept(key) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    const std::string upgrade =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
    size_t consumed = 0;
    std::string error;
    assert(ws_validate_upgrade(upgrade + "DATA", key, consumed, error));
    assert(consumed == upgrade.size());
    assert(!ws_validate_upgrade("HTTP/1.1 200 OK\r\n\r\n", key, consumed, error));
    assert(!ws_validate_upgrade(upgrade.substr(0, upgrade.size() - 2), key, consumed, error));
    assert(!ws_validate_upgrade(upgrade + std::string(9000, 'X'), "wrongkey", consumed, error));
    std::string duplicate = upgrade;
    duplicate.insert(duplicate.size() - 2, "Upgrade: websocket\r\n");
    assert(!ws_validate_upgrade(duplicate, key, consumed, error));
    std::string extensions = upgrade;
    extensions.insert(extensions.size() - 2, "Sec-WebSocket-Extensions: permessage-deflate\r\n");
    assert(!ws_validate_upgrade(extensions, key, consumed, error));

    // RFC 6455, Section 5.7: masked Hello frame from a client.
    const std::vector<uint8_t> hello {'H','e','l','l','o'};
    const uint8_t mask[] {0x37, 0xfa, 0x21, 0x3d};
    std::vector<uint8_t> encoded;
    assert(ws_encode_client_frame(1, hello, mask, encoded, error));
    assert(encoded == (std::vector<uint8_t>{0x81,0x85,0x37,0xfa,0x21,0x3d,0x7f,0x9f,0x4d,0x51,0x58}));
    assert(!ws_encode_client_frame(3, hello, mask, encoded, error));
    assert(!ws_encode_client_frame(9, std::vector<uint8_t>(126, 0), mask, encoded, error));

    WsFrame frame;
    const uint8_t server[] {0x82, 0x03, 'O', 'K', '!'};
    assert(ws_decode_server_frame(server, 4, frame, consumed, error) == WsParseResult::Incomplete);
    assert(ws_decode_server_frame(server, 5, frame, consumed, error) == WsParseResult::Ready);
    assert(consumed == 5 && frame.fin && frame.opcode == 2);
    assert((frame.payload == std::vector<uint8_t>{'O','K','!'}));
    const uint8_t masked[] {0x82, 0x81, 0,0,0,0, 'X'};
    assert(ws_decode_server_frame(masked, sizeof masked, frame, consumed, error) == WsParseResult::Invalid);
    const uint8_t compressed[] {0xc2, 0x00};
    assert(ws_decode_server_frame(compressed, sizeof compressed, frame, consumed, error) == WsParseResult::Invalid);
    const uint8_t huge[] {0x82, 0x7f, 0, 0, 0, 1, 0, 0, 0, 0};
    assert(ws_decode_server_frame(huge, sizeof huge, frame, consumed, error) == WsParseResult::Invalid);

    // Deterministic malformed/fuzz corpus; never allocate unbounded memory.
    std::mt19937 rng(0x49534141u);
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::vector<uint8_t> bytes(rng() % 200);
        for (uint8_t& b : bytes) b = static_cast<uint8_t>(rng());
        const auto result = ws_decode_server_frame(bytes.data(), bytes.size(), frame, consumed, error);
        if (result == WsParseResult::Ready) {
            assert(consumed <= bytes.size() && consumed >= 2);
            assert(frame.payload.size() <= kMaxWsPayload);
        }
    }
    std::cout << "RFC6455 + 20000 malformed frames: OK\n";
}
