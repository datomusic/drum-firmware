#include "musin/audio/pcm_decoder.h"

consteval int test_dummy(){
  const unsigned char data[2] = {100, 0};
  PcmDecoder decoder(data, 2);

  decoder.reset();
  AudioBlock block;
  const auto count = decoder.read_samples(block);
  assert(count == 1);
  auto block_data = block.begin();
  assert(block_data[0] == 100);

  return 0;
}

static_assert(test_dummy() == 0);
