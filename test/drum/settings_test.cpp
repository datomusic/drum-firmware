#include "drum/settings.h"

#include <catch2/catch_test_macros.hpp>

using drum::settings::DESCRIPTORS;
using drum::settings::find_descriptor;
using drum::settings::Id;
using drum::settings::Settings;

TEST_CASE("Settings defaults", "[settings]") {
  Settings settings;

  SECTION("Every setting starts at its descriptor default") {
    for (const auto &descriptor : DESCRIPTORS) {
      REQUIRE(settings.get(descriptor.id) == descriptor.default_value);
    }
  }

  SECTION("MIDI channel defaults to 10 (GM percussion)") {
    REQUIRE(settings.get(Id::MidiChannel) == 10);
  }

  SECTION("Slider defaults to controlling pitch only") {
    REQUIRE(settings.get(Id::SliderMode) == drum::settings::slider_mode::PITCH);
  }
}

TEST_CASE("Settings set validation", "[settings]") {
  Settings settings;

  SECTION("Accepts values within range") {
    REQUIRE(settings.set(Id::MidiChannel, 1));
    REQUIRE(settings.get(Id::MidiChannel) == 1);
    REQUIRE(settings.set(Id::MidiChannel, 16));
    REQUIRE(settings.get(Id::MidiChannel) == 16);
  }

  SECTION("Rejects out-of-range values and keeps the previous value") {
    REQUIRE(settings.set(Id::MidiChannel, 5));
    REQUIRE_FALSE(settings.set(Id::MidiChannel, 0));
    REQUIRE_FALSE(settings.set(Id::MidiChannel, 17));
    REQUIRE_FALSE(settings.set(Id::MidiChannel, 127));
    REQUIRE(settings.get(Id::MidiChannel) == 5);
  }

  SECTION("Rejects unknown setting ids") {
    REQUIRE_FALSE(settings.set(static_cast<Id>(0x7F), 1));
  }

  SECTION("Slider mode accepts any combination of pitch, gain and decay") {
    using namespace drum::settings::slider_mode;
    REQUIRE(settings.set(Id::SliderMode, PITCH | GAIN | DECAY));
    REQUIRE(settings.get(Id::SliderMode) == 7);
    REQUIRE(settings.set(Id::SliderMode, 0));
    REQUIRE_FALSE(settings.set(Id::SliderMode, 8));
    REQUIRE(settings.get(Id::SliderMode) == 0);
  }
}

TEST_CASE("Settings descriptor lookup", "[settings]") {
  SECTION("Known id resolves to its descriptor") {
    const auto *descriptor = find_descriptor(Id::MidiChannel);
    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->name == "midi_channel");
    REQUIRE(descriptor->min == 1);
    REQUIRE(descriptor->max == 16);
  }

  SECTION("Unknown id resolves to nullptr") {
    REQUIRE(find_descriptor(static_cast<Id>(0x7F)) == nullptr);
  }

  SECTION("Ids and names are unique across the registry") {
    for (size_t i = 0; i < DESCRIPTORS.size(); ++i) {
      for (size_t j = i + 1; j < DESCRIPTORS.size(); ++j) {
        REQUIRE(DESCRIPTORS[i].id != DESCRIPTORS[j].id);
        REQUIRE(DESCRIPTORS[i].name != DESCRIPTORS[j].name);
      }
    }
  }
}
