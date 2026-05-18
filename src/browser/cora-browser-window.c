/* CoraOS Web Browser - Window Implementation (Part 1)
 *
 * Real tabbed web browser using WebKitGTK engine.
 * Full web standards support via WebKit rendering engine.
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-browser-window.h"
#include <webkit/webkit.h>
#include <string.h>

#define DEFAULT_HOME "https://start.duckduckgo.com"

/* Per-tab data */
typedef struct {
    WebKitWebView *web_view;
    GtkWidget     *tab_page;     /* AdwTabPage */
} BrowserTab;

struct _CoraBrowserWindow {
    AdwApplicationWindow parent_instance;

    /* UI */
    GtkWidget          *toolbar_view;
    GtkWidget          *header_bar;
    GtkWidget          *url_entry;
    GtkWidget          *back_btn;
    GtkWidget          *forward_btn;
    GtkWidget          *reload_btn;
    GtkWidget          *home_btn;
    GtkWidget          *new_tab_btn;
    GtkWidget          *menu_btn;
    AdwTabView         *tab_view;
    AdwTabBar          *tab_bar;

    /* State */
    WebKitWebContext   *web_context;
    char               *homepage;
};

G_DEFINE_TYPE (CoraBrowserWindow, cora_browser_window, ADW_TYPE_APPLICATION_WINDOW)

/* Forward declarations */
static void     create_tab_with_uri  (CoraBrowserWindow *self, const char *uri);
static void     update_navigation_ui (CoraBrowserWindow *self);
static WebKitWebView *get_current_web_view (CoraBrowserWindow *self);


/* === WebView Signal Handlers === */

static void
on_web_view_load_changed (WebKitWebView *web_view, WebKitLoadEvent event,
                          gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);

    if (web_view != get_current_web_view (self))
        return;

    switch (event) {
    case WEBKIT_LOAD_STARTED:
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->url_entry), 0.1);
        break;
    case WEBKIT_LOAD_COMMITTED: {
        const char *uri = webkit_web_view_get_uri (web_view);
        if (uri)
            gtk_editable_set_text (GTK_EDITABLE (self->url_entry), uri);
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->url_entry), 0.5);
        break;
    }
    case WEBKIT_LOAD_FINISHED:
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->url_entry), 0.0);
        update_navigation_ui (self);
        break;
    default:
        break;
    }
}

static void
on_web_view_title_changed (WebKitWebView *web_view, GParamSpec *pspec,
                           gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    const char *title = webkit_web_view_get_title (web_view);

    /* Find the tab page for this web view and update its title */
    int n = adw_tab_view_get_n_pages (self->tab_view);
    for (int i = 0; i < n; i++) {
        AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
        GtkWidget *child = adw_tab_page_get_child (page);
        if (GTK_WIDGET (web_view) == child) {
            adw_tab_page_set_title (page, title ? title : "New Tab");
            break;
        }
    }

    /* Update window title if this is the active tab */
    if (web_view == get_current_web_view (self)) {
        g_autofree char *win_title = g_strdup_printf ("%s - CoraOS Browser",
                                                      title ? title : "New Tab");
        gtk_window_set_title (GTK_WINDOW (self), win_title);
    }
}

static void
on_web_view_load_progress (WebKitWebView *web_view, GParamSpec *pspec,
                           gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);

    if (web_view != get_current_web_view (self))
        return;

    double progress = webkit_web_view_get_estimated_load_progress (web_view);
    if (progress >= 1.0)
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->url_entry), 0.0);
    else
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->url_entry), progress);
}

