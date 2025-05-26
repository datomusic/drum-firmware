#include "test_support.h"

#include "musin/audio/block.h" // Include AudioBlock definition
#include "musin/audio/memory_reader.h"

TEST_CASE("MemorySampleReader streams samples") {
  CONST_BODY(({
    const int16_t samples[2] = {123, 33};
    // Use the namespaced version
    musin::MemorySampleReader decoder(samples, 2);

    // Assuming AudioBlock is still global based on previous info
    AudioBlock block;
    auto block_data = block.begin();

    auto count = decoder.read_samples(block);
    REQUIRE(count == 2);
    REQUIRE(block_data[0] == 123);
    REQUIRE(block_data[1] == 33);

    count = decoder.read_samples(block);
    REQUIRE(count == 0);

    decoder.reset();
    count = decoder.read_samples(block);
    REQUIRE(count == 2);
  }));
}
