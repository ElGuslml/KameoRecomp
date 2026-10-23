// HTTP requests for the launcher's updates, with WinHTTP (part of Windows, and of Wine and Proton). HTTPS for
// GitHub; plain HTTP works too, for tests against a local server (--launcher_update_url).

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace launcher::net {

struct Response {
  int status = 0;     // the HTTP status; 0 when no answer came
  std::string body;
  std::string error;  // the system's message when no answer came
  bool rate_limited = false;  // GitHub: 403 or 429 with no calls left (x-ratelimit-remaining: 0)
};

// GET into memory, following redirects. `accept`: the Accept header, or null.
Response Get(const std::string& url, const char* accept = nullptr);

// GET into `file`, following redirects (GitHub's assets redirect to their storage). `progress` gets the bytes so far
// and the total (0 if unknown). Stops when `cancel` becomes true. False with `error` set when it did not complete.
bool Download(const std::string& url, const std::filesystem::path& file,
              const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>& cancel,
              std::string* error);

}  // namespace launcher::net