static gboolean
on_web_view_decide_policy (WebKitWebView *web_view,
                           WebKitPolicyDecision *decision,
                           WebKitPolicyDecisionType type,
                           gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);

    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        WebKitNavigationPolicyDecision *nav_decision =
            WEBKIT_NAVIGATION_POLICY_DECISION (decision);
        WebKitNavigationAction *action =
            webkit_navigation_policy_decision_get_navigation_action (nav_decision);
        WebKitURIRequest *request =
            webkit_navigation_action_get_request (action);
        const char *uri = webkit_uri_request_get_uri (request);

        /* Open in new tab instead of new window */
        create_tab_with_uri (self, uri);
        webkit_policy_decision_ignore (decision);
        return TRUE;
    }

    return FALSE;
}

static WebKitWebView *
on_create_new_tab (WebKitWebView *web_view, WebKitNavigationAction *action,
                   gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitURIRequest *request = webkit_navigation_action_get_request (action);
    const char *uri = webkit_uri_request_get_uri (request);

    create_tab_with_uri (self, uri);
    return NULL; /* We handle it ourselves */
}

/* === Download Handling === */

static void
on_download_started (WebKitWebContext *context, WebKitDownload *download,
                     gpointer user_data)
{
    /* Set download destination to ~/Downloads */
    const char *uri = webkit_download_get_request (download) ?
        webkit_uri_request_get_uri (webkit_download_get_request (download)) : "file";

    g_autofree char *basename = g_path_get_basename (uri);
    g_autofree char *dest = g_build_filename (
        g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD),
        basename, NULL);
    g_autofree char *dest_uri = g_filename_to_uri (dest, NULL, NULL);

    webkit_download_set_destination (download, dest_uri);
    g_message ("Downloading: %s -> %s", uri, dest);
}


/* === Tab Management === */

static WebKitWebView *
get_current_web_view (CoraBrowserWindow *self)
{
    AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);
    if (!page)
        return NULL;

    GtkWidget *child = adw_tab_page_get_child (page);
    if (WEBKIT_IS_WEB_VIEW (child))
        return WEBKIT_WEB_VIEW (child);

    return NULL;
}

static void
create_tab_with_uri (CoraBrowserWindow *self, const char *uri)
{
    WebKitWebView *web_view;
    WebKitSettings *settings;
    AdwTabPage *page;

    /* Create WebView with settings */
    settings = webkit_settings_new_with_settings (
        "enable-javascript", TRUE,
        "enable-smooth-scrolling", TRUE,
        "enable-webgl", TRUE,
        "enable-media-stream", TRUE,
        "enable-mediasource", TRUE,
        "enable-developer-extras", TRUE,
        "default-font-family", "Noto Sans",
        "default-font-size", 16,
        "default-monospace-font-family", "Fira Mono",
        "default-monospace-font-size", 13,
        "user-agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 "
                      "(KHTML, like Gecko) CoraOS/1.0 Safari/605.1.15",
        NULL
    );

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new_with_context (self->web_context));
    webkit_web_view_set_settings (web_view, settings);
    g_object_unref (settings);

    /* Connect signals */
    g_signal_connect (web_view, "load-changed",
                      G_CALLBACK (on_web_view_load_changed), self);
    g_signal_connect (web_view, "notify::title",
                      G_CALLBACK (on_web_view_title_changed), self);
    g_signal_connect (web_view, "notify::estimated-load-progress",
                      G_CALLBACK (on_web_view_load_progress), self);
    g_signal_connect (web_view, "decide-policy",
                      G_CALLBACK (on_web_view_decide_policy), self);
    g_signal_connect (web_view, "create",
                      G_CALLBACK (on_create_new_tab), self);

    gtk_widget_set_hexpand (GTK_WIDGET (web_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (web_view), TRUE);

    /* Add as tab */
    page = adw_tab_view_append (self->tab_view, GTK_WIDGET (web_view));
    adw_tab_page_set_title (page, "New Tab");
    adw_tab_view_set_selected_page (self->tab_view, page);

    /* Load URI */
    const char *load_uri = (uri && uri[0] != '\0') ? uri : self->homepage;
    webkit_web_view_load_uri (web_view, load_uri);
}

