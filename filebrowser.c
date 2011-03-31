/*
    Filebrowser plugin for the DeaDBeeF audio player
    http://sourceforge.net/projects/deadbeef-fb/

    Copyright (C) 2011 Jan D. Behrens <zykure@web.de>

    Based on Geany treebrowser plugin:
        treebrowser.c - v0.20
        Copyright 2010 Adrian Dimitrov <dimitrov.adrian@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include "support.h"
#include "utils.h"
//#include <plugins/artwork/artwork.h>

//#define trace(...) { fprintf (stderr, "filebrowser: " __VA_ARGS__); }
#define trace(fmt,...)

#define     CONFSTR_FB_ENABLED              "filebrowser.enabled"
#define     CONFSTR_FB_HIDDEN               "filebrowser.hidden"
#define     CONFSTR_FB_DEFAULT_PATH         "filebrowser.defaultpath"
#define     CONFSTR_FB_SHOW_HIDDEN_FILES    "filebrowser.showhidden"
#define     CONFSTR_FB_FILTER_ENABLED       "filebrowser.filter_enabled"
#define     CONFSTR_FB_FILTER               "filebrowser.filter"
#define     CONFSTR_FB_FILTER_AUTO          "filebrowser.autofilter"
#define     CONFSTR_FB_SHOW_BOOKMARKS       "filebrowser.showbookmarks"
#define     CONFSTR_FB_SHOW_ICONS           "filebrowser.showicons"
#define     CONFSTR_FB_WIDTH                "filebrowser.sidebar_width"
#define     CONFSTR_FB_COVERART             "filebrowser.coverart_files"

#define     CONFSTR_FB_DEFAULT_PATH_DEFAULT ""
#define     CONFSTR_FB_FILTER_DEFAULT       ""
#define     CONFSTR_FB_COVERART_DEFAULT     "cover.jpg;folder.jpg;front.jpg"

static DB_misc_t plugin;
DB_functions_t *deadbeef;
//static DB_artwork_plugin_t *artwork_plugin;
static ddb_gtkui_t *gtkui_plugin;

static void filebrowser_startup (void);
static void filebrowser_shutdown (void);
static void create_sidebar (void);
static void treebrowser_chroot (gchar *directory);
static gboolean treebrowser_browse (gchar *directory, gpointer parent);
static void treebrowser_bookmarks_set_state (void);
static void treebrowser_load_bookmarks (void);
static void gtk_tree_store_iter_clear_nodes (gpointer iter, gboolean delete_root);
static void plugin_init (void);
static void plugin_cleanup (void);
static void on_drag_data_get (GtkWidget *widget, GdkDragContext *drag_context,
                GtkSelectionData *sdata, guint info, guint time,
                gpointer user_data);

static gboolean             CONFIG_CHROOT_ON_DCLICK     = TRUE;
static gboolean             CONFIG_SHOW_TREE_LINES      = FALSE;
static gint                 CONFIG_DIR_ICON_SIZE        = 24;
static gint                 CONFIG_FILE_ICON_SIZE       = 16;

static gboolean             CONFIG_ENABLED              = TRUE;
static gboolean             CONFIG_HIDDEN               = FALSE;
static const gchar *        CONFIG_DEFAULT_PATH         = NULL;
static gboolean             CONFIG_SHOW_HIDDEN_FILES    = FALSE;
static gboolean             CONFIG_FILTER_ENABLED       = TRUE;
static const gchar *        CONFIG_FILTER               = NULL;
static gboolean             CONFIG_FILTER_AUTO          = TRUE;
static gboolean             CONFIG_SHOW_BOOKMARKS       = TRUE;
static gboolean             CONFIG_SHOW_ICONS           = TRUE;
static gint                 CONFIG_WIDTH                = 200;
static const gchar *        CONFIG_COVERART             = NULL;

static GtkWidget            *mainmenuitem;
static GtkWidget            *vbox_playlist;
static GtkWidget            *hbox_all;
static GtkWidget            *treeview;
static GtkTreeStore         *treestore                  = NULL;
static GtkWidget            *sidebar_vbox               = NULL;
static GtkWidget            *sidebar_vbox_bars;
static GtkWidget            *addressbar;
static gchar                *addressbar_last_address    = NULL;
static gchar                *known_extensions           = NULL;

static GtkTreeIter          bookmarks_iter;
static gboolean             bookmarks_expanded          = FALSE;
static GSList               *expanded_rows              = NULL;

static GtkTreeViewColumn    *treeview_column_text;
static GtkCellRenderer      *render_icon, *render_text;

static gboolean             flag_on_expand_refresh      = FALSE;
static gboolean             pending_disable             = FALSE; // True if plugin is disabled on next startup

static void
filebrowser_setup_dragdrop (void)
{
    GtkTargetEntry entry = {
        .target = "text/plain",
        .flags = GTK_TARGET_SAME_APP,
        .info = 0
    };

    gtk_drag_source_set (treeview, GDK_BUTTON1_MASK, &entry, 1,
                    GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drag_source_add_uri_targets (treeview);
    g_signal_connect(treeview, "drag-data-get", G_CALLBACK (on_drag_data_get), NULL);
}

static void
update_gtkui_listview_headers (void)
{
    GtkWidget *headers_menuitem = lookup_widget (gtkui_plugin->get_mainwin (), "view_headers");
    gboolean menu_enabled = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (headers_menuitem));
    gboolean conf_enabled = deadbeef->conf_get_int ("gtkui.headers.visible", 1);

    /* Nasty workaround: emit the "headers visible" menuitem signal once or
     * twice to update the playlist view
     * TODO: Would be better to have direct acces to the ddblistview instance.
     */
    if (! conf_enabled) {
        if (! menu_enabled)
            g_signal_emit_by_name (headers_menuitem, "activate");
        g_signal_emit_by_name (headers_menuitem, "activate");
    }
}

static void
create_autofilter ()
{
    /* This uses GString to dynamically append all known extensions into a string */
    GString *buf = g_string_sized_new (256);  // reasonable initial size

    struct DB_decoder_s **decoders = deadbeef->plug_get_decoder_list ();
    for (gint i = 0; decoders[i]; i++) {
        const gchar **exts = decoders[i]->exts;
        for (gint j = 0; exts[j]; j++)
            g_string_append_printf (buf, "*.%s;", exts[j]);
    }

    if (known_extensions)
        g_free (known_extensions);
    known_extensions = g_string_free (buf, FALSE);
    trace("autofilter: %s\n", known_extensions);
}

static void
on_menu_toggle (GtkMenuItem *menuitem, gpointer *user_data)
{
    CONFIG_HIDDEN = ! gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem));

    if (CONFIG_HIDDEN)
        gtk_widget_hide (sidebar_vbox);
    else
        gtk_widget_show (sidebar_vbox);
}

static int
filebrowser_create_interface (void)
{
    trace("create sidebar\n");
    create_sidebar ();
    if (!sidebar_vbox)
        return -1;

    gtk_widget_set_size_request (sidebar_vbox, CONFIG_WIDTH, -1);

    /* Really dirty hack to include the sidebar in main GUI */
    trace("modify interface\n");
    GtkWidget *mainbox  = lookup_widget (gtkui_plugin->get_mainwin (), "vbox1");
    GtkWidget *tabstrip = lookup_widget (gtkui_plugin->get_mainwin (), "tabstrip");
    GtkWidget *playlist = lookup_widget (gtkui_plugin->get_mainwin (), "frame1");

    vbox_playlist = gtk_vbox_new (FALSE, 0);
    g_object_ref (tabstrip);    // prevent destruction of widget by removing from container
    g_object_ref (playlist);
    gtk_container_remove (GTK_CONTAINER (mainbox), tabstrip);
    gtk_container_remove (GTK_CONTAINER (mainbox), playlist);
    gtk_box_pack_start (GTK_BOX (vbox_playlist), tabstrip, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox_playlist), playlist, TRUE, TRUE, 0);
    g_object_unref (tabstrip);
    g_object_unref (playlist);

    hbox_all = gtk_hpaned_new ();
    gtk_paned_pack1 (GTK_PANED (hbox_all), sidebar_vbox, FALSE, TRUE);
    gtk_paned_pack2 (GTK_PANED (hbox_all), vbox_playlist, TRUE, TRUE);

    gtk_container_add (GTK_CONTAINER (mainbox), hbox_all);
    gtk_box_reorder_child (GTK_BOX (mainbox), hbox_all, 2);

    gtk_widget_show_all (hbox_all);
    update_gtkui_listview_headers ();

    filebrowser_setup_dragdrop ();

    return 0;
}

