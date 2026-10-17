# Changelog

## 1.3-dev1 — test build, 2026-10-17

- Resolve the private renderer's function and variable exports directly,
  with complete name/table bounds checks and explicit export kinds. Invalid
  metadata cannot fall back to a CFW lookup that bypasses these checks.
- Keep the existing renderer ownership/ABI checks and PAF NID translation.
- Add regressions for malformed records, unterminated names, incomplete
  tables, wrong-module pointers and absence of private-export fallback.
- Provide `1.3-dev1` as a PRO-C/ARK test build, with `vsh.txt` instructions
  for PRO-C.
- Verify a fresh `1.3-dev1` install (`93b9c0a`) on PSP-1000/6.61 ARK-4 on
  2026-10-17: icon generation, renderer startup, playback-state binding and
  slot 7 patching succeeded, with a clean startup log and cleared boot guard.
  The user reported good visuals; PRX and icon integrity checks passed.
- Record subsequent confirmation that `1.3-dev1` works on PRO-C hardware.
  Full model coverage, extended playback and suspend/resume remain unverified.

## 1.2 — 2026-10-17

- Enable all retail PSP models: 1000, 2000, 3000, Go and Street/E1000. Version
  1.1's startup check silently rejected every model except the 1000.
- Select ms0: or ef0: from the installed plugin path for logs, the boot guard,
  storage sync and the cassette resource, including its RAM redirect.
- Query the actual user-memory partition and share its bounds with the
  renderer. Validate relocated modules in expanded user RAM without widening
  checks on devices where that RAM is unavailable. Retain exact firmware
  signatures, resource validation and the boot guard.
- Log model, storage and memory bounds. Add sanitizer regressions
  across model IDs, both storage devices, low/high address relocations and
  invalid partition data.
- Confirm `1.2-dev3` (`c776a51`) startup and Music-hook success from the
  PSP-1000 device log on 2026-10-17, running 6.61 ARK-4 per the previously
  reported System Information. Memory-partition bounds were correct, the
  renderer started, the icon and playback-state binding succeeded, and
  slot 7 was patched. The installed binary and icon passed integrity checks;
  the boot guard was absent. Other models, extended playback and
  suspend/resume still need hardware testing.
- Finalize the diagnostic version label as `1.2`; runtime behavior is
  unchanged from the hardware-tested `1.2-dev3` build.

- Install one `viz_tape.prx` and one PLUGINS.TXT entry. The kernel plugin
  embeds the renderer and a worker loads it into user memory; nothing is
  extracted to the card. The renderer keeps its user-mode drawing context.
- Handle renderer load/start failures without changing the native draw
  pointer, and revisit a visualizer that loads before the renderer is ready.
- Verify the embedded runtime bytes and 64-byte buffer alignment during the
  binary audit. The release ZIP and install checksums include only one PRX.
- Correct the initial single-file build's startup failure: the 6.60/6.61
  firmware import expects size before buffer, despite the SDK declaration.
  Use a temporary zero-padded user buffer covering the full loaded image,
  since the plain-PRX loader can copy beyond the file length through BSS.
- Preserve the failed startup log when the boot guard skips activation, and
  flush renderer load/start checkpoints. Corrected logs identify `1.2-dev2`.
- Add regressions for the firmware argument order, padded buffer and cleanup,
  allocation failures, and recovery-log preservation.
- Hardware retest passed on 2026-10-17 with `1.2-dev2` (`0494423`): PSP-1000,
  6.61 ARK-4 per System Information, alongside xmb_gamecache. Startup completed,
  the user renderer started, the icon and playback-state binding succeeded,
  slot 7 was patched, and the boot guard cleared. Host tests and the binary audit pass;
  extended playback and suspend/resume testing of this build is outstanding.
- Correct the test record's firmware identification: ARK hooks the plugin's
  version API to return a 6.60 compatibility value on 6.61. The raw
  `devkit=06060010` log value does not establish a test on actual 6.60 firmware.

- Validate the icon resource again before every Music-module redirect, so a
  file removed or changed over USB cannot reuse its startup validation. Keep
  the stock icon when validation fails or USB storage is active.
- Build the first-install icon during startup, before registering hooks, so
  opening Music immediately no longer misses the cassette icon.
- Skip activation if the status thread cannot be created or started. Hooks
  are registered only after that succeeds, keeping active hooks protected by
  the boot guard during startup.
- Add sanitizer-backed lifecycle tests for first launch, storage changes,
  reporter exit, USB state changes, thread failures and handler chaining.

## 1.1 — the icon makes itself; simpler install

- **No more icon step.** The kernel plugin now creates the cassette icon on
  the PSP: on first start it reads the stock `visualizer_plugin.rco` from
  flash0 (read-only), dry-runs the built-in patch to confirm the exact result,
  and only then writes `seplugins/viz_tape/visualizer.rco`. A different or
  damaged stock file never touches the card; partial files are removed.
- **Copy-and-go release.** The ZIP now holds a `seplugins/viz_tape` folder
  (just the two plugins) to copy onto the memory card, with plain-language
  instructions.
- **No `enabled.txt`.** Like any ARK plugin, Viz Tape is now switched on and
  off only by its PLUGINS.TXT lines. The development-era one-boot test ticket
  is gone too; the boot guard now protects every start.
- **Shorter `status.txt`.** It is now a brief startup summary — one line each
  for the start, the icon, fast-forward detection and the cassette hook (or why
  something was skipped). The periodic performance lines are gone; it is
  rewritten at each start and stays under 1 KB.

## 1.0 — first public release

Everything below was developed and tested incrementally on real hardware
(PSP-1000, ARK-4, 6.61, 128 GB card); this is the first version published.

- **Animated cassette** replacing the Music player's last visualizer: red
  Side A and blue Side B, 15 minutes of playback per side, tape winding
  between reels behind the window, and a rounded perspective flip between
  sides. The cassette slides in and latches when the visualizer opens.
- **Fast-forward and rewind** read the visualizer's own playback state: the
  tape winds 12× faster with the reels at 3× (kept below the speed where six
  teeth would appear to spin backwards), and rewind runs it backwards.
- **Cassette selector icon**, shipped as a patch applied to the user's own
  stock resource; the kernel checks the result's size and checksum before
  using it.
- **Efficient rendering**: only the reel strip is redrawn while playing,
  unchanged frames are reused, and the rest comes from a cache — about
  1.9 ms per frame at ~60 fps on a PSP-1000 (down from ~16.6 ms at ~30 fps).
- **Smoothed square reel teeth**, evenly spaced, with smoothed ring edges.
- **Persistent activation** via `enabled.txt`, plus a **boot guard**: a fault
  within 15 seconds of an XMB start skips the plugin on the next start only,
  so it can never cause a boot loop.
- **Fails inert**: every RAM change first verifies the exact firmware code it
  touches; a mismatch is logged to `status.txt` and skipped. No flash writes.
- Host test suite under AddressSanitizer/UBSan, a binary import audit and
  reproducible builds.
