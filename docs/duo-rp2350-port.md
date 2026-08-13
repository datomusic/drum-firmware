 # Porting the DUO to RP2350 / musin

Feasibility research and refactor plan for moving the Dato DUO from the
i.MX RT1011 (Brains 2) to the RP2350 platform used by the DRUM.

**Status (2026-08-13): milestone 1 is running on hardware and the control
mapping is confirmed playable.** The `duo/` app boots on the Submarine board,
enumerates USB, sequences, responds to DRUM's panel per §6, does MIDI I/O,
and renders the full DUO graph minus the delay at **16.0–16.4%** audio ISR
load. One item remains: the **RAM-residency disassembly walk** (§12.3).
Decisions marked **[DECIDED]** are settled; **[OPEN]** ones need an owner.

**Sources:** `duo-imxrt/` (Brains 2 firmware), `drum-firmware/` (`musin/` +
`drum/`). Line references were accurate at time of writing; verify before
relying on them.

---

## 0. Scope — read this first

The plan is deliberately split at a **gate**.

**Milestone 1 — "duo-on-drum."** Most of the DUO's functionality running on
the DRUM's Submarine board, as a new `duo/` app in this repo alongside
`drum/`, sharing `musin/`. **The delay line is out of scope.** DRUM's control
surface is remapped to drive DUO parameters (§6). Output goes through the
AIC3204 that is physically on the board. No persistence, no new PCB, no
output-stage abstraction.

**The gate.** With a working duo-on-drum in hand, re-evaluate the questions
that are currently guesses (§9): does musin need to grow, how do we handle
code duplication between two apps in one repo, should musin become its own
repository, and does the DUO go forward with the AIC3204.

This ordering matters. The original version of this document put an
output-stage refactor first and the delay feedback loop second — both are
now past the gate, because both are in service of decisions the milestone is
supposed to *inform* rather than assume.

---

## 1. Verdict

**Confirmed by construction.** Milestone 1 was built and runs; the estimate
below held, and the one surprise was a platform-integration bug rather than
anything in the port itself (§12.2).

Feasible, and lower-risk than a platform migration normally is, because both
halves of the problem are already partly solved in `musin/`:

- `musin/audio` is **already a partial Teensy Audio port** — the same library
  the DUO's synth is built from — converted to a pull-based graph.
- `musin/` already has drivers for nearly every DUO peripheral: WS2812 LEDs,
  analog mux scanning, keypad matrix, sync in/out, MIDI DIN + USB.

With the delay deferred, the remaining milestone-1 work is **mechanical**:
porting ~7 Teensy Audio synthesis classes into musin's pull model, removing
Arduino, and authoring the control mapping. The one genuine architectural
mismatch — the delay feedback loop — has been moved past the gate (§4.2).

### Cost drivers, roughly ranked (milestone 1 only)

| Item | Size | Risk |
|---|---|---|
| Porting synthesis classes | Medium | Low — mechanical |
| Arduino removal (mostly FastLED) | Medium | Low — tedious |
| Control mapping + LED remap | Small | Low — but must be authored, §6 |
| ISR CPU load unknown | — | Medium — measured, not designed around |
| Audible parity / voicing pass | Medium | Medium — subjective, lands late |

---

## 2. What the DUO is, in porting terms

The application is [`brains2/apps/duo/main.cpp`](../../duo-imxrt/brains2/apps/duo/main.cpp)
— a single-threaded `main_loop()` with a ~11 ms LED/MIDI frame driving:

- **A mono Teensy Audio synth graph** ([`Synth.h`](../../duo-imxrt/brains2/apps/duo/duo-firmware/src/Synth.h)):
  2 oscillators → mixer → state-variable filter → amp envelope → bitcrusher →
  delay with feedback → output mixer. Plus kick and hi-hat voices
  ([`DrumSynth.h`](../../duo-imxrt/brains2/apps/duo/duo-firmware/src/DrumSynth.h)).
- **`shared/duo/`** — sequencer, note stack, tempo handler, pitch, MIDI helpers.
- **Arduino-flavoured platform glue** — `Arduino.h`, `FastLED`, `Keypad`,
  `elapsedMillis`, and the NXP `fsl_*` SDK.

The structurally important fact: **`shared/duo/` is already mostly
platform-free.** `Sequencer.h`, `seq.cpp`, `note_stack.h` and `Pitch.h`
include only `<cstdint>` / `<cstring>` and port as-is. The exceptions are
`TempoHandler.h` and `MidiFunctions.h`, which pull in `MIDI.h` and
`lib/tempo.h` / `lib/sync.h`.

---

## 3. Platform comparison

| | Brains 2 (RT1011) | RP2350 (DRUM) |
|---|---|---|
| Core | Cortex-M7 @ 500 MHz, cache | Dual Cortex-M33 @ 150 MHz, FPU + DSP ext |
| RAM | 128 KB | 520 KB |
| Audio out | PT8211 / PCM5100A (I²S) | AIC3204 codec (I²S + I²C) |
| LEDs | FlexIO + DMA | PIO + DMA |
| Bootloader | i.MX ROM DFU | RP2350 BOOTSEL + A/B partitions |
| Persistence | EEPROM (**unused**) | littlefs on flash |
| Style | Arduino + NXP SDK | Bare Pico SDK, C++20 |

### 3.1 CPU headroom — probably a non-issue