static void
on_tab_selected (AdwTabView *tab_view, GParamSpec *pspec, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);

    if (!web_view)
        return;

    /* Update URL bar */
    const char *uri = webkit_web_view_get_uri (web_view);
    if (uri)
        gtk_editable_set_text (GTK_EDITABLE (self->url_entry), uri);

    /* Update title */
    const char *title = webkit_web_view_get_title (web_view);
    g_autofree char *win_title = g_strdup_printf ("%s - CoraOS Browser",
                                                  title ? title : "New Tab");
    gtk_window_set_title (GTK_WINDOW (self), win_title);

    update_navigation_ui (self);
}

/* === Navigation Buttons === */

static void
update_navigation_ui (CoraBrowserWindow *self)
{
    WebKitWebView *web_view = get_current_web_view (self);
    if (!web_view)
        return;

    gtk_widget_set_sensitive (self->back_btn,
                             webkit_web_view_can_go_back (web_view));
    gtk_widget_set_sensitive (self->forward_btn,
                             webkit_web_view_can_go_forward (web_view));
}

static void
on_back_clicked (GtkButton *btn, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);
    if (web_view && webkit_web_view_can_go_back (web_view))
        webkit_web_view_go_back (web_view);
}

static void
on_forward_clicked (GtkButton *btn, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);
    if (web_view && webkit_web_view_can_go_forward (web_view))
        webkit_web_view_go_forward (web_view);
}

static void
on_reload_clicked (GtkButton *btn, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);
    if (web_view)
        webkit_web_view_reload (web_view);
}

static void
on_home_clicked (GtkButton *btn, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);
    if (web_view)
        webkit_web_view_load_uri (web_view, self->homepage);
}

static void
on_url_entry_activate (GtkEntry *entry, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    WebKitWebView *web_view = get_current_web_view (self);
    const char *text;

    if (!web_view)
        return;

    text = gtk_editable_get_text (GTK_EDITABLE (entry));
    if (!text || text[0] == '\0')
        return;

    /* Determine if it's a URL or search query */
    g_autofree char *uri = NULL;
    if (g_str_has_prefix (text, "http://") ||
        g_str_has_prefix (text, "https://") ||
        g_str_has_prefix (text, "file://")) {
        uri = g_strdup (text);
    } else if (strchr (text, '.') != NULL && !strchr (text, ' ')) {
        /* Looks like a domain */
        uri = g_strdup_printf ("https://%s", text);
    } else {
        /* Search query */
        g_autofree char *escaped = g_uri_escape_string (text, NULL, FALSE);
        uri = g_strdup_printf ("https://duckduckgo.com/?q=%s", escaped);
    }

    webkit_web_view_load_uri (web_view, uri);
}

static void
on_new_tab_clicked (GtkButton *btn, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    create_tab_with_uri (self, self->homepage);
}


/* === Menu === */

static void
on_private_window (GSimpleAction *action, GVariant *param, gpointer user_data)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (user_data);
    /* Create ephemeral web context for private browsing */
    WebKitWebContext *priv_ctx = webkit_web_context_new_ephemeral ();
    WebKitWebView *web_view = WEBKIT_WEB_VIEW (
        webkit_web_view_new_with_context (priv_ctx));

    AdwTabPage *page = adw_tab_view_append (self->tab_view, GTK_WIDGET (web_view));
    adw_tab_page_set_title (page, "Private Tab");
    adw_tab_page_set_icon (page,
        g_themed_icon_new ("security-high-symbolic"));
    adw_tab_view_set_selected_page (self->tab_view, page);

    webkit_web_view_load_uri (web_view, self->homepage);
    g_object_unref (priv_ctx);
}

