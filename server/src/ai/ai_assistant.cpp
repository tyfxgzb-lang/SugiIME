#include "ai_assistant.h"

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace
{
constexpr auto kIdleDelay = std::chrono::milliseconds(600);
constexpr int kTimeoutMs = 8000;

using ApplyCallback =
    std::function<void(const std::string &candidate, const std::string &identity, uint64_t generation)>;

std::mutex g_mutex;
std::condition_variable g_cv;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_generation{0};

AiAssistant::Request g_latest;
ApplyCallback g_apply_callback;

std::wstring Utf8ToWide(const std::string &input)
{
    if (input.empty())
        return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
    if (len <= 0)
        return std::wstring();
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), out.data(), len);
    return out;
}

std::string WideToUtf8(const std::wstring &input)
{
    if (input.empty())
        return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return std::string();
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), out.data(), len, nullptr, nullptr);
    return out;
}

// Split an endpoint URL like "https://api.deepseek.com/chat/completions" into
// host (api.deepseek.com) and path (/chat/completions). Returns false on
// malformed input.
bool ParseEndpoint(const std::string &endpoint, std::wstring &host, std::wstring &path)
{
    std::string rest = endpoint;
    const std::string https_prefix = "https://";
    const std::string http_prefix = "http://";
    if (rest.rfind(https_prefix, 0) == 0)
        rest = rest.substr(https_prefix.size());
    else if (rest.rfind(http_prefix, 0) == 0)
        rest = rest.substr(http_prefix.size());
    else
        return false;

    size_t slash = rest.find('/');
    std::string host_str = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path_str = slash == std::string::npos ? "/" : rest.substr(slash);
    if (host_str.empty())
        return false;
    host = Utf8ToWide(host_str);
    path = Utf8ToWide(path_str);
    return !host.empty();
}

bool HttpPostJson(const std::wstring &host, const std::wstring &path, const std::string &payload,
                  const std::string &auth_header, std::string &response_out)
{
    HINTERNET hSession = WinHttpOpen(L"SugiIMEServer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    WinHttpSetTimeouts(hSession, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + Utf8ToWide(auth_header) + L"\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), static_cast<DWORD>(headers.size()), WINHTTP_ADDREQ_FLAG_ADD);

    std::string body = payload;
    BOOL send_ok =
        WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.data(), static_cast<DWORD>(body.size()),
                           static_cast<DWORD>(body.size()), 0);
    if (!send_ok)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BOOL recv_ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!recv_ok)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::string response;
    DWORD bytes_available = 0;
    do
    {
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available))
            break;
        if (bytes_available == 0)
            break;
        std::string buffer(bytes_available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read))
            break;
        response.append(buffer.data(), bytes_read);
    } while (bytes_available > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    response_out = std::move(response);
    return true;
}

std::string BuildPrompt(const AiAssistant::Request &req)
{
    const auto &cfg = req.config;
    std::string prompt = cfg.prompt;
    if (prompt.empty())
    {
        prompt =
            "你是一个日语输入法联想引擎。根据输入的罗马音/假名切分和前文上下文，生成自然的日语候选。";
    }

    std::string segments;
    for (size_t i = 0; i < req.pinyin_segments.size(); ++i)
    {
        if (i > 0)
            segments += " ";
        segments += req.pinyin_segments[i];
    }

    nlohmann::json user_content = {{"segments", segments},
                                   {"context", req.context},
                                   {"candidate_limit", cfg.candidate_limit}};
    return prompt + "\n\n" + user_content.dump();
}

std::string BuildPayload(const AiAssistant::Request &req)
{
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", "You are a Japanese input method suggestion engine."}});
    messages.push_back({{"role", "user"}, {"content", BuildPrompt(req)}});

    nlohmann::json payload = {{"model", req.config.model},
                              {"messages", messages},
                              {"temperature", 0.7},
                              {"max_tokens", 512}};
    return payload.dump();
}