500 MHz M7 → 150 MHz M33 looks alarming. It isn't, and there's direct evidence:
**Brains 1 ran essentially this same audio graph on a 72 MHz Cortex-M4**
(K20DX256). The RP2350's M33 has the DSP extension and a single-precision FPU,
so PJRC's SIMD-optimised int16 paths map over cleanly — `musin` has already
demonstrated this by porting `filter` and `crusher`.

At 150 MHz you have roughly 2× the headroom of the original target, plus a
second core in reserve. Memory is a straight win: 520 KB vs 128 KB, and the
pull model eliminates Teensy's `AudioMemory(192)` block pool (~50 KB) outright.

> **Retired 2026-08-13.** Step 0 measured **3.7%** audio ISR load on hardware
> (bandlimited saw → SVF lowpass → envelope, XIP build). Track A close-out:
> the **full graph minus delay measures 15–16%** with all voices sounding.
> CPU headroom is a settled question; the delay and second core remain in
> reserve.

---

## 4. Audio port

`musin/audio` runs at 44.1 kHz, 128-sample blocks
([`musin.cmake:139`](../musin/musin.cmake)), on a pull model:
`BufferSource::fill_buffer(AudioBlock&)`.

### 4.1 Inventory

**Already in musin, directly reusable:**

| DUO uses | musin equivalent |
|---|---|
| `AudioFilterStateVariable` | `musin/audio/filter.h` — same PJRC SVF, LP/BP/HP via an `Outputs` struct |
| `AudioEffectBitcrusher` | `musin/audio/crusher.h` |
| `AudioMixer4` | `musin/audio/mixer.h` (templated on N) |
| DSP intrinsics | `musin/audio/dspinst.h` + `port/intrinsics.h` |

**Must be ported for milestone 1** — all PJRC, all int16, all mechanical.
✅ **All ported and running** (`musin/audio/synth_waveform.*`, `synth_dc.*`,
`synth_whitenoise.*`, `synth_simple_drum.*`, `analyze_peak.*`, and
`duo/audio/effect_custom_envelope.*`):

`synth_waveform` (the big one), `synth_dc`, `synth_whitenoise`,
`synth_simple_drum`, ~~`effect_multiply`~~ (included by `Synth.h` but never
instantiated — not ported), `analyze_peak` (drives the DUO's LED3
audio meter; on DRUM, replicate on one of the sequencer button LEDs), plus
Dato's own
[`effect_custom_envelope`](../../duo-imxrt/shared/duo/effect_custom_envelope.cpp),
which currently inherits `AudioStream` and needs reshaping to `BufferSource`.

**Deferred with the delay:** `effect_delay`, `effect_fade`, and the delay's
feedback filter/fader nodes. The 350 ms delay line is ~31 KB of RAM — trivial
to add later; it is the *topology*, not the memory, that is deferred.

Licensing is fine — the Teensy Audio Library is MIT with a funding-notice
clause, and musin already carries it.

### 4.2 [DEFERRED past the gate] The delay feedback loop

`Synth.h` contains a cyclic graph:

```
delay1 → delay_filter → delay_fader → mixer_delay → delay1
```

Teensy's **push** model tolerates this for free: every block boundary inserts
one block of latency, so the cycle resolves naturally. A **pull** graph
recursing through `fill_buffer` around a cycle is infinite recursion.

**Required, when it lands:** an explicit one-block delay node — a
`BufferSource` that returns the previous block's cached output and schedules
the next render — inserted at the cycle break.

**The subtlety:** it must sit at the same point where Teensy's implicit latency
sits. Put it elsewhere and the delay time shifts by one block (2.9 ms) and the
feedback filter's phase response changes.

**Accepted debt.** Deferring this removes the highest-risk item from milestone
1, but it also means **musin's pull model remains unproven against cyclic
graphs** until the delay is built. Nothing in milestone 1 will surface a
problem here. When the delay is picked up, build the one-block node *first, in
isolation*, before wiring any of the delay signal path — it is the only piece
that can invalidate the graph architecture, and that is as true after the gate
as it was before.

### 4.3 [DECIDED] Audio ISR RAM-residency discipline

`AGENTS.md` documents a hard-won constraint: audio renders inside the I2S DMA
IRQ, which stays enabled during flash erase. Every function reachable from that
ISR — **including through vtables** — must be RAM-resident, or the device hard-
faults and watchdog-reboots. Symptom: random reboots ~10 s after interaction.

Every synthesis class ported in §4.1 lands in that call graph.

**The trigger is flash writes, and milestone 1 has none** (§7.2) — so the fault
cannot fire. That is a reason to relax vigilance about *symptoms*, not about
*annotations*: code written without `__not_in_flash_func` now becomes a latent
fault that appears the day persistence is added, far from the change that
caused it. **Annotate as you port.** Budget the disassembly walk once, at the
end of milestone 1, rather than per-class.

Mitigating factor: keep parameter updates on the main-loop side of the fence,
as `synth_update()` already does with `AudioNoInterrupts()`. The float-heavy
setters (`filter1.frequency()`, envelope setters) never need to be ISR-safe.

### 4.4 [DECIDED] Bandlimited waveforms

`Synth.h` branches on `__IMXRT1011__`:

- **RT1011:** `WAVEFORM_BANDLIMIT_SAWTOOTH` / `_PULSE`, `mixer1.gain(0, 0.2f)`
- **Teensy 3.2:** plain `WAVEFORM_SAWTOOTH` / `_PULSE`, `mixer1.gain(0, 0.4f)`

