// duo-on-drum milestone 1: full DUO synth graph (minus delay) plus the DUO
// sequencer/arpeggiator, driven by musin::timing and the §6 control mapping.
//
// Prints audio ISR CPU load once per second.

// Bring-up aid: set to 1 to light one more LED after each init step, so a dark
// panel localises the hang instead of only proving the main loop was never
// reached. Bit-banged, so it works before any driver is up. Set to 0 for
// normal builds — while enabled it owns the LED data pin and the panel shows
// the beacon rather than the DUO's display.
#define DUO_BOOT_BEACON 0

#include "duo/audio/effect_custom_envelope.h"
#include "duo/seq.h"
#include "duo/ui/boot_beacon.h"
#include "duo/ui/duo_display.h"
#include "musin/audio/analyze_peak.h"
#include "musin/audio/audio_output.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/audio/synth_dc.h"
#include "musin/audio/synth_simple_drum.h"
#include "musin/audio/synth_waveform.h"
#include "musin/audio/synth_whitenoise.h"
#include "musin/hal/analog_mux_scanner.h"
#include "musin/hal/null_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_out.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_handler.h"
#include "musin/ui/keypad_hc138.h"
#include "musin/usb/usb.h"

#include "etl/array.h"
#include "etl/observer.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "pico/rand.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"
}

#include "duo/synth_params.h"

namespace {

// =====================================================================
// Hardware maps (same Submarine surface as the DRUM)
// =====================================================================
const etl::array<uint32_t, 3> keypad_decoder_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN,
    DATO_SUBMARINE_MUX_ADDR2_PIN};
const etl::array<uint32_t, 5> keypad_columns_pins = {
    DATO_SUBMARINE_KEYPAD_COL1_PIN, DATO_SUBMARINE_KEYPAD_COL2_PIN,
    DATO_SUBMARINE_KEYPAD_COL3_PIN, DATO_SUBMARINE_KEYPAD_COL4_PIN,
    DATO_SUBMARINE_KEYPAD_COL5_PIN};
const etl::array<uint32_t, 4> analog_address_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN,
    DATO_SUBMARINE_MUX_ADDR2_PIN, DATO_SUBMARINE_MUX_ADDR3_PIN};
constexpr uint8_t KEYPAD_ROWS = 8;
constexpr uint8_t KEYPAD_COLS = 5;

// Mux channels (drum_pizza_hardware.h control IDs)
enum MuxChannel : uint8_t {
  MUX_DRUM1 = 0,
  MUX_DRUM2 = 2,
  MUX_PITCH1 = 3,
  MUX_PITCH2 = 4,
  MUX_PLAYBUTTON = 5,
  MUX_RANDOM = 6,
  MUX_VOLUME = 7,
  MUX_PITCH3 = 8,
  MUX_REPEAT = 12,
  MUX_SPEED = 14,
  MUX_PITCH4 = 15,
};

// =====================================================================
// DUO globals (duo-imxrt globals.h, minimal-change per §7.6)
// =====================================================================
constexpr etl::array<uint8_t, 10> SCALE = {49, 51, 54, 56, 58,
                                           61, 63, 66, 68, 70};
constexpr uint8_t MIDI_CHANNEL = 1;

float osc_saw_frequency = 0.f;
float osc_pulse_frequency = 0.f;
float osc_pulse_target_frequency = 0.f;
uint8_t osc_pulse_midi_note = 0;
int transpose = 0;
bool random_flag = false;
bool double_speed = false;
uint8_t note_is_playing = 0;

synth_parameters synth;

// Namespace scope so the MIDI output queue drain and the keypad share one
// logger, and so it outlives everything that borrows it.
musin::NullLogger logger;

