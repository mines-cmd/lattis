# Legendary captures

Real output of Heroic's bundled Legendary, version 0.21.1 "Lowlife", captured 2026-11-18 with game A Short Hike (`d6407c9e6fd54cb492b8c6635480d792`, Windows build `1.10.1_patched_Windows`, 214 MiB download). The runs used a scratch copy of the config folder (`LEGENDARY_CONFIG_PATH=<copy>/legendary`) and a copy of the game folder, so Heroic's own files were not touched. Paths are rewritten to `/home/user/...`; tokens and account files were not copied.

To force an update, the copy's `installed.json` version was set to `1.10.0_old` and `manifests/` was emptied, so Legendary downloaded the whole build. The old-manifest error and the 0 MiB "reusable" size in the logs come from that setup, not from a normal update.

| File | What it is |
| --- | --- |
| `installed.json` | `legendaryConfig/legendary/installed.json` for the installed game; `version` is the installed build |
| `installed.outdated.json` | the same with `version` set to `1.10.0_old` |
| `assets.before-check.json`, `assets.after-check.json` | `assets.json` (`Windows[]`, trimmed to the game and the three entries that changed) before and after `list-installed --check-updates` |
| `check-updates.stdout`, `check-updates.stderr` | what `legendary list-installed --check-updates` printed |
| `update.stderr` | `legendary update <appName> -y` to the end, stderr; stdout was empty |
| `cancel-sigint.stderr` | the same update stopped with SIGINT after 30% progress |
| `cancel-sigterm.stderr` | stopped with SIGTERM after 30% progress |
| `resume-file.txt` | `<config>/tmp/<appName>.resume` left by the SIGTERM run |
| `update-resumed.stderr` | `legendary update` run with that resume file in place |

## Findings

- Progress is on stderr, every second, as four lines: `[DLManager] INFO: = Progress: 47.06% (272/578), Running for 00:00:02, ETA: 00:00:02`, then ` - Downloaded: …`, ` - Cache usage: …`, ` + Download …`, ` + Disk …`. The percentage is of chunks (the number in parentheses), not bytes. The first line shows 0.00%. Last lines are `All done! Download manager quitting...` and `Finished installation process in N seconds.`; stdout stays empty.
- Exit code is 0 on success and also after SIGINT.
- `installed.json` keeps the old `version` until the update finishes, then gets the new one.
- SIGINT (Ctrl-C) stops cleanly: `Command was aborted via KeyboardInterrupt, cleaning up...`, no leftover processes, no resume file.
- SIGTERM kills only the parent. About twenty Python worker processes stay alive, finish the download on their own within seconds and then exit; `installed.json` is not updated. Cancelling with `QProcess::terminate()` is therefore wrong; send SIGINT.
- The resume file is `<config>/tmp/<appName>.resume`, one `sha1:relative/path` line per finished file (not `.resumedata`). With it in place Legendary logs `Found previously interrupted download. Download will be resumed if possible.` and `Skipping 133 files based on resume data.`, and the next run's progress counts only the remaining chunks (0% to 100% of 391, not 578).
- `list-installed --check-updates` prints `Logging in to check for updates...` and `Updating entitlements.` on stderr, then the installed list on stdout. It does not print which games are outdated. It rewrites `assets.json` (new `build_version` values appear) and `entitlements.json`; it takes about 2.4 s.