**Take the RT1011 voicing: bandlimited, with `mixer1.gain(0, 0.2f)`.** The
RP2350 is expected to have the headroom, and this keeps the target sound
identical to the shipping DUO — the voicing pass then has a reference rather
than a third variant to reconcile.

Take the gain staging with it. The two branches are a matched pair; porting
bandlimited oscillators at `0.4f` is the one way to get this wrong, and it
propagates through the whole graph.

The bandlimited path is the more expensive of the two, so §3.1's CPU
measurement now covers the worst case — which is the useful number. If it does
come back tight, the plain waveforms are a known-good fallback that costs one
`#define` and a gain change.

---

## 5. [DEFERRED past the gate] Output-stage refactor

Milestone 1 runs on the AIC3204 that is on the Submarine board, through the
existing `#ifdef DATO_SUBMARINE` path, unchanged. The refactor below is
recorded because the analysis is done — but it should not be built until the
codec question (§9) is answered.

**Why this is not a violation of "make the change easy, then make the easy
change."** Preparatory refactoring earns its keep when it makes *the change you
are about to make* easy. Milestone 1 does not add a second DAC; it uses the one
soldered to the board. Building the abstraction now would mean designing an
interface against a single implementation, for a second implementation that
may never be chosen (§7.4) — speculative generality, and the exact failure mode
§7.4 warns about in the opposite direction. The refactor becomes preparatory
work the moment the codec decision says a second output stage is real, and not
before.

### 5.1 The problem

`musin::AudioOutput` is written codec-shaped. All 11 `#ifdef DATO_SUBMARINE`
sites live in [`audio_output.cpp`](../musin/audio/audio_output.cpp); app-side
consumers are three call sites in
[`audio_engine.cpp:230-244`](../drum/audio_engine.cpp). The AIC3204 driver
(~1300 lines) is self-contained in `musin/drivers`.

Two defects:

1. **`DATO_SUBMARINE` is a *board identity* macro gating a *codec capability*.**
   It means "which PCB" where it should mean "is there a codec." Already wrong
   on the DRUM; actively obstructive the moment a second board wants an AIC3204,
   or a DRUM variant wants a dumb DAC.

2. **The `#else` branches are stubs that silently succeed:**
   - `volume()` — *"No codec defined, maybe control digital volume? For now,
     just return true"*
   - `mute()` / `unmute()` — sets `is_muted` and nothing else
   - `headphones_inserted()` — returns `nullopt`

   A stub returning `true` is a trap for whoever brings up the next board.

Note that milestone 1 does not *hit* these stubs — it takes the codec branch
throughout. The defect is real but not on the critical path.

### 5.2 The refactor, when it happens

Replace the ifdef ladder with a small output-stage interface:

- `init()` **that can fail** — an AIC3204 with a dead I²C bus is a dead product
- `volume(float)`
- `mute()` / `unmute()`
- `headphones_inserted()` → `std::optional<bool>`

Two implementations: **`Aic3204OutputStage`** (lift the current
`#ifdef DATO_SUBMARINE` bodies in verbatim) and **`Pcm5100OutputStage`** (GPIO
amp-enable + HP-enable + HP-detect, optionally the PCM5100A `XSMT` mute pin,
volume applied digitally in the graph). Select via a
`MUSIN_AUDIO_OUTPUT_STAGE` config, **not** board identity.

### 5.3 Digital volume on a dumb DAC

The DUO already solves this and always has. `audio_volume()` in `Synth.h`
scales `mixer_output` gains from `AMP_POT`, with a low-volume threshold that
hard-zeros the gains. Headphone detect is a plain GPIO (`HP_DETECT_PIN`); mute
is the amp/HP enable GPIOs in
[`audio.cpp`](../../duo-imxrt/brains2/core/boards/DUO_BRAINS_2.3/audio.cpp) —
including a neat runtime polarity autodetect on the amp mute line, worth
preserving.

Open when this is picked up: should `AudioOutput::volume()` forward to a
graph-level gain, or be explicitly unsupported (returning failure) on dumb-DAC
boards? Prefer whichever is *loud* about the limitation. Do not keep the silent
`true`.

---

## 6. Control mapping — DRUM surface → DUO parameters

**[DECIDED]** This is the defining shape of milestone 1: not a subset of the
DUO's panel, but a deliberate remap of DRUM's panel onto DUO parameters.

Two fits are exact rather than approximate, which is what makes this workable:

- DUO has **8 sequencer steps** ([`seq.h:9`](../../duo-imxrt/shared/duo/seq.h))
  → one DRUM track row is 8 buttons.
- DUO's keyboard is **exactly 10 keys** — `SCALE[]` has 10 entries
  ([`globals.h:9`](../../duo-imxrt/brains2/apps/duo/globals.h)),
  `KEYB_0..KEYB_9` in
  [`buttons.h:35`](../../duo-imxrt/brains2/apps/duo/buttons.h) → 8 + 2.

### 6.1 Buttons and pads

| DUO control | DRUM control | Notes |
|---|---|---|
| `SEQ_START` | Play button | 1:1 |
| `STEP_1..STEP_8` | Track 4 (inner) row | Exact fit |
| `KEYB_0..KEYB_7` | Track 1 (outer) row | |
| `KEYB_8`, `KEYB_9` | 2 buttons of Track 2 | Keyboard overflow |
| `BTN_DOWN`, `BTN_UP` | Remaining Track 2 buttons | Octave; kept on the keyboard row |
| `BTN_SEQ1` | `REPEAT` | Pressure-sensitive button, `pizza_controls.cpp:403` |
| `BTN_SEQ2` | `RANDOM` | Pressure-sensitive button, `pizza_controls.cpp:402` |
| Kick pad | Drumpad 1 | |
| Hi-hat pad | Drumpad 2 | |
| — | Drumpads 3, 4 | **Unused** in milestone 1 |
| — | Track 3 row | **Unmapped** |

