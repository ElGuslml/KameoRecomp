// The launcher's updates (updater.h).

#include "updater.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>

#include <nlohmann/json.hpp>

#include "game.h"
#include "net.h"
#include "zip.h"

namespace launcher {
namespace {

using json = nlohmann::json;

// The files an update replaced but could not delete (in use), for the next start (CleanUpAfterUpdate).
constexpr const wchar_t* kLeftovers = L"update-leftovers.txt";

std::string String(const json& object, const char* key) {
  auto it = object.find(key);
  return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::string Utf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

std::filesystem::path FromUtf8(const std::string& path) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

bool EndsWith(const std::string& text, const std::string& end) {
  return text.size() >= end.size() &&
         std::equal(end.rbegin(), end.rend(), text.rbegin(), [](char a, char b) {
           return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
         });
}

std::string Lower(std::string text) {
  for (char& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

// The SHA-256 of a file in lower-case hex (Windows' CNG), or empty.
std::string Sha256(const std::filesystem::path& path) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::string result;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return result;
  if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
    std::ifstream file(path, std::ios::binary);
    std::vector<char> buffer(1024 * 1024);
    bool ok = bool(file);
    while (ok && file) {
      file.read(buffer.data(), std::streamsize(buffer.size()));
      std::streamsize got = file.gcount();
      if (got > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(got), 0) != 0) ok = false;
    }
    unsigned char digest[32];
    if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
      static const char kHex[] = "0123456789abcdef";
      for (unsigned char b : digest) {
        result += kHex[b >> 4];
        result += kHex[b & 15];
      }
    }
    BCryptDestroyHash(hash);
  }
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return result;
}

// A path inside the archive's folder that is safe to write under the launcher's: relative, without "..", drive
// letters or control characters.
bool SafeRelative(const std::string& name) {
  if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos) return false;
  size_t start = 0;
  while (start <= name.size()) {
    size_t end = name.find('/', start);
    std::string part = name.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty() || part == "." || part == ".." || part.find('\\') != std::string::npos) return false;
    for (char c : part) {
      if (static_cast<unsigned char>(c) < 0x20) return false;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}

// What an update never writes: the game's files, the user's data and settings.
bool Kept(const std::string& relative) {
  std::string first = Lower(relative.substr(0, relative.find('/')));
  return first == "assets" || first == "userdata" || first == "logs" || first == "update" || EndsWith(relative, ".toml");
}

}  // namespace

Version Version::Parse(const std::string& text) {
  Version v;
  size_t at = 0;
  if (at < text.size() && (text[at] == 'v' || text[at] == 'V')) ++at;
  int parts[3] = {0, 0, 0};
  int count = 0;
  while (count < 3 && at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) {
    int value = 0;
    while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])) && value < 100000) {
      value = value * 10 + (text[at++] - '0');
    }
    parts[count++] = value;
    if (at < text.size() && text[at] == '.') {
      ++at;
    } else {
      break;
    }
  }
  v.major = parts[0];
  v.minor = parts[1];
  v.patch = parts[2];
  v.valid = count >= 2;
  return v;
}

const Release* UpdateCheck::Newer(const Version& running) const {
  return !releases.empty() && running < releases.front().version ? &releases.front() : nullptr;
}

const Release* UpdateCheck::Of(const Version& version) const {
  for (const Release& release : releases) {
    if (release.version == version) return &release;
  }
  return nullptr;
}

