#ifndef DRUM_FIRMWARE_BOOTROM_SERVICES_H
#define DRUM_FIRMWARE_BOOTROM_SERVICES_H

#include "drum/firmware/update_interfaces.h"
#include "musin/hal/logger.h"

#include <cstddef>
#include <cstdint>

#include "etl/array.h"
#include "etl/optional.h"

namespace drum::firmware {

class BootRomFirmwarePartitionManager final : public FirmwarePartitionManager {
public:
  explicit BootRomFirmwarePartitionManager(musin::Logger &logger);

  etl::optional<PartitionRegion>
  begin_staging(const FirmwareImageMetadata &metadata) override;

  void abort_staging() override;

  PartitionError commit_staging(const FirmwareImageMetadata &metadata) override;

private:
  struct SlotInfo {
    PartitionRegion region{0U, 0U};
    bool valid = false;
  };

  bool refresh_partition_layout();
  bool load_partition_table();
  bool parse_partition_table();
  bool determine_active_slot();

  musin::Logger &logger_;
  SlotInfo slot_a_;
  SlotInfo slot_b_;
  bool staging_active_ = false;
  PartitionRegion staging_region_{0U, 0U};
  FirmwareImageMetadata staging_metadata_{};
  std::uint32_t active_slot_id_ = 0U;
};

class BootRomPartitionFlashWriter final : public PartitionFlashWriter {
public:
  explicit BootRomPartitionFlashWriter(musin::Logger &logger);

  std::size_t page_size_bytes() const override;
  std::size_t max_chunk_size_bytes() const override;

  bool begin(const PartitionRegion &region,
             const FirmwareImageMetadata &metadata) override;

  bool write_chunk(const etl::span<const std::uint8_t> &chunk) override;

  bool finalize() override;

  void cancel() override;

  std::uint32_t bytes_written() const override;

  static constexpr std::size_t BUFFER_SIZE = 256U;

private:
  bool ensure_erased(std::uint32_t relative_offset, std::uint32_t length);
  bool flush_buffer();
  void reset_state();
  static constexpr std::uint32_t align_up(std::uint32_t value,
                                          std::uint32_t alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
  }

  musin::Logger &logger_;
  PartitionRegion region_{0U, 0U};
  FirmwareImageMetadata metadata_{};
  bool busy_ = false;
  std::uint32_t bytes_written_ = 0U;
  std::uint32_t erased_bytes_ = 0U;
  std::uint32_t buffer_base_offset_ = 0U;
  std::size_t buffer_count_ = 0U;
  etl::array<std::uint8_t, BUFFER_SIZE> buffer_{};
};

} // namespace drum::firmware

#endif
