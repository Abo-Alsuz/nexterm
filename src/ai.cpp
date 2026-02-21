#include "ai.h"
#include <curl/curl.h>
#include <iostream>
#include <regex>
#include <sstream>

static std::string unescape_json(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i+1]) {
                case 'n':  out += '\n'; i++; break;
                case 't':  out += '\t'; i++; break;
                case 'r':  out += '\r'; i++; break;
                case '\\': out += '\\'; i++; break;
                case '"':  out += '"';  i++; break;
                case 'u':
                    if (i + 5 < s.size()) {
                        std::string hex = s.substr(i+2, 4);
                        unsigned int cp = std::stoul(hex, nullptr, 16);
                        if (cp == 0x003c) out += '<';
                        else if (cp == 0x003e) out += '>';
                        else if (cp == 0x0026) out += '&';
                        else out += '?';
                        i += 5;
                    }
                    break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static size_t curl_write(void* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static std::string json_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    out += "\"";
    return out;
}

static std::string json_get(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find_first_not_of(" \t\n\r", pos + 1);
    if (pos == std::string::npos) return "";
    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        while (end != std::string::npos && json[end-1] == '\\')
            end = json.find('"', end + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    }
    size_t end = json.find_first_of(",}\n", pos);
    return json.substr(pos, end - pos);
}

AIClient::AIClient(const AIConfig& cfg) : m_cfg(cfg) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

AIClient::~AIClient() {
    cancel();
    if (m_thread.joinable()) m_thread.join();
    curl_global_cleanup();
}

void AIClient::cancel() {
    m_cancel.store(true);
}

void AIClient::send_async(const std::string& user_message, AICallback callback) {
    if (m_busy.load()) return;
    m_busy.store(true);
    m_cancel.store(false);

    if (m_thread.joinable()) m_thread.join();

    m_thread = std::thread([this, user_message, callback]() {
        AIResponse resp;
        try {
            if (m_cfg.backend == "ollama")
                resp = do_ollama(user_message);
            else if (m_cfg.backend == "anthropic")
                resp = do_anthropic(user_message);
            else
                resp = do_openai(user_message);
        } catch (const std::exception& e) {
            resp.error = true;
            resp.error_msg = e.what();
        }
        m_busy.store(false);
        callback(resp);
    });
}

std::string AIClient::extract_command(const std::string& text) {
    std::regex re("<cmd>([^<]+)</cmd>");
    std::smatch m;
    if (std::regex_search(text, m, re))
        return m[1].str();
    return "";
}

static std::string http_post(const std::string& url,
                              const std::string& body,
                              const std::vector<std::string>& headers,
                              bool& ok) {
    CURL* curl = curl_easy_init();
    if (!curl) { ok = false; return "curl init failed"; }

    std::string response;
    struct curl_slist* hlist = nullptr;
    for (auto& h : headers)
        hlist = curl_slist_append(hlist, h.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    ok = (res == CURLE_OK);

    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    return response;
}

AIResponse AIClient::do_ollama(const std::string& msg) {
    AIResponse resp;

    std::string body = "{"
        "\"model\": " + json_string(m_cfg.model) + ","
        "\"stream\": false,"
        "\"messages\": ["
            "{\"role\": \"system\", \"content\": " + json_string(m_cfg.system_prompt) + "},"
            "{\"role\": \"user\",   \"content\": " + json_string(msg) + "}"
        "]"
    "}";

    bool ok;
    std::string raw = http_post(m_cfg.endpoint, body,
        {"Content-Type: application/json"}, ok);

    if (!ok) { resp.error = true; resp.error_msg = "HTTP request failed"; return resp; }

    size_t msg_pos = raw.find("\"message\"");
    if (msg_pos != std::string::npos) {
        std::string sub = raw.substr(msg_pos);
        resp.text = unescape_json(json_get(sub, "content"));
    }
    if (resp.text.empty()) resp.text = unescape_json(json_get(raw, "response"));

    resp.command = extract_command(resp.text);
    resp.has_command = !resp.command.empty();
    return resp;
}

AIResponse AIClient::do_openai(const std::string& msg) {
    AIResponse resp;

    std::string body = "{"
        "\"model\": " + json_string(m_cfg.model) + ","
        "\"max_tokens\": " + std::to_string(m_cfg.max_tokens) + ","
        "\"messages\": ["
            "{\"role\": \"system\", \"content\": " + json_string(m_cfg.system_prompt) + "},"
            "{\"role\": \"user\",   \"content\": " + json_string(msg) + "}"
        "]"
    "}";

    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (!m_cfg.api_key.empty())
        headers.push_back("Authorization: Bearer " + m_cfg.api_key);

    bool ok;
    std::string raw = http_post(m_cfg.endpoint, body, headers, ok);
    if (!ok) { resp.error = true; resp.error_msg = "HTTP request failed"; return resp; }

    size_t choices = raw.find("\"choices\"");
    if (choices != std::string::npos) {
        size_t content = raw.find("\"content\"", choices);
        if (content != std::string::npos) {
            size_t start = raw.find('"', content + 9) + 1;
            size_t end = start;
            while (end < raw.size()) {
                if (raw[end] == '"' && raw[end-1] != '\\') break;
                end++;
            }
            resp.text = unescape_json(raw.substr(start, end - start));
        }
    }

    resp.command = extract_command(resp.text);
    resp.has_command = !resp.command.empty();
    return resp;
}

AIResponse AIClient::do_anthropic(const std::string& msg) {
    AIResponse resp;

    std::string body = "{"
        "\"model\": " + json_string(m_cfg.model) + ","
        "\"max_tokens\": " + std::to_string(m_cfg.max_tokens) + ","
        "\"system\": " + json_string(m_cfg.system_prompt) + ","
        "\"messages\": ["
            "{\"role\": \"user\", \"content\": " + json_string(msg) + "}"
        "]"
    "}";

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "anthropic-version: 2023-06-01"
    };
    if (!m_cfg.api_key.empty())
        headers.push_back("x-api-key: " + m_cfg.api_key);

    bool ok;
    std::string raw = http_post(m_cfg.endpoint, body, headers, ok);
    if (!ok) { resp.error = true; resp.error_msg = "HTTP request failed"; return resp; }

    size_t text_pos = raw.find("\"text\"");
    if (text_pos != std::string::npos) {
        size_t start = raw.find('"', text_pos + 6) + 1;
        size_t end = start;
        while (end < raw.size()) {
            if (raw[end] == '"' && raw[end-1] != '\\') break;
            end++;
        }
        resp.text = unescape_json(raw.substr(start, end - start));
    }

    resp.command = extract_command(resp.text);
    resp.has_command = !resp.command.empty();
    return resp;
}
