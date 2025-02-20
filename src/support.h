/*
 * NE PAS �DITER CE FICHIER - il est g�n�r� par Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

/*
 * Public Functions.
 */

/*
 * This function returns a widget in a component created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 */
GtkWidget *  lookup_widget(GtkWidget *widget, const gchar *widget_name);

/* get_widget() is deprecated. Use lookup_widget instead. */
#define get_widget lookup_widget

/* Use this function to set the directory containing installed pixmaps. */
void        add_pixmap_directory(const gchar *directory);


/*
 * Private Functions.
 */

/* This is used to create the pixmaps in the interface. */
GtkWidget *  create_pixmap(GtkWidget *widget, const gchar *filename);
