/* UTILITY FUNCTIONS */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "utils.h"


/* Get pixbuf icon from current icon theme */
GdkPixbuf *
utils_pixbuf_from_stock (const gchar *icon_name, gint size)
{
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    if (icon_theme)
        return gtk_icon_theme_load_icon (icon_theme, icon_name, size, 0, NULL);

    return NULL;
}

/* Check if two strings are exactly the same */
gboolean
utils_str_equal (const gchar *a, const gchar *b)
{
    /* (taken from libexo from os-cillation) */
    if (a == NULL && b == NULL)
        return TRUE;
    else if (a == NULL || b == NULL)
        return FALSE;

    while (*a == *b++)
        if (*a++ == '\0')
            return TRUE;

    return FALSE;
}

/* Compare two strings, ignoring case differences */
gint
utils_str_casecmp (const gchar *s1, const gchar *s2)
{
    gchar *tmp1, *tmp2;
    gint result;

    g_return_val_if_fail (s1 != NULL, 1);
    g_return_val_if_fail (s2 != NULL, -1);

    tmp1 = g_strdup (s1);
    tmp2 = g_strdup (s2);

    /* first ensure strings are UTF-8 */
    if (! g_utf8_validate (s1, -1, NULL))
        setptr (tmp1, g_locale_to_utf8 (s1, -1, NULL, NULL, NULL));
    if (! g_utf8_validate (s2, -1, NULL))
        setptr (tmp2, g_locale_to_utf8 (s2, -1, NULL, NULL, NULL));

    if (tmp1 == NULL) {
        g_free (tmp2);
        return 1;
    }
    if (tmp2 == NULL) {
        g_free (tmp1);
        return -1;
    }

    /* then convert the strings into a case-insensitive form */
    setptr (tmp1, g_utf8_strdown (tmp1, -1));
    setptr (tmp2, g_utf8_strdown (tmp2, -1));

    /* compare */
    result = strcmp (tmp1, tmp2);
    g_free (tmp1);
    g_free (tmp2);
    return result;
}

/* Get list of filenames inside a directory */
GSList *
utils_get_file_list_full (const gchar *path, gboolean full_path, gboolean sort, GError **error)
{
    GSList *list = NULL;
    GDir *dir;
    const gchar *filename;

    if (error)
        *error = NULL;
    g_return_val_if_fail (path != NULL, NULL);

    dir = g_dir_open (path, 0, error);
    if (dir == NULL)
        return NULL;

    foreach_dir (filename, dir) {
        list = g_slist_prepend (list, full_path ? g_build_path (G_DIR_SEPARATOR_S, path, filename, NULL) : g_strdup (filename));
    }
    g_dir_close (dir);

    /* sorting last is quicker than on insertion */
    if (sort)
        list = g_slist_sort (list, (GCompareFunc) utils_str_casecmp);
    else
        list = g_slist_reverse (list);
    return list;
}

GSList *
utils_get_file_list (const gchar *path, guint *length, GError **error)
{
    GSList *list = utils_get_file_list_full (path, FALSE, TRUE, error);

    if (length)
        *length = g_slist_length (list);
    return list;
}

/* Convert text in local encoding to UTF8 */
gchar *
utils_get_utf8_from_locale(const gchar *locale_text)
{
    gchar *utf8_text;

    if (! locale_text)
        return NULL;
    utf8_text = g_locale_to_utf8 (locale_text, -1, NULL, NULL, NULL);
    if (utf8_text == NULL)
        utf8_text = g_strdup (locale_text);
    return utf8_text;
}

/* Get current home directory */
gchar *
utils_get_home_dir (void)
{
    /* Note Glib documentation - get_home_dir() may not give the real home directory! */
    if (g_getenv ("HOME"))
        return g_strdup (g_getenv ("HOME"));
    return g_strdup (g_get_home_dir ());
}

