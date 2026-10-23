// The game's achievements for the launcher (achievements.h).

#include "achievements.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string_view>

#include <toml.hpp>

#include "game.h"

namespace launcher {
namespace {

// XLanguage -> the key of its texts in the list.
const char* LanguageKey(uint32_t language) {
  constexpr const char* kKeys[] = {"en", "en", "ja", "de", "fr", "es", "it", "ko", "zh-Hant", "pt", "zh-Hans", "pl", "ru"};
  return language < std::size(kKeys) ? kKeys[language] : "en";
}

// The game's user data folder: userdata\ next to it, or Documents\<game::kDocumentsFolder> (src/startup.cpp,
// ConfigureUserData).
// The one that has the list.
std::filesystem::path AchievementsFolder(const std::filesystem::path& exe_dir) {
  wchar_t name[16];
  std::swprintf(name, std::size(name), L"%08X.info.toml", game::kTitleId);
  std::error_code ec;
  std::filesystem::path portable = exe_dir / L"userdata" / L"achievements";
  if (std::filesystem::exists(portable / name, ec)) {
    return portable;
  }
  PWSTR documents = nullptr;
  std::filesystem::path result;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
    std::filesystem::path other = std::filesystem::path(documents) / game::kDocumentsFolder / L"achievements";
    if (std::filesystem::exists(other / name, ec)) {
      result = other;
    }
  }
  CoTaskMemFree(documents);
  return result.empty() ? portable : result;
}

// A PNG (or any image WIC reads) as RGBA pixels.
bool DecodeImage(IWICImagingFactory* factory, const std::filesystem::path& path, Achievement& out) {
  IWICBitmapDecoder* decoder = nullptr;
  if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                &decoder))) {
    return false;
  }
  bool ok = false;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICBitmapSource* converted = nullptr;
  UINT width = 0, height = 0;
  if (SUCCEEDED(decoder->GetFrame(0, &frame)) &&
      SUCCEEDED(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &converted)) &&
      SUCCEEDED(converted->GetSize(&width, &height)) && width > 0 && height > 0 && width <= 1024 && height <= 1024) {
    out.icon.resize(size_t(width) * height * 4);
    ok = SUCCEEDED(converted->CopyPixels(nullptr, width * 4, UINT(out.icon.size()), out.icon.data()));
    out.icon_width = int(width);
    out.icon_height = int(height);
  }
  if (!ok) {
    out.icon.clear();
  }
  if (converted) converted->Release();
  if (frame) frame->Release();
  decoder->Release();
  return ok;
}

// A TOML file, or nullopt when it is missing or cannot be parsed (being written).
std::optional<toml::table> ReadToml(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  try {
    return toml::parse(content);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::filesystem::path FromUtf8(std::string_view path) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

}  // namespace

std::optional<Achievements> LoadAchievements(const std::filesystem::path& exe_dir, uint32_t language) {
  std::filesystem::path folder = AchievementsFolder(exe_dir);
  wchar_t title[16];
  std::swprintf(title, std::size(title), L"%08X", game::kTitleId);
  std::optional<toml::table> read = ReadToml(folder / (std::wstring(title) + L".info.toml"));
  if (!read) {
    return std::nullopt;
  }
  const toml::table& info = *read;
  const toml::array* list = info["achievements"].as_array();
  if (!list) {
    return std::nullopt;
  }

  // The texts: the launcher's language, else the game's default, else English, else any.
  std::string wanted = LanguageKey(language);
  std::string fallback = info["default_language"].value_or<std::string>("en");
  auto text = [&](const toml::table& achievement, const char* field) -> std::string {
    const toml::table* texts = achievement[field].as_table();
    if (!texts) {
      return {};
    }
    for (const std::string& key : {wanted, fallback, std::string("en")}) {
      if (auto value = (*texts)[key].value<std::string>()) {
        return *value;
      }
    }
    for (const auto& [key, value] : *texts) {
      if (auto string = value.value<std::string>()) {
        return *string;
      }
    }
    return {};
  };

  // The unlocks.
  toml::table unlocks = ReadToml(folder / (std::wstring(title) + L".toml")).value_or(toml::table{});  // none yet
  const toml::table* unlocked = unlocks["unlocked"].as_table();

  HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IWICImagingFactory* factory = nullptr;
  CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

  Achievements result;
  for (const toml::node& node : *list) {
    const toml::table* entry = node.as_table();
    if (!entry) {
      continue;
    }
    Achievement a;
    a.id = uint32_t((*entry)["id"].value_or<int64_t>(0));
    a.gamerscore = uint32_t((*entry)["gamerscore"].value_or<int64_t>(0));
    a.flags = uint32_t((*entry)["flags"].value_or<int64_t>(0));
    a.label = text(*entry, "label");
    a.description = text(*entry, "description");
    a.unachieved_description = text(*entry, "unachieved_description");
    if (unlocked) {
      if (const toml::table* unlock = (*unlocked)[std::to_string(a.id)].as_table()) {
        a.unlocked = uint64_t((*unlock)["filetime"].value_or<int64_t>(0));
        if (!a.unlocked) {
          a.unlocked = 1;  // unlocked at an unknown time
        }
      }
    }
    if (auto icon = (*entry)["icon"].value<std::string>(); icon && factory) {
      std::filesystem::path path = folder / FromUtf8(*icon);
      // Only inside the achievements folder.
      std::filesystem::path relative = path.lexically_normal().lexically_relative(folder.lexically_normal());
      if (!relative.empty() && *relative.begin() != L"..") {
        DecodeImage(factory, path, a);
      }
    }
    result.total_gamerscore += a.gamerscore;
    if (a.unlocked) {
      ++result.unlocked;
      result.gamerscore += a.gamerscore;
    }
    result.items.push_back(std::move(a));
  }
  if (factory) {
    factory->Release();
  }
  if (SUCCEEDED(com)) {
    CoUninitialize();
  }
  return result;
}

bool WriteAchievementList(const std::filesystem::path& exe_dir) {
  std::filesystem::path game = exe_dir / game::kRecompExecutable;
  std::wstring command_line = L"\"" + game.wstring() + L"\" --achievements_cache_only=true";
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(game.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      exe_dir.c_str(), &startup, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  if (WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, 5000);
  }
  CloseHandle(process.hProcess);
  return true;
}

std::string FormatDate(uint64_t filetime, uint32_t language) {
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;  // 1970-01-01 in FILETIME units (100 ns)
  if (filetime < kUnixEpoch) {
    return {};
  }
  std::time_t seconds = std::time_t((filetime - kUnixEpoch) / 10'000'000ULL);
  std::tm local = {};
  if (localtime_s(&local, &seconds) != 0) {
    return {};
  }
  char text[32];
  if (language == 1) {  // English: month first
    std::snprintf(text, sizeof(text), "%02d/%02d/%04d", local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);
  } else if (language == 3) {  // German
    std::snprintf(text, sizeof(text), "%02d.%02d.%04d", local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
  } else {
    std::snprintf(text, sizeof(text), "%02d/%02d/%04d", local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
  }
  return text;
}

}  // namespace launcher