static int
filebrowser_restore_interface (void)
{
    // save current width of sidebar
    if (CONFIG_ENABLED && ! CONFIG_HIDDEN && ! pending_disable) {
        GtkAllocation alloc;
        gtk_widget_get_allocation (sidebar_vbox, &alloc);
        CONFIG_WIDTH = alloc.width;
    }

    trace("remove sidebar\n");
    if (!sidebar_vbox)
        return -1;

    /* Really dirty hack to include the sidebar in main GUI */
    trace("modify interface\n");
    GtkWidget *mainbox  = lookup_widget (gtkui_plugin->get_mainwin (), "vbox1");
    GtkWidget *tabstrip = lookup_widget (gtkui_plugin->get_mainwin (), "tabstrip");
    GtkWidget *playlist = lookup_widget (gtkui_plugin->get_mainwin (), "frame1");

    gtk_widget_hide (mainbox);

    g_object_ref (tabstrip);    // prevent destruction of widget by removing from container
    g_object_ref (playlist);
    gtk_container_remove (GTK_CONTAINER (vbox_playlist), tabstrip);
    gtk_container_remove (GTK_CONTAINER (vbox_playlist), playlist);
    gtk_box_pack_start (GTK_BOX (mainbox), tabstrip, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (mainbox), playlist, TRUE, TRUE, 0);
    gtk_box_reorder_child (GTK_BOX (mainbox), tabstrip, 2);
    gtk_box_reorder_child (GTK_BOX (mainbox), playlist, 3);
    g_object_unref (tabstrip);
    g_object_unref (playlist);

    gtk_container_remove (GTK_CONTAINER (hbox_all), sidebar_vbox);
    gtk_container_remove (GTK_CONTAINER (hbox_all), vbox_playlist);
    gtk_container_remove (GTK_CONTAINER (mainbox), hbox_all);

    gtk_widget_show_all (mainbox);
    update_gtkui_listview_headers ();

    sidebar_vbox = NULL;

    return 0;
}


static int
filebrowser_create_menu (void)
{
    trace("create menu entry\n");
    GtkWidget *viewmenu = lookup_widget (gtkui_plugin->get_mainwin (), "View_menu");

    mainmenuitem = gtk_check_menu_item_new_with_mnemonic (_("_Filebrowser"));
    gtk_container_add (GTK_CONTAINER (viewmenu), mainmenuitem);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mainmenuitem), ! CONFIG_HIDDEN);

    gtk_widget_show (mainmenuitem);
    g_signal_connect (mainmenuitem, "activate", G_CALLBACK (on_menu_toggle), NULL);

    return 0;
}

gboolean
filebrowser_update (void *ctx) {
    trace("update treeview\n");
    treebrowser_chroot (NULL);  // update treeview

    /* This function MUST return false because it's called from g_idle_add() */
    return FALSE;
}

static void
filebrowser_save_config (void)
{
    trace("save config\n");
    deadbeef->conf_set_int (CONFSTR_FB_ENABLED,             CONFIG_ENABLED);
    deadbeef->conf_set_int (CONFSTR_FB_HIDDEN,              CONFIG_HIDDEN);
    deadbeef->conf_set_int (CONFSTR_FB_SHOW_HIDDEN_FILES,   CONFIG_SHOW_HIDDEN_FILES);
    deadbeef->conf_set_int (CONFSTR_FB_FILTER_ENABLED,      CONFIG_FILTER_ENABLED);
    deadbeef->conf_set_int (CONFSTR_FB_FILTER_AUTO,         CONFIG_FILTER_AUTO);
    deadbeef->conf_set_int (CONFSTR_FB_SHOW_BOOKMARKS,      CONFIG_SHOW_BOOKMARKS);
    deadbeef->conf_set_int (CONFSTR_FB_SHOW_ICONS,          CONFIG_SHOW_ICONS);
    deadbeef->conf_set_int (CONFSTR_FB_WIDTH,               CONFIG_WIDTH);

    if (CONFIG_DEFAULT_PATH)
        deadbeef->conf_set_str (CONFSTR_FB_DEFAULT_PATH,    CONFIG_DEFAULT_PATH);
    if (CONFIG_FILTER)
        deadbeef->conf_set_str (CONFSTR_FB_FILTER,          CONFIG_FILTER);
    if (CONFIG_COVERART)
        deadbeef->conf_set_str (CONFSTR_FB_COVERART,        CONFIG_COVERART);
}

static void
filebrowser_load_config (void)
{
    trace("load config\n");
    if (CONFIG_DEFAULT_PATH)
        g_free ((gchar*) CONFIG_DEFAULT_PATH);
    if (CONFIG_FILTER)
        g_free ((gchar*) CONFIG_FILTER);
    if (CONFIG_COVERART)
        g_free ((gchar*) CONFIG_COVERART);

    CONFIG_ENABLED              = deadbeef->conf_get_int (CONFSTR_FB_ENABLED,             TRUE);
    CONFIG_HIDDEN               = deadbeef->conf_get_int (CONFSTR_FB_HIDDEN,              FALSE);
    CONFIG_SHOW_HIDDEN_FILES    = deadbeef->conf_get_int (CONFSTR_FB_SHOW_HIDDEN_FILES,   FALSE);
    CONFIG_FILTER_ENABLED       = deadbeef->conf_get_int (CONFSTR_FB_FILTER_ENABLED,      TRUE);
    CONFIG_FILTER_AUTO          = deadbeef->conf_get_int (CONFSTR_FB_FILTER_AUTO,         TRUE);
    CONFIG_SHOW_BOOKMARKS       = deadbeef->conf_get_int (CONFSTR_FB_SHOW_BOOKMARKS,      TRUE);
    CONFIG_SHOW_ICONS           = deadbeef->conf_get_int (CONFSTR_FB_SHOW_ICONS,          TRUE);
    CONFIG_WIDTH                = deadbeef->conf_get_int (CONFSTR_FB_WIDTH,               200);

    CONFIG_DEFAULT_PATH         = g_strdup (deadbeef->conf_get_str (CONFSTR_FB_DEFAULT_PATH,        CONFSTR_FB_DEFAULT_PATH_DEFAULT));
    CONFIG_FILTER               = g_strdup (deadbeef->conf_get_str (CONFSTR_FB_FILTER,              CONFSTR_FB_FILTER_DEFAULT));
    CONFIG_COVERART             = g_strdup (deadbeef->conf_get_str (CONFSTR_FB_COVERART,            CONFSTR_FB_COVERART_DEFAULT));
}

