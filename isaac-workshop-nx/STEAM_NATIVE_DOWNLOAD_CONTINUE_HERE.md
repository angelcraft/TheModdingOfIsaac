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

## 2026-11-20 - 0.6 anonymous CM login probe

Hardware report from Phylos: 0.5 connects and Steam accepts WebSocket.
New branch-only, reversible 0.6 work:
- Portable CMsgClientHello (EMsg 9805) and anonymous CMsgClientLogon (EMsg 5514) encoding.
- Strict CMsgClientLogonResponse (751) parser, including heartbeat metadata.
- RFC 6455 binary send/receive on the existing TLS socket, masking outbound frames and answering ping with pong; timeouts, fragmentation and size budgets.
- Steam CM EMsg::Multi unpacking (gzip or raw) with bounds for recursion, output and frame count.
- New Y diagnostic performs Hello, anonymous logon and reads Steam EResult, without touching files or requesting account credentials.
- 40,000 malformed-frame fuzz cases, deterministic tests and local ASan/UBSan passed; GitHub Actions ARM64 still needs to be verified for this commit.

No authenticated Steam CM session, depot key, manifest request code, CDN chunks, installation or direct Workshop download yet. Anonymous access may be insufficient for AppID 250900; Android anonymous DepotDownloader returned `App 250900 ... not available`. Do not claim mod downloading works or ask for Steam password. Next: actual user QR token handling in volatile memory, authorized CM logon, depot manifest + CDN client with AES/zlib and verified filesystem staging. Keep previous 0.5 NRO rollback artifact available.

## 2026-11-20 - 0.7 Steam Guard QR account logon (source candidate, CI pending)

- QR poll now retains only the verified client access_token in transient memory, validates its character set and erases it on destruction. Refresh token is not retained.
- Steam CM token login protobuf uses account_name field 50 and access_token field 108 with a public individual SteamID; no Steam password or persistent credentials.
- [Minus] QR path automatically attempts an authenticated CM login after Guard approval. [Y] continues independent anonymous login diagnostic. Both close the TLS socket after the probe.
- Outgoing masked WebSocket frames and intermediate protobuf vectors are wiped when sent; host-only malformed-input tests expanded.
- Still no Workshop manifest request, depot key, CDN download, nor installation. Avoid claiming downloads are usable.
- Actions 0.6 workflow failed to auto-trigger on 2026-11-20 despite successful GitHub ref update; request manual workflow_dispatch if this persists. No NRO handoff until ARM64 link succeeds.
