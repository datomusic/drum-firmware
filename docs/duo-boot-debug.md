# duo-on-drum: boot bring-up debug log

**Status: BOOTING.** Root cause was a PIO/DMA resource conflict caused by init
ordering in `duo/main.cpp` — see §7 below. The DUO now enumerates USB, reaches
its main loop and renders audio at **16.0–16.4%** ISR load with a live peak
reading.

This records what was fixed and — more usefully — what was *ruled out with
evidence*, so a fresh session does not re-derive it.

Symptom throughout: after flashing the DUO firmware to the Submarine board,
no USB device enumerates, no LEDs, no sound, no interaction.

---

## Read this first: the symptom is nearly information-free

`musin::usb::background_update()` (which pumps `tud_task()`) is only called
from the DUO main loop. `musin::usb::init(false)` calls `tusb_init()` and
returns without pumping.

**So USB never enumerates unless the main loop is reached.** "No USB device"
therefore means only "never reached the main loop" — it does not localise the
fault at all. Likewise the LEDs: `DuoDisplay::init()` ends with
`clear()` + `show()`, so the panel is dark until the first main-loop
`update()`. A hang anywhere in `main()` produces exactly this symptom.

Do not read "completely dark, no USB" as evidence about *which* init step
failed. It is compatible with all of them.

---

## Fixed (with evidence)

### 1. The image was never being selected by the bootrom

`duo/build.sh` did a bare `picotool load -f` with no `-p`. Picotool treats
that as an A/B update and writes the *inactive* partition. On-device:

```
Partition 0:  drum-1.0.0-rc.3    version 1.0
Partition 1:  duo-...-babf2ca0   version 1.0
last booted partition:  partition 0
```

DUO and DRUM both declare binary version 1.0 (`pico_set_binary_version(...
MAJOR 1 MINOR 0)` in both CMakeLists), so the bootrom has no tiebreak and
boots partition 0. **The DUO had never executed a single instruction.**

Fix: `duo/build.sh` now defaults to `-p 0`, verifies the partition exists
first (as `drum/build.sh` does), and reboots into it with
`picotool reboot -f -g N`. Note `reboot` uses `-g`, `load` uses `-p`.

### 2. Stack overflow in `main()`

`keypad` and `scanner` were automatic locals:

| | DUO `main()` | DRUM `main()` |
|---|---|---|
| stack frame | **1940 bytes** | 16 bytes |
| stack size | 2048 | 2048 |
| headroom before first call | **~56 bytes** | ~2008 |

`sizeof(Keypad_HC138<8,5>)` is **1744 bytes** (DWARF). DRUM keeps these at
namespace scope; DUO put them on a 2 KB stack. Guaranteed overflow on the
first call, silent because no stack guard was configured.

Fix: `keypad`, `keypad_handler`, `scanner`, `logger` are now `static` in
`duo/main.cpp`. `logger` had to move too — a static keypad holding a
reference to a stack-local logger would dangle. Frame is now **76 bytes**.

Independent confirmation it could not have worked: the audio ISR nests on
this same stack. Deepest pull-graph chain is
`fill_buffers_from_irq (260) → AudioMixer<4> (260) → AudioMixer<2> (260) →
CustomEnvelope (36)` = 816 bytes, plus ~104 for the exception/FP frame.
Main-loop deepest path is 680 (float `printf`). Pre-fix that is ~2900
against 2048.

### 3. Stack guard + larger stack

`duo/CMakeLists.txt` now sets `PICO_STACK_SIZE=4096` and
`PICO_USE_STACK_GUARDS=1` as **global** definitions before SDK init —
`crt0.S` reserves the `.stack` section, so a target-scoped definition would
silently do nothing. Verified in the ELF: `.stack_dummy` 0x800 → 0x1000,
`__StackBottom`/`__StackTop` = 0x20081000/0x20082000, and

```
runtime_init_per_core_install_stack_guard:
    msr  MSPLIM, r0      ← called from runtime init
```

On Cortex-M33 this is the hardware stack-limit register: overflow now raises
a UsageFault at the offending instruction instead of corrupting silently.
SCRATCH_Y is 4 K, which bounds `PICO_STACK_SIZE`; SCRATCH_X stays free
(core 1 unused).

