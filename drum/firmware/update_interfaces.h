#ifndef DRUM_FIRMWARE_UPDATE_INTERFACES_H
#define DRUM_FIRMWARE_UPDATE_INTERFACES_H

#include <cstddef>
#include <cstdint>

#include "etl/optional.h"
#include "etl/span.h"

namespace drum::firmware {

struct PartitionRegion {
  std::uint32_t offset;
  std::uint32_t length;
};

struct FirmwareImageMetadata {
  std::uint8_t format_version;
  std::uint8_t partition_hint;
  std::uint32_t declared_size;
  std::uint32_t checksum;
  std::uint32_t version_tag;
};

enum class PartitionError : std::uint8_t {
  None = 0,
  InvalidHint,
  OutOfSpace,
  BootRomFailure,
  Busy,
  UnexpectedState
};

class FirmwarePartitionManager {
public:
  virtual ~FirmwarePartitionManager() = default;

  virtual etl::optional<PartitionRegion>
  begin_staging(const FirmwareImageMetadata &metadata) = 0;

  virtual void abort_staging() = 0;

  virtual PartitionError
  commit_staging(const FirmwareImageMetadata &metadata) = 0;
};

class PartitionFlashWriter {
public:
  virtual ~PartitionFlashWriter() = default;

  virtual std::size_t page_size_bytes() const = 0;
  virtual std::size_t max_chunk_size_bytes() const = 0;

  virtual bool begin(const PartitionRegion &region,
                     const FirmwareImageMetadata &metadata) = 0;

  virtual bool write_chunk(const etl::span<const std::uint8_t> &chunk) = 0;

  virtual bool finalize() = 0;

  virtual void cancel() = 0;

  virtual std::uint32_t bytes_written() const = 0;
};

} // namespace drum::firmware

#endif