The DUO reads its pads as *digital* through the analog muxes via
`muxDigitalRead` ([`pins.h`](../../duo-imxrt/brains2/core/lib/pins.h)). DRUM's
`musin::ui::Drumpad` is pressure-sensitive — a superset. Milestone 1 can simply
threshold to a gate; velocity is available if the drum voices want it.

### 6.2 Analog controls

| DUO pot | DRUM control |
|---|---|
| `TEMPO_POT` | `SPEED` |
| `AMP_POT` | `VOLUME` |
| `OSC_DETUNE_POT` | Pitch slider 1 |
| `OSC_PW_POT` (waveform) | Pitch slider 2 |
| `FILTER_FREQ_POT` | Pitch slider 3 |
| `AMP_ENV_POT` (release) | Pitch slider 4 |
| `FILTER_RES_POT` | **Homeless** |
| `GATE_POT` | **Homeless** |

Unmapped on the DRUM side: `FILTER`, `CRUSH`, `SWING`.

**[DECIDED] The two homeless pots stay homeless.** In particular
`FILTER_FREQ_POT` is on a pitch slider and *not* on DRUM's `FILTER` control:
that control drives frequency and resonance from a single value, inversely
coupled (`pizza_controls.cpp:483-485`), which does not decompose into the DUO's
two independent pots.

### 6.3 LEDs

DUO drives 20 LEDs; DRUM has 37 (`NUM_LEDS`, `drum_pizza_hardware.h`) in a
different physical arrangement — 8 steps × 4 tracks plus 4 pads plus play.
`Leds.h` indexes physically (`physical_leds[i+9]` for the keyboard, etc.), so
porting it off FastLED (§7.1) and re-indexing onto DRUM's `LED_ARRAY` are the
same job. Do them together, driven by §6.1's mapping.

### 6.4 [DEFERRED past the gate] Real DUO hardware mux budget

For the record, against
[`dato_submarine.h`](../musin/boards/dato_submarine.h): the DUO's own control
surface is **8 pots + 14 digital-via-mux = 22 signals**, while
`AnalogMuxScanner` is hardcoded to one ADC pin and `NUM_CHANNELS = 16`
([`analog_mux_scanner.h:12`](../musin/hal/analog_mux_scanner.h)). The DUO
Brains uses **three** mux I/O lines sharing three address lines — effectively
24 channels.

**This does not bite milestone 1**, which uses DRUM's surface, not the DUO's.
It becomes real when a DUO-on-RP2350 PCB exists: generalise `AnalogMuxScanner`
to N mux inputs sharing address lines. Contained change; noted here so it is
not discovered when pads don't respond.

### 6.5 Peripheral inventory

| DUO peripheral | RP2350 / musin path | Effort |
|---|---|---|
| SK6812 LEDs (FlexIO+DMA) | `musin/drivers/ws2812.pio` + `ws2812-dma.h` | Low — exists, see §6.3 |
| Pots via analog muxes | `musin/hal/analog_mux_scanner` | Low — **DUO already muxes** |
| Sync in / out / detect | `musin/timing/sync_in.cpp`, `sync_out.cpp` | Low |
| MIDI DIN (UART) + USB MIDI | `musin/midi/*`, TinyUSB | Low — both projects use TinyUSB and the same MIDI libs |
| Button matrix (LGPL `Keypad`) | `musin/ui/keypad_hc138` | Medium — different topology; **removes an LGPL dep** |
| PT8211 / PCM5100A | §5 — deferred | — |
| ~~BS814A cap-touch~~ | **Out of scope — not used** | — |
| ~~EEPROM~~ | **Out of scope — not used** | — |

---

## 7. Decisions

### 7.1 [DECIDED] Drop Arduino, go musin-native — ✅ done

`duo/` has no Arduino dependency. `Leds.h` became
`duo/ui/duo_display.cpp` on `musin::ws2812`, re-indexed onto DRUM's
`LED_ARRAY` in the same pass as predicted (§6.3), carrying the FastLED-derived
colour maths (`fade_light_by`, `blend`) across as portable C++.

Concrete cost, enumerated honestly:

| Arduino dependency | Replacement | Notes |
|---|---|---|
| `millis()` / `micros()` / `delay()` | `to_ms_since_boot(get_absolute_time())`, `time_us_32()`, `sleep_ms()` | Trivial per site, touches nearly every file |
| `map()` | One `constexpr` helper | Trivial |
| `elapsedMillis` | Drop — musin uses `absolute_time_t` | Trivial |
| **`FastLED`** | `musin::ws2812` | **The real chunk** — `Leds.h` is 172 lines of `CRGB` arithmetic, plus the §6.3 re-index |
| `Keypad` (LGPL) | `musin::ui::Keypad_HC138` | Also removes LGPL from the tree |
| `AudioStream` | `BufferSource` | In `effect_custom_envelope` |
| `Serial.begin(31250)` | musin MIDI UART | Trivial |
| `MIDI.h` | **Survives untouched** | musin vendors the same FortySevenEffects lib |