static GtkWidget *
create_menu_popover (CoraBrowserWindow *self)
{
    GtkWidget *popover, *box;

    popover = gtk_popover_new ();
    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start (box, 6);
    gtk_widget_set_margin_end (box, 6);
    gtk_widget_set_margin_top (box, 6);
    gtk_widget_set_margin_bottom (box, 6);

    struct { const char *label; const char *icon; void (*cb)(GtkButton*, gpointer); } items[] = {
        { "New Tab",        "tab-new-symbolic",          (void*)on_new_tab_clicked },
        { "Private Tab",    "security-high-symbolic",    NULL },
        { "Reload",         "view-refresh-symbolic",     (void*)on_reload_clicked },
        { "Home",           "go-home-symbolic",          (void*)on_home_clicked },
    };

    for (int i = 0; i < G_N_ELEMENTS (items); i++) {
        GtkWidget *btn = gtk_button_new ();
        GtkWidget *btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *icon = gtk_image_new_from_icon_name (items[i].icon);
        GtkWidget *label = gtk_label_new (items[i].label);
        gtk_box_append (GTK_BOX (btn_box), icon);
        gtk_box_append (GTK_BOX (btn_box), label);
        gtk_button_set_child (GTK_BUTTON (btn), btn_box);
        gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE);
        gtk_widget_set_hexpand (btn, TRUE);

        if (items[i].cb) {
            g_signal_connect (btn, "clicked",
                              G_CALLBACK (items[i].cb), self);
        } else if (g_strcmp0 (items[i].label, "Private Tab") == 0) {
            g_signal_connect_swapped (btn, "clicked", G_CALLBACK (
                +[](GtkButton *b, gpointer data) {
                    on_private_window (NULL, NULL, data);
                }), self);
        }

        gtk_box_append (GTK_BOX (box), btn);
    }

    gtk_popover_set_child (GTK_POPOVER (popover), box);
    return popover;
}

/* === UI Construction === */

static void
cora_browser_window_build_ui (CoraBrowserWindow *self)
{
    GtkWidget *content_box;

    gtk_window_set_title (GTK_WINDOW (self), "CoraOS Browser");
    gtk_window_set_default_size (GTK_WINDOW (self), 1200, 800);

    /* Main layout */
    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (self), content_box);

    /* Header bar with navigation */
    self->header_bar = adw_header_bar_new ();
    adw_header_bar_set_show_title (ADW_HEADER_BAR (self->header_bar), FALSE);

    /* Back */
    self->back_btn = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_tooltip_text (self->back_btn, "Back");
    gtk_widget_set_sensitive (self->back_btn, FALSE);
    g_signal_connect (self->back_btn, "clicked", G_CALLBACK (on_back_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->back_btn);

    /* Forward */
    self->forward_btn = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_tooltip_text (self->forward_btn, "Forward");
    gtk_widget_set_sensitive (self->forward_btn, FALSE);
    g_signal_connect (self->forward_btn, "clicked", G_CALLBACK (on_forward_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->forward_btn);

    /* Reload */
    self->reload_btn = gtk_button_new_from_icon_name ("view-refresh-symbolic");
    gtk_widget_set_tooltip_text (self->reload_btn, "Reload");
    g_signal_connect (self->reload_btn, "clicked", G_CALLBACK (on_reload_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->reload_btn);

    /* URL entry (center, expands) */
    self->url_entry = gtk_entry_new ();
    gtk_widget_set_hexpand (self->url_entry, TRUE);
    g_object_set (self->url_entry, "placeholder-text", "Search or enter URL...", NULL);
    gtk_entry_set_input_purpose (GTK_ENTRY (self->url_entry), GTK_INPUT_PURPOSE_URL);
    g_signal_connect (self->url_entry, "activate",
                      G_CALLBACK (on_url_entry_activate), self);
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (self->header_bar), self->url_entry);

    /* New tab button */
    self->new_tab_btn = gtk_button_new_from_icon_name ("tab-new-symbolic");
    gtk_widget_set_tooltip_text (self->new_tab_btn, "New Tab");
    g_signal_connect (self->new_tab_btn, "clicked",
                      G_CALLBACK (on_new_tab_clicked), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->new_tab_btn);

    /* Menu button */
    self->menu_btn = gtk_menu_button_new ();
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->menu_btn),
                                   "open-menu-symbolic");
    GtkWidget *menu_pop = create_menu_popover (self);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->menu_btn), menu_pop);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->menu_btn);

    gtk_box_append (GTK_BOX (content_box), self->header_bar);

    /* Tab bar */
    self->tab_view = ADW_TAB_VIEW (adw_tab_view_new ());
    adw_tab_view_set_shortcuts (self->tab_view,
        ADW_TAB_VIEW_SHORTCUT_ALL_SHORTCUTS);

    self->tab_bar = ADW_TAB_BAR (adw_tab_bar_new ());
    adw_tab_bar_set_view (self->tab_bar, self->tab_view);
    adw_tab_bar_set_autohide (self->tab_bar, FALSE);
    adw_tab_bar_set_expand_tabs (self->tab_bar, TRUE);

    gtk_box_append (GTK_BOX (content_box), GTK_WIDGET (self->tab_bar));

    /* Tab view content */
    gtk_widget_set_vexpand (GTK_WIDGET (self->tab_view), TRUE);
    gtk_box_append (GTK_BOX (content_box), GTK_WIDGET (self->tab_view));

    /* Watch for tab selection changes */
    g_signal_connect (self->tab_view, "notify::selected-page",
                      G_CALLBACK (on_tab_selected), self);
}

