#pragma once
#include "config.h"
#include "statusbar.h"
#include "ai.h"
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <string>
#include <memory>

class Terminal {
public:
    Terminal(const NextermConfig& cfg, StatusBar* statusbar);

    GtkWidget* widget() { return m_vte; }
    GtkWidget* ai_bar() { return m_ai_bar; }

    void toggle_ai_mode();
    void apply_config(const NextermConfig& cfg);

private:
    NextermConfig m_cfg;
    StatusBar*    m_statusbar;
    TermMode      m_mode = TermMode::NORMAL;

    GtkWidget*    m_vte      = nullptr;
    GtkWidget*    m_ai_bar   = nullptr;
    GtkWidget*    m_ai_entry = nullptr;
    GtkWidget*    m_ai_resp  = nullptr;
    GtkWidget*    m_ai_btn   = nullptr;
    std::string   m_pending_cmd;

    std::unique_ptr<AIClient> m_ai;

    void build_ai_bar();
    void submit_ai_prompt();
    void show_ai_response(const AIResponse& resp);
    void run_suggested_command();

    void apply_colors(const ColorConfig& c);
    void apply_font(const FontConfig& f);

    static void on_child_exited(VteTerminal*, int, gpointer);
    static void on_commit(VteTerminal*, const gchar* text, guint size, gpointer);
    static gboolean on_ai_entry_key(GtkEventControllerKey*, guint keyval,
                                guint keycode, GdkModifierType mods, gpointer data);
};