### 4. Init-failure path made diagnosable

`AudioOutput::init()` returning false previously spun in a `printf`/`sleep`
loop that never pumped USB — a codec failure was indistinguishable from a
dead board. It now calls `musin::usb::background_update()` every pass and
blinks the play LED red via `DuoDisplay::show_error(bool)`.

### 5. RNG never seeded

`std::rand()` was used for the default pattern with no seed. The reference
firmware does seed, in
`duo-imxrt/brains2/core/boards/DUO_BRAINS_2.3/pins.cpp:111`
(`randomSeed` from three pot readings). Now `srand(get_rand_32())`, matching
DRUM; required linking `pico_rand`.

### 6. Build scripts flashed stale images

`UF2_FILE=$(find build -name "*.uf2" -print -quit)` took the first UF2 in
directory order, not the newest. Since the version-suffix commit
(`4b83fbe7`) changed output filenames, both scripts had been uploading
**stale images from previous builds**. Now `ls -t`, excluding
`partition_table.uf2` and `-direct.uf2`.

---

### 7. ⭐ ROOT CAUSE: PIO/DMA claim conflict from init ordering

`audio_i2s_setup()` takes its state machine and DMA channel from the
**hardcoded** `i2s_config` in `musin/audio/audio_output.cpp`:

```c
struct audio_i2s_config i2s_config = {
    .dma_channel = 0,
    .pio_sm = 0,
};
```

and claims them with `pio_sm_claim()` / `dma_channel_claim()`, both of which
**panic** when the resource is already taken. `WS2812_DMA::init()` instead
calls `dma_claim_unused_channel(true)` and
`pio_claim_free_sm_and_add_program_for_gpio_range()`, so it takes the *lowest
free* channel — channel 0 — if it runs first.

| | order | result |
|---|---|---|
| `drum/main.cpp` | `audio_engine.init()` (151) **then** `pizza_display.init()` (154) | audio gets ch 0, LEDs get ch 1 — works |
| `duo/main.cpp` (pre-fix) | `display.init()` **then** `AudioOutput::init()` | LEDs get ch 0, `dma_channel_claim(0)` panics |

A `panic()` in a Release build with no host attached spins silently, which is
exactly the dark board with no USB. DRUM only works here **by accident of
ordering** — nothing documents or enforces the dependency.

**Fix:** `duo/main.cpp` now calls `AudioOutput::init()` before
`display.init()`, keeping the result in `audio_output_ok` so the error-blink
path still runs once the display is up. The ordering requirement is documented
at the call site.

**Latent trap, not yet fixed:** the hardcoded `pio_sm 0` / `dma_channel 0` will
do this to *any* app that brings up a PIO or DMA peripheral before audio. The
real fix is for `i2s_config` to claim dynamically, but that is a `musin` change
touching DRUM's working audio path — raise it at the gate (port plan §9.1,
"does musin need to expand").

### 8. Diagnostic tool: `duo/ui/boot_beacon.h`

The symptom is nearly information-free (see top of this file), so localisation
needed a signal that depends on nothing. `boot_beacon::signal(n)` bit-bangs the
WS2812 line with interrupts masked — no PIO, no DMA, no semaphore, no alarm
pool — and lights `n` LEDs. `BEACON(n)` calls after each init step in `main()`
turn "dark board" into a stage number.

Enable with `#define DUO_BOOT_BEACON 1` at the top of `duo/main.cpp`. It owns
the LED data pin while enabled, so the real display is suppressed; the loop
alternates 11/12 LEDs so "reached the loop" is distinguishable from "hung on
the last init step". **This is how stage 8 (the old numbering) pinned the hang
inside `AudioOutput::init()`.** Left in the tree, disabled, for the next
bring-up.

## Ruled out — do not re-investigate without new evidence

