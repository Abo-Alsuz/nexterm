#include "terminal.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glib.h>

static GdkRGBA hex_to_rgba(const std::string& hex) {
    GdkRGBA color = {0, 0, 0, 1};
    gdk_rgba_parse(&color, hex.c_str());
    return color;
}

Terminal::Terminal(const NextermConfig& cfg, StatusBar* statusbar)
    : m_cfg(cfg), m_statusbar(statusbar) {

    m_vte = vte_terminal_new();

    apply_colors(cfg.colors);
    apply_font(cfg.font);

    vte_terminal_set_scrollback_lines(VTE_TERMINAL(m_vte), cfg.general.scrollback_lines);
    vte_terminal_set_bold_is_bright(VTE_TERMINAL(m_vte), cfg.font.bold_is_bright);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(m_vte), TRUE);

    const char* shell_argv[] = { cfg.general.shell.c_str(), nullptr };
    char** env = g_get_environ();
    vte_terminal_spawn_async(
        VTE_TERMINAL(m_vte),
        VTE_PTY_DEFAULT,
        nullptr,
        (char**)shell_argv,
        env,
        G_SPAWN_SEARCH_PATH,
        nullptr, nullptr, nullptr,
        -1,
        nullptr,
        nullptr, nullptr);
    g_strfreev(env);

    g_signal_connect(m_vte, "child-exited", G_CALLBACK(on_child_exited), this);
    g_signal_connect(m_vte, "commit",       G_CALLBACK(on_commit),       this);

    build_ai_bar();

    m_ai = std::make_unique<AIClient>(cfg.ai);
}

void Terminal::apply_colors(const ColorConfig& c) {
    GdkRGBA fg = hex_to_rgba(c.foreground);
    GdkRGBA bg = hex_to_rgba(c.background);

    GdkRGBA palette[16] = {
        hex_to_rgba(c.black),   hex_to_rgba(c.red),
        hex_to_rgba(c.green),   hex_to_rgba(c.yellow),
        hex_to_rgba(c.blue),    hex_to_rgba(c.magenta),
        hex_to_rgba(c.cyan),    hex_to_rgba(c.white),
        hex_to_rgba(c.bright_black),   hex_to_rgba(c.bright_red),
        hex_to_rgba(c.bright_green),   hex_to_rgba(c.bright_yellow),
        hex_to_rgba(c.bright_blue),    hex_to_rgba(c.bright_magenta),
        hex_to_rgba(c.bright_cyan),    hex_to_rgba(c.bright_white),
    };

    vte_terminal_set_colors(VTE_TERMINAL(m_vte), &fg, &bg, palette, 16);

    GdkRGBA cursor = hex_to_rgba(m_cfg.colors.cursor);
    vte_terminal_set_color_cursor(VTE_TERMINAL(m_vte), &cursor);
}

void Terminal::apply_font(const FontConfig& f) {
    std::string desc = f.family + " " + std::to_string((int)f.size);
    PangoFontDescription* pfd = pango_font_description_from_string(desc.c_str());
    vte_terminal_set_font(VTE_TERMINAL(m_vte), pfd);
    pango_font_description_free(pfd);
}

void Terminal::build_ai_bar() {
    m_ai_bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(m_ai_bar, 350, -1);

    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(m_ai_bar), sep);

    m_ai_resp = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(m_ai_resp), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(m_ai_resp), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(m_ai_resp), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(m_ai_resp), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(m_ai_resp), 4);
    gtk_widget_set_can_focus(m_ai_resp, FALSE);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 350);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), m_ai_resp);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(m_ai_bar), scroll);

    GtkWidget* input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(input_row, 8);
    gtk_widget_set_margin_end(input_row, 8);
    gtk_widget_set_margin_bottom(input_row, 4);

    GtkWidget* prompt_lbl = gtk_label_new("AI >");
    gtk_box_append(GTK_BOX(input_row), prompt_lbl);

    m_ai_entry = gtk_entry_new();
    gtk_widget_set_hexpand(m_ai_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_ai_entry), "Ask anything... (Enter to send, Esc to exit AI mode)");
    gtk_box_append(GTK_BOX(input_row), m_ai_entry);
    gtk_box_append(GTK_BOX(input_row), m_ai_entry);

    m_ai_btn = gtk_button_new_with_label("Run");
    gtk_widget_set_sensitive(m_ai_btn, FALSE);
    g_signal_connect_swapped(m_ai_btn, "clicked",
        G_CALLBACK(+[](Terminal* t){ t->run_suggested_command(); }), this);
    gtk_box_append(GTK_BOX(input_row), m_ai_btn);

    gtk_box_append(GTK_BOX(m_ai_bar), input_row);

    g_signal_connect_swapped(m_ai_entry, "activate",
    G_CALLBACK(+[](Terminal* t){ t->submit_ai_prompt(); }), this);

    gtk_widget_set_visible(m_ai_bar, FALSE);
}

