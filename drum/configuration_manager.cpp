#include "drum/configuration_manager.h"
#include "config_default.h"
#include "jsmn/jsmn.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace drum {

bool ConfigurationManager::load() {
  printf("Loading configuration from %s\n", CONFIG_PATH);
  FILE *config_file = fopen(CONFIG_PATH, "r");
  if (!config_file) {
    printf("INFO: Could not open %s. Loading embedded default configuration.\n", CONFIG_PATH);
    return parse_json_buffer(reinterpret_cast<const char *>(config_default_json),
                             config_default_json_len);
  }

  static char buffer[MAX_CONFIG_FILE_SIZE];
  size_t file_size = fread(buffer, 1, sizeof(buffer) - 1, config_file);
  fclose(config_file);

  if (file_size == 0) {
    printf("WARNING: %s is empty. Loading embedded default configuration.\n", CONFIG_PATH);
    return parse_json_buffer(reinterpret_cast<const char *>(config_default_json),
                             config_default_json_len);
  }

  buffer[file_size] = '\0'; // Null-terminate the buffer for safety

  return parse_json_buffer(buffer, file_size);
}

bool ConfigurationManager::parse_json_buffer(const char *buffer, size_t size) {
  // Since the default config can be empty, handle that case gracefully.
  if (size == 0) {
    printf("INFO: Configuration buffer is empty. No settings loaded.\n");
    sample_configs_.clear();
    return true;
  }

  jsmn_parser parser;
  jsmntok_t tokens[MAX_JSON_TOKENS];

  jsmn_init(&parser);
  int r = jsmn_parse(&parser, buffer, size, tokens, MAX_JSON_TOKENS);

  if (r < 0) {
    printf("ERROR: Failed to parse JSON: %d\n", r);
    return false;
  }

  if (r < 1 || tokens[0].type != JSMN_OBJECT) {
    printf("ERROR: JSON root is not an object.\n");
    return false;
  }

  // Find the 'samples' key
  for (int i = 1; i < r; i++) {
    if (json_string_equals(buffer, &tokens[i], "samples")) {
      if (tokens[i + 1].type == JSMN_ARRAY) {
        if (!parse_samples(buffer, &tokens[i + 1], r - (i + 1))) {
          return false; // Error parsing samples array
        }
        i += tokens[i + 1].size + 1; // Move to the next top-level key
      } else {
        printf("WARNING: 'samples' key is not followed by an array.\n");
      }
    }
    // Future: Parse 'settings' here
  }

  return true;
}

bool ConfigurationManager::parse_samples(const char *json, jsmntok *tokens,
                                         [[maybe_unused]] int count) {
  if (tokens->type != JSMN_ARRAY) {
    return false;
  }

  sample_configs_.clear();
  int token_idx = 1; // Start inside the array

  for (int i = 0; i < tokens->size; ++i) { // For each object in the 'samples' array
    jsmntok *obj_tok = &tokens[token_idx];
    if (obj_tok->type != JSMN_OBJECT) {
      printf("WARNING: Item in samples array is not an object.\n");
      token_idx += obj_tok->size + 1;
      continue;
    }

    SampleConfig current_config{};
    bool slot_found = false;

    int props_in_obj = obj_tok->size;
    token_idx++; // Move to first key in object

    for (int j = 0; j < props_in_obj; j++) {
      jsmntok *key = &tokens[token_idx];
      jsmntok *val = &tokens[token_idx + 1];

      if (json_string_equals(json, key, "slot")) {
        current_config.slot = atoi(json + val->start);
        slot_found = true;
      } else if (json_string_equals(json, key, "path")) {
        current_config.path.assign(json + val->start, val->end - val->start);
      } else if (json_string_equals(json, key, "note")) {
        current_config.note = atoi(json + val->start);
      } else if (json_string_equals(json, key, "track")) {
        current_config.track = atoi(json + val->start);
      } else if (json_string_equals(json, key, "color")) {
        current_config.color = strtol(json + val->start, nullptr, 10);
      }
      token_idx += 2; // Move to next key-value pair
    }

    if (slot_found) {
      if (!sample_configs_.full()) {
        sample_configs_.push_back(current_config);
        printf("  - Parsed sample: slot=%d, path=%s, note=%d, track=%d, color=%lu\n",
               current_config.slot, current_config.path.c_str(), current_config.note,
               current_config.track, current_config.color);
      } else {
        printf("WARNING: Max samples reached, skipping remaining entries.\n");
        break;
      }
    }
  }
  return true;
}

const etl::ivector<SampleConfig> &ConfigurationManager::get_sample_configs() const {
  return sample_configs_;
}

// Helper to compare a jsmn string token with a C-string.
bool ConfigurationManager::json_string_equals(const char *json, const jsmntok *token,
                                              const char *str) {
  return token->type == JSMN_STRING && (int)strlen(str) == token->end - token->start &&
         strncmp(json + token->start, str, token->end - token->start) == 0;
}

} // namespace drum
