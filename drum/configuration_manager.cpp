#include "drum/configuration_manager.h"
#include "config_default.h"
#include "etl/string.h"      // For etl::from_chars
#include "etl/string_view.h" // For etl::string_view
#include "jsmn/jsmn.h"
#include <cstdio>

namespace drum {

ConfigurationManager::ConfigurationManager(musin::Logger &logger) : logger_(logger) {
}

bool ConfigurationManager::load() {
  logger_.info("Loading configuration from /config.json");
  FILE *config_file = fopen(CONFIG_PATH, "r");
  if (!config_file) {
    logger_.info("Could not open /config.json. Loading embedded default configuration.");
    return parse_json_buffer(reinterpret_cast<const char *>(config_default_json),
                             config_default_json_len);
  }

  static char buffer[MAX_CONFIG_FILE_SIZE];
  size_t file_size = fread(buffer, 1, sizeof(buffer) - 1, config_file);
  fclose(config_file);

  if (file_size == 0) {
    logger_.warn("/config.json is empty. Loading embedded default configuration.");
    return parse_json_buffer(reinterpret_cast<const char *>(config_default_json),
                             config_default_json_len);
  }

  buffer[file_size] = '\0'; // Null-terminate the buffer for safety

  return parse_json_buffer({buffer, file_size});
}

bool ConfigurationManager::parse_json_buffer(etl::string_view buffer) {
  // Since the default config can be empty, handle that case gracefully.
  if (size == 0) {
    logger_.info("Configuration buffer is empty. No settings loaded.");
    sample_configs_.clear();
    return true;
  }

  jsmn_parser parser;
  jsmntok_t tokens[MAX_JSON_TOKENS];

  jsmn_init(&parser);
  int r = jsmn_parse(&parser, buffer.data(), buffer.size(), tokens, MAX_JSON_TOKENS);

  if (r < 0) {
    logger_.error("Failed to parse JSON", r);
    return false;
  }

  if (r < 1 || tokens[0].type != JSMN_OBJECT) {
    logger_.error("JSON root is not an object.");
    return false;
  }

  // Find the 'samples' key
  for (int i = 1; i < r; i++) {
    if (tokens[i].type == JSMN_STRING &&
        json_string_equals({buffer.data() + tokens[i].start,
                            static_cast<size_t>(tokens[i].end - tokens[i].start)},
                           "samples")) {
      if (tokens[i + 1].type == JSMN_ARRAY) {
        if (!parse_samples(buffer, &tokens[i + 1], r - (i + 1))) {
          return false; // Error parsing samples array
        }
        i += tokens[i + 1].size + 1; // Move to the next top-level key
      } else {
        logger_.warn("'samples' key is not followed by an array.");
      }
    }
    // Future: Parse 'settings' here
  }

  return true;
}

bool ConfigurationManager::parse_samples(etl::string_view json, jsmntok *tokens,
                                         [[maybe_unused]] int count) {
  if (tokens->type != JSMN_ARRAY) {
    return false;
  }

  sample_configs_.clear();
  int token_idx = 1; // Start inside the array

  for (int i = 0; i < tokens->size; ++i) { // For each object in the 'samples' array
    jsmntok *obj_tok = &tokens[token_idx];
    if (obj_tok->type != JSMN_OBJECT) {
      logger_.warn("Item in samples array is not an object.");
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
      etl::string_view key_sv(json.data() + key->start, key->end - key->start);
      etl::string_view val_sv(json.data() + val->start, val->end - val->start);

      if (key->type == JSMN_STRING) {
        if (key_sv == "slot") {
          etl::from_chars(val_sv.begin(), val_sv.end(), current_config.slot);
          slot_found = true;
        } else if (key_sv == "path") {
          current_config.path.assign(val_sv.begin(), val_sv.end());
        } else if (key_sv == "note") {
          etl::from_chars(val_sv.begin(), val_sv.end(), current_config.note);
        } else if (key_sv == "track") {
          etl::from_chars(val_sv.begin(), val_sv.end(), current_config.track);
        } else if (key_sv == "color") {
          etl::from_chars(val_sv.begin(), val_sv.end(), current_config.color);
        }
      }
      token_idx += 2; // Move to next key-value pair
    }

    if (slot_found) {
      if (!sample_configs_.full()) {
        sample_configs_.push_back(current_config);
        logger_.info("  - Parsed sample for slot", (int32_t)current_config.slot);
        logger_.info(current_config.path);
        logger_.info("    note", (int32_t)current_config.note);
        logger_.info("    track", (int32_t)current_config.track);
        logger_.info("    color", current_config.color);
      } else {
        logger_.warn("Max samples reached, skipping remaining entries.");
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
bool ConfigurationManager::json_string_equals(etl::string_view json_token,
                                              etl::string_view str) const {
  return json_token == str;
}

} // namespace drum
