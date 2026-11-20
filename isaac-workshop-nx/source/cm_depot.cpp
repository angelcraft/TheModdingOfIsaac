#include "isaacnx/cm_depot.hpp"
#include "isaacnx/cm_wire.hpp"

#include <algorithm>

namespace isaacnx {
namespace {
void varint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 128) {
        out.push_back(static_cast<uint8_t>(value) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}
void unsigned_field(std::vector<uint8_t>& out, unsigned id, uint64_t value) {
    varint(out, uint64_t(id) << 3);
    varint(out, value);
}
void fixed64_field(std::vector<uint8_t>& out, unsigned id, uint64_t value) {
    varint(out, (uint64_t(id) << 3) | 1);
    for (unsigned i = 0; i < 8; ++i)
        out.push_back(static_cast<uint8_t>(value >> (i * 8)));
}
struct Field {
    uint32_t id = 0;
    uint32_t type = 0;
    uint64_t number = 0;
    const uint8_t* bytes = nullptr;
    size_t length = 0;
};
bool read_varint(const uint8_t* data, size_t length, size_t& at, uint64_t& value) {
    value = 0;
    for (unsigned i = 0; i < 10; ++i) {
        if (at >= length) return false;
        const uint8_t next = data[at++];
        if (i == 9 && next > 1) return false;
        value |= uint64_t(next & 127) << (7 * i);
        if (!(next & 128)) return true;
    }
    return false;
}
bool read_field(const std::vector<uint8_t>& data, size_t& at, Field& field) {
    uint64_t tag = 0;
    if (!read_varint(data.data(), data.size(), at, tag) ||
        tag < 8 || (tag >> 3) > 536870911) return false;
    field = {};
    field.id = uint32_t(tag >> 3);
    field.type = uint32_t(tag & 7);
    switch (field.type) {
    case 0: return read_varint(data.data(), data.size(), at, field.number);
    case 1:
        if (data.size() - at < 8) return false;
        for (unsigned i = 0; i < 8; ++i)
            field.number |= uint64_t(data[at++]) << (8 * i);
        return true;
    case 2: {
        uint64_t n = 0;
        if (!read_varint(data.data(), data.size(), at, n) || n > data.size() - at)
            return false;
        field.bytes = data.data() + at;
        field.length = static_cast<size_t>(n);
        at += field.length;
        return true;
    }
    case 5:
        if (data.size() - at < 4) return false;
        at += 4;
        return true;
    default: return false;
    }
}
} // namespace

CmDepotKeyResult::~CmDepotKeyResult() {
    volatile uint8_t* data = key.data();
    for (size_t i = 0; i < key.size(); ++i) data[i] = 0;
}

bool cm_build_depot_key_request(const CmLogonResult& session,
                                uint32_t app_id, uint32_t depot_id,
                                uint64_t source_job_id,
                                std::vector<uint8_t>& out,
                                std::string& error) {
    out.clear();
    if (session.result != 1 || session.steamid == 0 ||
        session.client_sessionid <= 0 || !app_id || !depot_id ||
        source_job_id == 0 || source_job_id == UINT64_MAX) {
        error = "Steam CM: sesion o depot invalido";
        return false;
    }
    std::vector<uint8_t> header, body;
    fixed64_field(header, 1, session.steamid);
    unsigned_field(header, 2, uint32_t(session.client_sessionid));
    fixed64_field(header, 10, source_job_id);
    unsigned_field(body, 1, depot_id);
    unsigned_field(body, 2, app_id);
    return encode_cm_proto_frame(kCmGetDepotDecryptionKey, header, body, out, error);
}

bool cm_parse_depot_key_response(const uint8_t* data, size_t length,
                                 uint64_t expected_job_id,
                                 uint32_t expected_depot,
                                 CmDepotKeyResult& result,
                                 std::string& error) {
    result.has_key = false;
    result.key.fill(0);
    result.depot_id = 0;
    result.eresult = 0;
    if (!expected_job_id || expected_job_id == UINT64_MAX || !expected_depot) {
        error = "Steam CM: respuesta de depot sin contexto";
        return false;
    }
    CmProtoFrame frame;
    if (!decode_cm_proto_frame(data, length, frame, error)) return false;
    struct WipeFrame {
        CmProtoFrame& frame;
        ~WipeFrame() {
            volatile uint8_t* h = frame.header.data();
            for (size_t i = 0; i < frame.header.size(); ++i) h[i] = 0;
            volatile uint8_t* b = frame.body.data();
            for (size_t i = 0; i < frame.body.size(); ++i) b[i] = 0;
        }
    } wipe_frame {frame};
    if (frame.emsg != kCmGetDepotDecryptionKeyResponse) {
        error = "Steam CM: tipo de respuesta de depot inesperado";
        return false;
    }
    bool found_job = false;
    for (size_t at = 0; at < frame.header.size();) {
        Field f;
        if (!read_field(frame.header, at, f)) {
            error = "Steam CM: cabecera depot corrupta"; return false;
        }
        if (f.id != 11) continue;
        if (f.type != 1 || f.number != expected_job_id || found_job) {
            error = "Steam CM: ID de trabajo depot no coincide"; return false;
        }
        found_job = true;
    }
    if (!found_job) { error = "Steam CM: respuesta depot sin job ID"; return false; }
    bool found_result = false, found_depot = false, found_key = false;
    std::array<uint8_t, 32> candidate {};
    struct WipeCandidate {
        std::array<uint8_t, 32>& key;
        ~WipeCandidate() {
            volatile uint8_t* data = key.data();
            for (size_t i = 0; i < key.size(); ++i) data[i] = 0;
        }
    } wipe_candidate {candidate};
    for (size_t at = 0; at < frame.body.size();) {
        Field f;
        if (!read_field(frame.body, at, f)) {
            error = "Steam CM: respuesta depot corrupta"; return false;
        }
        if (f.id == 1) {
            if (found_result || f.type != 0 || f.number > 1000) {
                error = "Steam CM: EResult depot invalido"; return false;
            }
            found_result = true;
            result.eresult = static_cast<uint32_t>(f.number);
        } else if (f.id == 2) {
            if (found_depot || f.type != 0 || f.number != expected_depot) {
                error = "Steam CM: depot equivocado"; return false;
            }
            found_depot = true;
            result.depot_id = static_cast<uint32_t>(f.number);
        } else if (f.id == 3) {
            if (found_key || f.type != 2 || f.length != result.key.size()) {
                error = "Steam CM: clave depot invalida"; return false;
            }
            found_key = true;
            std::copy(f.bytes, f.bytes + f.length, candidate.begin());
        }
    }
    if (!found_result || !found_depot ||
        (result.eresult == 1 && !found_key) ||
        (result.eresult != 1 && found_key)) {
        result.key.fill(0);
        error = "Steam CM: respuesta depot incompleta";
        return false;
    }
    result.has_key = found_key && result.eresult == 1;
    if (result.has_key)
        std::copy(candidate.begin(), candidate.end(), result.key.begin());
    error.clear();
    return true;
}
} // namespace isaacnx