UpdateCheck CheckForUpdates(const std::string& url) {
  UpdateCheck check;
  net::Response response = net::Get(url, "application/vnd.github+json");
  if (response.status != 200) {
    check.rate_limited = response.rate_limited;
    check.error = !response.error.empty() ? response.error : "HTTP " + std::to_string(response.status);
    return check;
  }
  json answer = json::parse(response.body, nullptr, false);
  if (answer.is_discarded() || !(answer.is_array() || answer.is_object())) {
    check.error = "unexpected answer";
    return check;
  }
  if (answer.is_object()) {
    answer = json::array({answer});  // one release (releases/latest)
  }
  for (const json& item : answer) {
    if (!item.is_object() || item.value("draft", false) || item.value("prerelease", false)) continue;
    Release release;
    release.tag = String(item, "tag_name");
    release.version = Version::Parse(release.tag);
    if (!release.version.valid) continue;
    release.name = String(item, "name");
    release.notes = String(item, "body");
    release.published = String(item, "published_at");
    release.page = String(item, "html_url");
    auto assets = item.find("assets");
    if (assets != item.end() && assets->is_array()) {
      // The zip (the Windows one if there are several), and its .sha256 if published.
      for (const json& asset : *assets) {
        std::string name = String(asset, "name");
        if (!EndsWith(name, ".zip")) continue;
        if (!release.asset_name.empty() && name.find("windows") == std::string::npos) continue;
        release.asset_name = name;
        release.asset_url = String(asset, "browser_download_url");
        release.asset_size = asset.value("size", uint64_t(0));
        std::string digest = String(asset, "digest");
        release.sha256 = digest.rfind("sha256:", 0) == 0 ? Lower(digest.substr(7)) : std::string();
      }
      for (const json& asset : *assets) {
        if (!release.asset_name.empty() && String(asset, "name") == release.asset_name + ".sha256") {
          release.sha256_url = String(asset, "browser_download_url");
        }
      }
    }
    check.releases.push_back(std::move(release));
  }
  std::sort(check.releases.begin(), check.releases.end(),
            [](const Release& a, const Release& b) { return b.version < a.version; });
  check.ok = true;
  return check;
}

Update::~Update() {
  Cancel();
}

void Update::Start(const Release& release, const std::filesystem::path& exe_dir) {
  Cancel();
  cancel_ = false;
  {
    std::lock_guard lock(mutex_);
    progress_ = {};
    progress_.state = State::kDownloading;
    progress_.total = release.asset_size;
  }
  worker_ = std::thread(&Update::Run, this, release, exe_dir);
}

void Update::Cancel() {
  cancel_ = true;
  if (worker_.joinable()) worker_.join();
}

Update::Progress Update::Get() const {
  std::lock_guard lock(mutex_);
  return progress_;
}

void Update::Fail(const std::string& error, Problem problem, const std::string& detail) {
  std::lock_guard lock(mutex_);
  progress_.state = State::kFailed;
  progress_.error = error;
  progress_.problem = problem;
  progress_.detail = detail;
}

void Update::Set(State state) {
  std::lock_guard lock(mutex_);
  progress_.state = state;
}