constexpr long map_range(long x, long in_min, long in_max, long out_min,
                         long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float midi_note_to_frequency(float note) {
  return 440.0f * std::exp2((note - 69.0f) / 12.0f);
}

// =====================================================================
// Audio graph (Synth.h + DrumSynth.h, delay omitted) — see Track A
// =====================================================================
struct LoadMeter : ::BufferSource {
  explicit LoadMeter(::BufferSource &inner) : inner(inner) {
  }

  __attribute__((section(".time_critical.LoadMeter_fill_buffer"))) void
  fill_buffer(::AudioBlock &out_samples) override {
    const uint32_t start = time_us_32();
    inner.fill_buffer(out_samples);
    busy_us = busy_us + (time_us_32() - start);
    blocks = blocks + 1;
  }

  ::BufferSource &inner;
  volatile uint32_t busy_us = 0;
  volatile uint32_t blocks = 0;
};

musin::audio::SynthWaveform osc_saw;
musin::audio::SynthWaveform osc_pulse;
musin::audio::SynthDc dc1;
duo::audio::CustomEnvelope envelope2(dc1); // filter envelope
musin::audio::AudioMixer<2> mixer1(&osc_saw, &osc_pulse);
musin::audio::LowpassModulated filter1(mixer1, envelope2);
duo::audio::CustomEnvelope envelope1(filter1); // amp envelope
musin::audio::AnalyzePeak peak1(envelope1);    // DUO LED3 meter tap
musin::audio::Crusher bitcrusher1(peak1);

musin::audio::SynthSimpleDrum kick_drum1;

musin::audio::SynthNoiseWhite hat_noise1;
duo::audio::CustomEnvelope hat_envelope1(hat_noise1);
musin::audio::Highpass hat_filter_hp(hat_envelope1);
musin::audio::Bandpass hat_filter_bp(hat_filter_hp);
musin::audio::SynthSimpleDrum hat_snappy;
musin::audio::AudioMixer<2> hat_mixer(&hat_filter_bp, &hat_snappy);

// Channel 1 is the delay return, silent until the delay lands post-gate.
musin::audio::AudioMixer<4> mixer_output(&bitcrusher1, nullptr, &kick_drum1,
                                         &hat_mixer);
musin::audio::AnalyzePeak peak2(mixer_output);
LoadMeter load_meter(peak2);

// Output mixer gains (Brains 2 board_audio_output.h defaults).
constexpr float MAIN_GAIN = 0.8f;
constexpr float DELAY_GAIN = 1.0f;
constexpr float KICK_GAIN = 1.0f;
constexpr float HAT_GAIN = 1.2f;

// --- Synth.h audio_init, RT1011 branch (§4.4 voicing) ---
void audio_init() {
  mixer1.gain(0, 0.2f); // OSC1
  osc_saw.begin(0.4f, 110, WAVEFORM_BANDLIMIT_SAWTOOTH);
  osc_pulse.pulseWidth(0.5f);
  osc_pulse.begin(.5f, 220, WAVEFORM_BANDLIMIT_PULSE);
  mixer1.gain(1, 0.4f); // OSC2

  filter1.filter.resonance(0.7f); // range 0.7-5.0
  filter1.filter.frequency(400);
  filter1.filter.octave_control(4);

  envelope1.attack(2);
  envelope1.decay(0);
  envelope1.sustain(1.0f);
  envelope1.release(400);

  dc1.amplitude(1.0); // Filter env needs an input signal
  envelope2.attack(15);
  envelope2.decay(0);
  envelope2.sustain(1.0f);
  envelope2.release(300);

  bitcrusher1.bits(16);
  bitcrusher1.sampleRate(44100);

  mixer_output.gain(0, MAIN_GAIN);
  mixer_output.gain(1, DELAY_GAIN);
  mixer_output.gain(2, KICK_GAIN);
  mixer_output.gain(3, HAT_GAIN);
}

// --- Synth.h audio_volume ---
void audio_volume(int volume) {
  static const int LOW_VOLUME_THRESHOLD = 4;

  if (volume < LOW_VOLUME_THRESHOLD) {
    mixer_output.gain(0, 0);
    mixer_output.gain(1, 0);
    mixer_output.gain(2, 0);
    mixer_output.gain(3, 0);
  } else {
    mixer_output.gain(0, (volume / (1023.f / MAIN_GAIN)));
    mixer_output.gain(1, (volume / (1023.f / DELAY_GAIN)));
    mixer_output.gain(2, ((volume + 512) / (2048.f / KICK_GAIN)));
    mixer_output.gain(3, ((volume + 512) / (2048.f / HAT_GAIN)));
  }
}

// --- DrumSynth.h, near-verbatim ---
void drum_init() {
  hat_envelope1.attack(2);
  hat_envelope1.release(0);
  hat_envelope1.sustain(0.0f);
  hat_filter_bp.filter.frequency(4000);
  hat_filter_hp.filter.frequency(6000);
  hat_snappy.length(30);
  hat_snappy.pitchMod(4.0f);
  hat_snappy.frequency(126);

  kick_drum1.length(100);
  kick_drum1.frequency(60);
  kick_drum1.pitchMod(4.0f);
}

void kick_noteon(uint8_t velocity) {
  const int kick_duration = 200 - velocity;
  // sidechain on the pulse osc
  osc_pulse.amplitude(0.35f);
  kick_drum1.length(kick_duration);
  kick_drum1.frequency(velocity / 4 + 40);
  kick_drum1.noteOn();
}

void kick_noteoff() {
  osc_pulse.amplitude(0.4f);
}

void hat_noteon(uint8_t velocity) {
  if (velocity > 63) {
    hat_snappy.noteOn();
  }

  hat_noise1.amplitude(0.8f);
  hat_envelope1.decay((velocity / 4) + 20);
  hat_filter_bp.filter.resonance(map_range(velocity, 0, 127, 100, 70) / 100.f);

  hat_mixer.gain(1, map_range(velocity, 0, 127, 0, 100) / 100.f); // snappy
  hat_mixer.gain(0, map_range(velocity, 0, 127, 50, 20) / 100.f); // noise

  hat_envelope1.noteOn();
}

void hat_noteoff() {
  hat_envelope1.noteOff();
  hat_noise1.amplitude(0.0f);
}

// =====================================================================
// Pitch (duo-imxrt Pitch.h, near-verbatim; glide unused on this surface)
// =====================================================================
const uint8_t DETUNE_OFFSET_SEMITONES[] = {4, 5, 7, 9};
int detune_amount = 0;

float detune(int note, int amount) { // amount goes from 0-1023
  if (amount > 850) {
    return midi_note_to_frequency(note) * (amount + 9000) / 10240.f;
  }
  if (amount > 680) {
    return midi_note_to_frequency(note - DETUNE_OFFSET_SEMITONES[0]);
  }
  if (amount > 510) {
    return midi_note_to_frequency(note - DETUNE_OFFSET_SEMITONES[1]);
  }
  if (amount > 340) {
    return midi_note_to_frequency(note - DETUNE_OFFSET_SEMITONES[2]);
  }
  if (amount > 170) {
    return midi_note_to_frequency(note - DETUNE_OFFSET_SEMITONES[3]);
  }
  return midi_note_to_frequency(note) * (amount + 9040) / 10240.f;
}

void pitch_update() {
  detune_amount = synth.detune;
  const float osc_saw_target_frequency =
      detune(osc_pulse_midi_note, detune_amount);
  osc_saw_frequency = osc_saw_target_frequency;
  osc_pulse_frequency = osc_pulse_target_frequency;
}

// =====================================================================
// Note handling (duo-imxrt main.cpp note_on/note_off)
// =====================================================================
void note_on(uint8_t midi_note, uint8_t velocity, bool enabled) {
  if (synth.accent) {
    velocity = 127;
  }

  note_is_playing = midi_note;

  if (enabled) {
    dc1.amplitude(velocity / 127.f); // DC amplitude controls filter env amount
    osc_pulse_midi_note = midi_note;
    osc_pulse_target_frequency = midi_note_to_frequency(midi_note);
    osc_saw.frequency(detune(osc_pulse_midi_note, detune_amount));

    MIDI::sendNoteOn(midi_note, velocity, MIDI_CHANNEL);
    envelope1.noteOn();
    envelope2.noteOn();
  }
}

void note_off() {
  if (note_is_playing) {
    MIDI::sendNoteOff(note_is_playing, 0, MIDI_CHANNEL);
    envelope1.noteOff();
    envelope2.noteOff();
    note_is_playing = 0;
  }
}

void sequencer_note_on(uint8_t midi_note, uint8_t velocity) {
  note_on(midi_note + transpose, velocity, true);
}

// =====================================================================
// Sequencer (duo-imxrt globals.h / Sequencer.h glue, reshaped)
// =====================================================================
void sequencer_randomize_step_offset(Sequencer::Sequencer &seq) {
  const uint8_t offset =
      seq.get_step_offset() + 1 + (std::rand() % (Sequencer::NUM_STEPS - 3));
  seq.set_step_offset(offset);
}

void sequencer_on_running_advance(Sequencer::Sequencer &seq) {
  if (random_flag) {
    sequencer_randomize_step_offset(seq);
  } else {
    seq.set_step_offset(0);
  }
}

Sequencer::Sequencer
    sequencer(Sequencer::Output::Callbacks{.note_on = sequencer_note_on,
                                           .note_off = note_off},
              sequencer_on_running_advance);

// MidiFunctions.h is a definition-carrying header included here, as in the
// reference firmware, because it closes over `synth`, `transpose`,
// `note_off()` and `sequencer` above (§7.6 minimal-change).
#include "duo/midi_functions.h"

// =====================================================================
// Timing: musin::timing stack (§7.3). SpeedAdapter runs at DOUBLE_SPEED so
// TempoEvents arrive at 24 PPQN — the DUO sequencer's native tick rate — and
// the DUO's own speed_mod divider logic stays untouched.
// =====================================================================
musin::timing::InternalClock internal_clock(120.0f);
musin::timing::MidiClockProcessor midi_clock_processor;
musin::timing::SyncIn sync_in(DATO_SUBMARINE_SYNC_IN_PIN,
                              DATO_SUBMARINE_SYNC_DETECT_PIN);
musin::timing::ClockRouter clock_router(internal_clock, midi_clock_processor,
                                        sync_in,
                                        musin::timing::ClockSource::INTERNAL);
musin::timing::SpeedAdapter
    speed_adapter(musin::timing::SpeedModifier::DOUBLE_SPEED);
musin::timing::TempoHandler tempo_handler(clock_router, speed_adapter, false,
                                          musin::timing::ClockSource::INTERNAL);

// MIDI clock out. This observes clock_router directly, which carries the raw
// 24 PPQN — speed_adapter's output is the sequencer's rate, not the wire rate.
// `send_when_stopped_as_master = true` matches the retired DUO TempoHandler,
// which sent clock from trigger() whenever the source was not MIDI, running or
// not (duo-imxrt shared/duo/TempoHandler.h). MidiClockOut suppresses the
// MIDI-source case itself, so an external clock is not echoed back.
musin::timing::MidiClockOut midi_clock_out(tempo_handler, true);

struct SequencerTicker : etl::observer<musin::timing::TempoEvent> {
  void notification(musin::timing::TempoEvent) override {
    sequencer.tick_clock();
  }
};

// DUO tempo pot curve (duo-imxrt tempo.cpp): 30-60 / 60-200 / 200-603 BPM.
float duo_bpm_from_pot(int potvalue) {
  if (potvalue < 128) {
    return 30.f + (potvalue / 128.f) * 30.f;
  } else if (potvalue < 895) {
    return 60.f + ((potvalue - 128) / 767.f) * 140.f;
  }
  return 200.f + ((potvalue - 895) / 128.f) * 403.f;
}

uint32_t last_sequencer_update;

// Sequencer.h sequencer_update(), reshaped onto musin::timing.
void sequencer_update() {
  unsigned speed_mod = (unsigned)Sequencer::NormalSpeed;

  if (tempo_handler.get_clock_source() !=
      musin::timing::ClockSource::INTERNAL) {
    if (synth.speed > 900) {
      speed_mod += 1;
    } else if (synth.speed < 127) {
      speed_mod -= 1;
    }
  }

  if (double_speed) {
    speed_mod += 1;
  }

  sequencer.speed_mod = (Sequencer::SpeedModifier)speed_mod;

  sequencer.set_gate_length(map_range(synth.gateLength, 0, 1023, 10, 200) *
                            1000);
  if (tempo_handler.get_clock_source() ==
      musin::timing::ClockSource::INTERNAL) {
    internal_clock.set_bpm(duo_bpm_from_pot(synth.speed));
  }

  const uint32_t cur_micros = time_us_32();
  const uint32_t delta = cur_micros - last_sequencer_update;
  last_sequencer_update = cur_micros;
  sequencer.update_gate(delta);
}

void sequencer_stop() {
  if (sequencer.is_running()) {
    MIDI::sendControlChange(123, 0, MIDI_CHANNEL);
    MIDI::sendRealTime(::midi::Stop);
  }
  sequencer.stop();
  tempo_handler.set_playback_state(musin::timing::PlaybackState::STOPPED);
}

void sequencer_start() {
  MIDI::sendRealTime(::midi::Continue);
  tempo_handler.set_playback_state(musin::timing::PlaybackState::PLAYING);
  sequencer.run();
}

// Transport in from a host. Unlike the reference, received realtime is not
// echoed back out: the reference's Brains 2 DIN and USB ports were separate,
// while here a DAW driving us over USB would see its own Start returned.
void midi_handle_start() {
  tempo_handler.set_playback_state(musin::timing::PlaybackState::PLAYING);
  sequencer.align_clock();
  sequencer.run();
}

void midi_handle_continue() {
  tempo_handler.set_playback_state(musin::timing::PlaybackState::PLAYING);
  sequencer.run();
}

void midi_handle_stop() {
  sequencer.stop();
  tempo_handler.set_playback_state(musin::timing::PlaybackState::STOPPED);
}

void midi_handle_clock() {
  midi_clock_processor.on_midi_clock_tick_received();
}

void midi_init() {
  MIDI::init(MIDI::Callbacks{
      .note_on = midi_note_on,
      .note_off = midi_note_off,
      .clock = midi_handle_clock,
      .start = midi_handle_start,
      .cont = midi_handle_continue,
      .stop = midi_handle_stop,
      .cc = midi_handle_cc,
      .pitch_bend = nullptr,
      .sysex = midi_handle_sysex,
  });
}

void sequencer_toggle_start() {
  if (sequencer.is_running()) {
    sequencer_stop();
  } else {
    sequencer_start();
  }
}

// =====================================================================
// Control mapping (§6.1 / §6.2)
// =====================================================================
// Keypad: col 3 = Track 1 (outer) row = KEYB_0..7 (step order is reversed on
// the panel: step index = 7 - row). Col 2 = Track 2: steps 0,1 = KEYB_8,9;
// steps 6,7 = octave down/up. Col 0 = Track 4 (inner) = STEP_1..8.
// Cols 1 (Track 3) and 4 (sample select) unmapped.
struct KeypadHandler : etl::observer<musin::ui::KeypadEvent> {
  void notification(musin::ui::KeypadEvent event) override {
    const uint8_t step_index = (KEYPAD_ROWS - 1) - event.row;
    const bool press = event.type == musin::ui::KeypadEvent::Type::Press;
    const bool release = event.type == musin::ui::KeypadEvent::Type::Release;

    if (event.col == 3) { // Track 1 row: KEYB_0..7
      keyboard_key(step_index, press, release);
    } else if (event.col == 2) { // Track 2 row
      if (step_index <= 1) {     // KEYB_8, KEYB_9
        keyboard_key(8 + step_index, press, release);
      } else if (step_index == 6) { // BTN_DOWN
        octave_key(-1, press, release);
      } else if (step_index == 7) { // BTN_UP
        octave_key(+1, press, release);
      }
    } else if (event.col == 0) { // Track 4 row: STEP_1..8
      if (press) {
        sequencer.toggle_step(step_index);
      }
    }
  }

  static void keyboard_key(uint8_t key, bool press, bool release) {
    if (key >= SCALE.size()) {
      return;
    }
    if (press) {
      sequencer.hold_note(SCALE[key]);
    } else if (release) {
      sequencer.release_note(SCALE[key]);
    }
  }

  static void octave_key(int direction, bool press, bool release) {
    if (press) {
      transpose += direction;
      if (transpose < -12) {
        transpose = -24;
      }
      if (transpose > 12) {
        transpose = 24;
      }
    } else if (release) {
      if (transpose < -12) {
        transpose = -12;
      }
      if (transpose > 12) {
        transpose = 12;
      }
    }
  }
};

// Analog buttons/pads read from the mux as gates (§6.1: threshold to a gate).
struct AnalogGate {
  uint8_t channel;
  bool pressed = false;

  // 12-bit ADC; drum's pads/buttons idle low.
  static constexpr uint16_t PRESS_THRESHOLD = 1400;
  static constexpr uint16_t RELEASE_THRESHOLD = 900;

  // Returns +1 on press edge, -1 on release edge, 0 otherwise.
  int update(uint16_t raw) {
    if (!pressed && raw > PRESS_THRESHOLD) {
      pressed = true;
      return +1;
    }
    if (pressed && raw < RELEASE_THRESHOLD) {
      pressed = false;
      return -1;
    }
    return 0;
  }
};

AnalogGate play_button{MUX_PLAYBUTTON};
AnalogGate repeat_button{MUX_REPEAT}; // DUO BTN_SEQ1
AnalogGate random_button{MUX_RANDOM}; // DUO BTN_SEQ2
AnalogGate drumpad_kick{MUX_DRUM1};
AnalogGate drumpad_hat{MUX_DRUM2};

// DUO process_key() branches for BTN_SEQ1/BTN_SEQ2/SEQ_START, near-verbatim.
void update_analog_buttons(musin::hal::AnalogMuxScanner &scanner) {
  switch (play_button.update(scanner.get_raw_value(MUX_PLAYBUTTON))) {
  case +1:
    sequencer_toggle_start();
    break;
  }

  switch (repeat_button.update(scanner.get_raw_value(MUX_REPEAT))) {
  case +1: // BTN_SEQ1
    if (sequencer.is_running()) {
      random_flag = true;
    } else {
      sequencer_randomize_step_offset(sequencer);
    }
    break;
  case -1:
    random_flag = false;
    break;
  }

  switch (random_button.update(scanner.get_raw_value(MUX_RANDOM))) {
  case +1: // BTN_SEQ2
    if (!sequencer.is_running()) {
      sequencer.advance();
    }
    double_speed = true;
    break;
  case -1:
    double_speed = false;
    break;
  }

  switch (drumpad_kick.update(scanner.get_raw_value(MUX_DRUM1))) {
  case +1:
    kick_noteon(127);
    break;
  case -1:
    kick_noteoff();
    break;
  }

  switch (drumpad_hat.update(scanner.get_raw_value(MUX_DRUM2))) {
  case +1:
    hat_noteon(127);
    break;
  case -1:
    hat_noteoff();
    break;
  }
}

// DUO pots_read(): §6.2 mapping. FILTER_RES and GATE pots are homeless on
// this surface [DECIDED §6.2]; they hold fixed values.
void pots_read(musin::hal::AnalogMuxScanner &scanner) {
  // 12-bit ADC to the DUO's 10-bit parameter scale.
  auto pot = [&](uint8_t channel) {
    return scanner.get_raw_value(channel) >> 2;
  };

  synth.speed = pot(MUX_SPEED);
  synth.amplitude = pot(MUX_VOLUME);
  synth.detune = pot(MUX_PITCH1);
  synth.pulseWidth = pot(MUX_PITCH2);
  synth.filter = pot(MUX_PITCH3);
  synth.release = pot(MUX_PITCH4);

  synth.resonance = 0;    // FILTER_RES_POT homeless: minimum (q = 0.7)
  synth.gateLength = 512; // GATE_POT homeless: mid position
  synth.glide = false;
  synth.crush = false;
  synth.accent = false;
  synth.delay = false;
}

// Synth.h synth_update(), near-verbatim (delay/crush inputs are fixed).
void synth_update() {
  float osc_saw_amplitude = 0.4f;

  if (synth.detune > 850) {
    osc_saw_amplitude = map_range(synth.detune, 850, 1023, 400, 0) / 1000.0f;
  } else if (synth.detune < 200) {
    osc_saw_amplitude = map_range(synth.detune, 0, 200, 200, 400) / 1000.0f;
  }

  float osc_pulse_pulseWidth =
      map_range(synth.pulseWidth, 0, 1023, 500, 950) / 1000.0f;
  float filter_resonance =
      map_range(synth.resonance, 0, 1023, 70, 320) / 100.0f;

  if (synth.accent) {
    filter_resonance = 4.0f;
  }

  osc_saw.frequency(osc_saw_frequency);
  osc_saw.amplitude(osc_saw_amplitude);

  osc_pulse.frequency(osc_pulse_frequency / 2);
  osc_pulse.pulseWidth(osc_pulse_pulseWidth);

  filter1.filter.frequency((synth.filter / 2) + 30);
  filter1.filter.resonance(filter_resonance);

  envelope1.release(((synth.release * synth.release) >> 11) + 30);

  audio_volume(synth.amplitude);
}

#if DUO_BOOT_BEACON
#define BEACON(stage) duo::ui::boot_beacon::signal(stage)
#else
#define BEACON(stage) ((void)0)
#endif

} // namespace

