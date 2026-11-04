# Progress

## 2026-11-05

- Confirmed fork parent and fetched upstream main over SSH after HTTPS fetch failed.
- Compared the upstream release and tested a full merge on a disposable branch; aborted after seven core-file conflicts.
- Tested direct cherry-pick of the upstream MQTT fix; it conflicted due to crypto/API drift. Restored the trial and began a local mbedTLS-compatible port.
- Preserved the existing `.zcode` untracking and ignore changes found in the workspace.
- Ported the UDP lifetime/lock change for the local mbedTLS code and built Wi-Fi successfully.
- The first ML307 build exposed a missing test-config font option; rebuilt using the Wi-Fi sdkconfig with only the board selection changed. ML307 build passed and the temporary sdkconfig was removed.
- Updated the Chinese, English, and Japanese READMEs, added root agent instructions, and wrote the workflow and next-work plan.
