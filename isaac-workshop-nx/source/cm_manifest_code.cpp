#include "isaacnx/cm_manifest_code.hpp"
#include "isaacnx/cm_wire.hpp"

#include <algorithm>
#include <cstring>

namespace isaacnx {
namespace {
void varint(std::vector<uint8_t>& out, uint64_t v) {
    while (v >= 128) { out.push_back(uint8_t(v) | 128); v >>= 7; }
    out.push_back(uint8_t(v));
}
void number(std::vector<uint8_t>& out, uint32_t tag, uint64_t v) {
    varint(out, uint64_t(tag) * 8);
    varint(out, v);
}
void fixed64(std::vector<uint8_t>& out, uint32_t tag, uint64_t v) {
    varint(out, uint64_t(tag) * 8 + 1);
    for (unsigned n = 0; n < 8; ++n) out.push_back(uint8_t(v >> (n * 8)));
}
void text(std::vector<uint8_t>& out, uint32_t tag, const char* data) {
    const size_t count = std::strlen(data);
    varint(out, uint64_t(tag) * 8 + 2);
    varint(out, count);
    out.insert(out.end(), data, data + count);
}
struct Field {
    uint32_t tag = 0;
    uint32_t wire = 0;
    uint64_t number = 0;
};
bool read_number(const uint8_t* data, size_t count, size_t& at, uint64_t& val) {
    val = 0;
    for (unsigned i = 0; i < 10; ++i) {
        if (at >= count) return false;
        const uint8_t b = data[at++];
        if (i == 9 && b > 1) return false;
        val |= uint64_t(b & 127) << (i * 7);
        if (!(b & 128)) return true;
    }
    return false;
}
bool next(const std::vector<uint8_t>& bytes, size_t& at, Field& field) {
    uint64_t key = 0;
    if (!read_number(bytes.data(), bytes.size(), at, key) ||
        key < 8 || key >> 3 > 536870911) return false;
    field = {};
    field.tag = uint32_t(key >> 3);
    field.wire = uint32_t(key & 7);
    switch (field.wire) {
    case 0: return read_number(bytes.data(), bytes.size(), at, field.number);
    case 1:
        if (bytes.size() - at < 8) return false;
        for (unsigned i = 0; i < 8; ++i)
            field.number |= uint64_t(bytes[at++]) << (i * 8);
        return true;
    case 2: {
        uint64_t len = 0;
        if (!read_number(bytes.data(), bytes.size(), at, len) ||
            len > bytes.size() - at) return false;
        at += static_cast<size_t>(len);
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

bool cm_build_manifest_code_request(const CmLogonResult& session,
                                    uint64_t manifest_id, uint64_t job_id,
                                    std::vector<uint8_t>& out,
                                    std::string& error) {
    out.clear();
    if (session.result != 1 || session.steamid == 0 ||
        session.client_sessionid <= 0 || !manifest_id ||
        !job_id || job_id == UINT64_MAX) {
        error = "Steam CM: manifest o sesion invalida";
        return false;
    }
    std::vector<uint8_t> header, body;
    fixed64(header, 1, session.steamid);
    number(header, 2, uint32_t(session.client_sessionid));
    fixed64(header, 10, job_id);
    text(header, 12, "ContentServerDirectory.GetManifestRequestCode#1");
    number(body, 1, 250900); // app_id
    number(body, 2, 250900); // depot_id (Isaac Workshop)
    number(body, 3, manifest_id);
    // Public branch omitted, as in SteamKit and ValvePython.
    return encode_cm_proto_frame(kCmServiceMethodCall, header, body, out, error);
}

bool cm_parse_manifest_code_reply(const uint8_t* bytes, size_t length,
                                  uint64_t expected_job_id,
                                  CmManifestCodeReply& reply,
                                  std::string& error) {
    reply = {};
    if (!expected_job_id || expected_job_id == UINT64_MAX) {
        error = "Steam CM: job manifest invalido";
        return false;
    }
    CmProtoFrame frame;
    if (!decode_cm_proto_frame(bytes, length, frame, error)) return false;
    if (frame.emsg != kCmServiceMethodResponse) {
        error = "Steam CM: respuesta manifest inesperada";
        return false;
    }
    bool found_job = false, found_result = false, found_code = false;
    for (size_t at = 0; at < frame.header.size();) {
        Field f;
        if (!next(frame.header, at, f)) {
            error = "Steam CM: cabecera manifest corrupta"; return false;
        }
        if (f.tag == 11) {
            if (found_job || f.wire != 1 || f.number != expected_job_id) {
                error = "Steam CM: job manifest no coincide"; return false;
            }
            found_job = true;
        } else if (f.tag == 13) {
            if (found_result || f.wire != 0 || f.number > 1000) {
                error = "Steam CM: EResult manifest invalido"; return false;
            }
            reply.eresult = uint32_t(f.number);
            found_result = true;
        }
    }
    for (size_t at = 0; at < frame.body.size();) {
        Field f;
        if (!next(frame.body, at, f)) {
            error = "Steam CM: respuesta manifest corrupta"; return false;
        }
        if (f.tag == 1) {
            if (found_code || f.wire != 0) {
                error = "Steam CM: codigo manifest invalido"; return false;
            }
            reply.request_code = f.number;
            found_code = true;
        }
    }
    if (!found_job || !found_result ||
        (reply.eresult == 1 && (!found_code || reply.request_code == 0)) ||
        (reply.eresult != 1 && reply.request_code != 0)) {
        reply = {};
        error = "Steam CM: respuesta manifest incompleta";
        return false;
    }
    error.clear();
    return true;
}
} // namespace isaacnx