static int
on_config_changed (DB_event_t *ev, uintptr_t data)
{
    gboolean    enabled         = CONFIG_ENABLED;
    gboolean    hidden          = CONFIG_HIDDEN;
    gchar *     default_path    = g_strdup (CONFIG_DEFAULT_PATH);
    gboolean    show_hidden     = CONFIG_SHOW_HIDDEN_FILES;
    gboolean    filter_enabled  = CONFIG_FILTER_ENABLED;
    gchar *     filter          = g_strdup (CONFIG_FILTER);
    gboolean    filter_auto     = CONFIG_FILTER_AUTO;
    gboolean    show_bookmarks  = CONFIG_SHOW_BOOKMARKS;
    gboolean    show_icons      = CONFIG_SHOW_ICONS;
    gint        width           = CONFIG_WIDTH;
    gchar *     coverart        = g_strdup (CONFIG_COVERART);

    gboolean do_update = FALSE;

    filebrowser_load_config ();

    trace("config changed - new settings: \n"
        "enabled:           %d \n"
        "hidden:            %d \n"
        "defaultpath:       %s \n"
        "show_hidden:       %d \n"
        "filter_enabled:    %d \n"
        "filter:            %s \n"
        "filter_auto:       %d \n"
        "show_bookmarks:    %d \n"
        "show_icons:        %d \n"
        "width:             %d \n"
        "coverart:          %s \n",
        CONFIG_ENABLED,
        CONFIG_HIDDEN,
        CONFIG_DEFAULT_PATH,
        CONFIG_SHOW_HIDDEN_FILES,
        CONFIG_FILTER_ENABLED,
        CONFIG_FILTER,
        CONFIG_FILTER_AUTO,
        CONFIG_SHOW_BOOKMARKS,
        CONFIG_SHOW_ICONS,
        CONFIG_WIDTH,
        CONFIG_COVERART
        );

    if (enabled != CONFIG_ENABLED) {
        if (CONFIG_ENABLED)
            filebrowser_startup ();
        else
            filebrowser_shutdown ();
        return 0;
    }

    if (! CONFIG_ENABLED)
        return 0;

    if (hidden != CONFIG_HIDDEN) {
        if (CONFIG_HIDDEN)
            gtk_widget_hide (sidebar_vbox);
        else
            gtk_widget_show (sidebar_vbox);
    }

    if (width != CONFIG_WIDTH)
        gtk_widget_set_size_request (sidebar_vbox, CONFIG_WIDTH, -1);

    if ((show_hidden != CONFIG_SHOW_HIDDEN_FILES) ||
            (filter_enabled != CONFIG_FILTER_ENABLED) ||
            (filter_enabled && (filter_auto != CONFIG_FILTER_AUTO)) ||
            (show_bookmarks != CONFIG_SHOW_BOOKMARKS) ||
            (show_icons != CONFIG_SHOW_ICONS))
        do_update = TRUE;

    if (CONFIG_FILTER_ENABLED) {
        if (CONFIG_FILTER_AUTO) {
            gchar *autofilter = g_strdup (known_extensions);
            create_autofilter ();
            if (! utils_str_equal (autofilter, known_extensions))
                do_update = TRUE;
            g_free (autofilter);
        }
        else
            if (! utils_str_equal (filter, CONFIG_FILTER))
                do_update = TRUE;
    }

    if (! utils_str_equal (coverart, CONFIG_COVERART))
        do_update = TRUE;

    g_free (default_path);
    g_free (filter);
    g_free (coverart);

    if (do_update && CONFIG_ENABLED)
        g_idle_add (filebrowser_update, NULL);

    return 0;
}

int
filebrowser_start (void)
{
    trace("start\n");
    filebrowser_load_config ();

    return 0;
}

int
filebrowser_stop (void)
{
    trace("stop\n");
    filebrowser_save_config ();

    if (CONFIG_DEFAULT_PATH)
        g_free ((gchar*) CONFIG_DEFAULT_PATH);
    if (CONFIG_FILTER)
        g_free ((gchar*) CONFIG_FILTER);
    if (CONFIG_COVERART)
        g_free ((gchar*) CONFIG_COVERART);

    return 0;
}

static void
filebrowser_startup (void)
{
    trace("startup\n");
    filebrowser_create_interface ();
    filebrowser_create_menu ();
    if (CONFIG_HIDDEN)
        gtk_widget_hide (sidebar_vbox);

    plugin_init ();
}

static void
filebrowser_shutdown (void)
{
    trace("shutdown\n");
    filebrowser_restore_interface ();
    if (mainmenuitem)
        gtk_widget_destroy (mainmenuitem);

    plugin_cleanup ();
}

static gboolean
filebrowser_init (void *ctx) {
    if (CONFIG_ENABLED)
        filebrowser_startup ();

    deadbeef->ev_subscribe (DB_PLUGIN (&plugin), DB_EV_CONFIGCHANGED,
                    DB_CALLBACK (on_config_changed), 0);

    /* This function MUST return false because it's called from g_idle_add() */
    return FALSE;
}

int
filebrowser_connect (void)
{
    trace("connect\n");
    //artwork_plugin = (DB_artwork_plugin_t *) deadbeef->plug_get_for_id ("artwork");
    gtkui_plugin = (ddb_gtkui_t *) deadbeef->plug_get_for_id ("gtkui");
    if (! gtkui_plugin)
        return -1;

    g_idle_add (filebrowser_init, NULL);

    return 0;
}

int
filebrowser_disconnect (void)
{
    trace("disconnect\n");
    deadbeef->ev_unsubscribe (DB_PLUGIN (&plugin), DB_EV_CONFIGCHANGED,
                    DB_CALLBACK (on_config_changed), 0);

    trace("cleanup\n");
    if (CONFIG_ENABLED)
        plugin_cleanup ();

    //artwork_plugin = NULL;
    gtkui_plugin = NULL;

    return 0;
}

static const char settings_dlg[] =
    "property \"Enable\"                        checkbox "              CONFSTR_FB_ENABLED              " 1 ;\n"
    "property \"Default path: \"                entry "                 CONFSTR_FB_DEFAULT_PATH         " \"" CONFSTR_FB_DEFAULT_PATH_DEFAULT     "\" ;\n"
    "property \"Filter files by extension\"     checkbox "              CONFSTR_FB_FILTER_ENABLED       " 1 ;\n"
    "property \"Shown files: \"                 entry "                 CONFSTR_FB_FILTER               " \"" CONFSTR_FB_FILTER_DEFAULT            "\" ;\n"
    "property \"Use auto-filter instead "
        "(based on active decoder plugins)\"    checkbox "              CONFSTR_FB_FILTER_AUTO           " 1 ;\n"
    "property \"Show hidden files\"             checkbox "              CONFSTR_FB_SHOW_HIDDEN_FILES    " 0 ;\n"
    "property \"Show bookmarks\"                checkbox "              CONFSTR_FB_SHOW_BOOKMARKS       " 1 ;\n"
    "property \"Show file icons\"               checkbox "              CONFSTR_FB_SHOW_ICONS           " 1 ;\n"
    "property \"Allowed coverart files: \"      entry "                 CONFSTR_FB_COVERART             " \"" CONFSTR_FB_COVERART_DEFAULT        "\" ;\n"
    "property \"Sidebar width: \"               spinbtn[150,300,1] "    CONFSTR_FB_WIDTH                " 200 ;\n"
;

static DB_misc_t plugin = {
    DB_PLUGIN_SET_API_VERSION
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.version_major = 0,
    .plugin.version_minor = 3,
    .plugin.id = "filebrowser",
    .plugin.name = "File Browser",
    .plugin.descr = "Simple file browser,\n" "based on Geany's treebrowser plugin",
    .plugin.copyright =
        "Copyright (C) 2011 Jan D. Behrens <zykure@web.de>\n"
        "\n"
        "Based on the Geany treebrowser plugin by Adrian Dimitrov.\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "http://sourceforge.net/projects/deadbeef-fb/",
    .plugin.start = filebrowser_start,
    .plugin.stop = filebrowser_stop,
    .plugin.connect = filebrowser_connect,
    .plugin.disconnect = filebrowser_disconnect,
    .plugin.configdialog = settings_dlg,
};

