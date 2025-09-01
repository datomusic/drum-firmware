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

#include "partition_flash_block_device.h"

namespace musin::filesystem {

#define PARTITION_FLASH_EXECUTE_TIMEOUT 10 * 1000

#define PARTITION_FLASH_BLOCK_DEVICE_ERROR_TIMEOUT -4001
#define PARTITION_FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED -4002
#define PARTITION_FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES -4003

typedef struct {
  uint32_t start;
  size_t length;
  mutex_t _mutex;
} partition_flash_config_t;

typedef struct {
  bool is_erase;
  size_t addr;
  size_t size;
  void *buffer;
} _partition_flash_update_param_t;

static const char DEVICE_NAME[] = "partition_flash";

static int _error_remap(int err) {
  switch (err) {
  case PICO_OK:
    return BD_ERROR_OK;
  case PICO_ERROR_TIMEOUT:
    return PARTITION_FLASH_BLOCK_DEVICE_ERROR_TIMEOUT;
  case PICO_ERROR_NOT_PERMITTED:
    return PARTITION_FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED;
  case PICO_ERROR_INSUFFICIENT_RESOURCES:
    return PARTITION_FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES;
  default:
    return err;
  }
}

static size_t partition_flash_target_offset(blockdevice_t *device) {
  partition_flash_config_t *config = (partition_flash_config_t *)device->config;
  return config->start;
}

static int partition_init(blockdevice_t *device) {
  device->is_initialized = true;
  return BD_ERROR_OK;
}

static int partition_deinit(blockdevice_t *device) {
  device->is_initialized = false;
  return 0;
}

static int partition_sync(blockdevice_t *device) {
  (void)device;
  return 0;
}

// Use the untranslated XIP window for reads to access any partition
static int partition_read(blockdevice_t *device, const void *buffer,
                          bd_size_t addr, bd_size_t size) {
  partition_flash_config_t *config = (partition_flash_config_t *)device->config;

  uint32_t xip_addr = XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE +
                      partition_flash_target_offset(device) + (size_t)addr;

  mutex_enter_blocking(&config->_mutex);

  // Use XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE to access entire flash
  const uint8_t *flash_contents = (const uint8_t *)xip_addr;
  memcpy((uint8_t *)buffer, flash_contents, (size_t)size);

  mutex_exit(&config->_mutex);
  return BD_ERROR_OK;
}

static void _partition_flash_update(void *param) {
  const _partition_flash_update_param_t *args =
      (_partition_flash_update_param_t *)param;
  if (args->is_erase) {
    flash_range_erase(args->addr, args->size);
  } else {
    flash_range_program(args->addr, (const uint8_t *)args->buffer, args->size);
  }
}

static int partition_erase(blockdevice_t *device, bd_size_t addr,
                           bd_size_t size) {
  _partition_flash_update_param_t param = {
      .is_erase = true,
      .addr = (size_t)(partition_flash_target_offset(device) + addr),
      .size = (size_t)size,
      .buffer = nullptr,
  };
  int err = flash_safe_execute(_partition_flash_update, &param,
                               PARTITION_FLASH_EXECUTE_TIMEOUT);
  return _error_remap(err);
}

static int partition_program(blockdevice_t *device, const void *buffer,
                             bd_size_t addr, bd_size_t size) {
  _partition_flash_update_param_t param = {
      .is_erase = false,
      .addr = (size_t)(partition_flash_target_offset(device) + addr),
      .size = (size_t)size,
      .buffer = (void *)buffer,
  };
  int err = flash_safe_execute(_partition_flash_update, &param,
                               PARTITION_FLASH_EXECUTE_TIMEOUT);
  return _error_remap(err);
}

static int partition_trim(blockdevice_t *device, bd_size_t addr,
                          bd_size_t size) {
  (void)device;
  (void)addr;
  (void)size;
  return BD_ERROR_OK;
}

static bd_size_t partition_size(blockdevice_t *device) {
  partition_flash_config_t *config = (partition_flash_config_t *)device->config;
  return (bd_size_t)config->length;
}

blockdevice_t *partition_flash_block_device_create(uint32_t flash_offset,
                                                   size_t size) {
  assert(flash_offset % FLASH_SECTOR_SIZE == 0);
  assert(size % FLASH_SECTOR_SIZE == 0);

  blockdevice_t *device = (blockdevice_t *)calloc(1, sizeof(blockdevice_t));
  if (device == NULL) {
    fprintf(stderr, "partition_flash_block_device_create: Out of memory\n");
    return NULL;
  }

  partition_flash_config_t *config =
      (partition_flash_config_t *)calloc(1, sizeof(partition_flash_config_t));
  if (config == NULL) {
    fprintf(stderr, "partition_flash_block_device_create: Out of memory\n");
    free(device);
    return NULL;
  }

  device->init = partition_init;
  device->deinit = partition_deinit;
  device->read = partition_read;
  device->erase = partition_erase;
  device->program = partition_program;
  device->trim = partition_trim;
  device->sync = partition_sync;
  device->size = partition_size;
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

void partition_flash_block_device_free(blockdevice_t *device) {
  if (device) {
    free(device->config);
    free(device);
  }
}

} // namespace musin::filesystem
