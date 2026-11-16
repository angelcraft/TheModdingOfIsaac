# Isaac Workshop NX - continue here (2026-11-16)

Branch: feature/isaac-workshop-nx-20261116 in private angelcraft/TheModdingOfIsaac.
Version in progress: 0.5 diagnostic-only; not a native SteamPipe downloader yet.

## Verified before this commit
- Android DepotDownloader downloaded Workshop item 836319872 and produced an 8.3 MB ZIP, but bridge HTTP to Switch returned 0.
- Switch QR Steam Guard approval was observed on actual console, but OAuth tokens were intentionally not stored or used as Steam CM auth.
- Switch SDK libcurl reports HEADER_WS_API=no, LINK_WS_RECV=no, LINK_WS_SEND=no (successful GitHub Actions run #43961235182).
- Portable Steam CM protobuf envelope parser and host tests built on Switch ARM64.

## Added in 0.5 (verify CI before user testing)
1. Strict CM directory JSON parser; host+port and Valve DNS domains checked, eight WSS URLs max.
2. Pure C++ RFC 6455 client masking, server frame parser, SHA-1 Sec-WebSocket-Accept validation; RFC-known vectors and 20,000 malformed frames in Ubuntu tests.
3. Native TLS connection with CURLOPT_CONNECT_ONLY=1L, curl_easy_send/recv, OS-seeded 16-byte key, HTTP/1.1 Upgrade to /cmsocket/; no credentials sent or stored. Y key for bounded connectivity probe.
4. Source-only build; excludes proprietary Steam game data. All artifacts remain NON-NATIVE-DOWNLOAD.

## Next work
- Validate GitHub Actions host-tests and devkitPro build; resolve compile errors.
- Run diagnostic NRO on Switch with [Y] Probar Steam CM WSS; obtain exact screen text without credentials. HTTP 101 only proves transport.
- Implement CM session with correctly scoped client access token (QR poll API currently deliberately discards tokens; do not log or persist them).
- Add CM logon/heartbeat/job routing, Workshop depot key and SteamPipe manifest, authenticated Steam CDN, validated/decrypted/decompressed content.
- Stage Workshop files with bounded filesystem checks; test actual Switch before declaring native downloads.
- Preserve manually imported ZIP and do not modify Atmosphere or game binaries.

## Caution
A WSS handshake has not yet been tested on console. Connection success is not download success. Never print OAuth tokens, bearer headers, Steam cookies, manifest keys, or QR request IDs.
