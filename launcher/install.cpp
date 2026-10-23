// The launcher's first run (install.h).

#include "install.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "game.h"

namespace launcher {
namespace {

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), out.data(), length, nullptr, nullptr);
  return out;
}

std::string SystemMessage(unsigned long code) {
  wchar_t* buffer = nullptr;
  DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr, DWORD(code), 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring text = length ? std::wstring(buffer, length) : L"Error " + std::to_wstring(code);
  LocalFree(buffer);
  while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L'.')) text.pop_back();
  return Utf8(text);
}

std::filesystem::path FromUtf8(const std::string& path) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

struct Handle {
  HANDLE h = INVALID_HANDLE_VALUE;
  ~Handle() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
};

}  // namespace

bool GameInstalled(const std::filesystem::path& exe_dir) {
  std::error_code ec;
  if (std::filesystem::exists(exe_dir / "assets" / game::kDiscExecutable, ec)) return true;
#ifdef LAUNCHER_DEFAULT_GAME_ROOT
  // A development build: the game falls back to the repository's assets\ (src/startup.cpp, DefaultGameRoot).
  if (std::filesystem::exists(std::filesystem::path(LAUNCHER_DEFAULT_GAME_ROOT) / game::kDiscExecutable, ec)) return true;
#endif
  return std::wstring(GetCommandLineW()).find(L"--game_data_root") != std::wstring::npos;
}

IsoCheck CheckIso(const std::filesystem::path& path) {
  IsoCheck check;
  check.path = path;
  std::error_code ec;
  check.file_size = std::filesystem::file_size(path, ec);
  switch (iso::Read(path, check.image)) {
    case iso::Status::kOk:
      break;
    case iso::Status::kNotXdvdfs:
      check.result = IsoCheck::Result::kNotXbox;
      return check;
    default:
      check.result = IsoCheck::Result::kUnreadable;
      return check;
  }
  auto xex = std::find_if(check.image.files.begin(), check.image.files.end(), [](const iso::File& f) {
    return _stricmp(f.path.c_str(), game::kDiscExecutable) == 0;
  });
  if (xex == check.image.files.end()) {
    check.result = IsoCheck::Result::kNoExecutable;
    return check;
  }
  std::vector<uint8_t> data;
  if (!iso::ReadFile(path, *xex, data)) {
    check.result = IsoCheck::Result::kUnreadable;
    return check;
  }
  check.title_id = iso::XexTitleId(data);
  check.crc = iso::Crc32(data.data(), data.size());
  check.result = check.title_id != game::kTitleId        ? IsoCheck::Result::kOtherGame
                 : check.crc != game::kExecutableCrc32 ? IsoCheck::Result::kOtherVersion
                                                       : IsoCheck::Result::kValid;
  return check;
}

uint64_t FreeSpace(const std::filesystem::path& path) {
  ULARGE_INTEGER available;
  return GetDiskFreeSpaceExW(path.c_str(), &available, nullptr, nullptr) ? available.QuadPart : 0;
}

Extraction::~Extraction() { Cancel(); }

void Extraction::Start(const std::filesystem::path& iso_path, const iso::Image& image,
                       const std::filesystem::path& destination) {
  Cancel();
  cancel_ = false;
  {
    std::lock_guard lock(mutex_);
    progress_ = {};
    progress_.state = State::kRunning;
    progress_.total = image.total_bytes;
  }
  worker_ = std::thread(&Extraction::Run, this, iso_path, image, destination);
}

void Extraction::Cancel() {
  cancel_ = true;
  if (worker_.joinable()) worker_.join();
}

Extraction::Progress Extraction::Get() const {
  std::lock_guard lock(mutex_);
  return progress_;
}

void Extraction::Fail(const std::string& path, unsigned long code) {
  std::lock_guard lock(mutex_);
  progress_.state = State::kFailed;
  progress_.error_path = path;
  progress_.error = SystemMessage(code);
}

