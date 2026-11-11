# WinVetSim 2.5 — What's New

This update makes WinVetSim more reliable during simulations and easier to start up. You don't need to do anything differently: install it and use it as before.

## New

- **Activity log in the main window.** The WinVetSim window now shows a running log of what the program is doing. If something goes wrong, check this log first.

## More reliable simulations

- **Steadier heartbeat and breathing signals.** The heartbeat and breathing signals sent to the patient simulator are more consistent, with fewer missed or doubled beats.
- **Atrial fibrillation (AFib) behaves correctly.** The irregular AFib rhythm now plays as intended instead of switching rapidly between irregular and regular beats.
- **Premature beats (VPCs) vary from session to session,** instead of repeating the same pattern every time the program starts.
- **One slow connection no longer holds up the others.** If one simulator controller has a network problem, the other controllers keep receiving their signals. A controller that disconnects is detected and can reconnect.
- **Scenario problems are reported.** If a scenario refers to a scene that doesn't exist, an error message now explains why the scenario stopped.

## Easier startup and shutdown

- **The control page opens in your browser automatically,** more reliably than before.
- **Leftover web servers are cleaned up automatically.** If WinVetSim was closed unexpectedly, the web server it uses could be left running in the background and prevent the next startup. WinVetSim now clears it automatically when it starts.
- **WinVetSim shuts down its web server when you close it.**

## Stability and security

- Fixed a crash caused by incomplete commands sent to the simulator.
- Fixed memory that was not released when a scenario stopped, which could slow the program during long sessions.
- Fixed a problem with how scenario file locations were read when opening scenarios.
- The built-in web server now starts in a safer way.
- Log messages no longer interrupt you with pop-up windows, and the log file no longer has blank lines between entries.

For full technical details, see `RELEASE_NOTES.md`.