DB_plugin_t *
ddb_misc_filebrowser_load (DB_functions_t *ddb) {
    deadbeef = ddb;
    return &plugin.plugin;
}


/* Treebrowser setup */
enum
{
    TREEBROWSER_COLUMN_ICON         = 0,
    TREEBROWSER_COLUMN_NAME         = 1,
    TREEBROWSER_COLUMN_URI          = 2,        // needed for browsing
    TREEBROWSER_COLUMN_TOOLTIP      = 3,
    TREEBROWSER_COLUMN_FLAG         = 4,        // needed for separator
    TREEBROWSER_COLUMNC,                        // count is set automatically

    TREEBROWSER_RENDER_ICON         = 0,
    TREEBROWSER_RENDER_TEXT         = 1,

    TREEBROWSER_FLAGS_SEPARATOR     = -1
};

/* Adding files to playlists */
enum
{
    PLT_CURRENT     = -1,
    PLT_NEW         = -2
};

/* Check if row defined by iter is expanded or not */
static gboolean
tree_view_row_expanded_iter (GtkTreeView *tree_view, GtkTreeIter *iter)
{
    GtkTreePath *path;
    gboolean expanded;

    path = gtk_tree_model_get_path (gtk_tree_view_get_model (tree_view), iter);
    expanded = gtk_tree_view_row_expanded (tree_view, path);
    gtk_tree_path_free (path);

    return expanded;
}

/* Check if file is filtered (return FALSE if file is filtered and not shown) */
static gboolean
check_filtered (const gchar *base_name)
{
    if (! CONFIG_FILTER_ENABLED)
        return TRUE;

    const gchar *filter;
    if (! CONFIG_FILTER_AUTO)
        filter = CONFIG_FILTER;
    else
        filter = known_extensions;
    if (strlen (filter) == 0)
        return TRUE;

    /* Use two filterstrings for upper- & lowercase matching */
    gchar *filter_u = g_ascii_strup (filter, -1);
    gchar *filter_d = g_ascii_strdown (filter, -1);
    gchar **filters_u = g_strsplit (filter_u, ";", 0);
    gchar **filters_d = g_strsplit (filter_d, ";", 0);

    gboolean filtered = FALSE;
    for (gint i = 0; filters_u[i] && filters_d[i]; i++) {
        if (utils_str_equal (base_name, "*")
                    || g_pattern_match_simple (filters_u[i], base_name)
                    || g_pattern_match_simple (filters_d[i], base_name)) {
            filtered = TRUE;
            break;
        }
    }

    g_free (filter_u);
    g_free (filter_d);
    g_strfreev (filters_u);
    g_strfreev (filters_d);

    return filtered;
}

/* Check if row should be expanded, returns NULL if not */
static GSList *
treeview_check_expanded (gchar *uri)
{
    if (! expanded_rows || ! uri)
        return NULL;

    GSList *node;
    for (node = expanded_rows; node; node = node->next)
        if (utils_str_equal (uri, node->data))
            break;
    return node;  // == NULL if last node was reached
}

static void
treeview_clear_expanded (void)
{
    if (! expanded_rows)
        return;

    for (GSList *node = expanded_rows; node; node = node->next)
        if (node->data)
            g_free (node->data);
    g_slist_free (expanded_rows);
    expanded_rows = g_slist_alloc ();
}

/* Restore previously expanded nodes */
static void
treeview_restore_expanded (gpointer parent)
{
    GtkTreeIter i;
    gchar *uri;
    gboolean valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (treestore), &i, parent);
    while (valid){
        gtk_tree_model_get (GTK_TREE_MODEL (treestore), &i,
                        TREEBROWSER_COLUMN_URI, &uri, -1);
        if (treeview_check_expanded (uri)) {
            gtk_tree_view_expand_row (GTK_TREE_VIEW (treeview),
                        gtk_tree_model_get_path (GTK_TREE_MODEL (treestore), &i),
                        FALSE);
            treebrowser_browse (uri, &i);
        }

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (treestore), &i);
    }
}

/* Check if file should be hidden (return TRUE if file is not shown) */
static gboolean
check_hidden (const gchar *filename)
{
    const gchar *base_name = NULL;
    base_name = g_path_get_basename (filename);

    if (! NZV (base_name))
        return FALSE;

    if ((! CONFIG_SHOW_HIDDEN_FILES) && (base_name[0] == '.'))
        return TRUE;

    return FALSE;
}

/* Get default dir from config, use home as fallback */
static gchar*
get_default_dir ()
{
    const gchar *path = CONFIG_DEFAULT_PATH;
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return g_strdup (path);

    return utils_get_home_dir ();
}

/* Get icon for selected URI - default icon or folder image */
static GdkPixbuf*
get_icon_for_uri (gchar *uri)
{
    if (! CONFIG_SHOW_ICONS)
        return NULL;

    if (! g_file_test (uri, G_FILE_TEST_IS_DIR)) {
        ////// TODO: handle mimetypes //////
        return utils_pixbuf_from_stock ("gtk-file", CONFIG_FILE_ICON_SIZE);
    }

    /* Check for cover art in folder, otherwise use default icon */
    GdkPixbuf *icon = NULL;
    gchar **coverart = g_strsplit (CONFIG_COVERART, ";", 0);
    for (gint i = 0; coverart[i]; i++) {
        gchar *coverpath = g_strconcat (uri, G_DIR_SEPARATOR_S, coverart[i], NULL);
        if (g_file_test (coverpath, G_FILE_TEST_EXISTS))
            icon = gdk_pixbuf_new_from_file_at_size (coverpath,
                            CONFIG_DIR_ICON_SIZE, CONFIG_DIR_ICON_SIZE, NULL);
        g_free (coverpath);

        if (icon)
            return icon;
    }

    // Fallback to default icon
    return utils_pixbuf_from_stock ("folder", CONFIG_DIR_ICON_SIZE);
}

/* Check if path entered in addressbar really is a directory */
static gboolean
treebrowser_checkdir (const gchar *directory)
{
    gboolean is_dir;
    static const GdkColor red       = { 0, 0xffff, 0xaaaa, 0xaaaa };
    static const GdkColor white     = { 0, 0xffff, 0xffff, 0xffff };
    static gboolean old_value       = TRUE;

    is_dir = g_file_test (directory, G_FILE_TEST_IS_DIR);

    if (old_value != is_dir) {
        gtk_widget_modify_base (GTK_WIDGET (addressbar),
                        GTK_STATE_NORMAL, is_dir ? NULL : &red);
        gtk_widget_modify_text (GTK_WIDGET (addressbar),
                        GTK_STATE_NORMAL, is_dir ? NULL : &white);
        old_value = is_dir;
    }

    if (! is_dir)
        return FALSE;

    return is_dir;
}

/* Change root directory of treebrowser */
static void
treebrowser_chroot(gchar *directory)
{
    if (! directory)
        directory = addressbar_last_address;

    if (! directory)
        directory = get_default_dir ();  // fallback

    if (g_str_has_suffix (directory, G_DIR_SEPARATOR_S))
        g_strlcpy(directory, directory, strlen (directory));

    gtk_entry_set_text (GTK_ENTRY (addressbar), directory);

    if (! directory || (strlen (directory) == 0))
        directory = G_DIR_SEPARATOR_S;

    if (! treebrowser_checkdir (directory))
        return;

    treebrowser_bookmarks_set_state ();

    gtk_tree_store_clear (treestore);
    setptr (addressbar_last_address, g_strdup (directory));

    treebrowser_browse (NULL, NULL);
    treebrowser_load_bookmarks ();
}

