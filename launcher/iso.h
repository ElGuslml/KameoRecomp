// Xbox 360 disc images (XDVDFS): the game partition's files, read from a full disc image (XGD1, XGD2, XGD3,
// as Redump dumps them) or a trimmed one (xiso). The launcher's first run extracts the game from one
// (install.cpp). Nothing here is specific to a game.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace iso {

struct File {
  std::string path;  // relative, '/' separators, UTF-8 (XDVDFS names are 8-bit)
  uint64_t offset;   // in the image file
  uint64_t size;
};

struct Image {
  uint64_t partition = 0;  // where the game partition starts in the image file
  std::vector<std::string> directories;
  std::vector<File> files;
  uint64_t total_bytes = 0;
};

enum class Status {
  kOk,
  kCannotOpen,  // the file cannot be opened or read
  kNotXdvdfs,   // no game partition: not an Xbox (360) disc image
  kDamaged,     // a partition whose directories or files do not fit in the image, or unsafe names
};

// Reads the image's directory tree.
Status Read(const std::filesystem::path& path, Image& image);

// Reads one file of the image (small ones: the executable to check it).
bool ReadFile(const std::filesystem::path& path, const File& file, std::vector<uint8_t>& out);

// CRC-32 (IEEE), as the game checks its executable (src/startup.cpp).
uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc = 0);

// The title ID in an XEX2 executable's execution info, or 0.
uint32_t XexTitleId(const std::vector<uint8_t>& xex);

}  // namespace iso
