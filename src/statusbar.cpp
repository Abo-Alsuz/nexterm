#include "statusbar.h"
#include <ctime>
#include <sstream>
#include <iomanip>

StatusBar::StatusBar(const StatusBarConfig& cfg) : m_cfg(cfg) {
    m_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_size_request(m_box, -1, cfg.height);
    gtk_widget_add_css_class(m_box, "statusbar");

    m_css = gtk_css_provider_new();
    std::string css =
        ".statusbar { "
        "  background-color: " + cfg.background + ";"
        "  color: " + cfg.foreground + ";"
        "  padding: 0 8px;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "}"
        ".statusbar label { color: " + cfg.foreground + "; }"
        ".mode-normal { color: #a6e3a1; font-weight: bold; }"
        ".mode-ai     { color: #89b4fa; font-weight: bold; }"
        ".sep { color: #45475a; margin: 0 4px; }";

    gtk_css_provider_load_from_string(m_css, css.c_str());
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(m_css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(left, TRUE);
    gtk_widget_set_halign(left, GTK_ALIGN_START);

    if (cfg.show_mode) {
        m_lbl_mode = gtk_label_new("NORMAL");
        gtk_widget_add_css_class(m_lbl_mode, "mode-normal");
        gtk_box_append(GTK_BOX(left), m_lbl_mode);

        GtkWidget* sep = gtk_label_new(" | ");
        gtk_widget_add_css_class(sep, "sep");
        gtk_box_append(GTK_BOX(left), sep);
    }

    if (cfg.show_cwd) {
        m_lbl_cwd = gtk_label_new("~");
        gtk_box_append(GTK_BOX(left), m_lbl_cwd);
    }

    gtk_box_append(GTK_BOX(m_box), left);

    m_lbl_ai = gtk_label_new("");
    gtk_widget_set_hexpand(m_lbl_ai, TRUE);
    gtk_widget_set_halign(m_lbl_ai, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(m_box), m_lbl_ai);

    GtkWidget* right = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(right, TRUE);
    gtk_widget_set_halign(right, GTK_ALIGN_END);

    if (cfg.show_cmd_count) {
        m_lbl_cmds = gtk_label_new("cmds: 0");
        gtk_box_append(GTK_BOX(right), m_lbl_cmds);

        GtkWidget* sep2 = gtk_label_new(" | ");
        gtk_widget_add_css_class(sep2, "sep");
        gtk_box_append(GTK_BOX(right), sep2);
    }

    if (cfg.show_time) {
        m_lbl_time = gtk_label_new(current_time().c_str());
        gtk_box_append(GTK_BOX(right), m_lbl_time);
    }

    gtk_box_append(GTK_BOX(m_box), right);

    g_timeout_add_seconds(1, tick, this);
}

void StatusBar::set_mode(TermMode mode) {
    m_mode = mode;
    refresh();
}

void StatusBar::set_cwd(const std::string& cwd) {
    m_cwd = cwd;
    refresh();
}

void StatusBar::increment_cmd_count() {
    m_cmd_count++;
    refresh();
}

void StatusBar::set_ai_status(const std::string& status) {
    m_ai_status = status;
    refresh();
}

void StatusBar::refresh() {
    if (m_lbl_mode) {
        std::string label = mode_label(m_mode);
        gtk_label_set_text(GTK_LABEL(m_lbl_mode), label.c_str());
        if (m_mode == TermMode::AI) {
            gtk_widget_remove_css_class(m_lbl_mode, "mode-normal");
            gtk_widget_add_css_class(m_lbl_mode, "mode-ai");
        } else {
            gtk_widget_remove_css_class(m_lbl_mode, "mode-ai");
            gtk_widget_add_css_class(m_lbl_mode, "mode-normal");
        }
    }
    if (m_lbl_cwd && !m_cwd.empty())
        gtk_label_set_text(GTK_LABEL(m_lbl_cwd), m_cwd.c_str());
    if (m_lbl_cmds) {
        std::string s = "cmds: " + std::to_string(m_cmd_count);
        gtk_label_set_text(GTK_LABEL(m_lbl_cmds), s.c_str());
    }
    if (m_lbl_ai)
        gtk_label_set_text(GTK_LABEL(m_lbl_ai), m_ai_status.c_str());
    if (m_lbl_time)
        gtk_label_set_text(GTK_LABEL(m_lbl_time), current_time().c_str());
}

std::string StatusBar::mode_label(TermMode m) {
    return m == TermMode::NORMAL ? "NORMAL" : "AI";
}

std::string StatusBar::current_time() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << tm->tm_hour << ":"
       << std::setw(2) << tm->tm_min  << ":"
       << std::setw(2) << tm->tm_sec;
    return ss.str();
}

gboolean StatusBar::tick(gpointer data) {
    auto* sb = static_cast<StatusBar*>(data);
    if (sb->m_lbl_time)
        gtk_label_set_text(GTK_LABEL(sb->m_lbl_time), current_time().c_str());
    return G_SOURCE_CONTINUE;
}
