# Magai Agent Instructions

This repository is the Magai-maintained fork of `78/xiaozhi-esp32`. Preserve upstream attribution and keep Magai-specific changes compatible with both `magai-wifi` and `magai-ml307` unless a task explicitly targets one board.

Use [docs/agent-development-workflow.md](docs/agent-development-workflow.md) for agent roles, task flow, review, and validation. Keep multi-step plans under `docs/plans/`.

## Engineering Constraints

- Inspect `git status`, the current branch, and relevant app/board docs before editing. Preserve existing user changes and avoid overlapping file ownership when delegating work.
- Keep board-specific behavior behind `Board`, `NetworkInterface`, and audio codec interfaces. Do not include Wi-Fi- or ML307-only APIs in shared app code.
- Route HTTP through `Board::GetInstance().GetNetwork()->CreateHttp()` and parse JSON with cJSON.
- Add new source files to the explicit source lists in `main/CMakeLists.txt`.
- Protect LVGL access with the display lock except in lifecycle callbacks that the app manager explicitly invokes while locked. Keep blocking network, decode, and storage work off the LVGL/main loop.
- App `Exit()` must stop timers/animations and release callbacks and UI-owned resources before the overlay is destroyed.
- Keep the current firmware baseline on ESP-IDF 5.4.x until an upstream migration is separately planned and both Magai board variants build.
- Do not hard-code personal SDK paths, credentials, or local machine configuration into tracked files. Personal `.zcode/` files are ignored; repository-wide agent instructions belong in `AGENTS.md`.

## Delivery

For code changes, review the complete diff, run `git diff --check`, and build both board variants when the change affects shared firmware. Flashing and serial validation should use the requested board and port. Report changed behavior, checks run, and remaining limitations. Commit or push only when the user asks for repository publication.
