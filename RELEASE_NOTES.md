# WinVetSim Release Notes
## Version 2.5 — Post 2.4 Changes

**Base commit:** e4e5ad6 — Release Version 2.4 (2025-11-07)  
**Changed files:** 20

---

## New Features

### Scrolling Log Window (`main.cpp`, `simutil.cpp`, `vetsimDefs.h`)

A live scrolling log display has been added to the main application window. All messages written via `log_message()` now appear in a read-only multiline edit control rendered below the header text.

- The log window uses a 14pt Consolas monospace font for readability.
- It resizes automatically when the main window is resized (`WM_SIZE` handler).
- Line count is capped at 500 lines; the oldest 100 lines are trimmed when the limit is reached, preventing unbounded memory growth.
- Log appending is fully thread-safe: background threads post messages asynchronously via `PostMessageW` with a custom `WM_APP_LOG` message rather than blocking `SendMessageW` calls. The UI thread processes all edit control operations in `WndProc`, eliminating a potential deadlock.

---

## Bug Fixes

### Command Injection via `system()` Eliminated (`WebSrv.cpp`)

`system()` calls used to launch and stop the PHP server have been replaced with `CreateProcessA()`. The `cmd /c` shell wrapper has been removed, eliminating the command injection surface that existed when `localConfig.html_path` or `phpPath` contained shell metacharacters or spaces. The PHP working directory is now validated with `GetFileAttributesA` before use; an invalid path falls back to the current directory. Process and thread handles are correctly closed after launch. Failures are logged with the specific Windows error code and message.

### Race Conditions on Pulse/Breath Counters Fixed (`pulse.cpp`)

`pulseSema` and `breathSema` mutex locks that had been commented out were re-enabled. Beat counters (`pulseCount`, `breathCount`) shared between timer threads are now properly protected, preventing torn reads and stale cached values on multi-core systems.

### Malformed HTTP Request Crash Fixed (`simstatus.cpp`)

The HTTP command parser now validates that a colon-delimited command string contains at least three tokens before accessing `v[1]` and `v[2]`. Previously a short or malformed command (e.g. `set:foo` with no third segment) caused an out-of-bounds `std::vector` access, crashing the simulation server.

### Memory Leak on Scenario Cleanup Fixed (`scenario.cpp`)

A new `scenario_free()` function has been added that performs a complete depth-first traversal of the scenario data structure, freeing every node allocated during XML parsing:

- All `scenario_event` nodes from `scenario->event_list`
- All `scenario_scene` nodes from `scenario->scene_list`
- All simple `scenario_trigger` nodes from each scene's `trigger_list`
- All `trigger_group` nodes from each scene's `group_list`
- All group-level `scenario_trigger` nodes from each group's `group_trigger_list`

Previously only the root `scenario_data` struct was freed. `scenario_free()` is now called at scenario stop before `free(scenario)`, and the pointer is set to `NULL` immediately after.

### Broadcast Server Shutdown and Socket Cleanup Fixed (`bcastServer.cpp`)

The broadcast reply server previously ran an infinite `while(1)` loop, making `closesocket()` and `WSACleanup()` permanently unreachable dead code. Three changes were made:

- `SO_RCVTIMEO` is now set to 1 second on the receive socket, so `recvfrom` wakes periodically even when idle.
- The loop condition changed from `while(1)` to `while(!closeFlag)`, using the existing project-wide shutdown flag.
- When the loop exits, `closesocket()` and `WSACleanup()` now execute correctly.

Additional fixes in the same file:
- `SO_BROADCAST` `setsockopt` argument corrected from `char '1'` (ASCII 49) to `int 1`.
- `recvfrom` return value now checked against `SOCKET_ERROR` (was incorrectly compared to `NULL`).
- `WSAStartup` return value now checked against `0` (was incorrectly compared to `NULL`).
- Return value of the `SO_RCVTIMEO` `setsockopt` call is now checked; a warning is logged if it fails.
- Duplicate `#include` directives (`<iostream>`, `<conio.h>`) removed.
- Typo fixed: `"WSAStartup returbs %d\n"` → `"WSAStartup returns %d\n"`.

