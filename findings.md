# Findings

- GitHub API identifies `DustReliant/xiaozhi-esp32-magai` as a fork of `78/xiaozhi-esp32`.
- Fork baseline: commit `1f602fa` / project version 2.0.4. Local Magai work is based on ESP-IDF 5.4.1.
- Fetched upstream tip: `a73e9a7`, project version 2.5.3, ESP-IDF 6.1. It is 485 commits ahead of the fork base and changes about 1,200 files.
- A trial merge conflicts in `main/CMakeLists.txt`, `main/Kconfig.projbuild`, `main/application.cc`, `main/audio/audio_service.cc`, `main/audio/audio_service.h`, `main/idf_component.yml`, and `main/mcp_server.cc`.
- Upstream commit `f5b495d` documents a lock inversion during blocking ML307 UDP sends. Its exact patch conflicts because upstream also migrated crypto APIs; the channel-lock and shared UDP lifetime fix is being adapted to the local mbedTLS version.
- `.zcode/agents` was tracked in the previous commit. The current staged deletions and `.gitignore` exclusion are preserved; the local folder remains available but ignored.
- Upstream commit `f5b495d` was adapted locally: `SendAudio()` now retains a shared UDP reference, keeps packet creation/encryption protected, and releases `channel_mutex_` before the potentially blocking `Udp::Send()` call.
- Wi-Fi build passes with IDF 5.4.1; image size is `0x388ef0` bytes with about 10% app-partition headroom.
- ML307 build passes with the same feature configuration as Wi-Fi and only the board option switched; image size is `0x2ee6f0` bytes with about 26% app-partition headroom.
- The first ML307 attempt used defaults that disabled Montserrat 48 and failed in the Bilibili app; reusing the Wi-Fi sdkconfig settings resolved the configuration-only failure. The temporary sdkconfig was removed afterward.
