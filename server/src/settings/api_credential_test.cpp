#include "api_credential_test.h"

#include "cloud/cloud_translation.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <vector>

namespace
{
constexpr long kConnectTimeoutMs = 5000;
constexpr long kRequestTimeoutMs = 15000;
constexpr std::size_t kMaxResponseBytes = 256 * 1024;

struct HttpResponse
{
    CURLcode code = CURLE_FAILED_INIT;
    long status = 0;
    std::string body;
    std::string error;
};

std::string Value(const ApiCredentialTest::Request &request, const char *key)
{
    const auto found = request.config.find(key);
    return found == request.config.end() ? std::string{} : CloudTranslation::TrimSecret(found->second);
}

size_t WriteResponse(char *data, size_t size, size_t count, void *user)
{
    const size_t bytes = size * count;
    auto *response = static_cast<std::string *>(user);
    if (bytes > kMaxResponseBytes - (std::min)(response->size(), kMaxResponseBytes))
        return 0;
    response->append(data, bytes);
    return bytes;
}

void InitCurl()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

bool IsHttpEndpoint(const std::string &endpoint)
{
    return endpoint.rfind("https://", 0) == 0 || endpoint.rfind("http://", 0) == 0;
}

std::string ErrorDetail(const HttpResponse &response)
{
    if (response.code != CURLE_OK)
        return response.error.empty() ? curl_easy_strerror(response.code) : response.error;
    try
    {
        const auto root = nlohmann::json::parse(response.body);
        if (root.contains("error"))
        {
            const auto &error = root.at("error");
            if (error.is_object() && error.contains("message") && error.at("message").is_string())
                return error.at("message").get<std::string>();
            if (error.is_string())
                return error.get<std::string>();
        }
        if (root.contains("message") && root.at("message").is_string())
            return root.at("message").get<std::string>();
        if (root.contains("errorMsg") && root.at("errorMsg").is_string())
            return root.at("errorMsg").get<std::string>();
    }
    catch (...)
    {
    }
    return "HTTP " + std::to_string(response.status);
}

HttpResponse PerformJsonPost(const std::string &endpoint, const std::string &token, const std::string &payload)
{
    InitCurl();
    HttpResponse response;
    CURL *curl = curl_easy_init();
    if (!curl)
        return response;
    char error[CURL_ERROR_SIZE] = {};
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string authorization = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, authorization.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kRequestTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    response.code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    response.error = error;
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

ApiCredentialTest::Result TestChat(const ApiCredentialTest::Request &request)
{
    const std::string token = Value(request, "token");
    const std::string endpoint = Value(request, "endpoint");
    const std::string model = Value(request, "model");
    if (!CloudTranslation::IsUsableSecret(token))
        return {false, "请先填写有效的 API Key。"};
    if (!IsHttpEndpoint(endpoint) || model.empty())
        return {false, "请填写有效的 HTTPS 接口地址和模型名。"};
    nlohmann::json body = {{"model", model},
                           {"stream", false},
                           {"max_tokens", 1},
                           {"messages", {{{"role", "user"}, {"content", "Reply OK"}}}}};
    const std::string provider = Value(request, "provider");
    if (provider == "deepseek")
        body["thinking"] = {{"type", "disabled"}};
    else if (provider == "siliconflow")
        body["enable_thinking"] = false;
    const HttpResponse response = PerformJsonPost(endpoint, token, body.dump());
    if (response.code == CURLE_OK && response.status >= 200 && response.status < 300)
        return {true, "连接成功，API Key 和模型配置有效。"};
    return {false, "测试失败：" + ErrorDetail(response)};
}
} // namespace

namespace ApiCredentialTest
{
Result Run(const Request &request)
{
    if (request.service == "ai.assistant")
        return TestChat(request);
    return {false, "不支持的配置测试类型。"};
}
} // namespace ApiCredentialTest