/* Browse given directory - update contents and fill in the treeview */
static gboolean
treebrowser_browse (gchar *directory, gpointer parent)
{
    GtkTreeIter     iter, iter_empty, *last_dir_iter = NULL;
    gboolean        is_dir;
    gboolean        expanded = FALSE;
    gboolean        has_parent;
    gchar           *utf8_name;
    GSList          *list, *node;

    gchar           *fname;
    gchar           *uri;
    gchar           *tooltip;

    if (! directory)
        directory = addressbar_last_address;

    if (! directory)
        directory = get_default_dir ();  // fallback

    directory = g_strconcat (directory, G_DIR_SEPARATOR_S, NULL);

    has_parent = parent ? gtk_tree_store_iter_is_valid (treestore, parent) : FALSE;
    if (has_parent)    {
        if (parent == &bookmarks_iter)
            treebrowser_load_bookmarks ();
    }
    else
        parent = NULL;

    if (has_parent && tree_view_row_expanded_iter (GTK_TREE_VIEW (treeview), parent)) {
        expanded = TRUE;
        treebrowser_bookmarks_set_state ();
    }

    gtk_tree_store_iter_clear_nodes (parent, FALSE);

    list = utils_get_file_list (directory, NULL, NULL);
    if (list != NULL) {
        gboolean all_hidden = TRUE;  // show "contents hidden" note if all files are hidden
        foreach_slist_free (node, list) {
            fname       = node->data;
            uri         = g_strconcat (directory, fname, NULL);
            is_dir      = g_file_test (uri, G_FILE_TEST_IS_DIR);
            utf8_name   = utils_get_utf8_from_locale (fname);
            tooltip     = utils_tooltip_from_uri (uri);

            if (! check_hidden (uri)) {
                GdkPixbuf *icon = NULL;

                if (is_dir) {
                    if (last_dir_iter == NULL)
                        gtk_tree_store_prepend (treestore, &iter, parent);
                    else {
                        gtk_tree_store_insert_after (treestore, &iter, parent, last_dir_iter);
                        gtk_tree_iter_free (last_dir_iter);
                    }
                    last_dir_iter = gtk_tree_iter_copy (&iter);

                    icon = get_icon_for_uri (uri);
                    gtk_tree_store_set (treestore, &iter,
                                    TREEBROWSER_COLUMN_ICON,    icon,
                                    TREEBROWSER_COLUMN_NAME,    fname,
                                    TREEBROWSER_COLUMN_URI,     uri,
                                    TREEBROWSER_COLUMN_TOOLTIP, tooltip,
                                    -1);
                    gtk_tree_store_prepend (treestore, &iter_empty, &iter);
                    gtk_tree_store_set (treestore, &iter_empty,
                                    TREEBROWSER_COLUMN_ICON,    NULL,
                                    TREEBROWSER_COLUMN_NAME,    _("(Empty)"),
                                    TREEBROWSER_COLUMN_URI,     NULL,
                                    TREEBROWSER_COLUMN_TOOLTIP, NULL,
                                    -1);
                    all_hidden = FALSE;
                }
                else {
                    if (check_filtered (utf8_name)) {
                        icon = get_icon_for_uri (uri);
                        gtk_tree_store_append (treestore, &iter, parent);
                        gtk_tree_store_set (treestore, &iter,
                                        TREEBROWSER_COLUMN_ICON,    icon,
                                        TREEBROWSER_COLUMN_NAME,    fname,
                                        TREEBROWSER_COLUMN_URI,     uri,
                                        TREEBROWSER_COLUMN_TOOLTIP, tooltip,
                                        -1);
                        all_hidden = FALSE;
                    }
                }

                if (icon)
                    g_object_unref (icon);
            }
            g_free (utf8_name);
            g_free (uri);
            g_free (fname);
            g_free (tooltip);
        }
        if (all_hidden) {
            /*  Directory with all contents hidden */
            gtk_tree_store_prepend (treestore, &iter_empty, parent);
            gtk_tree_store_set (treestore, &iter_empty,
                            TREEBROWSER_COLUMN_ICON,    NULL,
                            TREEBROWSER_COLUMN_NAME,    _("(Contents hidden)"),
                            TREEBROWSER_COLUMN_URI,     NULL,
                            TREEBROWSER_COLUMN_TOOLTIP, _("This directory has files in it, but they are filtered out"),
                            -1);
        }
    }
    else {
        /*  Empty directory */
        gtk_tree_store_prepend (treestore, &iter_empty, parent);
        gtk_tree_store_set (treestore, &iter_empty,
                        TREEBROWSER_COLUMN_ICON,    NULL,
                        TREEBROWSER_COLUMN_NAME,    _("(Empty)"),
                        TREEBROWSER_COLUMN_URI,     NULL,
                        TREEBROWSER_COLUMN_TOOLTIP, _("This directory has nothing in it"),
                        -1);
    }

    if (has_parent) {
        if (expanded)
            gtk_tree_view_expand_row (GTK_TREE_VIEW(treeview),
                            gtk_tree_model_get_path (GTK_TREE_MODEL(treestore), parent),
                                FALSE);
    }
    else
        treebrowser_load_bookmarks ();

    g_free (directory);

    treeview_restore_expanded (parent);

    return FALSE;
}

/* Set "bookmarks expanded" flag according to treeview */
static void
treebrowser_bookmarks_set_state ()
{
    if (gtk_tree_store_iter_is_valid (treestore, &bookmarks_iter))
        bookmarks_expanded = tree_view_row_expanded_iter (GTK_TREE_VIEW (treeview),
                        &bookmarks_iter);
    else
        bookmarks_expanded = FALSE;
}

