#ifndef PARTITION_MANAGER_H_B2QXRLNK
#define PARTITION_MANAGER_H_B2QXRLNK

#include "musin/hal/logger.h"
#include <cstdint>
#include <optional>

extern "C" {
typedef struct blockdevice blockdevice_t;
}

namespace musin::filesystem {

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
  static uint8_t pt_work_area_[3264];
  bool partition_table_loaded_;

  bool load_partition_table();
};

} // namespace musin::filesystem

#endif /* end of include guard: PARTITION_MANAGER_H_B2QXRLNK */