void Terminal::toggle_ai_mode() {
    if (m_mode == TermMode::NORMAL) {
        m_mode = TermMode::AI;
        gtk_widget_set_visible(m_ai_bar, TRUE);
        gtk_widget_grab_focus(m_ai_entry);
    } else {
        m_mode = TermMode::NORMAL;
        gtk_widget_set_visible(m_ai_bar, FALSE);
        gtk_widget_grab_focus(m_vte);
    }
    if (m_statusbar) m_statusbar->set_mode(m_mode);
}

void Terminal::submit_ai_prompt() {
    std::cerr << "[DEBUG] submit_ai_prompt called\n";
    const char* text = gtk_editable_get_text(GTK_EDITABLE(m_ai_entry));
    if (!text || strlen(text) == 0) return;
    if (m_ai->is_busy()) return;

    std::string prompt = text;
    gtk_editable_set_text(GTK_EDITABLE(m_ai_entry), "");
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_ai_resp));
    gtk_text_buffer_set_text(buf, "Thinking...", -1);
    gtk_widget_set_sensitive(m_ai_btn, FALSE);
    if (m_statusbar) m_statusbar->set_ai_status("AI thinking...");

    m_ai->send_async(prompt, [this](AIResponse resp) {
        g_idle_add_once([](gpointer data) {
            auto* pair = static_cast<std::pair<Terminal*, AIResponse>*>(data);
            pair->first->show_ai_response(pair->second);
            delete pair;
        }, new std::pair<Terminal*, AIResponse>(this, resp));
    });
}

void Terminal::show_ai_response(const AIResponse& resp) {
    if (m_statusbar) m_statusbar->set_ai_status("");

    if (resp.error) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_ai_resp));
        gtk_text_buffer_set_text(buf, ("Error: " + resp.error_msg).c_str(), -1);
        return;
    }

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(m_ai_resp));
    gtk_text_buffer_set_text(buf, resp.text.c_str(), -1);

    if (resp.has_command) {
        m_pending_cmd = resp.command;
        gtk_widget_set_sensitive(m_ai_btn, TRUE);
        std::string btn_label = "Run: " + resp.command;
        gtk_button_set_label(GTK_BUTTON(m_ai_btn), btn_label.c_str());
    } else {
        m_pending_cmd.clear();
        gtk_widget_set_sensitive(m_ai_btn, FALSE);
        gtk_button_set_label(GTK_BUTTON(m_ai_btn), "Run");
    }
}

void Terminal::run_suggested_command() {
    if (m_pending_cmd.empty()) return;
    std::string cmd = m_pending_cmd + "\n";
    vte_terminal_feed_child(VTE_TERMINAL(m_vte), cmd.c_str(), cmd.size());
    m_pending_cmd.clear();
    gtk_widget_set_sensitive(m_ai_btn, FALSE);
    gtk_button_set_label(GTK_BUTTON(m_ai_btn), "Run");
    toggle_ai_mode();
    if (m_statusbar) m_statusbar->increment_cmd_count();
}

void Terminal::on_child_exited(VteTerminal*, int, gpointer data) {
    auto* self = static_cast<Terminal*>(data);
    const char* shell_argv[] = { self->m_cfg.general.shell.c_str(), nullptr };
    char** env = g_get_environ();
    vte_terminal_spawn_async(
        VTE_TERMINAL(self->m_vte), VTE_PTY_DEFAULT, nullptr,
        (char**)shell_argv, env, G_SPAWN_SEARCH_PATH,
        nullptr, nullptr, nullptr, -1, nullptr, nullptr, nullptr);
    g_strfreev(env);
}

void Terminal::on_commit(VteTerminal*, const gchar* text, guint size, gpointer data) {
    auto* self = static_cast<Terminal*>(data);
    for (guint i = 0; i < size; i++) {
        if (text[i] == '\r' || text[i] == '\n') {
            if (self->m_statusbar)
                self->m_statusbar->increment_cmd_count();
        }
    }
}

void Terminal::apply_config(const NextermConfig& cfg) {
    m_cfg = cfg;
    apply_colors(cfg.colors);
    apply_font(cfg.font);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(m_vte), cfg.general.scrollback_lines);
    m_ai = std::make_unique<AIClient>(cfg.ai);
}
