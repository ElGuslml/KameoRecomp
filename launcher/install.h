// The launcher's first run: what a disc image is for this game, and extracting its files next to the
// launcher (assets\) on a thread of its own. The screens are page_install.cpp.

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "iso.h"

namespace launcher {

// The game's files are installed: assets\default.xex next to the launcher, the repository's assets\ of a
// development build (the path the game itself falls back to), or a --game_data_root on the command line.
bool GameInstalled(const std::filesystem::path& exe_dir);

struct IsoCheck {
  enum class Result {
    kValid,
    kUnreadable,    // cannot be opened, or a damaged image
    kNotXbox,       // no Xbox game partition
    kNoExecutable,  // no default.xex at its root
    kOtherGame,     // another title ID
    kOtherVersion,  // this game, another executable (another region or version)
  };
  Result result = Result::kUnreadable;
  std::filesystem::path path;
  uint64_t file_size = 0;
  iso::Image image;
  uint32_t title_id = 0;
  uint32_t crc = 0;
};

// Reads the image and its executable (a few MB). Slow on a slow disk: run it off the UI thread.
IsoCheck CheckIso(const std::filesystem::path& path);

// Free bytes on the volume of `path`, or 0 if unknown.
uint64_t FreeSpace(const std::filesystem::path& path);

class Extraction {
 public:
  enum class State { kIdle, kRunning, kDone, kFailed, kCancelled };
  struct Progress {
    State state = State::kIdle;
    uint64_t done = 0;
    uint64_t total = 0;
    std::string current;  // the file being written, relative, UTF-8
    std::string error;    // with kFailed: the system's message
    std::string error_path;
  };

  ~Extraction();
  // Copies the image's files into `destination`, the disc executable last. Each file is written as name.part and
  // renamed when complete; a failed or cancelled extraction removes what it wrote.
  void Start(const std::filesystem::path& iso_path, const iso::Image& image, const std::filesystem::path& destination);
  // Stops the extraction and waits for its thread.
  void Cancel();
  Progress Get() const;

 private:
  void Run(std::filesystem::path iso_path, iso::Image image, std::filesystem::path destination);
  void Fail(const std::string& path, unsigned long code);

  std::thread worker_;
  mutable std::mutex mutex_;
  Progress progress_;
  std::atomic<bool> cancel_{false};
};

}  // namespace launcher
