// A zip reader (zip.h).

#include "zip.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

#include "iso.h"

// stb_image's zlib decoder: only the PNG path is compiled, which brings it.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_FAILURE_STRINGS
#include <stb_image.h>

namespace launcher::zip {
namespace {

constexpr uint32_t kEndOfCentralDirectory = 0x06054b50;
constexpr uint32_t kCentralDirectoryEntry = 0x02014b50;
constexpr uint32_t kLocalHeader = 0x04034b50;

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

}  // namespace

bool Archive::Open(const std::filesystem::path& path, std::string* error) {
  entries_.clear();
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    *error = "cannot open the archive";
    return false;
  }
  data_.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  size_t size = data_.size();
  if (size < 22) {
    *error = "not a zip archive";
    return false;
  }
  // The end of central directory record, in the last 64 KB (its comment can be that long).
  size_t end = SIZE_MAX;
  for (size_t at = size - 22 + 1; at-- > 0 && size - at <= 22 + 65535;) {
    if (U32(&data_[at]) == kEndOfCentralDirectory) {
      end = at;
      break;
    }
  }
  if (end == SIZE_MAX) {
    *error = "not a zip archive";
    return false;
  }
  uint16_t count = U16(&data_[end + 10]);
  uint32_t directory_size = U32(&data_[end + 12]);
  uint32_t directory_offset = U32(&data_[end + 16]);
  if (directory_offset == 0xFFFFFFFF || count == 0xFFFF || uint64_t(directory_offset) + directory_size > end) {
    *error = "unsupported zip archive (ZIP64)";
    return false;
  }
  size_t at = directory_offset;
  for (uint16_t i = 0; i < count; ++i) {
    if (at + 46 > end || U32(&data_[at]) != kCentralDirectoryEntry) {
      *error = "damaged zip archive";
      return false;
    }
    Entry entry;
    uint16_t flags = U16(&data_[at + 8]);
    entry.method = U16(&data_[at + 10]);
    entry.crc = U32(&data_[at + 16]);
    entry.compressed = U32(&data_[at + 20]);
    entry.size = U32(&data_[at + 24]);
    uint16_t name_length = U16(&data_[at + 28]);
    uint16_t extra_length = U16(&data_[at + 30]);
    uint16_t comment_length = U16(&data_[at + 32]);
    entry.local_offset = U32(&data_[at + 42]);
    if (at + 46 + name_length > end) {
      *error = "damaged zip archive";
      return false;
    }
    entry.name.assign(reinterpret_cast<const char*>(&data_[at + 46]), name_length);
    if ((flags & 1) || entry.compressed == 0xFFFFFFFF || entry.size == 0xFFFFFFFF || entry.local_offset == 0xFFFFFFFF) {
      *error = "unsupported zip entry (encrypted or ZIP64): " + entry.name;
      return false;
    }
    entries_.push_back(std::move(entry));
    at += 46 + name_length + extra_length + comment_length;
  }
  return true;
}

bool Archive::Extract(const Entry& entry, std::vector<uint8_t>& out, std::string* error) const {
  size_t at = size_t(entry.local_offset);
  if (at + 30 > data_.size() || U32(&data_[at]) != kLocalHeader) {
    *error = "damaged zip archive: " + entry.name;
    return false;
  }
  size_t start = at + 30 + U16(&data_[at + 26]) + U16(&data_[at + 28]);
  if (start + entry.compressed > data_.size() || entry.size > 0x7FFFFFFF || entry.compressed > 0x7FFFFFFF) {
    *error = "damaged zip archive: " + entry.name;
    return false;
  }
  out.resize(size_t(entry.size));
  const char* source = reinterpret_cast<const char*>(&data_[start]);
  if (entry.method == 0) {
    if (entry.compressed != entry.size) {
      *error = "damaged zip archive: " + entry.name;
      return false;
    }
    std::memcpy(out.data(), source, out.size());
  } else if (entry.method == 8) {
    // The decoder wants room for at least one byte.
    std::vector<uint8_t> room;
    char* target = reinterpret_cast<char*>(out.data());
    if (out.empty()) {
      room.resize(1);
      target = reinterpret_cast<char*>(room.data());
    }
    int written = stbi_zlib_decode_noheader_buffer(target, int(std::max<size_t>(out.size(), 1)), source,
                                                   int(entry.compressed));
    if (written < 0 || size_t(written) != out.size()) {
      *error = "damaged zip archive: " + entry.name;
      return false;
    }
  } else {
    *error = "unsupported compression in " + entry.name;
    return false;
  }
  if (iso::Crc32(out.data(), out.size()) != entry.crc) {
    *error = "damaged zip archive (CRC): " + entry.name;
    return false;
  }
  return true;
}

}  // namespace launcher::zip
