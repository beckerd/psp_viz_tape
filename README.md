# Viz Tape — cassette visualizer for the PSP Music player
<img width="440" height="496" alt="Screenshot 2026-10-14 at 10 45 55 PM" src="https://github.com/user-attachments/assets/ac097019-8985-4320-aab3-720a413feb3b" />

![Viz Tape: the cassette slides in, winds and flips from Side A to Side B](docs/viz_tape.gif)

I wanted the PSP's Music player to look like a tape deck, so this plugin
turns its last visualizer into an animated cassette: the reels turn, the tape
winds from one reel to the other, and after 15 minutes it flips to Side B.

Version 1.2 enables PSP-1000, PSP-2000, PSP-3000, PSP Go and PSP Street/E1000
on ARK-4 with matching 6.61 firmware modules. See [Tested on](#tested-on) for
the hardware results and the remaining tests.

This branch builds **1.3-dev1**, a PRO-C compatibility test with bounded local
lookup of the renderer's exports. A fresh install passed on a PSP-1000 with
ARK-4, and the test build has also been reported working on PRO-C hardware.

This was written by Astra and Opus5.5 if you do not want code written by AI on your PSP
this project is not for you.

## What it does

- **A cassette that plays along.** Red Side A and blue Side B, 15 minutes of
  playback per side. The tape winds from the left reel to the right, visible
  through the window, and the packs grow and shrink as it goes.
- **Flips.** At the end of a side the cassette lifts, turns over in
  perspective and settles on the other side, repeating A → B → A.
- **Fast-forward and rewind.** Seeking in a song winds the tape 12× faster
  with the reels spinning fast; rewind runs it backwards, flips included.
- **Pause-aware.** Pausing, hiding the visualizer or putting the PSP to sleep
  stops the tape. Opening the visualizer slides the cassette in and latches it.
- **Cassette icon** in the visualizer selector, made automatically on the
  PSP the first time it starts.
- **Light on the CPU.** Only the reel strip is redrawn while playing, and
  frames where nothing moved are reused: about 1.9 ms per frame at ~60 fps on
  a PSP-1000.
- Sony keeps drawing the track title, time and controls on top.

## Install

1. Download and unzip [viz_tape-v1.3-dev1.zip](https://github.com/beckerd/psp_viz_tape/releases/download/v1.3-dev1/viz_tape-v1.3-dev1.zip)
   from the [1.3 test release](https://github.com/beckerd/psp_viz_tape/releases/tag/v1.3-dev1).
2. Connect your PSP to your computer in USB mode.
3. Copy the `seplugins` folder from the ZIP onto the root of the memory card
   or PSP Go internal storage.
   If the card already has a `seplugins` (or `SEPLUGINS`) folder, let your
   computer merge them; nothing else in it is replaced.
4. **ARK-4:** open `seplugins/PLUGINS.TXT` in a plain-text editor (create it
   if it doesn't exist) and add this one line:

   ```
   vsh, ms0:/seplugins/viz_tape/viz_tape.prx, on
   ```

   **PSP Go internal storage:** use this line instead:

   ```
   vsh, ef0:/seplugins/viz_tape/viz_tape.prx, on
   ```

   **PRO-C:** use `seplugins/vsh.txt` instead, with this line:

   ```
   ms0:/seplugins/viz_tape/viz_tape.prx 1
   ```

   For Go internal storage, change `ms0:` to `ef0:`. Use the format for your
   CFW; `vsh.txt` does not use ARK's commas or `vsh,` prefix.

   Keep one Viz Tape entry. The plugin saves its icon, log and boot guard in
   `seplugins/viz_tape` on the same storage device as the installed PRX.

5. Eject the card, exit USB mode and restart your PSP.
6. Play a song in the Music player and switch to the last visualizer.

That's it. On the first start the plugin creates the cassette icon during
startup, ready for the first Music session. [INSTALL.txt](INSTALL.txt)
in the ZIP has the same steps, plus how to update and uninstall.

**Updating from 1.0 or 1.1:** replace the two old Viz Tape lines with the single
line above, copy the new `seplugins` folder over the old one, then restart the
PSP before opening Music. Delete the obsolete `viz_tape_render.prx`; it is now
embedded in `viz_tape.prx`. The old `enabled.txt` is also no longer used.

## Tested on

- PSP-1000 (32 MB), 6.61 ARK-4 as shown in System Information,
  128 GB memory card, alongside xmb_gamecache (public 1.2 and test 1.3-dev1).
- PRO-C hardware: test `1.3-dev1` reported working.

On 2026-10-17, a fresh `1.3-dev1` install generated its icon, started the
renderer and patched the visualizer successfully. The user reported that it
looked good; the startup log was clean, file integrity checks passed, and
the boot guard cleared. PRO-C hardware success was subsequently confirmed.
This change does not establish that public 1.2 fails on PRO-C. Full model
coverage, extended playback and suspend/resume remain unverified for this build.

## If something goes wrong

- **Boot guard.** For the first 15 seconds after each XMB start, the plugin
  keeps `boot-guard.txt` on the card. If a start crashes in that window, the
  next start skips the plugin once, so a fault can never become a boot loop.
  (Launching a game or entering USB mode within 15 seconds has the same
  harmless one-start effect.)
- On PRO-C, disable its entry in `vsh.txt` by changing the final `1` to `0`,
  or use the Recovery Menu's Plugins section, then restart.
- On ARK, change the Viz Tape line in PLUGINS.TXT to `off` over USB,
  or hold **START** while powering on to bypass ARK plugins for one boot, or
  use ARK's **Recovery Menu** (hold R at power-on). See ARK's
  [recovery guide](https://github.com/PSP-Archive/ARK-4/wiki/Recovery).
- Diagnostics are written to `seplugins/viz_tape/status.txt` on the installation
  device (`ms0:` for Memory Stick or `ef0:` for PSP Go internal storage):
  `PATCHED final brown-line slot 7` means the cassette is hooked,
  `ICON created visualizer.rco from the stock resource` means the icon was
  made, `ICON cassette resource selected in RAM` means it is in use, and
  `SEEK fast-forward/rewind state found` means seeking is detected.
  A boot-guard recovery preserves the failed startup's log and appends the
  skip reason. Renderer load/start checkpoints are flushed before each call.
  Startup also records the actual model ID, selected storage device and user
  partition bounds. The `devkit` field may be ARK's compatibility value.
- If the cassette doesn't appear, check the PLUGINS.TXT line and
  `seplugins/viz_tape/viz_tape.prx`. A `RENDERER load failed` or
  `RENDERER start failed` status means the embedded runtime could not start;
  the stock visualizer is kept.

## Known limitations

- Only the verified 6.61 module layouts are supported; other layouts
  leave the stock visualizer selected.
- Only the last visualizer is replaced; the spectrum and others are untouched.
- The tape clock restarts when the XMB restarts, and is not tied to the
  position in the song.
- Don't remove the memory card while the Music player is open: the player
  reads the cassette icon's `visualizer.rco` from the card.
- The icon is only made when the stock resource matches the supported patch.
  A different resource keeps the stock icon.

## Building from source

Requires the [pspdev toolchain](https://github.com/pspdev/pspdev) and Python 3:

```
export PSPDEV=~/pspdev
export PATH=$PSPDEV/bin:$PATH
make
```

`make` builds the user-mode renderer, embeds it in `viz_tape.prx`, audits both
modules' imports and the embedded bytes/alignment, and writes `SHA256SUMS`.
Only `viz_tape.prx` is installed. `viz_tape_render.prx` is a build intermediate.
`make release` builds the ZIP with the single installable PRX.

`make host-check` runs the host tests (rendering, animation, mesh, frame cache,
normal/expanded address checks, model-independent startup, both storage paths,
relocated firmware signatures, bounded export parsing, embedded runtime loading, USB lifecycle, boot
guard and the on-PSP icon builder) under AddressSanitizer and UndefinedBehaviorSanitizer.
`make profile-check` and `make icon-check` also verify the firmware signatures,
but need private firmware captures that are not in this repository.

## How it works (short version)

The kernel plugin registers a CFW module-start handler. When the Music
player's visualizer module loads, it verifies the module's exact layout —
sizes, the final effect's 15-entry vtable, code signatures and import stubs —
and only then replaces that effect's one draw pointer in user RAM with the
renderer's. It also locates the visualizer's playback-state word (play,
pause, fast-forward, rewind) for the renderer to read.

The renderer is embedded as a user PRX inside the kernel plugin. A startup
worker loads it from memory into the PSP's user partition and starts it before
any draw pointer can refer to it. If Music loads first, the worker checks and
patches that already-loaded visualizer after the renderer starts. No renderer
file is extracted to the card. Loading uses the size-first 6.60/6.61 firmware
ABI and a temporary zero-padded user buffer covering the renderer's full
memory image, including BSS. That buffer is freed after loading.
The kernel queries the real user-memory partition and passes its bounds to
the renderer; checks neither assume 32 MB nor grant access to extra RAM based
on a model number alone. Kernel patching and user-mode drawing remain
separate at runtime, with one file and one plugin entry to install.

The private renderer's function and variable exports are resolved directly
from its module tables, with bounded names, counts and pointers. Invalid
metadata leaves the stock visualizer selected. PAF functions still use the
CFW's NID translation and the existing native-import checks.

The user-mode renderer draws the cassette in software into a fixed 480×272
buffer, run-length-merges it into rectangles and submits them through the
same PAF drawing functions the native effect uses, with no per-frame
allocation. While the tape plays, only the reel strip is redrawn and the rest
comes from a cache.

For the icon, the plugin carries a small patch: the new cassette image and
the resource header that references it. On first start it reads the PSP's
own `flash0:/vsh/resource/visualizer_plugin.rco` (read-only), checks that
applying the patch gives exactly the expected result, and only then writes
`seplugins/viz_tape/visualizer.rco` on the installation device (`ms0:` or `ef0:`).
A second guarded change then points
the Music player at that file (in RAM) after checking its exact size and
checksum again on each Music-module load, including after USB mode. A missing
or invalid file leaves the stock icon selected. Only the last visualizer's
thumbnail differs. Nothing is ever written to flash.



## Disclaimer

This software is provided **as is, without warranty of any kind**, and the
author accepts no liability for any damage arising from its use — see
sections 15 and 16 of the [LICENSE](LICENSE). Use it at your own risk.

Just in case: after an XMB freeze, hold the power switch off and disable the
plugin through your CFW's Recovery Menu or its plugin list over USB.
ARK also supports holding START at power-on to bypass plugins for one boot.

## Credits & license

GPL-3.0-or-later. Built with PSPSDK and ARK-4's module-start API. The RCO
work used [RCOMage's format definitions](https://github.com/kakaroto/RCOMage)
and [Jpcsp's LZR decoder](https://github.com/jpcsp/jpcsp) — thanks to both
projects.

The cassette artwork and code were made for this project. The icon patch
contains the new cassette image and the rebuilt 868-byte resource header that
references it; every stock image is copied from your own PSP's file, and
Sony's resource itself is not distributed.
