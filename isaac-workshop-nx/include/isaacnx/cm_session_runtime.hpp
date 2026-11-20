#pragma once
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
}