/* === GObject === */

static void
cora_browser_window_finalize (GObject *object)
{
    CoraBrowserWindow *self = CORA_BROWSER_WINDOW (object);
    g_free (self->homepage);
    g_clear_object (&self->web_context);
    G_OBJECT_CLASS (cora_browser_window_parent_class)->finalize (object);
}

static void
cora_browser_window_init (CoraBrowserWindow *self)
{
    self->homepage = g_strdup (DEFAULT_HOME);
    self->web_context = NULL;
}

static void
cora_browser_window_class_init (CoraBrowserWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = cora_browser_window_finalize;
}

CoraBrowserWindow *
cora_browser_window_new (GtkApplication *app, const char *initial_uri)
{
    CoraBrowserWindow *self = g_object_new (CORA_TYPE_BROWSER_WINDOW,
                                            "application", app,
                                            NULL);

    /* Create persistent web context with cookie/cache storage */
    g_autofree char *data_dir = g_build_filename (
        g_get_user_data_dir (), "coraos-browser", NULL);
    g_autofree char *cache_dir = g_build_filename (
        g_get_user_cache_dir (), "coraos-browser", NULL);

    WebKitWebsiteDataManager *data_mgr = webkit_website_data_manager_new (
        "base-data-directory", data_dir,
        "base-cache-directory", cache_dir,
        NULL
    );

    self->web_context = webkit_web_context_new_with_website_data_manager (data_mgr);
    g_object_unref (data_mgr);

    /* Enable favicons */
    webkit_web_context_set_favicon_database_directory (self->web_context,
        g_build_filename (data_dir, "favicons", NULL));

    /* Download handler */
    g_signal_connect (self->web_context, "download-started",
                      G_CALLBACK (on_download_started), self);

    /* Cookie persistence */
    WebKitCookieManager *cookie_mgr =
        webkit_web_context_get_cookie_manager (self->web_context);
    g_autofree char *cookie_file = g_build_filename (data_dir, "cookies.db", NULL);
    webkit_cookie_manager_set_persistent_storage (cookie_mgr, cookie_file,
        WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);

    cora_browser_window_build_ui (self);

    /* Open initial tab */
    create_tab_with_uri (self, initial_uri ? initial_uri : self->homepage);

    return self;
}

void
cora_browser_window_new_tab (CoraBrowserWindow *self, const char *uri)
{
    g_return_if_fail (CORA_IS_BROWSER_WINDOW (self));
    create_tab_with_uri (self, uri);
}