**FastLED mitigation:** `brains2/core/lib` already vendors the FastLED-derived
`color.h`, `hsv2rgb`, `pixeltypes.h` and `lib8tion`. These are portable C++ and
can come across on top of `musin::ws2812` — you are replacing the *driver*, not
the colour maths.

### 7.2 [DECIDED] No flash persistence in milestone 1

EEPROM is unused today; the MIDI channel is hardcoded to `1` in `main_init()`
with the EEPROM calls commented out. So persistence is a **new feature
decision**, not a port task.

Shipping the current behaviour costs nothing and **means the audio ISR never
races a flash erase**, so §4.3's fault cannot fire during milestone 1 — with
the annotation caveat in that section. Adopting DRUM's littlefs
`settings_manager` is what drags the discipline in; revisit past the gate.

### 7.3 [DECIDED] Adopt `musin::timing`, retire DUO's `TempoHandler`

DRUM's timing stack (`clock_router`, `internal_clock`, `speed_adapter`,
`sync_in/out`, `midi_clock_processor`, and its own
`musin/timing/tempo_handler`) is strictly more capable than DUO's single
`TempoHandler` class, and it is where the two products converge.

**Naming hazard:** musin already has a class called `TempoHandler`
(`musin/timing/tempo_handler.h`) that is *not* the DUO's
`shared/duo/TempoHandler.h`. "Retire DUO's TempoHandler" means deleting the
DUO one and wiring the sequencer to musin's timing stack — musin's
`TempoHandler` is the closest counterpart and likely the integration point.

**Two behaviours to verify are preserved** — do not assume:

1. The tempo pot nudging `speed_mod` when the clock source is **external**
   (`Sequencer.h`, `sequencer_update()`). `speed_adapter` looks like the right
   home. Note this rides on `SPEED` per §6.2.
2. **Volca sync-pulse interpolation.** *Not confirmed present in musin.* Check
   before committing.

### 7.4 [DEFERRED past the gate] PCM5100A vs AIC3204

Milestone 1 develops against the AIC3204 because it is what is on the Submarine
board. That is a hardware fact, not yet a product decision.

The arguments, preserved for the gate:

*For AIC3204:* strictly the harder path and already written and shipping — init
that can fail, an I²C bus to get wrong, register state, a real bring-up
sequence. An abstraction that survives it accepts a PCM5100A trivially
underneath. Designed the other way round, you get an interface with no concept
of failure and retrofit it.

*For keeping PCM5100A as a populate option:* the README documents that **Brains
2 exists because the K20DX256 became unavailable.** Eliminating the second
source for the audio output stage, one generation after being forced into a
platform migration by a sourcing problem, deserves to be a conscious trade. The
AIC3204 is the more expensive and more supply-constrained part, and DUO is the
higher-volume, more cost-sensitive product. A PCM5100A also cannot meaningfully
fail to initialise.

> ⚠️ **Caveat that gates everything above.** This assumes AIC3204-on-DUO is
> being considered for *feature* reasons (analog volume, built-in HP detect,
> line-in bypass, BOM commonality). If there is an existing **blocker** with the
> PCM5100A — a noise floor measurement, a click/pop issue, something in the
> analog design — that outweighs it and the answer flips. **Confirm at the
> gate.**

### 7.5 PCM5100A hardware notes

- Needs **no MCLK** (internal PLL) — just BCK / LRCK / DATA, exactly what
  `pico_audio_i2s` emits. Clean match.
- ⚠️ Check `PICO_AUDIO_I2S_CLOCK_PINS_SWAPPED=1`
  ([`musin.cmake:171`](../musin/musin.cmake)) against the BCK/LRCK wiring —
  that flag is set for the DRUM board.
- **[OPEN] Stereo vs mono.** The PCM5100A is stereo; musin builds
  `PICO_AUDIO_I2S_MONO_INPUT=1`. The DUO currently uses L and R as
  *independently gained* headphone and speaker preamps. Either un-mono the I²S
  path (more ISR work per block) or collapse to mono and switch destinations
  with the amp/HP GPIOs. **Recommend mono** — closer to what the DRUM does —
  unless the separate gain staging is doing something worth keeping.

### 7.6 [DECIDED] Minimal-change port of `shared/duo/`

`Sequencer`, `seq.cpp`, `note_stack` and `Pitch` come across **close to as-is**
— globals, `#define keyboard_set_note(note)` macros and all. The goal of
milestone 1 is proven DUO behaviour on new hardware, and every deviation is a
place behaviour can change silently, which the post-gate voicing pass would
then have to disentangle from genuine porting bugs.

This is a conscious trade. `duo/` will not initially read like `drum/`'s newer
code — dependency-injected, `etl::observer`-based, small classes — and the
cleanup becomes a second pass someone has to fund. Two things make that
acceptable: the code being imported is small and well-bounded, and the
milestone's whole purpose is to produce evidence for the gate (§9), where the
question of how `duo/` and `drum/` should share structure gets answered
properly rather than guessed at now.

**The exception is the glue.** `Leds.h` and the button handling in `main.cpp`
are being rewritten against DRUM's hardware regardless — the FastLED removal,
the LED re-index (§6.3) and the §6 control mapping leave little of the original
standing. There is nothing to preserve there, so write those in `drum/`'s
style. The line is: **behaviour-critical and portable → minimal change;
platform glue → reshape.**

Note that milestone 1 ports without first migrating DUO's Unity test suite to
Catch2 (§11). That raises the cost of getting the minimal-change port wrong,
which is a further argument for keeping the deviations near zero.

