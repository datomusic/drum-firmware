extern "C" {
#include "blockdevice/flash.h"
#include <assert.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/flash.h>
#include <pico/mutex.h>
#include <stdio.h>
#include <string.h>
}

#include "safe_flash_block_device.h"

namespace musin::filesystem {

#define SAFE_FLASH_EXECUTE_TIMEOUT 10 * 1000

#define SAFE_FLASH_BLOCK_DEVICE_ERROR_TIMEOUT -4001
#define SAFE_FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED -4002
#define SAFE_FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES -4003

typedef struct {
  uint32_t start;
  size_t length;
  mutex_t _mutex;
} safe_flash_config_t;

typedef struct {
  bool is_erase;
  size_t addr;
  size_t size;
  void *buffer;
} _safe_flash_update_param_t;

static const char DEVICE_NAME[] = "safe_flash";

static int _error_remap(int err) {
  switch (err) {
  case PICO_OK:
    return BD_ERROR_OK;
  case PICO_ERROR_TIMEOUT:
    return SAFE_FLASH_BLOCK_DEVICE_ERROR_TIMEOUT;
  case PICO_ERROR_NOT_PERMITTED:
    return SAFE_FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED;
  case PICO_ERROR_INSUFFICIENT_RESOURCES:
    return SAFE_FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES;
  default:
    return err;
  }
}

static size_t safe_flash_target_offset(blockdevice_t *device) {
  safe_flash_config_t *config = (safe_flash_config_t *)device->config;
  return config->start;
}

static int safe_init(blockdevice_t *device) {
  device->is_initialized = true;
  return BD_ERROR_OK;
}

static int safe_deinit(blockdevice_t *device) {
  device->is_initialized = false;
  return 0;
}

static int safe_sync(blockdevice_t *device) {
  (void)device;
  return 0;
}

// Use the untranslated XIP window for reads to access any partition
static int safe_read(blockdevice_t *device, const void *buffer, bd_size_t addr,
                     bd_size_t size) {
  safe_flash_config_t *config = (safe_flash_config_t *)device->config;

  uint32_t xip_addr = XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE +
                      safe_flash_target_offset(device) + (size_t)addr;

  mutex_enter_blocking(&config->_mutex);

  // Use XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE to access entire flash
  const uint8_t *flash_contents = (const uint8_t *)xip_addr;
  memcpy((uint8_t *)buffer, flash_contents, (size_t)size);

  mutex_exit(&config->_mutex);
  return BD_ERROR_OK;
}

static void _safe_flash_update(void *param) {
  const _safe_flash_update_param_t *args = (_safe_flash_update_param_t *)param;
  if (args->is_erase) {
    flash_range_erase(args->addr, args->size);
  } else {
    flash_range_program(args->addr, (const uint8_t *)args->buffer, args->size);
  }
}

static int safe_erase(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
  _safe_flash_update_param_t param = {
      .is_erase = true,
      .addr = safe_flash_target_offset(device) + addr,
      .size = (size_t)size,
      .buffer = nullptr,
  };
  int err = flash_safe_execute(_safe_flash_update, &param,
                               SAFE_FLASH_EXECUTE_TIMEOUT);
  return _error_remap(err);
}

static int safe_program(blockdevice_t *device, const void *buffer,
                        bd_size_t addr, bd_size_t size) {
  _safe_flash_update_param_t param = {
      .is_erase = false,
      .addr = safe_flash_target_offset(device) + addr,
      .size = (size_t)size,
      .buffer = (void *)buffer,
  };
  int err = flash_safe_execute(_safe_flash_update, &param,
                               SAFE_FLASH_EXECUTE_TIMEOUT);
  return _error_remap(err);
}

static int safe_trim(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
  (void)device;
  (void)addr;
  (void)size;
  return BD_ERROR_OK;
}

static bd_size_t safe_size(blockdevice_t *device) {
  safe_flash_config_t *config = (safe_flash_config_t *)device->config;
  return (bd_size_t)config->length;
}

blockdevice_t *safe_flash_block_device_create(uint32_t flash_offset,
                                              size_t size) {
  assert(flash_offset % FLASH_SECTOR_SIZE == 0);
  assert(size % FLASH_SECTOR_SIZE == 0);

  blockdevice_t *device = (blockdevice_t *)calloc(1, sizeof(blockdevice_t));
  if (device == NULL) {
    fprintf(stderr, "safe_flash_block_device_create: Out of memory\n");
    return NULL;
  }

  safe_flash_config_t *config =
      (safe_flash_config_t *)calloc(1, sizeof(safe_flash_config_t));
  if (config == NULL) {
    fprintf(stderr, "safe_flash_block_device_create: Out of memory\n");
    free(device);
    return NULL;
  }

  device->init = safe_init;
  device->deinit = safe_deinit;
  device->read = safe_read;
  device->erase = safe_erase;
  device->program = safe_program;
  device->trim = safe_trim;
  device->sync = safe_sync;
  device->size = safe_size;
  device->read_size = 1;
  device->erase_size = FLASH_SECTOR_SIZE; // 4096 byte
  device->program_size = FLASH_PAGE_SIZE; // 256 byte
  device->name = DEVICE_NAME;
  device->is_initialized = false;

  config->start = flash_offset;
  config->length = size > 0 ? size : (PICO_FLASH_SIZE_BYTES - flash_offset);
  mutex_init(&config->_mutex);
  device->config = config;
  device->init(device);

  return device;
}

void safe_flash_block_device_free(blockdevice_t *device) {
  if (device) {
    free(device->config);
    free(device);
  }
}

} // namespace musin::filesystem
