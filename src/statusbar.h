#pragma once
#include "config.h"
#include <gtk/gtk.h>
#include <string>

enum class TermMode { NORMAL, AI };

class StatusBar {
public:
    StatusBar(const StatusBarConfig& cfg);

    GtkWidget* widget() { return m_box; }

    void set_mode(TermMode mode);
    void set_cwd(const std::string& cwd);
    void increment_cmd_count();
    void set_ai_status(const std::string& status);

    static gboolean tick(gpointer data);

private:
    StatusBarConfig m_cfg;
    TermMode m_mode = TermMode::NORMAL;
    int m_cmd_count = 0;
    std::string m_cwd;
    std::string m_ai_status;

    GtkWidget* m_box        = nullptr;
    GtkWidget* m_lbl_mode   = nullptr;
    GtkWidget* m_lbl_cwd    = nullptr;
    GtkWidget* m_lbl_cmds   = nullptr;
    GtkWidget* m_lbl_ai     = nullptr;
    GtkWidget* m_lbl_time   = nullptr;

    void refresh();
    static std::string mode_label(TermMode m);
    static std::string current_time();

    GtkCssProvider* m_css = nullptr;
};