---

## 8. Sequencing

### Milestone 1 — duo-on-drum — ✅ **substantially complete**

Lives in `duo/`, a sibling app to `drum/` sharing `musin/`.

Progress, as of 2026-08-13:

| | Status |
|---|---|
| Step 0 — walking skeleton + CPU measurement | ✅ done — 3.7% on the partial graph |
| Track A — audio graph (delay excluded) | ✅ done — 16.0–16.4% on the full graph |
| Track B — sequencer, timing, LEDs, control mapping | ✅ done — played by hand and confirmed (§12.1) |
| MIDI I/O | ✅ done — notes, CC, clock, transport confirmed; SysEx untested (§12.4) |
| Close-out — ISR RAM-residency disassembly walk | ❌ **not done** (§4.3, §12.3) |

The "done when" criterion below is met. The close-out walk is the only
outstanding milestone-1 work.

#### Step 0 — walking skeleton: one key, one note ⭐ ✅

Before any batch porting, get a single thin slice working end to end:

1. Port `synth_waveform` (bandlimited, §4.4) and `effect_custom_envelope` —
   two of the seven classes.
2. Chain them through the **existing** `musin/audio/filter.h` to the AIC3204.
3. Drive it from **one button** on the Track 1 row, through `Keypad_HC138` and
   the §6.1 mapping.
4. **Measure ISR CPU load here**, on this partial graph.

**Why this comes first.** Risk #1 — CPU headroom — is the only thing that can
still change the feasibility answer, and measuring it after all seven classes
are ported means paying the full porting cost before learning whether the
budget holds. On the bandlimited voicing a single oscillator plus envelope is a
meaningful fraction of the final load; if it comes back alarming, §4.4's plain
waveforms are still a live fallback rather than sunk work.

It also forces one **early integration** between the two tracks below. As
written they barely touch and would otherwise meet for the first time at
close-out, which is where mapping and graph assumptions would collide with the
least time left to absorb it.

Everything after this step is repeating a proven shape rather than discovering
one.

**Track A — audio graph** ✅

1. Port the remaining PJRC classes (§4.1) — `synth_dc`, `synth_whitenoise`,
   `synth_simple_drum`, `effect_multiply`, `analyze_peak` — delay excluded,
   annotating for RAM residency as you go (§4.3).
2. Wire up the full DUO patch minus the delay; trigger notes over USB MIDI.
3. Re-measure CPU load against the step-0 number.

**Track B — application** (can run in parallel with Track A) ✅ built, unplayed

1. Port `shared/duo/` sequencer + note stack, minimal-change per §7.6
   (near-free — `<cstdint>` only).
2. Wire to `musin::timing` (§7.3), verifying the two behaviours listed there.
3. Port `Leds.h` off FastLED **and** re-index to DRUM's `LED_ARRAY` (§6.3,
   §7.1) — reshaped, per §7.6's glue exception.
4. Implement the rest of the §6 control mapping on `Keypad_HC138` +
   `AnalogMuxScanner` + `Drumpad`.

**Close-out:** one disassembly walk per `AGENTS.md` to confirm no ISR-reachable
function sits in flash. ❌ **Still outstanding** — see §12.3.

**Done when:** the DUO plays, sequences and responds to DRUM's panel per §6,
with a measured CPU number. ✅ **met** — confirmed by hand on 2026-08-13.

### The gate — re-evaluate (§9)

### After the gate, in rough order

- **Delay feedback loop** (§4.2) — one-block node first, in isolation.
- **Output-stage refactor** (§5), once the codec question is answered.
- **Real DUO-on-RP2350 board** — generalise the mux scanner (§6.4), PCM5100A
  output stage if retained, sync, MIDI DIN, power button and soft power-off.
- **Update, persistence, tooling** — A/B partitions +
  `musin/flash/firmware_writer.cpp` + SysEx update, replacing the i.MX ROM DFU
  path. **`tools/updater` gets rewritten**, as does factory and production test
  tooling. Revisit §7.2 here, and §4.3 becomes live.
- **Voicing pass** against a reference DUO. §4.4 and the delay-latency question
  both land here. Budget real time; subjective and iterative.

---

## 9. The gate — what milestone 1 exists to inform

These are deliberately unanswered now. Answering them with a working
duo-on-drum in hand is the point.

1. **Does `musin` need to expand to cover the DUO's needs?** Milestone 1 will
   have produced a concrete list: the ported synthesis classes, whatever the
   control mapping forced, the N-mux scanner generalisation (§6.4). Decide then
   what is genuinely shared infrastructure versus app-specific.
2. **How do we handle two apps and code duplication in one repo?** `drum/` and
   `duo/` will have visibly overlapping sequencer, display and control code.

   Milestone 1 **deliberately duplicates rather than unifies.** This is a
   position, not procrastination: duplication is cheaper than the wrong
   abstraction, and until `duo/` exists there is only one consumer, so any
   shared interface extracted now would be shaped entirely by `drum/`'s needs
   and fitted to `duo/` by force. The duplication is time-boxed to this gate —
   with both apps working, the seams that are genuinely shared become visible
   and can be extracted from evidence.
3. **Should `musin` split into its own repository?** Only answerable once (1)
   and (2) are, and once there is a second real consumer to prove the seams.
4. **Does the DUO go forward with the AIC3204?** (§7.4, including its caveat.)
   This gates the output-stage refactor (§5) and the DUO PCB.