| Hypothesis | Evidence against |
|---|---|
| The DUO port broke DRUM | Symbol-table diff of DRUM at HEAD vs the last known-good `drum-1.0.0.elf`: **zero** differing symbols/sizes. The new synth sources are unreferenced by DRUM and dropped by the linker; `filter.h`/`dspinst.h` changes are purely additive. |
| TBYB (try-before-you-buy) | **Applies to DRUM, not DUO.** DRUM is `IMAGE_TYPE flags=0x9021` (TBYB set), DUO is `0x1021` (clear) — `duo/CMakeLists.txt` never sets `PICO_CRT0_IMAGE_TYPE_TBYB=1`. The DUO image is already effectively bought. |
| RAM exhaustion | ~48 KB used of 520 KB. |
| USB descriptor problem | `diff drum/usb_descriptors.c duo/usb_descriptors.c` is empty. |
| Build misconfiguration | `-D` flags for `musin/audio/audio_output.cpp` are identical between the DRUM and DUO builds, both directions. |
| GPIO conflict with the codec | Mux addr 6–9, keypad cols 10–14, WS2812 16, I²C 18/19, codec reset 20, ADC 26. No overlap. |
| `SCALE[std::rand() % 9]` off-by-one | **Faithful port, leave it.** Reference is `SCALE[random(9)]` (`duo-imxrt/shared/duo/Sequencer.h:84`); Arduino `random(9)` is 0–8. Changing it is a silent behavioural deviation (port plan §7.6). |

---

## Closed

1. ~~Has a build containing the stack fix actually been flashed?~~ Superseded:
   the boot fault is §7, and the firmware now runs.
2. ~~`AudioOutput::init()` may hang rather than return false.~~ **Confirmed —
   this was it,** though via `panic()` on the DMA claim, not the AIC3204's
   blocking I²C. The codec path was never the problem.
3. ~~Nothing has confirmed the DUO reaching its main loop.~~ Confirmed reached:
   `/dev/cu.usbmodem101` enumerates and prints the per-second load report.
4. ~~Unexplained: partition 0 held a known-good DRUM rc.3 yet the board showed
   nothing.~~ The board hardware is fine; §7 explains the DUO side.

## Still open

1. **Flashing this device needs `-p 0` explicitly.** A bare `picotool load -f`
   is treated as an A/B update and lands in **partition 1**, which the bootrom
   never selects (both images declare version 1.0 — see fix #1). Observed again
   this session: a partition-1 flash looked exactly like "the fix didn't work".
   `duo/build.sh` already defaults to `-p 0`; don't hand-run `picotool load`
   without it.
2. **Both partitions now hold DUO images.** Partition 0 and 1 are both `duo`
   builds, so the DRUM is no longer on this unit. Restore it deliberately when
   needed.
3. **The `i2s_config` hardcoded-resource trap** (§7) is unfixed by design.
4. **Beyond boot:** the §6 control mapping has still not been played by hand
   (port plan §11), and no MIDI I/O is wired yet.

### Practical: picotool no longer needs manual BOOTSEL

Now that the firmware runs and services USB, `picotool load -f -p 0 <uf2>`
reboots the device into BOOTSEL by itself. `picotool reboot -f -g 0` afterwards
may report `ERROR: ... rebooting` while still succeeding — check for the serial
port rather than trusting the exit status.

---

## Practical notes

```bash
export PATH="/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin:$PATH"
./duo/build.sh -n          # build only
./duo/build.sh             # build + flash to partition 0
picotool info -a -f        # both partitions + last booted partition
```

- Device must be in BOOTSEL (hold the button while plugging in) once the
  firmware is dead — it cannot reboot itself into it.
- CI lints with **clang-format 18** (`/opt/homebrew/opt/llvm@18/bin/clang-format`);
  the local v22 gives false passes.
- clangd in this tree reports bogus `'cstdint' file not found` /
  `type_traits` errors — it lacks the ARM toolchain headers. The build is the
  authority.
- `drum/build.sh --direct` (added this session) pre-buys a DRUM image by
  clearing TBYB, so a plain flash survives a power cycle. Shared logic lives
  in `tools/picobin.py`, used by `tools/clear_tbyb_uf2.py` and
  `tools/make_factory_uf2.py`. Regression-checked: the factory composer still
  reproduces `drum-factory-image-1.0.0-rc.3.uf2` byte-identically.
