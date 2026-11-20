#pragma once
#include <string>

namespace isaacnx {
// Transient anonymous Steam CM logon test. The socket closes on return;
// no Steam account credentials, keys, mods, or SD files are touched.
bool cm_probe_anonymous_login(const std::string& wss_url, std::string& error);
}
