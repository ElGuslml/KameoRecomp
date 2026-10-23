// Xbox 360 disc images (iso.h).
//
// The layout, checked on Redump dumps (POTCAWERecomp's, Kameo's and GRAW's):
// - the game partition starts at 0x18300000 (XGD1), 0xFD90000 (XGD2) or 0x2080000 (XGD3) in a full disc image, at 0
//   in a trimmed one;
// - its volume descriptor is sector 32 of the partition (2,048-byte sectors): "MICROSOFT*XBOX*MEDIA" at the start
//   and at 0x7EC, the root directory's sector and size (u32 little endian) at 0x14;
// - a directory is a binary tree of entries: u16 left and right offsets in 4-byte units (0 = none), u32 start
//   sector, u32 size, u8 attributes (0x10 = directory), u8 name length, the name; 0xFFFF in both offsets pads a
//   sector's end.

#include "iso.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstring>
#include <set>
#include <utility>

namespace iso {
namespace {

constexpr uint64_t kSector = 2048;
constexpr char kMagic[] = "MICROSOFT*XBOX*MEDIA";
constexpr uint64_t kPartitions[] = {0, 0xFD90000, 0x2080000, 0x18300000};
constexpr int kMaxDepth = 32;
constexpr size_t kMaxEntries = 100000;
constexpr uint32_t kMaxDirectorySize = 64 * 1024 * 1024;

class Reader {
 public:
  explicit Reader(const std::filesystem::path& path) {
    handle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_FLAG_RANDOM_ACCESS, nullptr);
    LARGE_INTEGER size;
    if (handle_ != INVALID_HANDLE_VALUE && GetFileSizeEx(handle_, &size)) {
      size_ = uint64_t(size.QuadPart);
    }
  }
  ~Reader() {
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
  }
  bool ok() const { return handle_ != INVALID_HANDLE_VALUE; }
  uint64_t size() const { return size_; }

  bool Read(uint64_t offset, void* out, size_t length) {
    if (offset + length > size_) return false;
    LARGE_INTEGER position;
    position.QuadPart = LONGLONG(offset);
    if (!SetFilePointerEx(handle_, position, nullptr, FILE_BEGIN)) return false;
    auto* bytes = static_cast<uint8_t*>(out);
    while (length) {
      DWORD chunk = DWORD(std::min<size_t>(length, 64u * 1024 * 1024));
      DWORD read = 0;
      if (!::ReadFile(handle_, bytes, chunk, &read, nullptr) || read != chunk) return false;
      bytes += read;
      length -= read;
    }
    return true;
  }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  uint64_t size_ = 0;
};

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint32_t BigU32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// A name that extracts safely: no separators, no "." or "..", no characters Windows refuses.
bool SafeName(const std::string& name) {
  if (name.empty() || name == "." || name == "..") return false;
  for (unsigned char c : name) {
    if (c < 0x20 || std::strchr("<>:\"/\\|?*", c)) return false;
  }
  return name.back() != ' ' && name.back() != '.';
}

// XDVDFS names are 8-bit: their bytes as Latin-1, in UTF-8.
std::string Utf8(const uint8_t* name, size_t length) {
  std::string out;
  for (size_t i = 0; i < length; ++i) {
    uint8_t c = name[i];
    if (c < 0x80) {
      out += char(c);
    } else {
      out += char(0xC0 | (c >> 6));
      out += char(0x80 | (c & 0x3F));
    }
  }
  return out;
}

struct Walk {
  Reader& reader;
  Image& image;
  uint64_t partition;
  bool damaged = false;

  void Directory(uint32_t sector, uint32_t size, const std::string& prefix, int depth) {
    if (size == 0) return;  // an empty directory
    if (depth > kMaxDepth || size > kMaxDirectorySize) {
      damaged = true;
      return;
    }
    std::vector<uint8_t> data(size);
    if (!reader.Read(partition + uint64_t(sector) * kSector, data.data(), size)) {
      damaged = true;
      return;
    }
    // The tree from its root entry; each entry once.
    std::vector<uint32_t> pending = {0};
    std::set<uint32_t> seen;
    while (!pending.empty() && !damaged) {
      uint32_t offset = pending.back();
      pending.pop_back();
      if (!seen.insert(offset).second) continue;
      if (offset + 14 > size) {
        damaged = true;
        return;
      }
      const uint8_t* e = data.data() + offset;
      uint16_t left = U16(e);
      uint16_t right = U16(e + 2);
      if (left == 0xFFFF && right == 0xFFFF) continue;  // padding: an empty directory's only entry
      uint32_t start = U32(e + 4);
      uint32_t length = U32(e + 8);
      uint8_t attributes = e[12];
      uint8_t name_length = e[13];
      if (offset + 14 + name_length > size) {
        damaged = true;
        return;
      }
      std::string name = Utf8(e + 14, name_length);
      if (!SafeName(name) || image.files.size() + image.directories.size() > kMaxEntries) {
        damaged = true;
        return;
      }
      if (left) pending.push_back(uint32_t(left) * 4);
      if (right) pending.push_back(uint32_t(right) * 4);
      std::string path = prefix + name;
      if (attributes & 0x10) {
        image.directories.push_back(path);
        Directory(start, length, path + "/", depth + 1);
      } else {
        uint64_t file_offset = partition + uint64_t(start) * kSector;
        if (file_offset + length > reader.size()) {
          damaged = true;
          return;
        }
        image.files.push_back({path, file_offset, length});
        image.total_bytes += length;
      }
    }
  }
};

}  // namespace

Status Read(const std::filesystem::path& path, Image& image) {
  image = {};
  Reader reader(path);
  if (!reader.ok()) return Status::kCannotOpen;
  for (uint64_t partition : kPartitions) {
    std::array<uint8_t, kSector> volume;
    if (!reader.Read(partition + 32 * kSector, volume.data(), volume.size())) continue;
    if (std::memcmp(volume.data(), kMagic, 20) != 0 || std::memcmp(volume.data() + 0x7EC, kMagic, 20) != 0) continue;
    image.partition = partition;
    Walk walk{reader, image, partition};
    walk.Directory(U32(volume.data() + 0x14), U32(volume.data() + 0x18), "", 0);
    return walk.damaged || image.files.empty() ? Status::kDamaged : Status::kOk;
  }
  return Status::kNotXdvdfs;
}

bool ReadFile(const std::filesystem::path& path, const File& file, std::vector<uint8_t>& out) {
  Reader reader(path);
  if (!reader.ok() || file.size > 256 * 1024 * 1024) return false;
  out.resize(size_t(file.size));
  return reader.Read(file.offset, out.data(), out.size());
}

uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc) {
  static const std::array<uint32_t, 256> table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  crc = ~crc;
  for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

uint32_t XexTitleId(const std::vector<uint8_t>& xex) {
  // XEX2: the optional headers (key, value) after the header's count at 0x14; key 0x00040006 is the execution
  // info, whose title ID is its fourth word. Big endian.
  if (xex.size() < 0x18 || std::memcmp(xex.data(), "XEX2", 4) != 0) return 0;
  uint32_t count = BigU32(xex.data() + 0x14);
  for (uint32_t i = 0; i < count && 0x20 + uint64_t(i) * 8 <= xex.size(); ++i) {
    const uint8_t* header = xex.data() + 0x18 + i * 8;
    if (BigU32(header) == 0x00040006) {
      uint32_t at = BigU32(header + 4);
      return uint64_t(at) + 16 <= xex.size() ? BigU32(xex.data() + at + 12) : 0;
    }
  }
  return 0;
}

}  // namespace iso