/* Get URI tooltip from URI, escaping ampersands ('&') */
gchar *
utils_tooltip_from_uri (const gchar *uri)
{
    if (! uri)
        return NULL;
    gchar **strings = g_strsplit (uri, "&", 0);
    gchar *result = g_strjoinv ("&amp;", strings);
    g_strfreev (strings);
    return result;
}

/* Get cache path for directory icon of URI */
gchar *
utils_make_cache_path (const gchar *uri, gint imgsize)
{
    /* Path for cached icon is constructed like this:
     *     $XDG_CACHE_HOME/deadbeef-fb/icons/<imgsize>/<uri-without-separators>.png
     * If $XDG_CACHE_HOME is undefined, $HOME/.cache/deadbeef-fb/ is used instead.
     *
     * Example: The coverart image
     *     /home/user/Music/SomeArtist/Album01/cover.jpg
     * will lead to the cached at
     *     /home/user/.cache/deadbeef-fb/24/home_user_Music_SomeArtist_Album01.png
     */
    const gchar *cache = g_getenv ("XDG_CACHE_HOME");
    GString *path, *fullpath;
    gchar *cachedir, *fname;

    path = g_string_sized_new (256);  // reasonable initial size
    g_string_printf (path,
                    cache ? "%s/deadbeef-fb/icons/%d/" : "%s/.cache/deadbeef-fb/icons/%d/",
                    cache ? cache : g_getenv ("HOME"),
                    imgsize);
    cachedir = g_string_free (path, FALSE);

    /* Create path if it doesn't exist already */
    if (! g_file_test (cachedir, G_FILE_TEST_IS_DIR))
        utils_check_dir (cachedir, 0755);

    fullpath = g_string_new (g_strdup (cachedir));

    fname = g_strdup (uri);
    for (gchar *p = fname+1; *p; p++) {
        if ((*p == G_DIR_SEPARATOR_S[0]) || (*p == ' ')) {
            *p = '_';
        }
    }

    g_string_append_printf (fullpath, "/%s.png", fname);

    g_free (cachedir);
    g_free (fname);

    return g_string_free (fullpath, FALSE);
}

/* Copied from  <deadbeef>/plugins/artwork/artwork.c  with few adjustments */
gint
utils_check_dir (const gchar *dir, mode_t mode)
{
    gchar *tmp = g_strdup (dir);
    gchar *slash = tmp;
    struct stat stat_buf;
    do
    {
        slash = strstr (slash+1, "/");
        if (slash)
            *slash = 0;
        if (-1 == stat (tmp, &stat_buf)) {
            int errno = mkdir (tmp, mode);
            if (0 != errno) {
                fprintf (stderr, "Failed to create %s (%d)\n", tmp, errno);
                g_free (tmp);
                return 0;
            }
        }
        if (slash)
            *slash = '/';
    }
    while (slash);

    g_free (tmp);
    return 1;
}

void
utils_construct_style (GtkWidget *widget, const gchar *bgcolor, const gchar *fgcolor, const gchar *bgcolor_sel, const gchar *fgcolor_sel)
{
    if (! widget)
        return;

    GString *style = g_string_new ("");
#if !GTK_CHECK_VERSION(3,0,0)
    style = g_string_append (style, "style \"deadbeef-filebrowser\" { \n");
    if (strlen(bgcolor) > 0)       g_string_append_printf (style, "    base[NORMAL]   = \"%s\" \n", bgcolor);
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    base[SELECTED] = \"%s\" \n", bgcolor_sel);
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    base[ACTIVE]   = \"%s\" \n", bgcolor_sel);
    if (strlen(fgcolor) > 0)       g_string_append_printf (style, "    text[NORMAL]   = \"%s\" \n", fgcolor);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    text[SELECTED] = \"%s\" \n", fgcolor_sel);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    text[ACTIVE]   = \"%s\" \n", fgcolor_sel);
    if (strlen(bgcolor) > 0)       g_string_append_printf (style, "    bg[NORMAL]     = \"%s\" \n", bgcolor);
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    bg[SELECTED]   = \"%s\" \n", bgcolor_sel);
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    bg[ACTIVE]     = \"%s\" \n", bgcolor_sel);
    if (strlen(fgcolor) > 0)       g_string_append_printf (style, "    fg[NORMAL]     = \"%s\" \n", fgcolor);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    fg[SELECTED]   = \"%s\" \n", fgcolor_sel);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    fg[ACTIVE]     = \"%s\" \n", fgcolor_sel);
    style = g_string_append (style, "} \n");
    style = g_string_append (style, "widget \"*.deadbeef_filebrowser_treeview\" style \"deadbeef-filebrowser\" \n");