### Unicode Path Handling Fixed in XML Reader (`XMLRead.cpp`)

The `XMLRead::open` function previously cast a `char*` path directly to `LPCWSTR` via an intermediate `TCHAR` array, producing garbage wide-character strings for every character in the path. `CreateFileA` is now used directly with the narrow `char*` path, ensuring scenario files are opened correctly.

### `appendToParseLog` Now Uses Its Argument (`scenario_xml.cpp`)

`appendToParseLog(char* str)` previously ignored its `str` parameter entirely and always read from `simmgr_shm->status.scenario.error_message`. The function now correctly converts and appends the passed string. The `MultiByteToWideChar` conversion was also rewritten to dynamically size the output buffer rather than using a fixed-size intermediate buffer. `validateScenes()` return value corrected from `NULL` to `0`.

### `log_message` Error Path Fixed (`simutil.cpp`)

When `fopen_s` fails to open the log file, the error was previously shown as a blocking `MessageBoxW` dialog. It is now routed to the scrolling log window via `append_text_to_edit()`. On `mbstowcs_s` conversion failure, the function now returns early rather than forwarding partial or garbage content to the log window. A redundant `wcscat_s` call that appended an extra `\n` (causing a blank line between every log entry) has been removed.

### `takeInstructorLock` Deadlock Fixed (`VetSim.cpp` / `simutil.cpp`)

The retry loop in `takeInstructorLock` used `INFINITE` as the `WaitForSingleObject` timeout, meaning `trycount` was never incremented and the loop could never actually retry. The function has been simplified to a single `WaitForSingleObject` call with a 1,000 ms finite timeout, returning `0` on success and `-1` on failure or abandonment.

### GDI Font Handle Lifecycle Fixed (`main.cpp`)

The `HFONT` created for the log window's Consolas font is now held at file scope (`hLogFont`) and explicitly destroyed with `DeleteObject(hLogFont)` in `WM_DESTROY`, before `PostQuitMessage(0)` is called. Previously the font handle leaked for the lifetime of the process and `DeleteObject` was unreachable due to incorrect ordering after `PostQuitMessage`.

### Duplicate `qrs_interval` Assignment Removed (`VetSim.cpp`)

`scan_commands()` contained an identical `qrs_interval` update block duplicated twice in sequence. The second block was dead code and has been removed.

---

## Refactoring / Code Quality

### Dead Code Removed (`VetSim.cpp`, `vetsim.h`, `scenario_xml.cpp`)

Commented-out blocks that had no effect on behaviour were removed across multiple files, including: disabled respiration duration overrides, disabled `vpc_delay` instructor commands, disabled heart-rate and awRR smoothing code, and dead COM port debug stubs. `forceInstructorLock()` and `closeVideoCapture()` declarations removed from `vetsim.h` (implementations were already absent).

### `append_text_to_edit` Declarations Added to Header (`vetsim.h`)

Both overloads of `append_text_to_edit` (`const wchar_t*` and `char*`) are now declared in `vetsim.h`, making the API visible to all translation units that include the main header.

### Shared Log Window Constants Centralised (`vetsimDefs.h`)

`WM_APP_LOG`, `LOG_WINDOW_MAX_LINES`, and `LOG_WINDOW_TRIM_LINES` are defined in `vetsimDefs.h` so both `simutil.cpp` and `main.cpp` share the same values without duplication.

---

## Files Changed

