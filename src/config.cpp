#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <filesystem>

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static bool parse_bool(const std::string& s) {
    return s == "true" || s == "1" || s == "yes";
}

struct RawConfig {
    std::string section;
    std::map<std::string, std::map<std::string, std::string>> data;
};

static RawConfig parse_toml(const std::string& path) {
    RawConfig raw;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[nexterm] Could not open config: " << path << "\n";
        return raw;
    }

    std::string line;
    std::string current_section = "general";
    bool in_multiline = false;
    std::string multiline_key;
    std::string multiline_val;

    while (std::getline(file, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (in_multiline) {
            if (t.find("\"\"\"") != std::string::npos) {
                size_t end = t.find("\"\"\"");
                multiline_val += t.substr(0, end);
                raw.data[current_section][multiline_key] = trim(multiline_val);
                in_multiline = false;
            } else {
                multiline_val += line + "\n";
            }
            continue;
        }

        if (t.front() == '[' && t.back() == ']') {
            current_section = t.substr(1, t.size() - 2);
            continue;
        }

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));

        if (val.front() != '"') {
            size_t hash = val.find('#');
            if (hash != std::string::npos)
                val = trim(val.substr(0, hash));
        }

        if (val == "\"\"\"") {
            in_multiline = true;
            multiline_key = key;
            multiline_val = "";
            continue;
        }
        if (val.size() >= 6 && val.substr(0, 3) == "\"\"\"" && val.substr(val.size()-3) == "\"\"\"") {
            val = val.substr(3, val.size() - 6);
        }

        raw.data[current_section][key] = strip_quotes(val);
    }

    return raw;
}

static std::string get(const RawConfig& raw, const std::string& section,
                       const std::string& key, const std::string& def = "") {
    auto sit = raw.data.find(section);
    if (sit == raw.data.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    return kit->second;
}

std::string NextermConfig::default_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/root";
    return std::string(home) + "/.config/nexterm/nexterm.toml";
}

NextermConfig NextermConfig::load(const std::string& path) {
    NextermConfig cfg;
    RawConfig raw = parse_toml(path);

    cfg.general.shell            = get(raw, "general", "shell", cfg.general.shell);
    cfg.general.scrollback_lines = std::stoi(get(raw, "general", "scrollback_lines", "10000"));
    cfg.general.title            = get(raw, "general", "title", cfg.general.title);

    cfg.font.family         = get(raw, "font", "family", cfg.font.family);
    cfg.font.size           = std::stod(get(raw, "font", "size", "13.0"));
    cfg.font.bold_is_bright = parse_bool(get(raw, "font", "bold_is_bright", "true"));

    cfg.colors.background = get(raw, "colors", "background", cfg.colors.background);
    cfg.colors.foreground = get(raw, "colors", "foreground", cfg.colors.foreground);
    cfg.colors.cursor     = get(raw, "colors", "cursor",     cfg.colors.cursor);
    cfg.colors.black      = get(raw, "colors", "black",      "#45475a");
    cfg.colors.red        = get(raw, "colors", "red",        "#f38ba8");
    cfg.colors.green      = get(raw, "colors", "green",      "#a6e3a1");
    cfg.colors.yellow     = get(raw, "colors", "yellow",     "#f9e2af");
    cfg.colors.blue       = get(raw, "colors", "blue",       "#89b4fa");
    cfg.colors.magenta    = get(raw, "colors", "magenta",    "#f5c2e7");
    cfg.colors.cyan       = get(raw, "colors", "cyan",       "#94e2d5");
    cfg.colors.white      = get(raw, "colors", "white",      "#bac2de");
    cfg.colors.bright_black   = get(raw, "colors", "bright_black",   "#585b70");
    cfg.colors.bright_red     = get(raw, "colors", "bright_red",     "#f38ba8");
    cfg.colors.bright_green   = get(raw, "colors", "bright_green",   "#a6e3a1");
    cfg.colors.bright_yellow  = get(raw, "colors", "bright_yellow",  "#f9e2af");
    cfg.colors.bright_blue    = get(raw, "colors", "bright_blue",    "#89b4fa");
    cfg.colors.bright_magenta = get(raw, "colors", "bright_magenta", "#f5c2e7");
    cfg.colors.bright_cyan    = get(raw, "colors", "bright_cyan",    "#94e2d5");
    cfg.colors.bright_white   = get(raw, "colors", "bright_white",   "#a6adc8");

    cfg.window.width       = std::stoi(get(raw, "window", "width",  "1200"));
    cfg.window.height      = std::stoi(get(raw, "window", "height", "800"));
    cfg.window.padding_x   = std::stoi(get(raw, "window", "padding_x", "8"));
    cfg.window.padding_y   = std::stoi(get(raw, "window", "padding_y", "8"));
    cfg.window.opacity     = std::stod(get(raw, "window", "opacity", "1.0"));
    cfg.window.decorations = get(raw, "window", "decorations", "full");

    cfg.statusbar.enabled        = parse_bool(get(raw, "statusbar", "enabled", "true"));
    cfg.statusbar.height         = std::stoi(get(raw, "statusbar", "height", "24"));
    cfg.statusbar.background     = get(raw, "statusbar", "background", "#181825");
    cfg.statusbar.foreground     = get(raw, "statusbar", "foreground", "#cdd6f4");
    cfg.statusbar.position       = get(raw, "statusbar", "position", "bottom");
    cfg.statusbar.show_mode      = parse_bool(get(raw, "statusbar", "show_mode", "true"));
    cfg.statusbar.show_cwd       = parse_bool(get(raw, "statusbar", "show_cwd", "true"));
    cfg.statusbar.show_time      = parse_bool(get(raw, "statusbar", "show_time", "true"));
    cfg.statusbar.show_cmd_count = parse_bool(get(raw, "statusbar", "show_cmd_count", "true"));

    cfg.keybinds.ai_mode       = get(raw, "keybinds", "ai_mode",       cfg.keybinds.ai_mode);
    cfg.keybinds.copy          = get(raw, "keybinds", "copy",          cfg.keybinds.copy);
    cfg.keybinds.paste         = get(raw, "keybinds", "paste",         cfg.keybinds.paste);
    cfg.keybinds.increase_font = get(raw, "keybinds", "increase_font", cfg.keybinds.increase_font);
    cfg.keybinds.decrease_font = get(raw, "keybinds", "decrease_font", cfg.keybinds.decrease_font);
    cfg.keybinds.reset_font    = get(raw, "keybinds", "reset_font",    cfg.keybinds.reset_font);
    cfg.keybinds.new_window    = get(raw, "keybinds", "new_window",    cfg.keybinds.new_window);

    cfg.ai.backend       = get(raw, "ai", "backend",   cfg.ai.backend);
    cfg.ai.endpoint      = get(raw, "ai", "endpoint",  cfg.ai.endpoint);
    cfg.ai.model         = get(raw, "ai", "model",     cfg.ai.model);
    cfg.ai.api_key       = get(raw, "ai", "api_key",   "");
    cfg.ai.max_tokens    = std::stoi(get(raw, "ai", "max_tokens", "512"));
    cfg.ai.system_prompt = get(raw, "ai", "system_prompt", cfg.ai.system_prompt);

    const char* env_key = std::getenv("NEXTERM_AI_KEY");
    if (env_key) cfg.ai.api_key = env_key;

    cfg.gpu.accelerated = parse_bool(get(raw, "gpu", "accelerated", "true"));

    return cfg;
}