#else
    style = g_string_append (style, "* { \n");
    if (strlen(bgcolor) > 0)       g_string_append_printf (style, "    background-color: %s; \n", bgcolor);
    if (strlen(fgcolor) > 0)       g_string_append_printf (style, "    color:            %s; \n", fgcolor);
    style = g_string_append (style, "} \n");
    style = g_string_append (style, "*:selected { \n");
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    background-color: %s; \n", bgcolor_sel);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    color:            %s; \n", fgcolor_sel);
    style = g_string_append (style, "} \n");
    style = g_string_append (style, "*:active { \n");
    if (strlen(bgcolor_sel) > 0)   g_string_append_printf (style, "    background-color: %s; \n", bgcolor_sel);
    if (strlen(fgcolor_sel) > 0)   g_string_append_printf (style, "    color:            %s; \n", fgcolor_sel);
    style = g_string_append (style, "} \n");
#endif

    gchar* style_str = g_string_free (style, FALSE);
    fprintf(stderr, "gtk style: \n%s", style_str);
#if !GTK_CHECK_VERSION(3,0,0)
    gtk_rc_parse_string (style_str);
#else
    GtkCssProvider *css_provider = gtk_css_provider_get_default ();  // do NOT free!
    gtk_css_provider_load_from_data (css_provider, style_str, -1, NULL);
    GtkStyleContext *style_ctx = gtk_widget_get_style_context (widget);  // do NOT free!
    gtk_style_context_add_provider (style_ctx, (GtkStyleProvider *) css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
#endif
    g_free (style_str);
}

gboolean
tree_view_expand_rows_recursive (GtkTreeModel *model, GtkTreeView *view, GtkTreePath *parent, gint max_depth)
{
    GtkTreeIter iter;
    if (! gtk_tree_model_get_iter(model, &iter, parent))  // check if path is valid
        return FALSE;

    if (max_depth > 0 && gtk_tree_path_get_depth (parent) >= max_depth)
        return FALSE;

    // when expanding, this should come *before* going down the tree
    gtk_tree_view_expand_row (view, parent, TRUE);

    GtkTreePath *path = gtk_tree_path_copy (parent);
    gtk_tree_path_down (path);
    while (tree_view_expand_rows_recursive (model, view, path, max_depth))
        gtk_tree_path_next (path);
    gtk_tree_path_free (path);

    return TRUE;
}

gboolean
tree_view_collapse_rows_recursive (GtkTreeModel *model, GtkTreeView *view, GtkTreePath *parent, gint max_depth)
{
    GtkTreeIter iter;
    if (! gtk_tree_model_get_iter(model, &iter, parent))  // check if path is valid
        return FALSE;

    if (max_depth > 0 && gtk_tree_path_get_depth (parent) >= max_depth)
        return FALSE;

    GtkTreePath *path = gtk_tree_path_copy (parent);
    gtk_tree_path_down (path);
    while (tree_view_collapse_rows_recursive (model, view, path, max_depth))
        gtk_tree_path_next (path);
    gtk_tree_path_free (path);

    // when expanding, this should come *after* going down the tree
    gtk_tree_view_collapse_row (view, parent);

    return TRUE;
}
