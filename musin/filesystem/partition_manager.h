#ifndef MUSIN_FILESYSTEM_PARTITION_MANAGER_H_
#define MUSIN_FILESYSTEM_PARTITION_MANAGER_H_

#include "musin/hal/logger.h"
#include <cstdint>
#include <optional>

namespace musin::filesystem {

struct PartitionInfo {
  uint32_t offset;
  uint32_t size;
};

class PartitionManager {
public:
  explicit PartitionManager(musin::Logger &logger);

  std::optional<PartitionInfo> find_data_partition();

private:
  musin::Logger &logger_;
  bool partition_table_loaded_ = false;

  bool load_partition_table();
  void log_boot_diagnostics();

  static uint8_t
      pt_work_area_[3264]; // Work area for bootrom partition functions
};

} // namespace musin::filesystem

#endif // MUSIN_FILESYSTEM_PARTITION_MANAGER_H_
