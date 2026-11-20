#include "isaacnx/cm_session_proto.hpp"
#include "isaacnx/cm_wire.hpp"

#include <cstdint>
#include <algorithm>
#include <vector>

namespace isaacnx {
namespace {

void write_varint(std::vector<uint8_t>& bytes, uint64_t value) {
    while (value >= 128) {
        bytes.push_back(static_cast<uint8_t>(value) | 0x80);
        value >>= 7;
    }
    bytes.push_back(static_cast<uint8_t>(value));
}

void write_uint(std::vector<uint8_t>& bytes, uint32_t field, uint64_t value) {
    write_varint(bytes, uint64_t(field) * 8);
    write_varint(bytes, value);
}

void write_fixed64(std::vector<uint8_t>& bytes, uint32_t field, uint64_t value) {
    write_varint(bytes, uint64_t(field) * 8 + 1);
    for (unsigned i = 0; i < 8; ++i)
        bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
}

void write_text(std::vector<uint8_t>& bytes, uint32_t field, const char* text) {
    std::string s(text);
    write_varint(bytes, uint64_t(field) * 8 + 2);
    write_varint(bytes, s.size());
    bytes.insert(bytes.end(), s.begin(), s.end());
}

struct Field {
    uint32_t number = 0;
    uint32_t wire = 0;
    uint64_t value = 0;
};

bool read_varint(const std::vector<uint8_t>& bytes, size_t& at, uint64_t& value) {
    value = 0;
    for (unsigned i = 0; i < 10; ++i) {
        if (at >= bytes.size()) return false;
        const uint8_t b = bytes[at++];
        if (i == 9 && b > 1) return false;
        value |= uint64_t(b & 127) << (7 * i);
        if (!(b & 0x80)) return true;
    }
    return false;
}

bool next_field(const std::vector<uint8_t>& bytes, size_t& at, Field& f) {
    uint64_t key = 0;
    if (!read_varint(bytes, at, key)) return false;
    if (key < 8 || key / 8 > 536870911) return false;
    f.number = static_cast<uint32_t>(key / 8);
    f.wire = static_cast<uint32_t>(key & 7);
    f.value = 0;
    switch (f.wire) {
        case 0: return read_varint(bytes, at, f.value);
        case 1:
            if (bytes.size() - at < 8) return false;
            for (unsigned i = 0; i < 8; ++i)
                f.value |= uint64_t(bytes[at++]) << (8 * i);
            return true;
        case 2: {
            uint64_t length = 0;
            if (!read_varint(bytes, at, length) || length > bytes.size() - at)
                return false;
            at += static_cast<size_t>(length);
            return true;
        }
        case 5:
            if (bytes.size() - at < 4) return false;
            at += 4;
            return true;
        default: return false;
    }
}
} // namespace

bool cm_build_hello(std::vector<uint8_t>& out, std::string& error) {
    std::vector<uint8_t> body;
    write_uint(body, 1, kSteamProtocolVersion);
    return encode_cm_proto_frame(kCmClientHello, {}, body, out, error);
}

bool cm_build_anonymous_logon(std::vector<uint8_t>& out, std::string& error) {
    std::vector<uint8_t> header;
    // Public universe = 1, EAccountType.AnonUser = 10, account id = 0.
    constexpr uint64_t kAnonymousSteamId = (uint64_t(1) << 56) | (uint64_t(10) << 52);
    write_fixed64(header, 1, kAnonymousSteamId);
    write_uint(header, 2, 0); // client_sessionid
    std::vector<uint8_t> body;
    write_uint(body, 1, kSteamProtocolVersion);
    write_uint(body, 3, 0); // cell_id; server can assign a cell
    write_text(body, 6, "english");
    write_uint(body, 7, 0); // EOSType.Unknown; no spoofed PC identity
    return encode_cm_proto_frame(kCmClientLogon, header, body, out, error);
}

bool cm_build_token_logon(const std::string& account_name,
                          const std::string& access_token,
                          std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    const auto account_char = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
    };
    const auto token_char = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
    };
    if (account_name.empty() || account_name.size() > 64 ||
        !std::all_of(account_name.begin(), account_name.end(), account_char) ||
        access_token.size() < 50 || access_token.size() > 4096 ||
        !std::all_of(access_token.begin(), access_token.end(), token_char)) {
        error = "Steam CM: cuenta o token QR invalidos";
        return false;
    }
    std::vector<uint8_t> header;
    // Public individual SteamID with zero account ID and desktop instance 1.
    // SteamKit uses this initial header and resolves the account from token.
    constexpr uint64_t initial_steamid =
        (uint64_t(1) << 56) | (uint64_t(1) << 52) | (uint64_t(1) << 32);
    write_fixed64(header, 1, initial_steamid);
    write_uint(header, 2, 0);
    std::vector<uint8_t> body;
    write_uint(body, 1, kSteamProtocolVersion);
    write_uint(body, 3, 0);
    write_text(body, 6, "english");
    write_uint(body, 7, 0);
    write_uint(body, 8, 0); // never ask Steam to save session credentials
    // Account name tag 50; Steam access token tag 108.
    write_varint(body, 50 * 8 + 2);
    write_varint(body, account_name.size());
    body.insert(body.end(), account_name.begin(), account_name.end());
    write_varint(body, 108 * 8 + 2);
    write_varint(body, access_token.size());
    body.insert(body.end(), access_token.begin(), access_token.end());
    const bool ok = encode_cm_proto_frame(kCmClientLogon, header, body, out, error);
    volatile uint8_t* clear = body.data();
    for (size_t i = 0; i < body.size(); ++i) clear[i] = 0;
    return ok;
}

