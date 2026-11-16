#include "isaacnx/ws_wire.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace isaacnx {
namespace {

uint32_t rol(uint32_t v, unsigned n) { return (v << n) | (v >> (32 - n)); }

std::array<uint8_t, 20> sha1(const std::string& input) {
    std::vector<uint8_t> data(input.begin(), input.end());
    const uint64_t length_bits = static_cast<uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (unsigned i = 0; i < 8; ++i)
        data.push_back(static_cast<uint8_t>(length_bits >> ((7 - i) * 8)));

    uint32_t h[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    for (size_t offset = 0; offset < data.size(); offset += 64) {
        uint32_t w[80] {};
        for (unsigned j = 0; j < 16; ++j) {
            const size_t p = offset + 4 * j;
            w[j] = (uint32_t(data[p]) << 24) | (uint32_t(data[p + 1]) << 16) |
                   (uint32_t(data[p + 2]) << 8) | uint32_t(data[p + 3]);
        }
        for (unsigned j = 16; j < 80; ++j)
            w[j] = rol(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (unsigned j = 0; j < 80; ++j) {
            uint32_t f, k;
            if (j < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
            else if (j < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
            else if (j < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
            else { f = b ^ c ^ d; k = 0xca62c1d6u; }
            const uint32_t t = rol(a, 5) + f + e + k + w[j];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::array<uint8_t, 20> digest {};
    for (unsigned i = 0; i < 5; ++i)
        for (unsigned j = 0; j < 4; ++j)
            digest[4 * i + j] = static_cast<uint8_t>(h[i] >> (24 - 8 * j));
    return digest;
}

std::string b64(const uint8_t* p, size_t n) {
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t(p[i]) << 16) |
                (i + 1 < n ? uint32_t(p[i + 1]) << 8 : 0) |
                (i + 2 < n ? uint32_t(p[i + 2]) : 0);
        out.push_back(alphabet[(v >> 18) & 63]);
        out.push_back(alphabet[(v >> 12) & 63]);
        out.push_back(i + 1 < n ? alphabet[(v >> 6) & 63] : '=');
        out.push_back(i + 2 < n ? alphabet[v & 63] : '=');
    }
    return out;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

bool contains_token(const std::string& field, const std::string& token) {
    size_t pos = 0;
    while (pos < field.size()) {
        size_t end = field.find(',', pos);
        if (end == std::string::npos) end = field.size();
        if (lower(trim(field.substr(pos, end - pos))) == token) return true;
        pos = end + 1;
    }
    return false;
}

bool opcode_valid(uint8_t op) {
    return op == 0 || op == 1 || op == 2 || op == 8 || op == 9 || op == 10;
}

} // namespace

std::string ws_client_key(const uint8_t nonce[16]) {
    return nonce ? b64(nonce, 16) : std::string();
}

std::string ws_expected_accept(const std::string& client_key) {
    static constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto h = sha1(client_key + kGuid);
    return b64(h.data(), h.size());
}

bool ws_validate_upgrade(const std::string& raw, const std::string& key,
                         size_t& consumed, std::string& error) {
    consumed = 0;
    const size_t end = raw.find("\r\n\r\n");
    if (end == std::string::npos || end + 4 > kMaxWsHttpHeader) {
        error = "Steam CM: encabezado WebSocket incompleto o excesivo";
        return false;
    }
    const size_t line_end = raw.find("\r\n");
    if (line_end == std::string::npos ||
        (raw.substr(0, line_end) != "HTTP/1.1 101" &&
         raw.compare(0, 13, "HTTP/1.1 101 ") != 0)) {
        error = "Steam CM: no se acepto HTTP 101";
        return false;
    }
    std::map<std::string, std::string> headers;
    for (size_t pos = line_end + 2; pos < end;) {
        const size_t next = raw.find("\r\n", pos);
        if (next == std::string::npos || next > end || next == pos) {
            error = "Steam CM: encabezado HTTP mal formado"; return false;
        }
        const size_t colon = raw.find(':', pos);
        if (colon == std::string::npos || colon >= next || colon == pos) {
            error = "Steam CM: campo HTTP mal formado"; return false;
        }
        const std::string name = lower(raw.substr(pos, colon - pos));
        const std::string value = trim(raw.substr(colon + 1, next - colon - 1));
        if (headers.count(name)) {
            error = "Steam CM: campo HTTP repetido"; return false;
        }
        headers[name] = value;
        pos = next + 2;
    }
    if (!contains_token(headers["upgrade"], "websocket") ||
        !contains_token(headers["connection"], "upgrade") ||
        headers["sec-websocket-accept"] != ws_expected_accept(key) ||
        headers.count("sec-websocket-extensions") ||
        headers.count("sec-websocket-protocol")) {
        error = "Steam CM: respuesta WebSocket no autenticada";
        return false;
    }
    consumed = end + 4;
    error.clear();
    return true;
}

bool ws_encode_client_frame(uint8_t opcode, const std::vector<uint8_t>& payload,
                            const uint8_t mask[4], std::vector<uint8_t>& out,
                            std::string& error) {
    out.clear();
    if (!mask || !opcode_valid(opcode) ||
        payload.size() > kMaxWsPayload ||
        (opcode >= 8 && payload.size() > 125)) {
        error = "Steam CM: trama WebSocket saliente invalida";
        return false;
    }
    out.push_back(static_cast<uint8_t>(0x80 | opcode));
    if (payload.size() <= 125) out.push_back(static_cast<uint8_t>(0x80 | payload.size()));
    else if (payload.size() <= 65535) {
        out.push_back(0xfe);
        out.push_back(static_cast<uint8_t>(payload.size() >> 8));
        out.push_back(static_cast<uint8_t>(payload.size()));
    } else {
        out.push_back(0xff);
        for (int i = 7; i >= 0; --i)
            out.push_back(static_cast<uint8_t>(static_cast<uint64_t>(payload.size()) >> (i * 8)));
    }
    out.insert(out.end(), mask, mask + 4);
    for (size_t i = 0; i < payload.size(); ++i)
        out.push_back(payload[i] ^ mask[i & 3]);
    error.clear();
    return true;
}

WsParseResult ws_decode_server_frame(const uint8_t* bytes, size_t length,
                                      WsFrame& frame, size_t& consumed,
                                      std::string& error) {
    frame = {};
    consumed = 0;
    if (length && !bytes) { error = "Steam CM: trama nula"; return WsParseResult::Invalid; }
    if (length < 2) return WsParseResult::Incomplete;
    const uint8_t first = bytes[0], second = bytes[1];
    const uint8_t opcode = first & 15;
    if ((first & 0x70) || (second & 0x80) || !opcode_valid(opcode) ||
        (opcode >= 8 && !(first & 0x80))) {
        error = "Steam CM: trama servidor WebSocket invalida"; return WsParseResult::Invalid;
    }
    size_t header = 2;
    uint64_t size = second & 127;
    if (size == 126) {
        if (length < header + 2) return WsParseResult::Incomplete;
        size = uint64_t(bytes[2]) << 8 | bytes[3];
        header += 2;
        if (size < 126) { error = "Steam CM: longitud no canonica"; return WsParseResult::Invalid; }
    } else if (size == 127) {
        if (length < header + 8) return WsParseResult::Incomplete;
        size = 0;
        for (unsigned i = 0; i < 8; ++i) size = (size << 8) | bytes[2 + i];
        header += 8;
        if (size <= 65535) { error = "Steam CM: longitud no canonica"; return WsParseResult::Invalid; }
    }
    if (size > kMaxWsPayload || (opcode >= 8 && size > 125)) {
        error = "Steam CM: trama servidor demasiado grande"; return WsParseResult::Invalid;
    }
    if (size > length - std::min(length, header) || length < header)
        return WsParseResult::Incomplete;
    consumed = header + static_cast<size_t>(size);
    frame.opcode = opcode;
    frame.fin = (first & 0x80) != 0;
    frame.payload.assign(bytes + header, bytes + consumed);
    error.clear();
    return WsParseResult::Ready;
}

} // namespace isaacnx
