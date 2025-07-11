#ifndef FILE_OPS_H_BSDNSIZL
#define FILE_OPS_H_BSDNSIZL

#include "etl/delegate.h"
#include "etl/span.h"
#include "etl/string.h"

template <typename Handle, int BlockSize> struct FileOperations {
  typedef Handle FileHandle;
  typedef etl::string<64> Path;
  typedef etl::delegate<Handle(const Path &path)> Open;

  const Open open;
  etl::delegate<void(Handle handle)> close;
  etl::delegate<size_t(Handle handle, etl::array<uint8_t, BlockSize> &output)>
      read;
  etl::delegate<void(Handle handle,
                     const etl::span<const uint8_t, BlockSize> bytes)>
      write;
};

#endif /* end of include guard: FILE_OPS_H_BSDNSIZL */