// Extract candidate strings from a Chat Completions response. The model is
// expected to return candidates as a JSON array (possibly inside a code
// fence), one candidate per line, or comma-separated.
std::vector<std::string> ParseCandidates(const std::string &response)
{
    std::vector<std::string> result;
    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(response);
    }
    catch (...)
    {
        return result;
    }

    std::string content;
    try
    {
        content = root["choices"][0]["message"]["content"].get<std::string>();
    }
    catch (...)
    {
        return result;
    }

    // Strip markdown code fences if present.
    std::string text = content;
    size_t fence = text.find("```");
    if (fence != std::string::npos)
    {
        size_t start = text.find('\n', fence);
        if (start != std::string::npos)
        {
            size_t end = text.find("```", start);
            if (end != std::string::npos)
                text = text.substr(start + 1, end - start - 1);
        }
    }

    // Try JSON array first.
    try
    {
        nlohmann::json arr = nlohmann::json::parse(text);
        if (arr.is_array())
        {
            for (const auto &item : arr)
            {
                if (item.is_string())
                    result.push_back(item.get<std::string>());
            }
            if (!result.empty())
                return result;
        }
    }
    catch (...)
    {
    }

    // Fall back to line splitting.
    std::string line;
    for (char c : text)
    {
        if (c == '\n' || c == '\r')
        {
            if (!line.empty())
            {
                // Trim leading numbering/bullets.
                size_t first = line.find_first_not_of(" \t0123456789.-、)");
                if (first != std::string::npos)
                    line = line.substr(first);
                if (!line.empty())
                    result.push_back(line);
                line.clear();
            }
        }
        else
        {
            line.push_back(c);
        }
    }
    if (!line.empty())
    {
        size_t first = line.find_first_not_of(" \t0123456789.-、)");
        if (first != std::string::npos)
            line = line.substr(first);
        if (!line.empty())
            result.push_back(line);
    }

    return result;
}

void RunRequest(const AiAssistant::Request &req)
{
    if (!g_apply_callback)
        return;

    std::wstring host, path;
    if (!ParseEndpoint(req.config.endpoint, host, path))
        return;

    const std::string payload = BuildPayload(req);
    std::string response;
    if (!HttpPostJson(host, path, payload, req.config.token, response))
        return;

    std::vector<std::string> candidates = ParseCandidates(response);
    int limit = req.config.candidate_limit > 0 ? req.config.candidate_limit : 3;
    for (const auto &cand : candidates)
    {
        if (limit-- <= 0)
            break;
        g_apply_callback(cand, req.identity, g_generation.load());
    }
}

void WorkerLoop()
{
    while (g_running)
    {
        AiAssistant::Request req;
        {
            std::unique_lock lock(g_mutex);
            if (g_latest.identity.empty())
            {
                g_cv.wait(lock, [] { return !g_running || !g_latest.identity.empty(); });
                if (!g_running)
                    break;
            }
            req = std::move(g_latest);
            g_latest = AiAssistant::Request{};
        }

        // Idle debounce: if a newer request arrives while we sleep, drop this one.
        {
            std::unique_lock lock(g_mutex);
            if (g_cv.wait_for(lock, kIdleDelay, [] { return !g_running || !g_latest.identity.empty(); }))
            {
                if (!g_running)
                    break;
                continue; // newer request pending
            }
        }

        RunRequest(req);
    }
}
} // namespace

void AiAssistant::Start(ApplyCallback apply_callback)
{
    g_apply_callback = std::move(apply_callback);
    if (g_running.exchange(true))
        return;
    g_worker = std::thread(WorkerLoop);
}

void AiAssistant::Stop()
{
    g_running = false;
    g_cv.notify_all();
    if (g_worker.joinable())
        g_worker.join();
}

void AiAssistant::OnInputChanged(Request request)
{
    if (!request.config.enabled)
        return;
    {
        std::lock_guard lock(g_mutex);
        g_latest = std::move(request);
        ++g_generation;
    }
    g_cv.notify_all();
}