5. **Is DUO-on-RP2350 a product, or a platform-convergence exercise?** Bears on
   how aggressively to share with `drum/` (2). Note that milestone 1 has
   already taken the conservative branch of this question for its own purposes
   — see §7.6 — so answering it at the gate costs nothing that has been
   foreclosed.

---

## 10. Risk register (milestone 1)

Outcomes added 2026-08-13.

| # | Risk | Severity | Mitigation | Outcome |
|---|---|---|---|---|
| 1 | ISR CPU load exceeds budget | High | Measured at step 0 on a partial graph, before the porting cost is sunk, and again at the end of Track A. Bandlimited voicing (§4.4) means it is the worst case. Brains-1-on-72 MHz-M4 precedent suggests it will be fine; second core in reserve, and plain waveforms are a fallback | ✅ **Retired.** 3.7% partial, 16.0–16.4% full. Never close to the budget; the fallback was not needed |
| 2 | Voicing doesn't match reference DUO | Medium | §4.4 settled on the shipping RT1011 voicing; voicing pass is post-gate | Open — the panel is confirmed playable (§12.1), but no A/B against a reference DUO has been done. Post-gate |
| 3 | RAM-residency annotations omitted, faulting later | Medium | Cannot fire in milestone 1 (§7.2), which is exactly why it gets missed. Annotate as you port; one disassembly walk at close-out | ⚠️ **Live.** Predicted correctly: the walk was skipped (§12.3) |
| 4 | Pull model unproven against cyclic graphs | Medium | Accepted debt (§4.2). Nothing in milestone 1 surfaces it | Open, unchanged — accepted debt; milestone 1 surfaced nothing, as predicted |
| 5 | Minimal-change port (§7.6) imports DUO's coupling into `duo/`, and the style cleanup never gets funded | Medium | Deliberate trade for behavioural safety, and the imported code is small and bounded. Sharpened by porting without the Unity suite (§11) — keep deviations near zero so behaviour changes stay attributable | Open — `duo/` does carry the imported coupling; cleanup unfunded |
| 6 | LED re-index diverges from control mapping | Low | Do §6.3 and §6.1 as one job | ✅ Retired — done as one job in `duo_display.cpp`; confirmed by playing it (§12.1) |
| 7 | RP2350 erratum **E9** — GPIO input latching when high-Z with internal pulldowns | Low | Use pull-ups (the matrix already does); audit any high-Z input | ✅ Not hit — keypad matrix uses pull-ups |
| 8 | RP2350 **E10** | Low | Already handled — `PICO_RP2350_A2_SUPPORTED ON` in `drum/CMakeLists.txt` | ✅ Retired — `PICO_RP2350_A2_SUPPORTED ON` also set in `duo/CMakeLists.txt` |
| 9 | Volca sync interpolation missing from `musin::timing` | Low | Verify per §7.3 | ⚠️ **Live** — still unverified (§11) |

---

## 11. What was not verified

Stated plainly so nobody over-trusts this document. Updated 2026-08-13 — the
first two items are now retired.

- ~~**No code was run or built.**~~ Milestone 1 is built and running on
  hardware.
