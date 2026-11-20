#include "isaacnx/cm_depot.hpp"
#include "isaacnx/cm_wire.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
void vint(std::vector<uint8_t>& out, uint64_t v) {
    while (v >= 128) { out.push_back(uint8_t(v) | 128); v >>= 7; }
    out.push_back(uint8_t(v));
}
void value(std::vector<uint8_t>& out, uint32_t id, uint64_t v) {
    vint(out, uint64_t(id) * 8);
    vint(out, v);
}
void fixed(std::vector<uint8_t>& out, uint32_t id, uint64_t v) {
    vint(out, uint64_t(id) * 8 + 1);
    for (unsigned i = 0; i < 8; ++i) out.push_back(uint8_t(v >> (8 * i)));
}
void bytes(std::vector<uint8_t>& out, uint32_t id, const std::vector<uint8_t>& blob) {
    vint(out, uint64_t(id) * 8 + 2);
    vint(out, blob.size());
    out.insert(out.end(), blob.begin(), blob.end());
}
std::vector<uint8_t> response(uint64_t job, uint32_t eresult, bool with_key) {
    std::vector<uint8_t> header, body, packet;
    fixed(header, 11, job);
    value(body, 1, eresult);
    value(body, 2, isaacnx::kIsaacWorkshopDepot);
    if (with_key) bytes(body, 3, std::vector<uint8_t>(32, 0x5a));
    std::string error;
    assert(isaacnx::encode_cm_proto_frame(
        isaacnx::kCmGetDepotDecryptionKeyResponse, header, body, packet, error));
    return packet;
}
} // namespace

int main() {
    constexpr uint64_t job = 2034025;
    std::string error;
    isaacnx::CmLogonResult login;
    login.result = 1;
    login.steamid = 0x110000100abcdd;
    login.client_sessionid = 4200;
    std::vector<uint8_t> packet;
    assert(isaacnx::cm_build_depot_key_request(
        login, 250900, isaacnx::kIsaacWorkshopDepot, job, packet, error));
    isaacnx::CmProtoFrame parsed;
    assert(isaacnx::decode_cm_proto_frame(packet.data(), packet.size(), parsed, error));
    assert(parsed.emsg == isaacnx::kCmGetDepotDecryptionKey);
    assert(parsed.header.size() == 21);
    assert(parsed.body == (std::vector<uint8_t> {0x08, 0x94, 0xa8, 0x0f,
                                                0x10, 0x94, 0xa8, 0x0f}));
    auto good = response(job, 1, true);
    isaacnx::CmDepotKeyResult result;
    assert(isaacnx::cm_parse_depot_key_response(
        good.data(), good.size(), job, 250900, result, error));
    assert(result.has_key && result.eresult == 1 &&
           result.key[0] == 0x5a && result.key[31] == 0x5a);
    auto denied = response(job, 15, false);
    assert(isaacnx::cm_parse_depot_key_response(
        denied.data(), denied.size(), job, 250900, result, error));
    assert(!result.has_key && result.eresult == 15 && result.key[0] == 0);
    assert(!isaacnx::cm_parse_depot_key_response(
        good.data(), good.size(), job + 1, 250900, result, error));
    assert(!isaacnx::cm_parse_depot_key_response(
        good.data(), good.size(), job, 123, result, error));
    auto missing = response(job, 1, false);
    assert(!isaacnx::cm_parse_depot_key_response(
        missing.data(), missing.size(), job, 250900, result, error));
    auto spoofed = response(job, 15, true);
    assert(!isaacnx::cm_parse_depot_key_response(
        spoofed.data(), spoofed.size(), job, 250900, result, error));
    std::mt19937 rng(765234);
    for (unsigned test = 0; test < 20000; ++test) {
        std::vector<uint8_t> fuzz(rng() % 256);
        for (uint8_t& b : fuzz) b = uint8_t(rng());
        (void)isaacnx::cm_parse_depot_key_response(
            fuzz.data(), fuzz.size(), job, 250900, result, error);
    }
    std::cout << "Depot request/key response, job validation, 20k fuzz: OK\n";
}
