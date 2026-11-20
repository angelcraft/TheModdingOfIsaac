#include "isaacnx/cm_manifest_code.hpp"
#include "isaacnx/cm_wire.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
void v(std::vector<uint8_t>& out, uint64_t n) {
    while (n >= 128) { out.push_back(uint8_t(n) | 128); n >>= 7; }
    out.push_back(uint8_t(n));
}
void field(std::vector<uint8_t>& out, uint32_t tag, uint64_t n) {
    v(out, tag * 8);
    v(out, n);
}
void fixed(std::vector<uint8_t>& out, uint32_t tag, uint64_t n) {
    v(out, tag * 8 + 1);
    for (unsigned i = 0; i < 8; ++i) out.push_back(uint8_t(n >> (i * 8)));
}
std::vector<uint8_t> make(uint64_t job, uint32_t status, uint64_t code,
                          bool include_code = true) {
    std::vector<uint8_t> header, body, frame;
    fixed(header, 11, job);
    field(header, 13, status);
    if (include_code) field(body, 1, code);
    std::string error;
    assert(isaacnx::encode_cm_proto_frame(
        isaacnx::kCmServiceMethodResponse, header, body, frame, error));
    return frame;
}
} // namespace

int main() {
    std::string error;
    isaacnx::CmLogonResult login;
    login.result = 1;
    login.steamid = 76561198000000123ULL;
    login.client_sessionid = 1177;
    constexpr uint64_t manifest = 8322528;
    constexpr uint64_t job = 330012;
    std::vector<uint8_t> request;
    assert(isaacnx::cm_build_manifest_code_request(
        login, manifest, job, request, error));
    isaacnx::CmProtoFrame parsed;
    assert(isaacnx::decode_cm_proto_frame(
        request.data(), request.size(), parsed, error));
    assert(parsed.emsg == isaacnx::kCmServiceMethodCall);
    const std::string name = "ContentServerDirectory.GetManifestRequestCode#1";
    assert(std::search(parsed.header.begin(), parsed.header.end(),
                       name.begin(), name.end()) != parsed.header.end());
    assert(parsed.body.size() > 8);
    assert(!isaacnx::cm_build_manifest_code_request(
        login, 0, job, request, error));
    isaacnx::CmManifestCodeReply reply;
    const auto ok = make(job, 1, 22334455667788ULL);
    assert(isaacnx::cm_parse_manifest_code_reply(
        ok.data(), ok.size(), job, reply, error));
    assert(reply.eresult == 1 && reply.request_code == 22334455667788ULL);
    const auto denied = make(job, 15, 0, false);
    assert(isaacnx::cm_parse_manifest_code_reply(
        denied.data(), denied.size(), job, reply, error));
    assert(reply.eresult == 15 && reply.request_code == 0);
    assert(!isaacnx::cm_parse_manifest_code_reply(
        ok.data(), ok.size(), job + 1, reply, error));
    const auto missing = make(job, 1, 0, false);
    assert(!isaacnx::cm_parse_manifest_code_reply(
        missing.data(), missing.size(), job, reply, error));
    const auto zero = make(job, 1, 0);
    assert(!isaacnx::cm_parse_manifest_code_reply(
        zero.data(), zero.size(), job, reply, error));
    const auto fake = make(job, 15, 512);
    assert(!isaacnx::cm_parse_manifest_code_reply(
        fake.data(), fake.size(), job, reply, error));
    std::mt19937 rng(128712);
    for (unsigned i = 0; i < 20000; ++i) {
        std::vector<uint8_t> random(rng() % 256);
        for (auto& b : random) b = uint8_t(rng());
        (void)isaacnx::cm_parse_manifest_code_reply(
            random.data(), random.size(), job, reply, error);
    }
    std::cout << "SteamPipe manifest request code and 20000 fuzz: OK\n";
}
