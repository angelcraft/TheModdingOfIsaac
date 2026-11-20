#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace isaacnx {
// Transient anonymous Steam CM logon test. The socket closes on return;
// no Steam account credentials, keys, mods, or SD files are touched.
bool cm_probe_anonymous_login(const std::string& wss_url, std::string& error);

// One-shot Steam Guard QR token login. Caller owns and wipes access_token.
// Uses the account only for a bounded CM logon test; no token persistence.
bool cm_probe_token_login(const std::string& wss_url,
                          const std::string& account_name,
                          const std::string& access_token,
                          std::string& error);

// Same ephemeral QR-token session, then request the Isaac Workshop
// depot decryption key. The key is wiped on return, never persisted.
// A successful request can return EResult != OK (e.g. missing entitlement).
bool cm_probe_token_depot_access(const std::string& wss_url,
                                 const std::string& account_name,
                                 const std::string& access_token,
                                 uint32_t& depot_eresult,
                                 std::string& error);

// Validates an individual Workshop manifest ID after depot authorization.
// The Steam manifest request code is verified, then immediately forgotten;
// the caller receives only EResult and never the short-lived code.
bool cm_probe_token_manifest_access(const std::string& wss_url,
                                    const std::string& account_name,
                                    const std::string& access_token,
                                    uint64_t manifest_id,
                                    uint32_t& depot_eresult,
                                    uint32_t& manifest_eresult,
                                    std::string& error);

// One-shot authorization ticket, valid only for an immediate CDN request.
// Secrets are never persisted and are cleared on scope exit. Not copyable.
struct CmManifestTicket {
    uint32_t depot_eresult = 0;
    uint32_t manifest_eresult = 0;
    uint64_t request_code = 0;
    bool has_depot_key = false;
    std::array<uint8_t, 32> depot_key {};
    CmManifestTicket() = default;
    ~CmManifestTicket();
    CmManifestTicket(const CmManifestTicket&) = delete;
    CmManifestTicket& operator=(const CmManifestTicket&) = delete;
};

bool cm_authorize_token_manifest(const std::string& wss_url,
                                 const std::string& account_name,
                                 const std::string& access_token,
                                 uint64_t manifest_id,
                                 CmManifestTicket& ticket,
                                 std::string& error);
}

