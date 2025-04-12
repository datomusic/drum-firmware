#ifndef LOG_TAGS_H_MS19DZ3O
#define LOG_TAGS_H_MS19DZ3O

#include <stdint.h>
#include <stdio.h>

enum Tag {
  Init = 2,
  Filesystem = 4,
  Audio = 8,
  All = UINT32_MAX
};

namespace Musin::Logging {
// Should remove this and let the host color output
static const bool show_level = true;
static const bool show_tags = true;
static const bool with_color = true;

static const bool only_tagged = false;
static const Tag tag_filter = Tag::All;
// static const Tag tag_filter = Audio;

static void print_tags(const uint32_t tags) {
  bool previous = false;
  if (Tag::Init & tags) {
    printf("init");
    previous = true;
  }

  if (Tag::Filesystem & tags) {
    if (previous) {
      printf(",");
    }
    previous = true;
    printf("filesystem");
  }

  if (Tag::Audio & tags) {
    if (previous) {
      printf(",");
    }
    previous = true;
    printf("audio");
  }
}

} // namespace Musin::Logging

#endif /* end of include guard: LOG_TAGS_H_MS19DZ3O */