/* Load user's bookmarks into top of tree */
static void
treebrowser_load_bookmarks ()
{
    gchar           *bookmarks;
    gchar           *contents, *path_full, *basename, *tooltip;
    gchar           **lines, **line;
    GtkTreeIter     iter;
    gchar           *pos;
    gchar           *name;
    GdkPixbuf       *icon = NULL;

    if (! CONFIG_SHOW_BOOKMARKS)
        return;

    const gchar *homedir = utils_get_home_dir ();
    bookmarks = g_build_filename (homedir, ".gtk-bookmarks", NULL);
    g_free ((gpointer *) homedir);

    if (g_file_get_contents (bookmarks, &contents, NULL, NULL)) {
        if (gtk_tree_store_iter_is_valid (treestore, &bookmarks_iter)) {
            bookmarks_expanded = tree_view_row_expanded_iter (GTK_TREE_VIEW (treeview),
                            &bookmarks_iter);
            gtk_tree_store_iter_clear_nodes (&bookmarks_iter, FALSE);
        }
        else {
            gtk_tree_store_prepend (treestore, &bookmarks_iter, NULL);
            icon = CONFIG_SHOW_ICONS ?
                            utils_pixbuf_from_stock ("user-bookmarks", CONFIG_DIR_ICON_SIZE) : NULL;
            gtk_tree_store_set (treestore, &bookmarks_iter,
                            TREEBROWSER_COLUMN_ICON,    icon,
                            TREEBROWSER_COLUMN_NAME,    _("Bookmarks"),
                            TREEBROWSER_COLUMN_URI,     NULL,
                            TREEBROWSER_COLUMN_TOOLTIP, _("Your personal bookmarks"),
                            -1);
            if (icon)
                g_object_unref (icon);

            gtk_tree_store_insert_after (treestore, &iter, NULL, &bookmarks_iter);
            gtk_tree_store_set (treestore, &iter,
                            TREEBROWSER_COLUMN_ICON,    NULL,
                            TREEBROWSER_COLUMN_NAME,    NULL,
                            TREEBROWSER_COLUMN_URI,     NULL,
                            TREEBROWSER_COLUMN_TOOLTIP, NULL,
                            TREEBROWSER_COLUMN_FLAG,    TREEBROWSER_FLAGS_SEPARATOR,
                            -1);
        }
        lines = g_strsplit (contents, "\n", 0);
        for (line = lines; *line; ++line) {
            if (**line) {
                pos = g_utf8_strchr (*line, -1, ' ');
                if (pos != NULL) {
                    *pos = '\0';
                    name = pos + 1;
                }
                else
                    name = NULL;
            }
            path_full = g_filename_from_uri (*line, NULL, NULL);
            basename  = path_full ? g_path_get_basename (path_full) : NULL;
            tooltip   = utils_tooltip_from_uri (path_full);
            if (path_full != NULL) {
                if (g_file_test (path_full, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
                    gtk_tree_store_append (treestore, &iter, &bookmarks_iter);
                    icon = CONFIG_SHOW_ICONS ?
                                    utils_pixbuf_from_stock ("folder", CONFIG_DIR_ICON_SIZE) : NULL;
                    gtk_tree_store_set (treestore, &iter,
                                    TREEBROWSER_COLUMN_ICON,    icon,
                                    TREEBROWSER_COLUMN_NAME,    basename,
                                    TREEBROWSER_COLUMN_URI,     path_full,
                                    TREEBROWSER_COLUMN_TOOLTIP, tooltip,
                                    -1);
                    if (icon)
                        g_object_unref(icon);
                    gtk_tree_store_append (treestore, &iter, &iter);
                    gtk_tree_store_set (treestore, &iter,
                                    TREEBROWSER_COLUMN_ICON,    NULL,
                                    TREEBROWSER_COLUMN_NAME,    _("(Empty)"),
                                    TREEBROWSER_COLUMN_URI,     NULL,
                                    TREEBROWSER_COLUMN_TOOLTIP, NULL,
                                    -1);
                }
                g_free (path_full);
                g_free (basename);
                g_free (tooltip);
            }
        }
        g_strfreev (lines);
        g_free (contents);

        if (bookmarks_expanded) {
            GtkTreePath *tree_path;

            tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (treestore), &bookmarks_iter);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (treeview), tree_path, FALSE);
            gtk_tree_path_free (tree_path);
        }
    }
}

/* Clear nodes from tree, optionally deleting the root node */
static void
gtk_tree_store_iter_clear_nodes (gpointer iter, gboolean delete_root)
{
    GtkTreeIter i;
    while (gtk_tree_model_iter_children (GTK_TREE_MODEL (treestore), &i, iter))
    {
        if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (treestore), &i))
            gtk_tree_store_iter_clear_nodes (&i, TRUE);
        if (gtk_tree_store_iter_is_valid (GTK_TREE_STORE (treestore), &i))
            gtk_tree_store_remove (GTK_TREE_STORE (treestore), &i);
    }
    if (delete_root)
        gtk_tree_store_remove (GTK_TREE_STORE (treestore), iter);
}

/* Add given URI to DeaDBeeF's current playlist */
static void
add_uri_to_playlist (gchar *uri, int plt)
{
    if (plt == PLT_CURRENT)
        plt = deadbeef->plt_get_curr ();
    else if (plt == PLT_NEW)
        plt = deadbeef->plt_add (deadbeef->plt_get_count (), _("New Playlist"));

    deadbeef->pl_add_files_begin (plt);
    if (g_file_test (uri, G_FILE_TEST_IS_DIR)) {
        if (deadbeef->pl_add_dir (uri, NULL, NULL) < 0)
            fprintf (stderr, "failed to add folder %s\n", uri);
    }
    else {
        if (deadbeef->pl_add_file (uri, NULL, NULL) < 0)
            fprintf (stderr, "failed to add file %s\n", uri);
    }
    deadbeef->pl_add_files_end ();

    deadbeef->plug_trigger_event_playlistchanged ();
}


/* ------------------
 * RIGHTCLICK MENU EVENTS
 * ------------------*/

static void on_button_go_up ();
static void on_button_go_home ();
static void on_button_go_root ();

static void
on_menu_add (GtkMenuItem *menuitem, gchar *uri)
{
    int plt = PLT_NEW;

    /* Some magic to get the requested playlist id */
    if (menuitem) {
        const gchar *label = gtk_menu_item_get_label (menuitem);
        gchar **slabel = g_strsplit (label, ":", 2);
        plt = atoi (slabel[0]) - 1; // this automatically selects curr_plt on conversion failure
        g_free ((gpointer *) label);
        g_strfreev (slabel);
    }

    add_uri_to_playlist (uri, plt);
}

static void
on_menu_add_current (GtkMenuItem *menuitem, gchar *uri)
{
    add_uri_to_playlist (uri, PLT_CURRENT);
}
static void
on_menu_add_new (GtkMenuItem *menuitem, gchar *uri)
{
    add_uri_to_playlist (uri, PLT_NEW);
}

static void
on_menu_enter_directory (GtkMenuItem *menuitem, gchar *uri)
{
    treebrowser_chroot (uri);
}

static void
on_menu_go_up (GtkMenuItem *menuitem, gpointer *user_data)
{
    on_button_go_up ();
}

static void
on_menu_refresh (GtkMenuItem *menuitem, gpointer *user_data)
{
    GtkTreeSelection    *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    GtkTreeIter         iter;
    GtkTreeModel        *model;
    gchar               *uri;

    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        gtk_tree_model_get (model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
        if (g_file_test (uri, G_FILE_TEST_IS_DIR))
            treebrowser_browse (uri, &iter);
        g_free (uri);
    }
    else
        treebrowser_browse (addressbar_last_address, NULL);
}

static void
on_menu_expand_all(GtkMenuItem *menuitem, gpointer *user_data)
{
    gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));
}

static void
on_menu_collapse_all(GtkMenuItem *menuitem, gpointer *user_data)
{
    gtk_tree_view_collapse_all (GTK_TREE_VIEW (treeview));
}

static void
on_menu_copy_uri(GtkMenuItem *menuitem, gchar *uri)
{
    GtkClipboard *cb = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (cb, uri, -1);
}

static void
on_menu_show_bookmarks (GtkMenuItem *menuitem, gpointer *user_data)
{
    CONFIG_SHOW_BOOKMARKS = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem));
    treebrowser_chroot (NULL);   // update tree
}

static void
on_menu_show_hidden_files(GtkMenuItem *menuitem, gpointer *user_data)
{
    CONFIG_SHOW_HIDDEN_FILES = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem));
    treebrowser_chroot (NULL);   // update tree
}

static void
on_menu_use_filter(GtkMenuItem *menuitem, gpointer *user_data)
{
    CONFIG_FILTER_ENABLED = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem));
    treebrowser_chroot (NULL);   // update tree
}

