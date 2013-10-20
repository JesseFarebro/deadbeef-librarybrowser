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

#include <gtk/gtk.h>


/* Config options */

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
#define     CONFSTR_FB_COVERART_SIZE        "filebrowser.coverart_size"

#define     DEFAULT_FB_DEFAULT_PATH         ""
#define     DEFAULT_FB_FILTER               ""  // auto-filter enabled by default
#define     DEFAULT_FB_COVERART             "cover.jpg;folder.jpg;front.jpg"


/* Treebrowser setup */
enum
{
    TREEBROWSER_COLUMN_ICON             = 0,
    TREEBROWSER_COLUMN_NAME             = 1,
    TREEBROWSER_COLUMN_URI              = 2,        // needed for browsing
    TREEBROWSER_COLUMN_TOOLTIP          = 3,
    TREEBROWSER_COLUMN_FLAG             = 4,        // needed for separator
    TREEBROWSER_COLUMNC,                            // count is set automatically

    TREEBROWSER_RENDER_ICON             = 0,
    TREEBROWSER_RENDER_TEXT             = 1,

    TREEBROWSER_FLAGS_SEPARATOR         = -1
};


/* Adding files to playlists */
enum
{
    PLT_CURRENT         = -1,
    PLT_NEW             = -2
};


static void         gtkui_update_listview_headers (void);
static void         setup_dragdrop (void);
static void         create_autofilter (void);
static void         save_config (void);
static void         load_config (void);
static gboolean     treeview_update (void *ctx);
static gboolean     filebrowser_init (void *ctx);
static int          handle_message (uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2);

static void         on_menu_toggle (GtkMenuItem *menuitem, gpointer *user_data);
static int          on_config_changed (uintptr_t data);
static void         on_drag_data_get (GtkWidget *widget, GdkDragContext *drag_context,
                            GtkSelectionData *sdata, guint info, guint time,
                            gpointer user_data);

static int          create_menu_entry (void);
static int          create_interface (GtkWidget *cont);
static int          restore_interface (void);
static GtkWidget *  create_popup_menu (gchar *name, gchar *uri);
static GtkWidget *  create_view_and_model (void);
static void         create_sidebar (void);

static void         gtk_tree_store_iter_clear_nodes (gpointer iter, gboolean delete_root);
static void         add_uri_to_playlist (gchar *uri, int plt);
static gboolean     check_filtered (const gchar *base_name);
static gboolean     check_hidden (const gchar *filename);
static gchar *      get_default_dir (void);
static GdkPixbuf *  get_icon_from_cache (const gchar *uri, const gchar *coverart,
                            gint imgsize);
static GdkPixbuf *  get_icon_for_uri (gchar *uri);
static gboolean     treeview_row_expanded_iter (GtkTreeView *tree_view, GtkTreeIter *iter);
static GSList *     treeview_check_expanded (gchar *uri);
static void         treeview_clear_expanded (void);
static void         treeview_restore_expanded (gpointer parent);
static gboolean     treeview_separator_func (GtkTreeModel *model, GtkTreeIter *iter,
                            gpointer data);
static gboolean     treebrowser_checkdir (const gchar *directory);
static void         treebrowser_chroot(gchar *directory);
static gboolean     treebrowser_browse (gchar *directory, gpointer parent);
static void         treebrowser_bookmarks_set_state (void);
static void         treebrowser_load_bookmarks (void);

static void         on_menu_add (GtkMenuItem *menuitem, gchar *uri);
static void         on_menu_add_current (GtkMenuItem *menuitem, gchar *uri);
static void         on_menu_add_new (GtkMenuItem *menuitem, gchar *uri);
static void         on_menu_enter_directory (GtkMenuItem *menuitem, gchar *uri);
static void         on_menu_go_up (GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_refresh (GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_expand_all(GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_collapse_all(GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_copy_uri(GtkMenuItem *menuitem, gchar *uri);
static void         on_menu_show_bookmarks (GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_show_hidden_files(GtkMenuItem *menuitem, gpointer *user_data);
static void         on_menu_use_filter(GtkMenuItem *menuitem, gpointer *user_data);

static void         on_button_add_current (void);
static void         on_button_refresh (void);
static void         on_button_go_up (void);
static void         on_button_go_home (void);
static void         on_button_go_root (void);
static void         on_button_go_default (void);
static void         on_addressbar_activate (GtkEntry *entry, gpointer user_data);

static gboolean     on_treeview_mouseclick (GtkWidget *widget, GdkEventButton *event,
                            GtkTreeSelection *selection);
static void         on_treeview_changed (GtkWidget *widget, gpointer user_data);
static void         on_treeview_row_activated (GtkWidget *widget, GtkTreePath *path,
                            GtkTreeViewColumn *column, gpointer user_data);
static void         on_treeview_row_expanded (GtkWidget *widget, GtkTreeIter *iter,
                        GtkTreePath *path, gpointer user_data);
static void         on_treeview_row_collapsed (GtkWidget *widget, GtkTreeIter *iter,
                            GtkTreePath *path, gpointer user_data);

static int          plugin_init (void);
static int          plugin_cleanup (void);


/* Exported public functions */

int                 filebrowser_start (void);
int                 filebrowser_stop (void);
int                 filebrowser_startup (GtkWidget *);
int                 filebrowser_shutdown (void);
int                 filebrowser_connect (void);
int                 filebrowser_disconnect (void);
DB_plugin_t *       ddb_misc_filebrowser_load (DB_functions_t *ddb);