void Update::Run(Release release, std::filesystem::path exe_dir) {
  std::error_code ec;
  std::filesystem::path folder = exe_dir / L"update";
  std::filesystem::path staging = folder / L"files";
  std::filesystem::remove_all(folder, ec);
  std::filesystem::create_directories(staging, ec);
  if (ec) {
    Fail("cannot write " + Utf8(folder));
    return;
  }
  auto clean = [&] { std::filesystem::remove_all(folder, ec); };
  if (release.asset_url.empty() || !SafeRelative(release.asset_name)) {
    Fail("the release has no archive");
    clean();
    return;
  }

  // The download.
  std::filesystem::path archive_path = folder / FromUtf8(release.asset_name);
  std::string error;
  bool downloaded = net::Download(
      release.asset_url, archive_path,
      [this](uint64_t done, uint64_t total) {
        std::lock_guard lock(mutex_);
        progress_.done = done;
        if (total) progress_.total = total;
      },
      cancel_, &error);
  if (!downloaded) {
    if (cancel_) {
      Set(State::kCancelled);
    } else {
      Fail(error);
    }
    clean();
    return;
  }

  // Its SHA-256: GitHub's digest of the asset, or the .sha256 published with it.
  Set(State::kVerifying);
  std::string expected = release.sha256;
  if (expected.empty() && !release.sha256_url.empty()) {
    net::Response sum = net::Get(release.sha256_url);
    if (sum.status == 200) expected = Lower(sum.body.substr(0, std::min<size_t>(64, sum.body.size())));
  }
  if (expected.size() != 64) {
    Fail("the release gives no SHA-256 for its archive", Problem::kNoChecksum);
    clean();
    return;
  }
  if (Sha256(archive_path) != expected) {
    Fail("the download does not match its SHA-256", Problem::kChecksumMismatch);
    clean();
    return;
  }

  // Extracted into update\files, without the archive's top folder.
  Set(State::kInstalling);
  zip::Archive archive;
  if (!archive.Open(archive_path, &error)) {
    Fail(error);
    clean();
    return;
  }
  std::string prefix;
  if (!archive.entries().empty()) {
    const std::string& first = archive.entries().front().name;
    size_t slash = first.find('/');
    if (slash != std::string::npos) {
      prefix = first.substr(0, slash + 1);
      for (const zip::Entry& entry : archive.entries()) {
        if (entry.name.rfind(prefix, 0) != 0) {
          prefix.clear();
          break;
        }
      }
    }
  }
  std::vector<std::string> files;
  std::vector<uint8_t> data;
  for (const zip::Entry& entry : archive.entries()) {
    if (entry.directory()) continue;
    std::string relative = entry.name.substr(prefix.size());
    if (relative.empty()) continue;
    if (!SafeRelative(relative)) {
      Fail("unsafe path in the archive: " + entry.name);
      clean();
      return;
    }
    if (Kept(relative)) continue;
    if (!archive.Extract(entry, data, &error)) {
      Fail(error);
      clean();
      return;
    }
    std::filesystem::path target = staging / FromUtf8(relative);
    std::filesystem::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out || !out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()))) {
      Fail("cannot write " + relative);
      clean();
      return;
    }
    files.push_back(relative);
  }
  auto has = [&](const char* name) { return std::find(files.begin(), files.end(), name) != files.end(); };
  if (!has(game::kLauncherExecutable) || !has(game::kRecompExecutable)) {
    Fail("the archive is not a release of this game");
    clean();
    return;
  }

  // Into place: each file replaced is renamed *.old-update first, and put back if any step fails.
  struct Moved {
    std::filesystem::path target, backup;
  };
  std::vector<Moved> moved;
  auto rollback = [&] {
    for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
      std::filesystem::remove(it->target, ec);
      if (!it->backup.empty()) MoveFileExW(it->backup.c_str(), it->target.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
  };
  for (const std::string& relative : files) {
    std::filesystem::path target = exe_dir / FromUtf8(relative);
    std::filesystem::path source = staging / FromUtf8(relative);
    std::filesystem::create_directories(target.parent_path(), ec);
    Moved step{target, {}};
    if (std::filesystem::exists(target, ec)) {
      // A backup left by an earlier update that is still in use (that update's launcher) gets a new name.
      std::filesystem::path backup = target;
      backup += L".old-update";
      for (int n = 2; std::filesystem::exists(backup, ec) && !std::filesystem::remove(backup, ec); ++n) {
        backup = target;
        backup += L".old-update" + std::to_wstring(n);
      }
      if (!MoveFileExW(target.c_str(), backup.c_str(), 0)) {
        DWORD code = GetLastError();
        bool in_use = code == ERROR_SHARING_VIOLATION || code == ERROR_ACCESS_DENIED || code == ERROR_LOCK_VIOLATION;
        Fail("cannot replace " + relative + " (" + std::to_string(code) + ")",
             in_use ? Problem::kInUse : Problem::kOther, relative);
        rollback();
        clean();
        return;
      }
      step.backup = backup;
    }
    moved.push_back(step);
    if (!MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
      Fail("cannot write " + relative + " (" + std::to_string(GetLastError()) + ")");
      rollback();
      clean();
      return;
    }
  }
  // The replaced files go now; those still open (the running launcher) at the next start, from a list.
  std::vector<std::string> leftovers;
  for (const Moved& step : moved) {
    if (!step.backup.empty() && !std::filesystem::remove(step.backup, ec)) {
      leftovers.push_back(Utf8(step.backup.lexically_relative(exe_dir)));
    }
  }
  if (!leftovers.empty()) {
    std::ofstream out(exe_dir / kLeftovers, std::ios::app);
    for (const std::string& line : leftovers) {
      std::string slashes = line;
      std::replace(slashes.begin(), slashes.end(), '\\', '/');
      out << slashes << "\n";
    }
  }
  clean();
  Set(State::kDone);
}

void CleanUpAfterUpdate(const std::filesystem::path& exe_dir) {
  std::error_code ec;
  std::filesystem::remove_all(exe_dir / L"update", ec);
  std::filesystem::path list = exe_dir / kLeftovers;
  std::ifstream in(list);
  if (!in) return;
  std::vector<std::string> remaining;
  for (std::string line; std::getline(in, line);) {
    if (line.empty() || !SafeRelative(line)) continue;
    std::filesystem::path path = exe_dir / FromUtf8(line);
    if (std::filesystem::exists(path, ec) && !std::filesystem::remove(path, ec)) remaining.push_back(line);
  }
  in.close();
  if (remaining.empty()) {
    std::filesystem::remove(list, ec);
  } else {
    std::ofstream out(list, std::ios::trunc);
    for (const std::string& line : remaining) out << line << "\n";
  }
}

}  // namespace launcher
