#pragma once
#include <string>

namespace isaacnx {

// Read-only diagnostic: opens a TLS socket to a validated Steam CM address,
// performs the RFC 6455 HTTP Upgrade, and closes it. No credentials, login,
// SteamPipe download, or filesystem changes are made.
bool probe_steam_cm_ws(const std::string& wss_url, std::string& error);

} // namespace isaacnx