void Extraction::Run(std::filesystem::path iso_path, iso::Image image, std::filesystem::path destination) {
  // The disc executable last: assets\default.xex appears only when everything else is there.
  std::stable_sort(image.files.begin(), image.files.end(), [](const iso::File& a, const iso::File& b) {
    bool a_last = _stricmp(a.path.c_str(), game::kDiscExecutable) == 0;
    bool b_last = _stricmp(b.path.c_str(), game::kDiscExecutable) == 0;
    return a_last != b_last ? b_last : a.offset < b.offset;  // otherwise in disc order, for sequential reads
  });

  std::vector<std::filesystem::path> written;
  std::filesystem::path partial;
  auto undo = [&] {
    std::error_code ec;
    if (!partial.empty()) std::filesystem::remove(partial, ec);
    for (const auto& file : written) std::filesystem::remove(file, ec);
    // The directories this extraction made, deepest first; remove() leaves one that is not empty (assets\ with its
    // README).
    for (auto it = image.directories.rbegin(); it != image.directories.rend(); ++it) {
      std::filesystem::remove(destination / FromUtf8(*it), ec);
    }
  };

  Handle source;
  source.h = CreateFileW(iso_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (source.h == INVALID_HANDLE_VALUE) {
    Fail(Utf8(iso_path.wstring()), GetLastError());
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(destination, ec);
  for (const std::string& directory : image.directories) {
    std::filesystem::create_directories(destination / FromUtf8(directory), ec);
  }

  constexpr DWORD kChunk = 8 * 1024 * 1024;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[kChunk]);
  uint64_t done = 0;
  for (const iso::File& file : image.files) {
    std::filesystem::path target = destination / FromUtf8(file.path);
    partial = target;
    partial += L".part";
    {
      std::lock_guard lock(mutex_);
      progress_.current = file.path;
    }
    std::filesystem::create_directories(target.parent_path(), ec);
    Handle out;
    out.h = CreateFileW(partial.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (out.h == INVALID_HANDLE_VALUE) {
      Fail(Utf8(target.wstring()), GetLastError());
      partial.clear();
      undo();
      return;
    }
    LARGE_INTEGER position;
    position.QuadPart = LONGLONG(file.offset);
    if (!SetFilePointerEx(source.h, position, nullptr, FILE_BEGIN)) {
      Fail(Utf8(iso_path.wstring()), GetLastError());
      CloseHandle(out.h);
      out.h = INVALID_HANDLE_VALUE;
      undo();
      return;
    }
    uint64_t left = file.size;
    while (left) {
      if (cancel_) {
        CloseHandle(out.h);
        out.h = INVALID_HANDLE_VALUE;
        undo();
        std::lock_guard lock(mutex_);
        progress_.state = State::kCancelled;
        return;
      }
      DWORD chunk = DWORD(std::min<uint64_t>(left, kChunk));
      DWORD read = 0;
      DWORD wrote = 0;
      if (!::ReadFile(source.h, buffer.get(), chunk, &read, nullptr) || read != chunk) {
        Fail(Utf8(iso_path.wstring()), read != chunk && GetLastError() == ERROR_SUCCESS ? ERROR_HANDLE_EOF : GetLastError());
        CloseHandle(out.h);
        out.h = INVALID_HANDLE_VALUE;
        undo();
        return;
      }
      if (!WriteFile(out.h, buffer.get(), chunk, &wrote, nullptr) || wrote != chunk) {
        Fail(Utf8(target.wstring()), GetLastError());
        CloseHandle(out.h);
        out.h = INVALID_HANDLE_VALUE;
        undo();
        return;
      }
      left -= chunk;
      done += chunk;
      std::lock_guard lock(mutex_);
      progress_.done = done;
    }
    CloseHandle(out.h);
    out.h = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(partial.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
      Fail(Utf8(target.wstring()), GetLastError());
      undo();
      return;
    }
    partial.clear();
    written.push_back(target);
  }
  std::lock_guard lock(mutex_);
  progress_.current.clear();
  progress_.state = State::kDone;
}

}  // namespace launcher
