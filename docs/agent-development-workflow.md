# Magai Agent Development Workflow

## Roles

- **Lead agent**: clarifies behavior from the request, records architecture decisions, assigns file ownership, integrates changes, and owns the final result.
- **Feature agent**: implements one bounded feature or module and reports changed files, behavior, tests, and limitations.
- **Review agent**: independently checks the diff for regressions, lifecycle errors, concurrency issues, and missing validation. It reports findings before summaries.
- **Board test agent**: runs static checks and firmware builds for Magai Wi-Fi and ML307, then performs device and serial-log checks when requested.
- **UI agent**: handles 240x280 LVGL layouts using `docs/ui/DESIGN.md` and the existing interaction model.

One agent owns each shared file at a time. Parallel work is limited to modules with clear boundaries; the lead reviews every integrated change.

## Branch Model

- `main` mirrors upstream `78/xiaozhi-esp32/main`; keep Magai feature commits off this branch.
- `master` holds the stable Magai baseline and release candidates.
- `dev` is the Magai integration branch. Start feature branches from `dev`, then merge reviewed work back to `dev`.
- Promote `dev` to `master` only after the relevant acceptance checklist passes and both `magai-wifi` and `magai-ml307` builds succeed.
- Sync upstream into `main` first. Selectively port changes to `dev`; a major version or ESP-IDF migration needs its own plan and validation before promotion.

## Task Flow

1. **Inspect**: read `git status`, branch/remotes, the relevant source, and its acceptance checklist. Treat existing local edits as user work.
2. **Plan**: for multi-module or board-sensitive work, write scope, dependencies, acceptance criteria, and rollback considerations in `docs/plans/`.
3. **Assign**: give each contributor a specific module, expected interface, and verification task. Keep changes to `main/CMakeLists.txt`, `main/application.cc`, shared audio code, and board registries under one owner.
4. **Implement**: use existing ESP-IDF, LVGL, network, settings, and App lifecycle APIs. Do not add dependencies without documenting their flash/RAM cost and compatibility.
5. **Review**: inspect the complete diff and check ownership, synchronization, cleanup, error paths, and whether callbacks can outlive their UI or task.
6. **Validate**: run `git diff --check`, relevant static/tests, and builds for both board variants. For ML307, use an isolated sdkconfig derived from the active Magai Wi-Fi config and change only the board selection so feature options stay aligned; keep the temporary config local and remove it after the build. Use `docs/magai-apps-acceptance.md` for device-level behavior. Flash and serial-test only the selected target.
7. **Deliver**: report behavior changes, exact validation results, known limitations, and commit/push status.

## Upstream Synchronization

`origin` is the Magai repository; `upstream` is `78/xiaozhi-esp32`. Fetch and review upstream on a dedicated branch. Check ESP-IDF and component-manager requirements first, then inspect conflicts in the build, application, audio, MCP, and board-registration layers. Merge Magai boards and apps only after both board builds pass. Do not force-push or replace local Magai commits during synchronization.