- ~~**No CPU measurement exists** for the DUO graph on RP2350 (risk #1).~~
  3.7% partial, 16.0–16.4% full graph minus delay. **Not re-measured since
  MIDI landed** — the per-frame CC scan and queue drain are small but not
  free.
- ~~**MIDI I/O has not been exercised against a host**~~ — notes, CC,
  transport and clock confirmed against a host (§12.4). **SysEx is still
  unexercised**, including the shortened serial number.
- **Schematics were not consulted** — hardware claims come from firmware pin
  definitions and board headers, which can drift from the actual PCB. The pin
  map is now indirectly confirmed for LEDs, keypad, mux, I²C and codec, since
  those all function.
- **`musin::timing` feature parity with `TempoHandler` was not fully checked** —
  only the two behaviours in §7.3 were identified as needing verification. Of
  those, the external-clock `speed_mod` nudge is implemented
  (`sequencer_update()` in `duo/main.cpp`) but was unreachable until §12.4
  added the missing `update_auto_source_switching()` call, and is still
  untested; **Volca sync-pulse interpolation is still unverified.**
- **The DUO's own test suite** (`shared/duo/test/`, Unity) was not assessed for
  migration to DRUM's Catch2 host-test setup. Still true, and §7.6's risk #5
  stands: nothing regression-tests the minimal-change port.
- ~~**The DRUM control mapping (§6) has not been tried by hand.**~~ Played and
  confirmed on 2026-08-13 — the outer ring does read as a keyboard and the
  remap is playable (§12.1). **Voicing parity is a separate question and is
  still untested** (§4.4, risk #2).

---

## 12. Current status and what remains before the gate

Written 2026-08-13, after bring-up. Bring-up debugging detail lives in
[`duo-boot-debug.md`](duo-boot-debug.md).

### 12.1 ✅ Control mapping confirmed by hand

**Played on hardware 2026-08-13: the §6 mapping works.** The open question was
never the counts — those were exact by construction — but whether the outer
ring reads as a keyboard and the remap is playable. It does. No mapping
decision in §6 needs revisiting, including the two homeless pots and the
deliberate choice to keep `FILTER_FREQ_POT` off DRUM's coupled `FILTER`
control (§6.2).

Note this validates the *mapping*, not the *voicing*. Audible parity against a
reference DUO (§4.4, risk #2) is a separate post-gate exercise and remains
untested.

### 12.2 The one real surprise: hardcoded PIO/DMA resources in `musin`

Not predicted by this document, and it cost the whole bring-up. `audio_i2s_setup()`
takes `pio_sm 0` and `dma_channel 0` from a **hardcoded** `i2s_config` in
`musin/audio/audio_output.cpp` and claims them with panic-on-conflict calls,
while `WS2812_DMA::init()` takes the lowest free DMA channel. Initialising the
display before the audio therefore panics; in a Release build with no host
attached that presents as a completely dead board.

`drum/` works only **by accident of ordering** — `audio_engine.init()` happens
to precede `pizza_display.init()`. Nothing documents or enforces it.

Fixed in `duo/` by reordering. The underlying fragility is deliberately left
alone because the fix touches DRUM's working audio path — **it is now the most
concrete input to gate question §9.1** ("does musin need to expand"), and it is
evidence of a specific kind: not a missing feature, but an implicit
initialisation contract that only a second consumer could reveal. That is
precisely what milestone 1 existed to produce.

### 12.3 Close-out: the RAM-residency disassembly walk

§4.3 budgeted one walk at the end of milestone 1; it has **not been done**.
Risk #3 predicted this would be the item that gets missed, because milestone 1
never writes flash and so cannot fire the fault. It was right. Do the walk
before persistence is added, not after — the annotations are latent until then.

### 12.4 ✅ MIDI I/O wired

`shared/duo/MidiFunctions.h` is ported to `duo/midi_functions.h` and wired into
`duo/main.cpp`. §6.5 rated this Low effort and that held — it needed no new
musin code. As in the reference, it is a definition-carrying header included
from the middle of `main.cpp`'s anonymous namespace, after `synth`,
`transpose`, `note_off()` and `sequencer` exist (§7.6 minimal-change).

What is wired:

- **Note in** → `sequencer.hold_note()` / `release_note()`; **note out** from
  `note_on()` / `note_off()`, so panel and sequencer notes echo to the host.
- **CC out** — the full §MidiFunctions chart (7, 65, 70, 71, 72, 74, 80, 81,
  94), change-detected, sent once per 11 ms frame as the reference does.
  `resonance`, `glide`, `delay` and `crush` are pinned by §6.2's homeless pots,
  so those simply never change.
- **CC in** — 123 All Notes Off.
- **Clock in** → `MidiClockProcessor`, plus `clock_router
  .update_auto_source_switching()` now called in the loop. That call was
  missing, so **sync-in auto-switching was also dead**, and with it §7.3's
  external-clock `speed_mod` nudge, which can only fire on a non-internal
  source.
- **Clock out** → `musin::timing::MidiClockOut`, observing **`clock_router`,
  not `speed_adapter`**: the router carries the raw 24 PPQN, while the adapter
  runs at `DOUBLE_SPEED` to feed the sequencer and so carries the sequencer's
  rate, not the wire rate. Constructed with
  `send_when_stopped_as_master = true` to match the retired DUO
  `TempoHandler`, which sent clock from `trigger()` whenever the source was
  not MIDI, running or not. `MidiClockOut` suppresses the MIDI-source case
  itself, so an external clock is not echoed back, and it additionally
  **bridges `EXTERNAL_SYNC` to MIDI clock out** — as the reference did.

  This was the one gap the first pass missed. The DUO's clock output lived
  inside `shared/duo/TempoHandler.h`'s `trigger()`, so retiring that class
  (§7.3) removed it, and unlike notes, CC and transport it had no other send
  site to survive in. A reminder that §7.3's "retire DUO's TempoHandler" moved
  more behaviour than its name suggests.
- **Transport** — Start/Continue/Stop in; Continue/Stop plus CC 123 out on
  panel start/stop, as the reference. **One deliberate deviation:** received
  realtime is *not* echoed back out. The reference does echo, which was
  harmless across Brains 2's separate DIN and USB ports but would return a
  DAW's own Start to it here.
- **SysEx** — firmware version, identity reply, reset transpose, reboot to
  bootloader (`reset_usb_boot`). Serial number keeps the 24-byte wire format
  of 4 groups of 5 right-aligned 7-bit values, but the RP2350's board id is
  64 bits where the i.MX's was 128, so **the leading two groups are always
  zero** — anything parsing DUO serials needs to tolerate that. Selftest is a
  no-op stub; there is no selftest on this surface.

`MIDI_CHANNEL` remains a hardcoded `1`, matching the reference's
`main_init()`. Persistence for it is a post-gate decision (§7.2).

**Confirmed on hardware 2026-08-13:** notes, CC, transport and clock all work
against a host.

### 12.5 Flashing notes

- Both flash partitions on the development unit currently hold `duo` images;
  the DRUM is no longer on it.
- Upload **must** target partition 0 explicitly. DUO and DRUM both declare
  binary version 1.0, so the bootrom has no tiebreak and always boots partition
  0; a bare `picotool load -f` is treated as an A/B update and lands in
  partition 1, which is never selected. `duo/build.sh` handles this.
- Now that the firmware services USB, `duo/build.sh` can force BOOTSEL itself
  — no manual replugging for normal iteration.

### 12.6 Unchanged by bring-up

Everything past the gate is untouched: the delay feedback loop (§4.2) and its
one-block node, the output-stage refactor (§5), the N-mux generalisation (§6.4),
persistence and update tooling (§7.2), and the voicing pass. The gate questions
in §9 are all still open, now with §12.2 as real evidence for the first one.
