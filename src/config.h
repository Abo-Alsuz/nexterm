#pragma once
#include <string>
#include <map>

struct GeneralConfig {
    std::string shell = "/bin/bash";
    int scrollback_lines = 10000;
    std::string title = "nexterm";
};

struct FontConfig {
    std::string family = "Monospace";
    double size = 13.0;
    bool bold_is_bright = true;
};

struct ColorConfig {
    std::string background = "#1e1e2e";
    std::string foreground = "#cdd6f4";
    std::string cursor     = "#f5e0dc";
    std::string black, red, green, yellow, blue, magenta, cyan, white;
    std::string bright_black, bright_red, bright_green, bright_yellow;
    std::string bright_blue, bright_magenta, bright_cyan, bright_white;
};

struct WindowConfig {
    int width = 1200;
    int height = 800;
    int padding_x = 8;
    int padding_y = 8;
    double opacity = 1.0;
    std::string decorations = "full";
};

struct StatusBarConfig {
    bool enabled = true;
    int height = 24;
    std::string background = "#181825";
    std::string foreground = "#cdd6f4";
    std::string position = "bottom";
    bool show_mode = true;
    bool show_cwd = true;
    bool show_time = true;
    bool show_cmd_count = true;
};

struct KeybindsConfig {
    std::string ai_mode       = "<Ctrl>space";
    std::string copy          = "<Ctrl><Shift>c";
    std::string paste         = "<Ctrl><Shift>v";
    std::string increase_font = "<Ctrl>equal";
    std::string decrease_font = "<Ctrl>minus";
    std::string reset_font    = "<Ctrl>0";
    std::string new_window    = "<Ctrl><Shift>n";
};

struct AIConfig {
    std::string backend   = "ollama";
    std::string endpoint  = "http://localhost:11434/api/chat";
    std::string model     = "qwen2.5:7b";
    std::string api_key   = "";
    int max_tokens        = 512;
    std::string system_prompt;
};

struct GPUConfig {
    bool accelerated = true;
};

struct NextermConfig {
    GeneralConfig   general;
    FontConfig      font;
    ColorConfig     colors;
    WindowConfig    window;
    StatusBarConfig statusbar;
    KeybindsConfig  keybinds;
    AIConfig        ai;
    GPUConfig       gpu;

    static NextermConfig load(const std::string& path);
    static std::string   default_config_path();
};
