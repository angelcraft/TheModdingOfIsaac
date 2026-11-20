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


## 2026-11-20 - 0.8 Isaac Workshop depot key access (source, no downloader)

- SteamDB confirms app 250900 has workshopdepot=250900; SteamTracking
  `CMsgClientGetDepotDecryptionKey` request EMsg 5438, response EMsg 5439.
- New independent `cm_depot` module builds request with session SteamID,
  client session ID and fixed64 source job ID. Strict response parser checks
  corresponding target job, depot, EResult and AES key length=32. All
  intermediate key copies and the owned key are explicitly cleared from RAM.
- After QR approval, one CM WebSocket connection now handles
  Hello -> token logon -> depot key request and reports Steam EResult.
  This is not an installer, does not persist tokens/keys and does not change SD.
- Tests pass locally: 46 Python tests, previous protobuf/WS fuzz tests,
  20k extra depot malformed-response fuzz cases and ASan+UBSan.
  The modified runtime compiles with host g++ -Werror. ARM64 devkitPro build
  is unverified because a native cross-toolchain is unavailable here.
- GitHub Actions v0.6 and v0.7 each failed at 'host-tests' after about 4 sec,
  while both pass locally with the exact remote source. Job logs provider
  returns BlobNotFound; investigate actual workflow failure before shipping.
- Next: code for `ContentServerDirectory.GetManifestRequestCode#1` using
  Workshop item hcontent_file; request code+CDN servers; download manifest,
  validate and decrypt filenames with the depot key, then fetch, decrypt,
  decompress and checksum chunks. Only after integrity checks install via
  existing bounded staging, preserving previous mods. Do not silently
  fallback to unauthorized external mod mirrors.


## 2026-11-20 - 0.9 Manifest code via authenticated Steam CM (source)

- Added `cm_manifest_code.hpp/.cpp` Steam unified call
  `ContentServerDirectory.GetManifestRequestCode#1` (EMsg 151) and
  reply (147), with fixed64 job IDs, EResult in header tag 13,
  and temporal request code in reply body tag 1. App/depot=250900,
  manifest ID from `hcontent_file`.
- QR from a Workshop item's detail screen now tests logon,
  depot key access, and manifest authorization in a single
  authenticated WSS session. Standalone [-] on home remains depot probe.
- Never display or persist access tokens, depot keys, or manifest codes.
  No file downloaded/installed via this path yet.
- Offline tests: 46 Python, 20,000 malformed WebSocket frames,
  20,000 malformed CM login messages, 20,000 CM Multi,
  20,000 depot key messages, 20,000 manifest code messages.
  Manifest and depot parser ASan+UBSan verified; session runtime
  compiles locally with `-Werror`.
- Actions has failed to start a usable host-tests job for v0.6/0.7/0.8
  (4-second failures); GitHub logs API returns BlobNotFound.
  Private repo browser cannot show job without signed-in session.
  Ask Phylos for a screenshot of GitHub Actions failure details;
  do not infer the fault from test source because exact source
  `make host-test` passes locally.
- Next technical milestone: `GetServersForSteamPipe`, HTTP manifest
  CDN fetch using authorized code, decoding signed SteamPipe manifest
  and encrypted filenames, content chunk download and integrity tests,
  then atomic staging into Isaac mod path. Do not claim download works.


## 2026-11-20 — 0.10 CDN manifest fetch (source milestone)

- SteamPipe directory GET `IContentServerDirectoryService/GetServersForSteamPipe/v1/`;
  strict bounded JSON and allowlist of Steam-owned HTTPS hostnames (no redirect).
- Steam CM authenticated session now passes short-lived manifest request code
  and depot key to an RAII in-memory ticket; both wiped on scope exit. Never
  printed, logged or stored. Previous auth-only functions remain callable.
- Uses `/depot/250900/manifest/<gid>/5/<code>` on at most four CDN servers.
  Response is bounded to 8 MiB. Raw protobuf or ZIP-wrapped manifest is
  structurally parsed, with 24 MiB expansion cap, CRC32, expected depot and
  manifest GID, section order and file mapping count. NO cryptographic
  signature validation or chunk downloading yet.
- Host: 47 Python tests, all prior CM/WS fuzz tests, new ZIP/protobuf tests
  with 20k malformed variants, ASan+UBSan (LSan disabled due runtime), and
  cm_session_runtime.cpp compiled with host g++ -Werror.
- ARM64 NRO CI pending for this commit. Hardware CDN connectivity **not yet
  observed**. Keep v0.9 NRO for rollback. No Steam game binaries or mods
  included or published.
- Next: prove on hardware that CDN returns a matching manifest; parse its
  FileMapping and chunks, decrypt filenames/chunks using volatile AES key,
  support SteamPipe compression, hashes and transactional install. Security:
  paths, file counts, sizes and collisions checked before SD writes.
