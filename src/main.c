/*
 * main.c initial généré par Glade. Éditer ce fichier à votre
 * convenance. Glade n'écrira plus dans ce fichier.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include "support.h"

GtkWidget *CFDWindow;
int
main(int argc, char *argv[])
{
	GtkWidget *AboutWindow;


	gtk_init(&argc, &argv);


	/*
	 * The following code was added by Glade to create one of each component
	 * (except popup menus), just so that you see something after building
	 * the project. Delete any components that you don't want shown initially.
	 */

	AboutWindow = CreateAboutWindow();
	gtk_widget_show(AboutWindow);


	gtk_main();
	/* gtk_item_factory_dump_rc ("config.cfg", NULL, 1); */
	return 0;
}
