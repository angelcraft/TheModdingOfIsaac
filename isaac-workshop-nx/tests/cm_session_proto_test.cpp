#include "isaacnx/cm_session_proto.hpp"
#include "isaacnx/cm_wire.hpp"
#include <algorithm>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace isaacnx;

int main() {
    std::string error;
    std::vector<uint8_t> hello, logon, heartbeat;
    CmProtoFrame frame;

    assert(cm_build_hello(hello, error));
    assert(decode_cm_proto_frame(hello.data(), hello.size(), frame, error));
    assert(frame.emsg == 9805);
    assert(frame.header.empty());
    assert((frame.body == std::vector<uint8_t>{0x08, 0xac, 0x80, 0x04}));

    assert(cm_build_anonymous_logon(logon, error));
    assert(decode_cm_proto_frame(logon.data(), logon.size(), frame, error));
    assert(frame.emsg == 5514);
    // Anonymous account header: field 1 fixed64, field 2 zero session id.
    assert(frame.header.size() == 11 && frame.header[0] == 0x09);
    assert(frame.header[8] == 0x01 && frame.header[9] == 0x10);
    assert(frame.body[0] == 0x08);

    const std::string fake_token(75, 'z');
    std::vector<uint8_t> token_logon;
    assert(cm_build_token_logon("my_account_1", fake_token, token_logon, error));
    assert(decode_cm_proto_frame(token_logon.data(), token_logon.size(), frame, error));
    assert(frame.emsg == 5514);
    const std::vector<uint8_t> account{'m','y','_','a','c','c','o','u','n','t','_','1'};
    assert(std::search(frame.body.begin(), frame.body.end(),
                       account.begin(), account.end()) != frame.body.end());
    assert(!cm_build_token_logon("bad\r\nHTTP:", fake_token, token_logon, error));
    assert(token_logon.empty());
    assert(!cm_build_token_logon("account", "short", token_logon, error));
    assert(!cm_build_token_logon("account", std::string(70, '.') + "\n",
                                 token_logon, error));

    // Body EResult=OK, heartbeat=30 seconds; protobuf header has steamid
    // and sessionid. Invalid/wrong-type frames must fail closed.
    const std::vector<uint8_t> reply_header{
        0x09, 0, 0, 0, 0, 0, 0, 0xa0, 0x01, 0x10, 0x2a
    };
    const std::vector<uint8_t> reply_body{0x08, 0x01, 0x18, 0x1e};
    std::vector<uint8_t> reply;
    assert(encode_cm_proto_frame(751, reply_header, reply_body, reply, error));
    CmLogonResult session;
    assert(cm_parse_logon_response(reply.data(), reply.size(), session, error));
    assert(session.result == 1 && session.heartbeat_seconds == 30);
    assert(session.client_sessionid == 42);
    assert(session.steamid == 0x01a0000000000000ULL);
    assert(cm_build_heartbeat(session, heartbeat, error));
    assert(decode_cm_proto_frame(heartbeat.data(), heartbeat.size(), frame, error));
    assert(frame.emsg == 703);
    assert(!cm_parse_logon_response(hello.data(), hello.size(), session, error));

    const std::vector<uint8_t> fail_body{0x08, 0x0f};
    assert(encode_cm_proto_frame(751, {}, fail_body, reply, error));
    assert(cm_parse_logon_response(reply.data(), reply.size(), session, error));
    assert(session.result == 15);
    assert(!cm_build_heartbeat(session, heartbeat, error));

    const std::vector<uint8_t> invalid_body{0x08,0x01,0x08,0x01};
    assert(encode_cm_proto_frame(751, {}, invalid_body, reply, error));
    assert(!cm_parse_logon_response(reply.data(), reply.size(), session, error));

    // Unknown protobuf fields are skipped, including length-delimited, but
    // malformed varints and truncated buffers are not accepted.
    const std::vector<uint8_t> unknown_body{0x08,0x01,0x92,0x01,0x02,0x41,0x42};
    assert(encode_cm_proto_frame(751, {}, unknown_body, reply, error));
    assert(cm_parse_logon_response(reply.data(), reply.size(), session, error));
    const std::vector<uint8_t> bad_length{0x08,0x01,0x92,0x01,0x70,0x41};
    assert(encode_cm_proto_frame(751, {}, bad_length, reply, error));
    assert(!cm_parse_logon_response(reply.data(), reply.size(), session, error));

    std::mt19937 rng(0x53454131u);
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> malformed(rng() % 150);
        for (auto& byte : malformed) byte = static_cast<uint8_t>(rng());
        // Ignore the return value; all paths must stay bounded and safe.
        (void)cm_parse_logon_response(malformed.data(), malformed.size(), session, error);
    }
    std::cout << "Steam CM hello/logon/heartbeat + 20000 malformed frames OK\n";
}
