#ifndef ROMPLER_H_A3V9GTDX
#define ROMPLER_H_A3V9GTDX

#include "AudioSampleGong.h"
#include "AudioSampleHihat.h"
#include "AudioSampleKick.h"
#include "AudioSampleSnare.h"
#include "AudioSampleTomtom.h"
#include "buffer_player.h"
#include "pitch_shifter.h"
#include <mixer.h>

namespace Rompler {
double playback_speed = 1;

struct Sound {
  Sound(const unsigned int *sample_data, const size_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader),
        player(pitch_shifter) {
  }

  void play() {
    pitch_shifter.set_speed(playback_speed);
    player.play();
  }

  AudioMemoryReader memory_reader;
  PitchShifter pitch_shifter;
  BufferPlayer player;
};

Sound kick(AudioSampleSnare, AudioSampleSnareSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);
Sound tom(AudioSampleHihat, AudioSampleHihatSize);

AudioMixer4 mixer;

static AudioConnection connections[] = {
    AudioConnection(snare.player, 0, mixer, 0),
    AudioConnection(kick.player, 0, mixer, 1),
    AudioConnection(hihat.player, 0, mixer, 2),
    AudioConnection(tom.player, 0, mixer, 3),
};

AudioStream &get_output() {
  return mixer;
};

}; // namespace Rompler

#endif /* end of include guard: ROMPLER_H_A3V9GTDX */
