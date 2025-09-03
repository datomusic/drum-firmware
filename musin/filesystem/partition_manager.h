#ifndef PARTITION_MANAGER_H_B2QXRLNK
#define PARTITION_MANAGER_H_B2QXRLNK

#include "musin/hal/logger.h"
#include <cstdint>
#include <optional>

// clang-format off
extern "C" {
#include "pico.h"
#include "boot/bootrom_constants.h"
#include "hardware/flash.h"
typedef struct blockdevice blockdevice_t;
}
// clang-format on

namespace musin::filesystem {

// Partition table constants
static constexpr uint32_t PARTITION_LOCATION_AND_FLAGS_SIZE = 2;
static constexpr uint32_t PARTITION_ID_SIZE = 2;
static constexpr uint32_t PARTITION_NAME_MAX =
    127; // name length is indicated by 7 bits
// Note: PARTITION_TABLE_MAX_PARTITIONS is defined in SDK as 16
static constexpr uint32_t PARTITION_TABLE_FIXED_INFO_SIZE =
    4 + PARTITION_TABLE_MAX_PARTITIONS *
            (PARTITION_LOCATION_AND_FLAGS_SIZE + PARTITION_ID_SIZE);
static constexpr uint32_t PARTITION_WORK_AREA_SIZE =
    PARTITION_TABLE_FIXED_INFO_SIZE;
static constexpr uint32_t SYS_INFO_BUFFER_SIZE = 8;
static constexpr uint32_t PARTITION_TABLE_INFO_BUFFER_SIZE = 256;
static constexpr uint32_t PARTITION_COUNT_MASK = 0x000000FF;
static constexpr uint32_t HAS_PARTITION_TABLE_FLAG = 0x00000100;
static constexpr uint32_t PARTITION_ID_DISPLAY_MASK = 0xFFFFFFFF;
static constexpr uint32_t FLASH_SECTOR_ALIGNMENT_MASK = FLASH_SECTOR_SIZE - 1;

struct PartitionInfo {
  uint32_t offset;
  uint32_t size;
  uint32_t flags_and_permissions;
};

class PartitionManager {
public:
  explicit PartitionManager(musin::Logger &logger);

  /**
   * @brief Discovers and returns information about a specific partition.
   *
   * @param partition_id The partition ID to find (e.g., 2 for Data partition).
   * @return Optional PartitionInfo if partition is found, std::nullopt
   * otherwise.
   */
  std::optional<PartitionInfo> find_partition(uint32_t partition_id);

  /**
   * @brief Discovers and returns information about the first partition of a
   * specific family type.
   *
   * @param family_bit The family bit to search for (e.g.,
   * PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_DATA_BITS).
   * @return Optional PartitionInfo if partition is found, std::nullopt
   * otherwise.
   */
  std::optional<PartitionInfo> find_partition_by_family(uint32_t family_bit);

  /**
   * @brief Creates a block device for the specified partition.
   *
   * @param partition_info The partition information.
   * @return Pointer to the created block device, or nullptr on failure.
   */
  blockdevice_t *
  create_partition_blockdevice(const PartitionInfo &partition_info);

  /**
   * @brief Performs boot diagnostics to check partition table availability.
   *
   * @return true if partition table is available, false otherwise.
   */
  bool check_partition_table_available();

private:
  musin::Logger &logger_;
  static uint8_t pt_work_area_[PARTITION_WORK_AREA_SIZE];
  bool partition_table_loaded_;

  bool load_partition_table();
};

} // namespace musin::filesystem

#endif /* end of include guard: PARTITION_MANAGER_H_B2QXRLNK */