static GtkWidget*
create_popup_menu (gchar *name, gchar *uri)
{
    GtkWidget *menu     = gtk_menu_new ();
    GtkWidget *plmenu   = gtk_menu_new ();  // submenu for playlists
    GtkWidget *item;

    gboolean is_exists      = g_file_test (uri, G_FILE_TEST_EXISTS);
    gboolean is_dir         = is_exists ? g_file_test (uri, G_FILE_TEST_IS_DIR) : FALSE;

    item = gtk_menu_item_new_with_mnemonic (_("_Add to current playlist"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_add_current), uri);
    gtk_widget_set_sensitive (item, is_exists);

    item = gtk_menu_item_new_with_mnemonic (_("Add to _new playlist"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_add_new), uri);
    gtk_widget_set_sensitive (item, is_exists);

    if (is_exists) {
        gchar plt_title[32];
        for (int i = 0; i < deadbeef->plt_get_count (); i++) {
            deadbeef->plt_lock ();
            void *handle = deadbeef->plt_get_handle (i);
            deadbeef->plt_get_title (handle, plt_title, sizeof(plt_title));
            deadbeef->plt_unlock ();

            gchar *label = g_strdup_printf("%s%d: %s",
                            i < 9 ? "_" : "",   // playlists 1..9 with mnemonic
                            i+1, plt_title);
            item = gtk_menu_item_new_with_mnemonic (label);
            gtk_container_add (GTK_CONTAINER (plmenu), item);
            g_signal_connect (item, "activate", G_CALLBACK (on_menu_add), uri);
            g_free (label);
        }
    }

    item = gtk_menu_item_new_with_label (_("Add to playlist ..."));
    gtk_container_add (GTK_CONTAINER (menu), item);
    gtk_widget_set_sensitive (item, is_exists);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), plmenu);

    item = gtk_separator_menu_item_new ();
    gtk_container_add(GTK_CONTAINER (menu), item);
    gtk_widget_show (item);

    item = gtk_menu_item_new_with_mnemonic (_("_Enter directory"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_enter_directory), uri);
    gtk_widget_set_sensitive (item, is_dir);

    item = gtk_menu_item_new_with_mnemonic (_("Go _up"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_go_up), NULL);

    item = gtk_menu_item_new_with_mnemonic (_("_Refresh"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_refresh), NULL);

    item = gtk_separator_menu_item_new ();
    gtk_container_add (GTK_CONTAINER (menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("Copy full _path to clipboard"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK (on_menu_copy_uri), uri);
    gtk_widget_set_sensitive (item, is_exists);

    item = gtk_separator_menu_item_new ();
    gtk_container_add (GTK_CONTAINER (menu), item);
    gtk_widget_show (item);

    item = gtk_menu_item_new_with_mnemonic (_("E_xpand all"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_expand_all), NULL);

    item = gtk_menu_item_new_with_mnemonic (_("_Collapse all"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_collapse_all), NULL);

    item = gtk_separator_menu_item_new ();
    gtk_container_add(GTK_CONTAINER (menu), item);

    item = gtk_check_menu_item_new_with_mnemonic (_("Show _bookmarks"));
    gtk_container_add(GTK_CONTAINER(menu), item);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BOOKMARKS);
    g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bookmarks), NULL);

    item = gtk_check_menu_item_new_with_mnemonic (_("Show _hidden files"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), CONFIG_SHOW_HIDDEN_FILES);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_show_hidden_files), NULL);

    item = gtk_check_menu_item_new_with_mnemonic (_("_Filter files"));
    gtk_container_add (GTK_CONTAINER (menu), item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), CONFIG_FILTER_ENABLED);
    g_signal_connect (item, "activate", G_CALLBACK (on_menu_use_filter), NULL);

    gtk_widget_show_all (menu);

    return menu;
}


/* ------------------
 * TOOLBAR`S EVENTS
 * ------------------ */

static void
on_button_add_current ()
{
    GtkTreeIter         iter;
    GtkTreeModel        *list_store;
    GtkTreeSelection    *selection;
    gchar               *uri;

    /* Get URI for current selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (gtk_tree_selection_get_selected (selection, &list_store, &iter)) {
        gtk_tree_model_get (GTK_TREE_MODEL (treestore), &iter,
                        TREEBROWSER_COLUMN_URI, &uri, -1);
        add_uri_to_playlist (uri, -1);
        g_free (uri);
    }
}

static void
on_button_refresh ()
{
    treebrowser_chroot (NULL);
}

static void
on_button_go_up ()
{
    treeview_clear_expanded ();
    gchar *uri = g_path_get_dirname (addressbar_last_address);
    treebrowser_chroot (uri);
    g_free (uri);
}

static void
on_button_go_home ()
{
    treeview_clear_expanded ();
    gchar *uri = utils_get_home_dir ();
    treebrowser_chroot (uri);
    g_free (uri);

}

static void
on_button_go_root ()
{
    treeview_clear_expanded ();
    gchar *uri = g_strdup (G_DIR_SEPARATOR_S);
    treebrowser_chroot (uri);
    g_free (uri);
}

static void
on_button_go_default ()
{
    treeview_clear_expanded ();
    gchar *uri = get_default_dir ();
    treebrowser_chroot (uri);
    g_free (uri);
}

static void
on_addressbar_activate (GtkEntry *entry, gpointer user_data)
{
    treeview_clear_expanded ();
    gchar *uri = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
    treebrowser_chroot (uri);
    g_free (uri);
}


/* ------------------
 * TREEVIEW EVENTS
 * ------------------ */

static gboolean
on_treeview_mouseclick (GtkWidget *widget, GdkEventButton *event,
                GtkTreeSelection *selection)
{
    GtkTreePath     *path = NULL;
    GtkTreeIter     iter;
    GtkTreeModel    *model;
    gchar           *name = "", *uri = "";

    gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x, event->y,
                    &path, NULL, NULL, NULL);
    if (path)
        gtk_tree_selection_select_path (selection, path);

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
        /* FIXME: name and uri should be freed, but they are passed to create_popup_menu()
         * that pass them directly to some callbacks, so we can't free them here for now.
         * Gotta find a way out... */
        gtk_tree_model_get (GTK_TREE_MODEL(treestore), &iter,
                        TREEBROWSER_COLUMN_NAME, &name,
                        TREEBROWSER_COLUMN_URI, &uri,
                        -1);

    if (event->button == 3) {
        gtk_menu_popup (GTK_MENU (create_popup_menu (name, uri)),
                        NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void
on_treeview_changed (GtkWidget *widget, gpointer user_data)
{
    GtkTreeIter     iter;
    GtkTreeModel    *model;
    gchar           *uri;
    gboolean        has_selection = FALSE;

    if (gtk_tree_selection_get_selected (GTK_TREE_SELECTION (widget), &model, &iter)) {
        gtk_tree_model_get (GTK_TREE_MODEL (treestore), &iter,
                        TREEBROWSER_COLUMN_URI, &uri, -1);
        if (uri == NULL)
            return;

        if (g_file_test (uri, G_FILE_TEST_EXISTS)) {
            if (g_file_test (uri, G_FILE_TEST_IS_DIR))
                treebrowser_browse (uri, &iter);
        }
        else
            gtk_tree_store_iter_clear_nodes (&iter, TRUE);

        g_free (uri);

        has_selection = TRUE;
    }

    if (user_data)
        gtk_widget_set_sensitive (GTK_WIDGET (user_data), has_selection);
}

static void
on_treeview_row_activated (GtkWidget *widget, GtkTreePath *path,
                GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeIter     iter;
    gchar           *uri;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (treestore), &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (treestore), &iter,
                    TREEBROWSER_COLUMN_URI, &uri, -1);

    if (uri == NULL)
        return;

    if (g_file_test (uri, G_FILE_TEST_IS_DIR)) {
        if (CONFIG_CHROOT_ON_DCLICK)
            treebrowser_chroot (uri);
        else {
            if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (widget), path))
                gtk_tree_view_collapse_row (GTK_TREE_VIEW (widget), path);
            else
                gtk_tree_view_expand_row (GTK_TREE_VIEW (widget), path, FALSE);
        }
    }

    g_free(uri);
}

