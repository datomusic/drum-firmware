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
struct Sound {
  Sound(const unsigned int *sample_data, const size_t data_length) : sample_data(sample_data), data_length(data_length) {
  }

  void play() {
    player.play(sample_data, data_length);
  }

  BufferPlayer<PitchShifter<AudioMemoryReader>> player;

private:
  const unsigned int *const sample_data;
  const size_t data_length;
};

Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);
Sound tom(AudioSampleTomtom, AudioSampleTomtomSize);

/*
Sound snare(AudioSampleSnare);
Sound hihat(AudioSampleHihat);
Sound tom(AudioSampleTomtom);
*/

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
