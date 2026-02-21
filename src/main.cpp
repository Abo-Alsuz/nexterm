#include "config.h"
#include "terminal.h"
#include "statusbar.h"
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <iostream>
#include <memory>
#include <filesystem>

struct AppState {
    NextermConfig cfg;
    Terminal*   terminal  = nullptr;
    StatusBar*  statusbar = nullptr;
    GtkWindow*  window    = nullptr;
};

static gboolean on_key_press(GtkEventControllerKey*, guint keyval,
                              guint, GdkModifierType mods, gpointer data) {
    auto* state = static_cast<AppState*>(data);
    const auto& kb = state->cfg.keybinds;

    guint ai_key; GdkModifierType ai_mods;
    gtk_accelerator_parse(kb.ai_mode.c_str(), &ai_key, &ai_mods);
    if (keyval == ai_key && (mods & ai_mods) == ai_mods) {
        state->terminal->toggle_ai_mode();
        return TRUE;
    }

    guint nw_key; GdkModifierType nw_mods;
    gtk_accelerator_parse(kb.new_window.c_str(), &nw_key, &nw_mods);
    if (keyval == nw_key && (mods & nw_mods) == nw_mods) {
        g_spawn_command_line_async("nexterm", nullptr);
        return TRUE;
    }

    guint paste_key; GdkModifierType paste_mods;
    gtk_accelerator_parse(kb.paste.c_str(), &paste_key, &paste_mods);
    if (keyval == paste_key && (mods & paste_mods) == paste_mods) {
        vte_terminal_paste_clipboard(state->terminal->vte());
        return TRUE;
}

    return FALSE;
}

static void activate(GtkApplication* app, gpointer data) {
    auto* state = static_cast<AppState*>(data);
    const auto& cfg = state->cfg;

    GtkWidget* window = gtk_application_window_new(app);
    state->window = GTK_WINDOW(window);
    gtk_window_set_title(GTK_WINDOW(window), cfg.general.title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), cfg.window.width, cfg.window.height);

    if (cfg.window.decorations == "none")
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    auto* sb   = new StatusBar(cfg.statusbar);
    auto* term = new Terminal(cfg, sb);
    state->statusbar = sb;
    state->terminal  = term;

    GtkWidget* vte_widget = term->widget();
    GtkWidget* ai_bar     = term->ai_bar();

    // Root is vertical (status bar top or bottom)
    GtkWidget* root    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // Content is horizontal (terminal + AI sidebar side by side)
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_widget_set_vexpand(vte_widget, TRUE);
    gtk_widget_set_hexpand(vte_widget, TRUE);
    gtk_widget_set_vexpand(content, TRUE);

    gtk_box_append(GTK_BOX(content), vte_widget);
    gtk_box_append(GTK_BOX(content), ai_bar);

    if (cfg.statusbar.position == "top") {
        if (cfg.statusbar.enabled) gtk_box_append(GTK_BOX(root), sb->widget());
        gtk_box_append(GTK_BOX(root), content);
    } else {
        gtk_box_append(GTK_BOX(root), content);
        if (cfg.statusbar.enabled) gtk_box_append(GTK_BOX(root), sb->widget());
    }

    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkEventControllerKey* key_ctrl =
        GTK_EVENT_CONTROLLER_KEY(gtk_event_controller_key_new());
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(key_ctrl), GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_key_press), state);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(key_ctrl));

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv) {
    AppState state;
    std::string config_path = NextermConfig::default_config_path();

    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--config")
            config_path = argv[i + 1];
    }

    if (!std::filesystem::exists(config_path)) {
        std::filesystem::create_directories(
            std::filesystem::path(config_path).parent_path());
        std::filesystem::copy_file(
            "/etc/nexterm/nexterm.toml", config_path,
            std::filesystem::copy_options::skip_existing);
    }

    state.cfg = NextermConfig::load(config_path);

    GtkApplication* app = gtk_application_new(
        "dev.nexterm.terminal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
