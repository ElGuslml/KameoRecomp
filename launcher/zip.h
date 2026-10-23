// A zip reader for the launcher's updates: the release's archive, as scripts/package-release.ps1 writes it (stored or
// deflated entries, no ZIP64, no encryption). Deflate is stb_image's decoder (the SDK tree's stb), the CRC32 the ISO
// reader's.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace launcher::zip {

struct Entry {
  std::string name;  // UTF-8, "/" separators, as stored
  uint16_t method = 0;  // 0 stored, 8 deflated
  uint32_t crc = 0;
  uint64_t compressed = 0;
  uint64_t size = 0;
  uint64_t local_offset = 0;
  bool directory() const { return !name.empty() && name.back() == '/'; }
};

class Archive {
 public:
  // Reads the whole file and its central directory. False with `error` set if it is not a zip this reader takes.
  bool Open(const std::filesystem::path& path, std::string* error);
  const std::vector<Entry>& entries() const { return entries_; }
  // The entry's data, inflated and checked against its CRC32. False with `error` set otherwise.
  bool Extract(const Entry& entry, std::vector<uint8_t>& out, std::string* error) const;

 private:
  std::vector<uint8_t> data_;
  std::vector<Entry> entries_;
};

}  // namespace launcher::zip
