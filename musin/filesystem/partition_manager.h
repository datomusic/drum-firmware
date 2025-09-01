#ifndef MUSIN_FILESYSTEM_PARTITION_MANAGER_H_
#define MUSIN_FILESYSTEM_PARTITION_MANAGER_H_

#include "etl/string.h"
#include "etl/string_view.h"
#include "etl/vector.h"
#include "musin/hal/logger.h"
#include <cstdint>
#include <optional>

namespace musin::filesystem {

struct PartitionInfo {
  uint32_t offset;
  uint32_t size;
  uint32_t flags_and_permissions;
  uint64_t partition_id;
  etl::string<128> name; // Fixed-size embedded string
  bool has_id;
  bool has_name;

  etl::string_view get_name() const noexcept {
    return etl::string_view(name.c_str(), name.size());
  }
};

class PartitionManager {
public:
  explicit PartitionManager(musin::Logger &logger);

  // Current method - keep for backward compatibility
  std::optional<PartitionInfo> find_data_partition();

  // Enhanced partition discovery methods
  std::optional<PartitionInfo> find_partition_by_id(uint32_t partition_id);
  std::optional<PartitionInfo> find_partition_by_name(etl::string_view name);

  // Fixed-size container for embedded safety (max 16 partitions per RP2350
  // spec)
  using PartitionList = etl::vector<PartitionInfo, 16>;
  PartitionList list_all_partitions();

  // Partition table status
  bool has_partition_table() const noexcept;
  int get_partition_count() const noexcept;

private:
  musin::Logger &logger_;
  bool partition_table_loaded_ = false;
  bool has_partition_table_ = false;
  int partition_count_ = 0;

  bool load_partition_table();
  void log_boot_diagnostics();
  std::optional<PartitionInfo> get_partition_info(uint32_t partition_index);

  // Work area for bootrom partition functions
  // Size calculation based on partition_info.c reference implementation:
  // PARTITION_TABLE_FIXED_INFO_SIZE = 4 + PARTITION_TABLE_MAX_PARTITIONS(16) *
  // 4 = 68 bytes Plus variable data: PARTITION_EXTRA_FAMILY_ID_MAX(8) * 4 +
  // name buffer = ~3200 bytes Total ~3264 bytes needed for worst case partition
  // table operations
  static constexpr size_t PT_WORK_AREA_SIZE = 3264;
  static uint8_t pt_work_area_[PT_WORK_AREA_SIZE];
};

} // namespace musin::filesystem

#endif // MUSIN_FILESYSTEM_PARTITION_MANAGER_H_
