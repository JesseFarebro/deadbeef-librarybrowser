#include "utils.h"


/* ------------------
 * UTILITY FUNCTIONS
 * ------------------ */

/* Get pixbuf icon from current icon theme */
GdkPixbuf *
utils_pixbuf_from_stock (const gchar *icon_name, gint size)
{
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    if (icon_theme)
        return gtk_icon_theme_load_icon (icon_theme, icon_name, size, 0, NULL);

    return NULL;
}

/* Some utility functions */
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

gchar *
utils_get_utf8_from_locale(const gchar *locale_text)
{
    gchar *utf8_text;

    if (! locale_text)
        return NULL;
    utf8_text = g_locale_to_utf8(locale_text, -1, NULL, NULL, NULL);
    if (utf8_text == NULL)
        utf8_text = g_strdup(locale_text);
    return utf8_text;
}

gchar *
utils_get_home_dir (void)
{
    /* Note Glib documentation - get_home_dir() may not give the real home directory! */
    if (g_getenv ("HOME"))
        return g_strdup (g_getenv ("HOME"));
    return g_strdup (g_get_home_dir ());
}

gchar *
utils_tooltip_from_uri (const gchar *uri)
{
    /* Tooltips can't have an ampersand '&' in them */
    if (! uri)
        return NULL;
    gchar **strings = g_strsplit (uri, "&", 0);
    gchar *result = g_strjoinv ("&amp;", strings);
    g_strfreev (strings);
    return result;
}