static void
on_treeview_row_expanded (GtkWidget *widget, GtkTreeIter *iter,
                GtkTreePath *path, gpointer user_data)
{
    gchar *uri;

    gtk_tree_model_get (GTK_TREE_MODEL (treestore), iter,
                    TREEBROWSER_COLUMN_URI, &uri, -1);
    if (uri == NULL)
        return;

    if (flag_on_expand_refresh == FALSE) {
        flag_on_expand_refresh = TRUE;
        treebrowser_browse(uri, iter);
        gtk_tree_view_expand_row (GTK_TREE_VIEW(treeview), path, FALSE);
        flag_on_expand_refresh = FALSE;
    }

    if (CONFIG_SHOW_ICONS) {
        GdkPixbuf *icon = get_icon_for_uri (uri);
        gtk_tree_store_set (treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
        g_object_unref (icon);
    }

    GSList *node = treeview_check_expanded (uri);
    if (! node)
        expanded_rows = g_slist_append (expanded_rows, g_strdup (uri));

    g_free (uri);
}

static void
on_treeview_row_collapsed (GtkWidget *widget, GtkTreeIter *iter,
                GtkTreePath *path, gpointer user_data)
{
    gchar *uri;

    gtk_tree_model_get (GTK_TREE_MODEL (treestore), iter,
                    TREEBROWSER_COLUMN_URI, &uri, -1);
    if (uri == NULL)
        return;

    if (CONFIG_SHOW_ICONS) {
        GdkPixbuf *icon = get_icon_for_uri (uri);
        gtk_tree_store_set (treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
        g_object_unref (icon);
    }

    for (GSList *node = expanded_rows; node; node = node->next)
        if (utils_str_equal (uri, node->data)) {
            g_free (node->data);
            expanded_rows = g_slist_delete_link (expanded_rows, node);
            break;
        }

    GSList *node = treeview_check_expanded (uri);
    if (node) {
        g_free (node->data);
        expanded_rows = g_slist_delete_link (expanded_rows, node);
    }

    g_free (uri);
}


/* ------------------
 * TREEBROWSER INITIAL FUNCTIONS
 * ------------------ */

static gboolean
treeview_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    gint flag;
    gtk_tree_model_get (model, iter, TREEBROWSER_COLUMN_FLAG, &flag, -1);
    return (flag == TREEBROWSER_FLAGS_SEPARATOR);
}

static GtkWidget*
create_view_and_model ()
{
    GtkWidget               *view;

    view                    = gtk_tree_view_new ();
    treeview_column_text    = gtk_tree_view_column_new ();
    render_icon             = gtk_cell_renderer_pixbuf_new ();
    render_text             = gtk_cell_renderer_text_new ();

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (view), treeview_column_text);

    gtk_tree_view_column_pack_start (treeview_column_text, render_icon, FALSE);
    gtk_tree_view_column_set_attributes (treeview_column_text, render_icon,
                    "pixbuf", TREEBROWSER_RENDER_ICON, NULL);

    gtk_tree_view_column_pack_start (treeview_column_text, render_text, TRUE);
    gtk_tree_view_column_add_attribute (treeview_column_text, render_text,
                    "text", TREEBROWSER_RENDER_TEXT);

    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), TRUE);
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (view), TREEBROWSER_COLUMN_NAME);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), treeview_separator_func,
                    NULL, NULL);
    gtk_tree_selection_set_mode (gtk_tree_view_get_selection(GTK_TREE_VIEW (view)),
                    GTK_SELECTION_SINGLE);

#if GTK_CHECK_VERSION(2, 10, 0)
    g_object_set (view, "has-tooltip", TRUE, "tooltip-column", TREEBROWSER_COLUMN_TOOLTIP, NULL);
    gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (view), CONFIG_SHOW_TREE_LINES);
#endif

    treestore = gtk_tree_store_new (TREEBROWSER_COLUMNC, GDK_TYPE_PIXBUF,
                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    gtk_tree_view_set_model (GTK_TREE_VIEW(view), GTK_TREE_MODEL (treestore));

    return view;
}

static void
create_sidebar ()
{
    if (sidebar_vbox)
        return;

    GtkWidget           *scrollwin;
    GtkWidget           *toolbar;
    GtkWidget           *wid, *button_add;
    GtkTreeSelection    *selection;

    treeview            = create_view_and_model ();
    sidebar_vbox        = gtk_vbox_new (FALSE, 0);
    sidebar_vbox_bars   = gtk_vbox_new (FALSE, 0);
    selection           = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    addressbar          = gtk_entry_new ();
    scrollwin           = gtk_scrolled_window_new (NULL, NULL);

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_UP));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Go to parent directory"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_go_up), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Refresh current directory"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_refresh), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GOTO_TOP));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Go to top directory"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_go_root), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_HOME));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Go to home directory"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_go_home), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Go to default directory"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_go_default), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_item_new ());
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (wid), TRUE);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);

    wid = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_ADD));
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (wid), _("Add selection to current playlist"));
    g_signal_connect (wid, "clicked", G_CALLBACK (on_button_add_current), NULL);
    gtk_container_add (GTK_CONTAINER (toolbar), wid);
    gtk_widget_set_sensitive (wid, FALSE);
    button_add = wid;

    gtk_container_add(GTK_CONTAINER (scrollwin), treeview);

    gtk_box_pack_start (GTK_BOX (sidebar_vbox_bars), addressbar, FALSE, TRUE, 1);
    gtk_box_pack_start (GTK_BOX (sidebar_vbox_bars), toolbar,  FALSE, TRUE, 1);

    gtk_box_pack_start (GTK_BOX (sidebar_vbox), sidebar_vbox_bars, FALSE, TRUE, 1);
    gtk_box_pack_start (GTK_BOX (sidebar_vbox), scrollwin, TRUE, TRUE, 1);

    g_signal_connect (selection,    "changed",              G_CALLBACK (on_treeview_changed),           button_add);
    g_signal_connect (treeview,     "button-press-event",   G_CALLBACK (on_treeview_mouseclick),        selection);
    g_signal_connect (treeview,     "row-activated",        G_CALLBACK (on_treeview_row_activated),     NULL);
    g_signal_connect (treeview,     "row-collapsed",        G_CALLBACK (on_treeview_row_collapsed),     NULL);
    g_signal_connect (treeview,     "row-expanded",         G_CALLBACK (on_treeview_row_expanded),      NULL);
    g_signal_connect (addressbar,   "activate",             G_CALLBACK (on_addressbar_activate),        NULL);

    gtk_widget_show_all (sidebar_vbox);
}

static void
plugin_init ()
{
    trace ("init\n");
    create_autofilter ();
    expanded_rows = g_slist_alloc ();
    treebrowser_chroot (NULL);
}

static void
plugin_cleanup ()
{
    trace ("cleanup\n");
    treeview_clear_expanded ();
    g_slist_free (expanded_rows);
    g_free (addressbar_last_address);
    g_free (known_extensions);

    expanded_rows = NULL;
    addressbar_last_address = NULL;
    known_extensions = NULL;
}

static void
on_drag_data_get (GtkWidget *widget, GdkDragContext *drag_context,
                  GtkSelectionData *sdata, guint info, guint time,
                  gpointer user_data)
{
    GtkTreeIter         iter;
    GtkTreeModel        *list_store;
    GtkTreeSelection    *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    if (gtk_tree_selection_get_selected (selection, &list_store, &iter)) {
        gchar *uri, *enc_uri;
        gtk_tree_model_get (GTK_TREE_MODEL (treestore), &iter,
                        TREEBROWSER_COLUMN_URI, &uri, -1);

        /* Encode Filename to URI - important! */
        enc_uri = g_filename_to_uri (uri, NULL, NULL);
        trace("dnd send: %s (%s)\n", uri, enc_uri);
        gtk_selection_data_set (sdata, sdata->target, 8, (guchar*) enc_uri, strlen (enc_uri));

        g_free (uri);
        g_free (enc_uri);
    }
}

/* END OF FILE */