bool cm_build_heartbeat(const CmLogonResult& session,
                        std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    if (session.result != 1 || session.steamid == 0 ||
        session.client_sessionid <= 0) {
        error = "Steam CM: sesion invalida para heartbeat";
        return false;
    }
    std::vector<uint8_t> header;
    write_fixed64(header, 1, session.steamid);
    write_uint(header, 2, static_cast<uint32_t>(session.client_sessionid));
    std::vector<uint8_t> body;
    write_uint(body, 1, 0); // do not request a heartbeat reply
    return encode_cm_proto_frame(kCmClientHeartbeat, header, body, out, error);
}

bool cm_parse_logon_response(const uint8_t* bytes, size_t count,
                             CmLogonResult& result, std::string& error) {
    result = {};
    CmProtoFrame cm;
    if (!decode_cm_proto_frame(bytes, count, cm, error)) return false;
    if (cm.emsg != kCmClientLogonResponse) {
        error = "Steam CM: no es respuesta de logon";
        return false;
    }
    bool found_result = false;
    for (size_t at = 0; at < cm.body.size();) {
        Field f;
        if (!next_field(cm.body, at, f)) {
            error = "Steam CM: protobuf de logon mal formado"; return false;
        }
        if (f.number == 1) {
            if (f.wire != 0 || f.value > 1000 || found_result) {
                error = "Steam CM: EResult de logon invalido"; return false;
            }
            found_result = true;
            result.result = static_cast<uint32_t>(f.value);
        } else if (f.number == 3) {
            if (f.wire != 0 || f.value > 3600) {
                error = "Steam CM: heartbeat invalido"; return false;
            }
            result.heartbeat_seconds = static_cast<uint32_t>(f.value);
        }
    }
    if (!found_result) {
        error = "Steam CM: falta EResult de logon"; return false;
    }
    for (size_t at = 0; at < cm.header.size();) {
        Field f;
        if (!next_field(cm.header, at, f)) {
            error = "Steam CM: cabecera de logon mal formada"; return false;
        }
        if (f.number == 1) {
            if (f.wire != 1) { error = "Steam CM: SteamID invalido"; return false; }
            result.steamid = f.value;
        } else if (f.number == 2) {
            if (f.wire != 0 || f.value > INT32_MAX) {
                error = "Steam CM: sessionid invalido"; return false;
            }
            result.client_sessionid = static_cast<int32_t>(f.value);
        }
    }
    error.clear();
    return true;
}
} // namespace isaacnx
