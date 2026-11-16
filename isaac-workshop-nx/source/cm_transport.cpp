#include "isaacnx/cm_transport.hpp"
#include "isaacnx/ws_wire.hpp"

#include <switch.h>
#include <curl/curl.h>
#include <sys/select.h>

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cctype>
#include <string>

namespace isaacnx {
namespace {

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

bool trusted_cm_endpoint(const std::string& wss, std::string& hostport) {
    constexpr char prefix[] = "wss://";
    constexpr char path[] = "/cmsocket/";
    if (wss.compare(0, sizeof(prefix) - 1, prefix) != 0 ||
        wss.size() < sizeof(prefix) + sizeof(path)) return false;
    if (wss.compare(wss.size() - sizeof(path) + 1, sizeof(path) - 1, path) != 0)
        return false;
    hostport = wss.substr(sizeof(prefix) - 1,
                          wss.size() - (sizeof(prefix) - 1) - (sizeof(path) - 1));
    if (hostport.size() > 127) return false;
    const size_t colon = hostport.rfind(':');
    if (colon == std::string::npos || colon == 0 ||
        colon == hostport.size() - 1) return false;
    const std::string host = hostport.substr(0, colon);
    const std::string port = hostport.substr(colon + 1);
    if (port.size() > 5 || !std::all_of(port.begin(), port.end(),
        [](char c) { return c >= '0' && c <= '9'; })) return false;
    const int num_port = std::atoi(port.c_str());
    if (num_port < 1 || num_port > 65535) return false;
    const bool trusted =
        (host.size() > 16 && host.compare(host.size() - 16, 16, ".steamserver.net") == 0) ||
        (host.size() > 17 && host.compare(host.size() - 17, 17, ".steampowered.com") == 0);
    if (!trusted || host.front() == '.' || host.back() == '.' ||
        host.find("..") != std::string::npos) return false;
    return std::all_of(host.begin(), host.end(), [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
               ch == '-' || ch == '.';
    });
}

// curl_easy_send/recv operate on the existing, certificate-verified TLS
// session; never use send/recv directly on its raw socket.
bool wait_socket(CURL* curl, bool write, Deadline deadline) {
    curl_socket_t fd = CURL_SOCKET_BAD;
    if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &fd) != CURLE_OK ||
        fd == CURL_SOCKET_BAD) return false;
    while (Clock::now() < deadline) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv {0, 200000};
        const int ready = select(static_cast<int>(fd) + 1,
                                 write ? nullptr : &fds,
                                 write ? &fds : nullptr, nullptr, &tv);
        if (ready > 0) return true;
        if (ready < 0) return false;
    }
    return false;
}

bool send_all(CURL* curl, const std::string& request, Deadline deadline) {
    size_t offset = 0;
    while (offset < request.size() && Clock::now() < deadline) {
        size_t written = 0;
        const CURLcode rc = curl_easy_send(curl, request.data() + offset,
                                           request.size() - offset, &written);
        if (rc == CURLE_AGAIN) {
            if (!wait_socket(curl, true, deadline)) return false;
            continue;
        }
        if (rc != CURLE_OK || written == 0) return false;
        offset += written;
    }
    return offset == request.size();
}

bool receive_upgrade(CURL* curl, const std::string& key,
                     Deadline deadline, std::string& error) {
    std::string received;
    while (Clock::now() < deadline) {
        if (received.size() >= kMaxWsHttpHeader) {
            error = "Steam CM: encabezados HTTPS demasiado grandes"; return false;
        }
        char buf[2048];
        size_t count = 0;
        const CURLcode rc = curl_easy_recv(curl, buf,
                std::min(sizeof(buf), kMaxWsHttpHeader - received.size()), &count);
        if (rc == CURLE_AGAIN) {
            if (!wait_socket(curl, false, deadline)) break;
            continue;
        }
        if (rc != CURLE_OK || count == 0) {
            error = "Steam CM: fallo al leer respuesta TLS"; return false;
        }
        received.append(buf, count);
        if (received.find("\r\n\r\n") != std::string::npos) {
            size_t consumed = 0;
            return ws_validate_upgrade(received, key, consumed, error);
        }
    }
    error = "Steam CM: tiempo agotado esperando HTTP 101";
    return false;
}

} // namespace

bool probe_steam_cm_ws(const std::string& wss_url, std::string& error) {
    std::string hostport;
    if (!trusted_cm_endpoint(wss_url, hostport)) {
        error = "Steam CM: destino WSS fuera de Valve"; return false;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        error = "Steam CM: no pude iniciar TLS"; return false;
    }
    // CONNECT_ONLY=1 works with old Switch libcurl builds without curl_ws_*.
    const std::string https = "https://" + hostport + "/cmsocket/";
    curl_easy_setopt(curl, CURLOPT_URL, https.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 9L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        error = "Steam CM: fallo TLS (curl " + std::to_string(static_cast<int>(rc)) + ")";
        curl_easy_cleanup(curl);
        return false;
    }

    uint8_t nonce[16] {};
    randomGet(nonce, sizeof nonce);
    const std::string key = ws_client_key(nonce);
    const std::string request =
        "GET /cmsocket/ HTTP/1.1\r\n"
        "Host: " + hostport + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: " + key + "\r\n"
        "User-Agent: IsaacWorkshopNX/0.5\r\n\r\n";

    const Deadline deadline = Clock::now() + std::chrono::seconds(12);
    const bool sent = send_all(curl, request, deadline);
    bool ok = false;
    if (!sent) error = "Steam CM: fallo al enviar Upgrade HTTPS";
    else ok = receive_upgrade(curl, key, deadline, error);
    curl_easy_cleanup(curl);
    if (ok) error.clear();
    return ok;
}

} // namespace isaacnx
