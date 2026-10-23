// HTTP requests with WinHTTP (net.h).

#include "net.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <vector>

#include "game.h"

namespace launcher::net {
namespace {

std::wstring Wide(const std::string& text) {
  if (text.empty()) return {};
  int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring out(size_t(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length);
  return out;
}

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length, nullptr, nullptr);
  return out;
}

// The system's text for an error, WinHTTP's own included (they are in winhttp.dll).
std::string ErrorText(DWORD code) {
  wchar_t* buffer = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
  HMODULE winhttp = GetModuleHandleW(L"winhttp.dll");
  if (winhttp && code >= 12000 && code < 13000) {
    flags |= FORMAT_MESSAGE_FROM_HMODULE;
  }
  DWORD length = FormatMessageW(flags, winhttp, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring text = length ? std::wstring(buffer, length) : L"Error " + std::to_wstring(code);
  LocalFree(buffer);
  while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L'.' || text.back() == L' ')) {
    text.pop_back();
  }
  return Utf8(text);
}

struct Handle {
  HINTERNET h = nullptr;
  ~Handle() {
    if (h) WinHttpCloseHandle(h);
  }
};

// One request, up to the response's headers. The caller reads the body.
struct Request {
  Handle session, connection, request;
  int status = 0;
  uint64_t length = 0;  // Content-Length, 0 if absent
  bool rate_limited = false;
  std::string error;

  bool Open(const std::string& url, const char* accept) {
    std::wstring wide_url = Wide(url);
    URL_COMPONENTS parts = {sizeof(parts)};
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = DWORD(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = DWORD(std::size(path));
    parts.dwExtraInfoLength = 1;  // the query stays on the path
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
      error = ErrorText(GetLastError());
      return false;
    }
    std::wstring object = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
                          std::wstring(parts.lpszExtraInfo ? parts.lpszExtraInfo : L"", parts.dwExtraInfoLength);
    // The launcher's name without spaces, and its version (GitHub wants a User-Agent).
    std::wstring agent = Wide(game::kLauncherName) + L"/" + Wide(game::kVersion);
    std::replace(agent.begin(), agent.end(), L' ', L'-');
    session.h = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) {
      // Before Windows 8.1 (and on some Wine versions): the proxy set in the registry.
      session.h = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
    connection.h = WinHttpConnect(session.h, host, parts.nPort, 0);
    if (!connection.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    request.h = WinHttpOpenRequest(connection.h, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    std::wstring headers = L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (accept) {
      headers += L"Accept: " + Wide(accept) + L"\r\n";
    }
    if (!WinHttpSendRequest(request.h, headers.c_str(), DWORD(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr)) {
      error = ErrorText(GetLastError());
      return false;
    }
    DWORD code = 0;
    DWORD size = sizeof(code);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &code, &size, WINHTTP_NO_HEADER_INDEX);
    status = int(code);
    wchar_t text[32] = {};
    size = sizeof(text);
    if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, text, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      length = std::wcstoull(text, nullptr, 10);
    }
    size = sizeof(text);
    if ((status == 403 || status == 429) &&
        WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CUSTOM, L"x-ratelimit-remaining", text, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      rate_limited = std::wcstoull(text, nullptr, 10) == 0;
    }
    return true;
  }

  // The next piece of the body: false at its end or on an error (`error` set then).
  bool Read(std::vector<char>& buffer, DWORD& got) {
    got = 0;
    if (!WinHttpReadData(request.h, buffer.data(), DWORD(buffer.size()), &got)) {
      error = ErrorText(GetLastError());
      return false;
    }
    return got > 0;
  }
};

}  // namespace

Response Get(const std::string& url, const char* accept) {
  Response response;
  Request request;
  if (!request.Open(url, accept)) {
    response.error = request.error;
    return response;
  }
  response.status = request.status;
  response.rate_limited = request.rate_limited;
  std::vector<char> buffer(64 * 1024);
  DWORD got = 0;
  while (request.Read(buffer, got)) {
    response.body.append(buffer.data(), got);
  }
  if (!request.error.empty()) {
    response.status = 0;
    response.error = request.error;
  }
  return response;
}

bool Download(const std::string& url, const std::filesystem::path& file,
              const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>& cancel,
              std::string* error) {
  Request request;
  if (!request.Open(url, "application/octet-stream")) {
    *error = request.error;
    return false;
  }
  if (request.status != 200) {
    *error = "HTTP " + std::to_string(request.status);
    return false;
  }
  HANDLE out = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    *error = ErrorText(GetLastError());
    return false;
  }
  std::vector<char> buffer(1024 * 1024);
  uint64_t done = 0;
  DWORD got = 0;
  bool ok = true;
  while (!cancel && request.Read(buffer, got)) {
    DWORD wrote = 0;
    if (!WriteFile(out, buffer.data(), got, &wrote, nullptr) || wrote != got) {
      *error = ErrorText(GetLastError());
      ok = false;
      break;
    }
    done += got;
    progress(done, request.length);
  }
  CloseHandle(out);
  if (ok && !request.error.empty()) {
    *error = request.error;
    ok = false;
  }
  if (ok && cancel) {
    ok = false;
  }
  if (ok && request.length && done != request.length) {
    *error = "the download ended early (" + std::to_string(done) + " of " + std::to_string(request.length) + " bytes)";
    ok = false;
  }
  if (!ok) {
    std::error_code ec;
    std::filesystem::remove(file, ec);
  }
  return ok;
}

}  // namespace launcher::net
