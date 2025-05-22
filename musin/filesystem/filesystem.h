#ifndef FILESYSTEM_H_A1PWKQIM
#define FILESYSTEM_H_A1PWKQIM

namespace musin::filesystem {

// Forward declarations for opaque C types used by the filesystem
struct filesystem_t;
struct blockdevice_t;

bool init(bool force_format);
bool format_filesystem(filesystem_t *lfs, blockdevice_t *flash);

}

#endif /* end of include guard: FILESYSTEM_H_A1PWKQIM */
