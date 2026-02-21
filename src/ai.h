#pragma once
#include "config.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>

struct AIResponse {
    std::string text;
    std::string command;
    bool has_command = false;
    bool error = false;
    std::string error_msg;
};

using AICallback = std::function<void(AIResponse)>;

class AIClient {
public:
    explicit AIClient(const AIConfig& cfg);
    ~AIClient();

    void send_async(const std::string& user_message, AICallback callback);
    void cancel();
    bool is_busy() const { return m_busy.load(); }

private:
    AIConfig m_cfg;
    std::atomic<bool> m_busy{false};
    std::atomic<bool> m_cancel{false};
    std::thread m_thread;

    AIResponse do_ollama(const std::string& msg);
    AIResponse do_openai(const std::string& msg);
    AIResponse do_anthropic(const std::string& msg);

    static std::string extract_command(const std::string& text);
};