| File | Nature of Change |
|---|---|
| `main.cpp` | Scrolling log window; WM_APP_LOG handler; GDI font lifecycle fix; WM_DESTROY ordering fix |
| `simutil.cpp` | Thread-safe log appending via PostMessageW; MessageBoxW replaced; mbstowcs_s fix; error path fix |
| `vetsimDefs.h` | Added WM_APP_LOG, LOG_WINDOW_MAX_LINES, LOG_WINDOW_TRIM_LINES |
| `vetsim.h` | Added append_text_to_edit declarations; removed dead function declarations |
| `bcastServer.cpp` | Shutdown fix; socket cleanup; setsockopt correctness; duplicate includes removed |
| `scenario.cpp` | scenario_free() added; memory fully released on scenario stop |
| `pulse.cpp` | Re-enabled pulse/breath semaphore locks |
| `simstatus.cpp` | HTTP vector bounds check added |
| `WebSrv.cpp` | system() replaced with CreateProcessA; injection surface removed; path validation added |
| `VetSim.cpp` | Duplicate qrs_interval block removed; takeInstructorLock simplified; dead code removed |
| `XMLRead.cpp` | CreateFileA used in place of TCHAR type-pun |
| `scenario_xml.cpp` | appendToParseLog fixed; MultiByteToWideChar buffer sizing fixed; return value corrected |
| `GenericHTTPClient.cpp` / `.h` | Updated (see diff) |
| `sim-parse.cpp` | Updated (see diff) |
| `simlog.cpp` | Updated (see diff) |
| `llist.cpp` | Updated (see diff) |
| `WinVetSim.vcxproj` / `.filters` | Project file updates |
| `args/args.vcxproj` | Project file updates |

---

## Version 2.4 — Changes from Version 2.3

**Base commit:** bdab196 — Release Version 2.3  
**Release commit:** e4e5ad6 — Release Version 2.4 (2025-11-07)  
**Changed files:** 8 (2 commits)

---

## New Features

### Controller Firmware Version via HTTP (`pulse.cpp`)

`getControllerVersion()` has been completely rewritten. The previous implementation opened a raw TCP socket and sent a literal `"version"` string, which no longer matched the controller protocol.

The new implementation fetches the controller's status page over HTTP using **libcurl** and parses the response with **nlohmann/json v3.12.0**:

- A `WriteCallback` / `ReadWebPage()` helper issues a `GET` request to `http://{ip}/cgi-bin/ctlstatus.cgi`.
- The response body is parsed for the `simCtlVersion` field using three progressive fallback strategies:
  1. **Full JSON parse** (`try_parse_full_json_for_simCtlVersion`) — succeeds when the CGI returns a clean JSON object.
  2. **Mixed-text extraction** (`extract_simCtlVersion_from_mixed_text`) — scans the raw body for the `simCtlVersion` key when HTML wrapper content surrounds the JSON.
  3. **Plain-string search** (`extract_simCtlVersion`) — last-resort substring scan for the version value.
- The extracted version string is stored in `simControllers.version`.

### `simControllers.version` Buffer Expanded (`vetsim.h`)

The `version` field in the `simControllers` structure has been widened from `char[32]` to `char[256]` to accommodate full firmware version strings returned by the HTTP endpoint.

---

## Changes

### Controller Version Display Temporarily Disabled (`simstatus.cpp`)

The block in `sendStatus()` that appended the controller version to the status reply has been commented out. This prevents stale or empty version data from appearing in status responses while the new HTTP-based fetch path is stabilised.

---

## Dependencies

### nlohmann/json v3.12.0 Added (`packages.config`)

A new `packages.config` file has been added to the project, declaring the **nlohmann.json 3.12.0** NuGet package. This provides the header-only JSON library used by the rewritten `getControllerVersion()` in `pulse.cpp`.

---

## Refactoring / Code Quality

### Unused Variable Removed (`main.cpp`)

An unused `errno_t sts` local variable in `getBuildDate()` has been removed. It was declared but never read, producing a compiler warning.

---

## Files Changed

| File | Nature of Change |
|---|---|
| `pulse.cpp` | `getControllerVersion()` rewritten; libcurl HTTP fetch; nlohmann/json parsing; three-strategy version extraction |
| `vetsim.h` | `simControllers.version` expanded from `char[32]` to `char[256]` |
| `simstatus.cpp` | Controller version display in `sendStatus()` commented out |
| `version.h` | `SIMMGR_VERSION_MIN` bumped from 3 to 4 |
| `packages.config` | New file; declares nlohmann.json v3.12.0 NuGet dependency |
| `main.cpp` | Unused `errno_t sts` removed from `getBuildDate()` |
| `WinVetSim.vcxproj` | Project file updated for new NuGet dependency and build settings |
| `WinVetSim.vcxproj.filters` | Filter file updated |