int main() {
  BEACON(1); // reached main() at all

  stdio_usb_init();
  musin::usb::init(false);
  BEACON(2);

  // The DUO seeds its RNG during pin init (BRAINS_2.3 pins.cpp: randomSeed
  // from three pot readings). That was an entropy hack for a part with no
  // RNG; the RP2350 has one, so use it. Without any seed the "random"
  // default pattern below is identical on every power-up.
  srand(get_rand_32());
  BEACON(3);

  // These are static, not automatic: Keypad_HC138<8,5> alone is 1744 bytes
  // against a 2 KB stack, so holding them as locals leaves main() with no
  // room to call anything.
  static musin::ui::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad(
      keypad_decoder_pins, keypad_columns_pins, logger);
  static KeypadHandler keypad_handler;
  keypad.add_observer(keypad_handler);
  keypad.init();
  BEACON(4);

  static musin::hal::AnalogMuxScanner scanner(DATO_SUBMARINE_ADC_PIN,
                                              analog_address_pins);
  scanner.init();
  BEACON(5);

  // The audio output must claim its PIO state machine and DMA channel before
  // anything else does. audio_i2s_setup() hardcodes pio_sm 0 and dma_channel 0
  // (i2s_config in musin/audio/audio_output.cpp) and claims them with
  // pio_sm_claim/dma_channel_claim, which panic when already taken — whereas
  // WS2812_DMA::init() calls dma_claim_unused_channel() and so grabs channel 0
  // if it goes first. Initialising the display before the audio therefore
  // panics inside AudioOutput::init(), which in a Release build with no host
  // attached is indistinguishable from a dead board. drum/main.cpp gets this
  // right by accident of ordering; keep the two in the same order.
  const bool audio_output_ok = AudioOutput::init();
  BEACON(6);

  static duo::ui::DuoDisplay display;
  display.init();
  BEACON(7);

  audio_init();
  drum_init();
  midi_init();
  BEACON(8);

  // Timing: clock -> speed adapter (24 PPQN out) -> tempo handler -> ticker
  clock_router.add_observer(speed_adapter);
  clock_router.add_observer(midi_clock_out);
  SequencerTicker ticker;
  tempo_handler.add_observer(ticker);

  // Default pattern, as sequencer_init() (random notes from the scale).
  for (int i = 0; i < Sequencer::NUM_STEPS; i++) {
    sequencer.set_step_note(i, SCALE[std::rand() % 9]);
  }
  sequencer_stop();
  BEACON(9);

  if (!audio_output_ok) {
    // Keep servicing USB so the device still enumerates and the message below
    // is actually readable; without it a codec failure is indistinguishable
    // from a board that never started. Blink the play LED as the out-of-band
    // signal for when no host is attached.
    absolute_time_t next_blink = get_absolute_time();
    bool lit = false;
    while (true) {
      musin::usb::background_update();
      if (absolute_time_diff_us(get_absolute_time(), next_blink) <= 0) {
        lit = !lit;
        display.show_error(lit);
        if (lit) {
          printf("AudioOutput::init failed\n");
        }
        next_blink = make_timeout_time_ms(500);
      }
    }
  }
  AudioOutput::attach_source(load_meter);
  AudioOutput::volume(0.6f);
  BEACON(10);

  last_sequencer_update = time_us_32();
  absolute_time_t next_report = make_timeout_time_ms(1000);
  uint32_t last_busy_us = 0;
  uint32_t last_blocks = 0;

  while (true) {
    const absolute_time_t now = get_absolute_time();

    keypad.scan();
    scanner.scan();
    update_analog_buttons(scanner);
    pots_read(scanner);

    pitch_update();
    synth_update();
    sequencer_update();

    internal_clock.update(now);
    sync_in.update(now);
    clock_router.update_auto_source_switching();

    AudioOutput::update();
    musin::usb::background_update();
    MIDI::read(MIDI_CHANNEL);
    // process_midi_output_queue sends at most one message per call, so drain
    // twice per loop to halve output latency (as drum/main.cpp, see #527).
    musin::midi::process_midi_output_queue(logger);
    musin::midi::process_midi_output_queue(logger);

    // ~11 ms LED/control frame, as the DUO's main_loop.
    static absolute_time_t next_led_frame = nil_time;
    if (absolute_time_diff_us(now, next_led_frame) <= 0) {
      next_led_frame = make_timeout_time_ms(11);
      midi_send_cc();
      static float peak_level = 0.0f;
      if (peak1.available()) {
        peak_level = peak1.read();
      }
#if DUO_BOOT_BEACON
      // Alternate between 11 and 12 lit LEDs at ~2 Hz: a visibly blinking
      // twelfth LED means the main loop is turning over, which no static
      // stage count can show. The real display is suppressed because the
      // beacon owns the data pin.
      static bool beacon_toggle = false;
      static absolute_time_t next_beacon = nil_time;
      if (absolute_time_diff_us(now, next_beacon) <= 0) {
        next_beacon = make_timeout_time_ms(500);
        beacon_toggle = !beacon_toggle;
        BEACON(beacon_toggle ? 12 : 11);
      }
      (void)peak_level;
#else
      display.update(sequencer, synth, peak_level);
#endif
    }

    if (absolute_time_diff_us(now, next_report) <= 0) {
      next_report = make_timeout_time_ms(1000);
      const uint32_t busy = load_meter.busy_us;
      const uint32_t blocks = load_meter.blocks;
      const uint32_t d_busy = busy - last_busy_us;
      const uint32_t d_blocks = blocks - last_blocks;
      last_busy_us = busy;
      last_blocks = blocks;
      if (d_blocks > 0) {
        // Budget per 128-sample block at 44.1 kHz is 2902.5 us.
        const float budget_us = static_cast<float>(d_blocks) * 2902.5f;
        printf("audio load: %.1f%% (%lu us over %lu blocks), peak %.2f\n",
               (static_cast<float>(d_busy) / budget_us) * 100.0f,
               static_cast<unsigned long>(d_busy),
               static_cast<unsigned long>(d_blocks),
               peak2.available() ? peak2.read() : 0.0f);
      }
    }
  }
}
