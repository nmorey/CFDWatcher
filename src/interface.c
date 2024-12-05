#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk-2.0/gtk/gtktoolbar.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "logo.xpm"
#include "About.xpm"

/* operations souris */
#define MOUSE_ZOOM             0x01
#define MOUSE_MEASURE          0x02

/*** gestion des couleurs ***/
/* nombre de valeurs differentes a prendre dans chaque composante RGB */
#define RGB_STEP 4
/* nombre total de couleurs ... ici 64 */
#define NB_COLORS (RGB_STEP * RGB_STEP * RGB_STEP)
/* et des couleurs supplementaires reservees */
#define BLACK NB_COLORS
#define WHITE (NB_COLORS + 1)
#define RED (NB_COLORS + 2)
#define GREEN (NB_COLORS + 3)
#define BLUE (NB_COLORS + 4)
#define XOR (NB_COLORS + 5)
#define GDASHED (NB_COLORS + 6)
#define RDASHED (NB_COLORS + 7)
#define TOTAL_COLORS (NB_COLORS + 8)
#define INVERT_COLOR(Color) (Color == BLACK?WHITE:BLACK)
#define MAXWPT 2048
#define ZOOM_RATIO 1.2
#define ZOOM_LIM 0.5

#define VIEW_WIDTH 400
#define VIEW_HEIGHT 300



#define MENU_NUMBER 30
extern char *logo_xmp;
extern char *About_xmp;
GtkWidget *CFDWindow = 0;

static void about_window_callback(){
    CreateAboutWindow();
}

static GtkItemFactoryEntry Menu[] =
{
	/***************************************************************************************************************/
	/*    Titre                   , Raccouci     , Callback                                   , Param , Type       */
	/***************************************************************************************************************/
	{ "/Fichier",							    NULL,	  NULL,						      0, "<Branch>"    },
	{ "/Fichier/Ouvrir un fichier trace",				    "<Control>o", on_MenuOpen_activate,				      0, "<Item>"      },
	{ "/Fichier/Ouvrir un fichier balise",				    "<Control>b", on_MenuOpenBalise_activate,			      0, "<Item>"      },
	{ "/Fichier/Lire le GPS",					    "<Control>g", SaveTrackWindow,				      0, "<Item>"      },
	{ "/Fichier/Quitter",						    "<Control>q", gtk_main_quit,				      0, "<Item>"      },
	/***************************************************************************************************************/
	{ "/Vue",							    NULL,	  NULL,						      0, "<Branch>"    },
	{ "/Vue/Zoomer",						    0,		  ZoomIn,					      0, "<Item>"      },
	{ "/Vue/Dezoomer",						    0,		  ZoomOut,					      0, "<Item>"      },
	{ "/Vue/Recadrer",						    0,		  Fit,						      0, "<Item>"      },
	/***************************************************************************************************************/
	{ "/CFD Delta",							    NULL,	  NULL,						      0, "<Branch>"    },
	{ "/CFD Delta/Chercher le meilleur Vol",			    "<Control>d", on_auto_activate,				      0, "<Item>"      },
	{ "/CFD Delta/separateur",					    0,		  NULL,						      0, "<Separator>" },
	{ "/CFD Delta/Distance Libre",					    0,		  on_distance_libre_activate,			      0, "<Item>"      },
	{ "/CFD Delta/Distance Libre avec point de contournement",	    0,		  on_distance_libre_avec_contournement_activate,      0, "<Item>"      },
	{ "/CFD Delta/Triangle Simple",					    0,		  on_triangle_activate,				      0, "<Item>"      },
	{ "/CFD Delta/Triangle FAI",					    0,		  on_triangle_fai_activate,			      0, "<Item>"      },
	{ "/CFD Delta/Aller-Retour",					    0,		  on_aller_retour_activate,			      0, "<Item>"      },
	{ "/CFD Delta/Quadrilatere",					    0,		  on_quadrilatere_activate,			      0, "<Item>"      },
	/***************************************************************************************************************/
	{ "/CFD Parapente",						    NULL,	  NULL,						      0, "<Branch>"    },
	{ "/CFD Parapente/Chercher le meilleur Vol",			    "<Control>p", on_autopara_activate,				      0, "<Item>"      },
	{ "/CFD Parapente/separateur",					    0,		  NULL,						      0, "<Separator>" },
	{ "/CFD Parapente/Distance Libre",				    0,		  on_distance_libre_activate,			      0, "<Item>"      },
	{ "/CFD Parapente/Distance Libre avec un point de contournement",   0,		  on_distance_libre_avec_contournement_activate,      0, "<Item>"      },
	{ "/CFD Parapente/Distance Libre avec deux point de contournement", 0,		  on_distance_libre_avec_deux_contournement_activate, 0, "<Item>"      },
	{ "/CFD Parapente/Triangle Simple",				    0,		  on_trianglepara_activate,			      0, "<Item>"      },
	{ "/CFD Parapente/Triangle FAI",				    0,		  on_trianglepara_fai_activate,			      0, "<Item>"      },
	{ "/CFD Parapente/Aller-Retour",				    0,		  on_aller_retourpara_activate,			      0, "<Item>"      },
	{ "/CFD Parapente/Quadrilatere",				    0,		  on_quadrilaterepara_activate,			      0, "<Item>"      },
	/***************************************************************************************************************/
	{ "/?",								    NULL,	  NULL,						      0, "<Branch>"    },
	{ "/?/About",							    0,		  about_window_callback,			      0, "<Item>"      }
};

extern double coordx[MAXWPT];
extern double coefx[MAXWPT];
extern double coordy[MAXWPT];
extern double coefy[MAXWPT];
extern int TableX[7];
extern int TableBorne[5];
extern int Waypoints;
extern int DepArv[3];
extern int DrawWpt[5];
extern BaliseStruct *FirstBalise;

GtkWidget *
mycreate_pixmap(GtkWidget *	widget,
		gchar **	XPM)
{
	GdkColormap *colormap;
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;
	GtkWidget *pixmap;


	colormap = gtk_widget_get_colormap(widget);
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap, &mask,
							  NULL, XPM);
	pixmap = gtk_pixmap_new(gdkpixmap, mask);
	gdk_pixmap_unref(gdkpixmap);
	gdk_bitmap_unref(mask);
	return pixmap;
}

/**   dessin de la figure ... */
void DrawDisplay(GtkWidget *Window)
{
	ContextStruct *Context;
	double k;
	int i;
	char WptName[256];
	BaliseStruct *Balise;


	/* on recupere la drawing area et ses parametres */
	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;


	gdk_draw_rectangle(Context->Pixmap, Context->ColorGC[Context->Background], 1, 0, 0, Context->Width, Context->Height);

	for (k = 0; k < Waypoints; k = k + 1 / (Context->Scale)) {
		if (((coordx[(int)floor(k)] < Context->RightWindow) &&
		     (coordx[(int)floor(k)] > Context->LeftWindow) &&
		     (coordy[(int)floor(k)] < Context->TopWindow) &&
		     (coordy[(int)floor(k)] > Context->BottomWindow)) ||
		    ((coordx[(int)ceil(k)] < Context->RightWindow) &&
		     (coordx[(int)ceil(k)] > Context->LeftWindow) &&
		     (coordy[(int)ceil(k)] < Context->TopWindow) &&
		     (coordy[(int)ceil(k)] > Context->BottomWindow)))
			gdk_draw_point(Context->Pixmap, Context->ColorGC[BLACK],
				       XReal2Graphic(Context, SplineEval(Waypoints, coordx, coefx, k)),
				       YReal2Graphic(Context, SplineEval(Waypoints, coordy, coefy, k)));
	}

	for (i = 0; TableX[i] != -1; i++) {
		if ((TableX[i + 1] == -1))
			break;
		if (TableX[i + 1] > TableX[i]) {
			gdk_draw_line(Context->Pixmap, Context->ColorGC[GREEN],
				      XReal2Graphic(Context, coordx[TableX[i]]),
				      YReal2Graphic(Context, coordy[TableX[i]]),
				      XReal2Graphic(Context, coordx[TableX[i + 1]]),
				      YReal2Graphic(Context, coordy[TableX[i + 1]]));
		} else {
			gdk_draw_line(Context->Pixmap, Context->ColorGC[GDASHED],
				      XReal2Graphic(Context, coordx[TableX[i]]),
				      YReal2Graphic(Context, coordy[TableX[i]]),
				      XReal2Graphic(Context, coordx[TableX[i + 1]]),
				      YReal2Graphic(Context, coordy[TableX[i + 1]]));
		}
	}

	for (i = 0; TableBorne[i] != -1; i += 2) {
		gdk_draw_line(Context->Pixmap, Context->ColorGC[RED],
			      XReal2Graphic(Context, coordx[TableBorne[i]]),
			      YReal2Graphic(Context, coordy[TableBorne[i]]),
			      XReal2Graphic(Context, coordx[TableBorne[i + 1]]),
			      YReal2Graphic(Context, coordy[TableBorne[i + 1]]));
	}





	for (i = 0; DepArv[i] >= 0; i++) {
		gdk_draw_arc(Context->Pixmap,
			     Context->ColorGC[RED], TRUE,
			     XReal2Graphic(Context, coordx[DepArv[i]]) - 3, YReal2Graphic(Context, coordy[DepArv[i]]) - 3,
			     6, 6, 0, 360 * 64);
		switch (i) {
		case 0:
			sprintf(WptName, "B D");
			break;
		case 1:
			sprintf(WptName, "B A");
			break;
		}
		NameWaypoint(Window,
			     XReal2Graphic(Context, coordx[DepArv[i]]),
			     YReal2Graphic(Context, coordy[DepArv[i]]),
			     WptName,
			     RED);
	}


	for (i = 0; DrawWpt[i] >= 0; i++) {
		gdk_draw_arc(Context->Pixmap,
			     Context->ColorGC[BLUE], TRUE,
			     XReal2Graphic(Context, coordx[DrawWpt[i]]) - 3, YReal2Graphic(Context, coordy[DrawWpt[i]]) - 3,
			     6, 6, 0, 360 * 64);
		sprintf(WptName, "B %d", i + 1);
		NameWaypoint(Window,
			     XReal2Graphic(Context, coordx[DrawWpt[i]]),
			     YReal2Graphic(Context, coordy[DrawWpt[i]]),
			     WptName,
			     BLUE);
	}

	if (Waypoints != 0) {
		for (Balise = FirstBalise; Balise; Balise = Balise->Next) {
			gdk_draw_arc(Context->Pixmap,
				     Context->ColorGC[BLUE], TRUE,
				     XReal2Graphic(Context, Balise->PlaneX) - 3, YReal2Graphic(Context, Balise->PlaneY) - 3,
				     6, 6, 0, 360 * 64);
			sprintf(WptName, "%s", Balise->Name);
			NameWaypoint(Window,
				     XReal2Graphic(Context, Balise->PlaneX),
				     YReal2Graphic(Context, Balise->PlaneY),
				     WptName,
				     BLUE);
		}
	}


	RefreshDisplay(Window);
}


/**  substitut a la fonction de base de gdk pour eviter de se poser des questions
 * sur les min/max du rectangle */
void
my_draw_rectangle(GdkDrawable *Drawable, GdkGC *GC, gint Fill, gint X1, gint Y1, gint X2, gint Y2)
{
	gint X, Y;
	gint W, H;

	X = MIN(X1, X2);
	Y = MIN(Y1, Y2);
	W = MAX(X1, X2) - X;
	H = MAX(Y1, Y2) - Y;
	if (W && H)             /* si une des 2 dimensions est nulle, on ne dessine rien */
		gdk_draw_rectangle(Drawable, GC, Fill, X, Y, W, H);
}

/**
 * renvoie la coordonnee reelle en X qui correspond a la coordonnee graphique passee */
double
XGraphic2Real(ContextStruct *Context, int X)
{
	return (double)(X - Context->ShiftX) / Context->Scale;
}

/**
 * renvoie la coordonnee reelle en Y qui correspond a la coordonnee graphique passee */
double
YGraphic2Real(ContextStruct *Context, int Y)
{
	return (double)((Context->Height - Y) - Context->ShiftY) / Context->Scale;
}

/**
 * renvoie la coordonnee graphique en X qui correspond a la coordonnee reelle passee
 * les coordonnees sont systematiquement tronquees a 1 plan image en dehors de la zone graphique
 * pour eviter des problemes de debordement des entiers */
int
XReal2Graphic(ContextStruct *Context, double X)
{
	double XGraphic;

	XGraphic = (X * Context->Scale) + Context->ShiftX;

	if (XGraphic < -(double)0x7fff)
		return -0x7fff;
	else if (XGraphic > (double)0x7fff)
		return 0x7fff;
	else
		return (int)rint(XGraphic);
}


/**
 * renvoie la coordonnee graphique en Y qui correspond a la coordonnee reelle passee
 * les coordonnees sont systematiquement tronquees a 1 plan image en dehors de la zone graphique
 * pour eviter des problemes de debordement des entiers */
int
YReal2Graphic(ContextStruct *Context, double Y)
{
	double YGraphic;

	YGraphic = Context->Height - (Y * Context->Scale) - Context->ShiftY;

	if (YGraphic < -(double)0x7fff)
		return -0x7fff;
	else if (YGraphic > (double)0x7fff)
		return 0x7fff;
	else
		return (int)rint(YGraphic);
}

/**
 * affiche le texte centre sur les coordonnees et un cadre entourant le tout */
void
CenterString(GtkWidget *Window, gint XMin, gint YMin, gint XMax, gint YMax, char *String)
{
	gint Xcenter, Ycenter;
	gint Width;
	gint Height;
	GdkFont *Police;
	ContextStruct *Context;
	int x, y;


	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	Police = gdk_font_load("-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*");
	Width = gdk_string_width(Police, String);
#ifndef WINDOWS
	Height = gdk_string_height(Police, String);
#else
	Height = 8;
#endif

	if ((ABS(XMax - XMin) > Width + 10) || (ABS(YMax - YMin) > Height + 10)) { /* il y a assez de place entre les deux points pour afficher les coordonnees */
		x = (XMax + XMin) / 2;
		y = (YMax + YMin) / 2;
	} else { /* pas assez de place, il faut mettre le texte a cote
		  * et de preference le plus au centre de l'ecran */
		if (MIN(XMin, XMax) < Context->Width / 2)
			x = MAX(XMin, XMax) + Width / 2 + 5;
		else
			x = MIN(XMin, XMax) - Width / 2 - 5;
		if (MIN(YMin, YMax) < Context->Height / 2)
			y = MAX(YMin, YMax);
		else
			y = MIN(YMin, YMax);
	}



	Xcenter = x - Width / 2;
	Ycenter = y - Height / 2 - 2;
	gdk_draw_rectangle(Context->DrawingArea->window,
			   Context->ColorGC[Context->Background],
			   TRUE,
			   x - Width / 2 - 2,
			   y - Height / 2 - 4, Width + 2, Height + 6);
	gdk_draw_rectangle(Context->DrawingArea->window,
			   Context->ColorGC[INVERT_COLOR(Context->Background)],
			   FALSE,
			   x - Width / 2 - 2,
			   y - Height / 2 - 4, Width + 2, Height + 6);
	gdk_draw_string(Context->DrawingArea->window,
			Police,
			Context->ColorGC[INVERT_COLOR(Context->Background)],
			Xcenter, Ycenter + 10, String);
}

void
NameWaypoint(GtkWidget *Window, gint XMin, gint YMin, char *String, int Color)
{
	gint Xcenter, Ycenter;
	gint Width;
	gint Height;
	GdkFont *Police;
	ContextStruct *Context;
	int x;


	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	Police = gdk_font_load("-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*");
	Width = gdk_string_width(Police, String);
#ifndef WINDOWS
	Height = gdk_string_height(Police, String);
#else
	Height = 8;
#endif


	/* pas assez de place, il faut mettre le texte a cote
	 *      et de preference le plus au centre de l'ecran */
	if (XMin < Context->Width / 2)
		x = XMin + Width / 2 + 10;
	else
		x = XMin - Width / 2 - 10;





	Xcenter = x - Width / 2;
	Ycenter = YMin - Height / 2 - 2;
	gdk_draw_rectangle(Context->Pixmap,
			   Context->ColorGC[Context->Background],
			   TRUE,
			   x - Width / 2 - 2,
			   YMin - Height / 2 - 4, Width + 2, Height + 6);
	gdk_draw_rectangle(Context->Pixmap,
			   Context->ColorGC[INVERT_COLOR(Context->Background)],
			   FALSE,
			   x - Width / 2 - 2,
			   YMin - Height / 2 - 4, Width + 2, Height + 6);
	gdk_draw_string(Context->Pixmap,
			Police,
			Context->ColorGC[Color],
			Xcenter, Ycenter + 10, String);
}


/**
 * affiche la distance entre le point courant et le point Ref memorise dans le contexte */
void
DrawMeasure(GtkWidget *Window, int X, int Y)
{
	char Label[256];
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	/* on trace une ligne joignant les 2 points */
	gdk_draw_line(Context->DrawingArea->window,
		      Context->ColorGC[INVERT_COLOR(Context->Background)],
		      Context->XStart, Context->YStart, X, Y);
	/* puis un rond a chaque extrmite de la ligne */
	gdk_draw_arc(Context->DrawingArea->window,
		     Context->ColorGC[INVERT_COLOR(Context->Background)], TRUE,
		     Context->XStart - 3, Context->YStart - 3,
		     6, 6, 0, 360 * 64);
	gdk_draw_arc(Context->DrawingArea->window,
		     Context->ColorGC[INVERT_COLOR(Context->Background)], TRUE,
		     X - 3, Y - 3,
		     6, 6, 0, 360 * 64);


	sprintf(Label, "d=%.2lf",
		sqrt(pow((XGraphic2Real(Context, X) - XGraphic2Real(Context, Context->XStart)), 2) +
		     pow((YGraphic2Real(Context, Y) - YGraphic2Real(Context, Context->YStart)), 2)));

	CenterString(Window,
		     Context->XStart, Context->YStart,
		     X, Y,
		     Label);
}

/**
 * recalcule la fenetre d'affichage en fonction de l'echelle et du shift */
void
UpdateViewWindow(ContextStruct *Context)
{
	Context->LeftWindow = XGraphic2Real(Context, 0);
	Context->RightWindow = XGraphic2Real(Context, Context->Width - 1);
	Context->BottomWindow = YGraphic2Real(Context, Context->Height - 1);
	Context->TopWindow = YGraphic2Real(Context, 0);
}



/**
 * procedure de mise a jour des rulers d'une fenetre de dessin a partir du contexte graphique */
void
UpdateRulers(GtkWidget *Window)
{
	GtkWidget *VerticalRuler;
	GtkWidget *HorizontalRuler;
	ContextStruct *Context;
	int Factor = 1;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	/* on met a jour les rulers */

	HorizontalRuler = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(Window), "HorizontalRuler");
	VerticalRuler = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(Window), "VerticalRuler");


	gtk_ruler_set_range(GTK_RULER(HorizontalRuler),
			    Context->LeftWindow / Factor, Context->RightWindow / Factor,
			    Context->LeftWindow / Factor, Context->RightWindow / Factor);
	gtk_ruler_set_range(GTK_RULER(VerticalRuler),
			    Context->TopWindow / Factor, Context->BottomWindow / Factor,
			    Context->TopWindow / Factor, Context->BottomWindow / Factor);
}


/**
 * procedure de mise a jour des ascenseurs horizontaux */
void
UpdateHorizontalAdjustment(GtkWidget *Window)
{
	GtkAdjustment *HorizontalAdjust;
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	HorizontalAdjust = (GtkAdjustment *)gtk_object_get_data(GTK_OBJECT(Window), "HorizontalAdjust");

	/* Horizontal: la fenetre fait 10% de plus que la taille globale */
	HorizontalAdjust->lower =
		(gfloat)MIN(Context->XMin - 0.1 * (Context->XMax - Context->XMin), Context->LeftWindow) - 1;
	HorizontalAdjust->upper =
		(gfloat)MAX(Context->XMax + 0.1 * (Context->XMax - Context->XMin), Context->RightWindow) + 1;
	/* Ca merde si on n'elargit pas d'1 pixel l'intervale ??????????????? */

	HorizontalAdjust->page_size = (gfloat)(Context->RightWindow - Context->LeftWindow);
	HorizontalAdjust->page_increment = (gfloat)(Context->RightWindow - Context->LeftWindow) / 2.0;
	HorizontalAdjust->step_increment = (gfloat)(Context->RightWindow - Context->LeftWindow) / 10.0;
	HorizontalAdjust->value = (gfloat)Context->LeftWindow;
	gtk_signal_emit_by_name(GTK_OBJECT(HorizontalAdjust), "changed");
}

/**
 * procedure de mise a jour des ascenseurs verticaux */
void
UpdateVerticalAdjustment(GtkWidget *Window)
{
	GtkAdjustment *VerticalAdjust;
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	VerticalAdjust = (GtkAdjustment *)gtk_object_get_data(GTK_OBJECT(Window), "VerticalAdjust");

	/* Vertical: la fenetre fait 10% de plus que la taille globale */
	/* ATTENTION,on passe tout en negatif pour l'inversion haut/bas */
	VerticalAdjust->lower =
		-(gfloat)MAX(Context->YMax + 0.1 * (Context->YMax - Context->YMin), Context->TopWindow) - 1;
	VerticalAdjust->upper =
		-(gfloat)MIN(Context->YMin - 0.1 * (Context->YMax - Context->YMin), Context->BottomWindow) + 1;
	/* Ca merde si on n'elargit pas d'1 pixel l'intervale ??????????????? */

	VerticalAdjust->page_size = (gfloat)(Context->TopWindow - Context->BottomWindow);
	VerticalAdjust->page_increment = (gfloat)(Context->TopWindow - Context->BottomWindow) / 2.0;
	VerticalAdjust->step_increment = (gfloat)(Context->TopWindow - Context->BottomWindow) / 10.0;
	VerticalAdjust->value = -(gfloat)Context->TopWindow;
	gtk_signal_emit_by_name(GTK_OBJECT(VerticalAdjust), "changed");
}

/**
 * procedure de mise a jour des ascenseurs hor et ver */
void
UpdateAdjustment(GtkWidget *Window)
{
	UpdateHorizontalAdjustment(Window);
	UpdateVerticalAdjustment(Window);
}



/**
 * la fonction transfere le pixmap dans la display area.
 * elle est appelee a chaque expose_event de la fenetre et apres chaque modif du dessin dans la pixmap */
void
RefreshDisplay(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	gdk_draw_pixmap(Context->DrawingArea->window,
			Context->DrawingArea->style->fg_gc[GTK_STATE_NORMAL], Context->Pixmap, 0, 0, 0, 0, Context->Width,
			Context->Height);
}


/**
 * la fonction est appelee a chaque configure_event (redimensionnement) de la fenetre principale
 *   le pixmap est detruit puis recree et la fonction DrawDisplay est appelee
 *   si la table des couleurs n'existe pas elle est cree */
void
ResetDisplay(GtkWidget *Window)
{
	ContextStruct *Context;


	/* on recupere la drawing area et ses parametres */
	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	Context->Width = Context->DrawingArea->allocation.width;
	Context->Height = Context->DrawingArea->allocation.height;

	if (Context->Pixmap)
		/* on detruit le pixmap existant */
		gtk_object_remove_data(GTK_OBJECT(Window), "Pixmap");
	/* et on en recree un nouveau */
	Context->Pixmap =
		gdk_pixmap_new(Context->DrawingArea->window, Context->Width, Context->Height,
			       (gdk_window_get_visual(Context->DrawingArea->window))->depth);
	gdk_draw_rectangle(Context->Pixmap, Context->ColorGC[Context->Background], 1, 0, 0, Context->Width, Context->Height);
	gtk_object_set_data_full(GTK_OBJECT(Window), "Pixmap", Context->Pixmap, (GtkDestroyNotify)gdk_pixmap_unref);

	if (Context->Scale == 0)
		/* l'echelle n'a pas ete calculee, il faut le faire en faisant un fit */
		Fit(Window);

	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}


/**
 * gestion du deplacement de l'ascenseur horizontal
 * la fonction est appelee en cas de mouvement des ascenseurs a la souris */
void
HorizontalMoveDisplay(GtkAdjustment *Adjust, GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	/* on decale la fenetre de visu en fonction de la position de l'ascenseur */
	Context->RightWindow += ((double)Adjust->value - Context->LeftWindow);
	Context->LeftWindow = (double)Adjust->value;
	Context->ShiftX = -Context->LeftWindow * Context->Scale;
	UpdateHorizontalAdjustment(Window); /* permet de reajuster la taille des ascenseurs en cas de recentrage du circuit */
	UpdateRulers(Window);
	DrawDisplay(Window);
}


/**
 * gestion du deplacement de l'ascenseur vertical
 * la fonction est appelee en cas de mouvement des ascenseurs a la souris */
void
VerticalMoveDisplay(GtkAdjustment *Adjust, GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	/* on decale la fenetre de visu en fonction de la position de l'ascenseur */
	/* ATTENTION,on passe tout en negatif pour l'inversion haut/bas */
	Context->BottomWindow += (-(double)Adjust->value - Context->TopWindow);
	Context->TopWindow = -(double)Adjust->value;
	Context->ShiftY = -Context->BottomWindow * Context->Scale;
	UpdateVerticalAdjustment(Window); /* permet de reajuster la taille des ascenseurs en cas de recentrage du circuit */
	UpdateRulers(Window);
	DrawDisplay(Window);
}

/**
 * applique un zoom sur la fenetre de dessin, le centre de la fenetre est invariant */
void
ZoomIn(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	if (((Context->RightWindow - Context->LeftWindow) / ZOOM_RATIO) < ZOOM_LIM) {
		GWarning("Zoom trop grand!");
		return;
	}
	if (((Context->TopWindow - Context->BottomWindow) / ZOOM_RATIO) < ZOOM_LIM) {
		GWarning("Zoom trop grand!");
		return;
	}
	Context->Scale *= ZOOM_RATIO;

	/* calcul du shift en partant du principe que le centre ne bouge pas */
	Context->ShiftX = Context->Width / 2 - (Context->Width / 2 - Context->ShiftX) * ZOOM_RATIO;
	Context->ShiftY = Context->Height / 2 - (Context->Height / 2 - Context->ShiftY) * ZOOM_RATIO;

	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}

/**
 * applique un de-zoom sur la fenetre de dessin, le centre de la fenetre est invariant */
void
ZoomOut(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	Context->Scale /= ZOOM_RATIO;

	/* calcul du shift en partant du principe que le centre ne bouge pas */
	Context->ShiftX = Context->Width / 2 - (Context->Width / 2 - Context->ShiftX) / ZOOM_RATIO;
	Context->ShiftY = Context->Height / 2 - (Context->Height / 2 - Context->ShiftY) / ZOOM_RATIO;

	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}

void GoUp(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	Context->ShiftY += 10;
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}

void GoDown(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	Context->ShiftY -= 10;
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}

void GoLeft(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	Context->ShiftX -= 10;
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}

void GoRight(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;
	Context->ShiftX += 10;
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}



void MouseZoomIn(GtkWidget *Window, int X, int Y)
{
	double Mult;
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	/* on commence par calculer le nouveau facteur d'echelle */
	Mult = MIN(ABS((double)Context->Width / ((double)Context->XStart - (double)X)),
		   ABS((double)Context->Height / ((double)Context->YStart - (double)Y)));
	if (((Context->RightWindow - Context->LeftWindow) / Mult) < ZOOM_LIM) {
		GWarning("Zoom trop grand!");
		return;
	}
	if (((Context->TopWindow - Context->BottomWindow) / Mult) < ZOOM_LIM) {
		GWarning("Zoom trop grand!");
		return;
	}
	Context->Scale = Context->Scale * Mult;

	/* le centre de la zone encadree doit devenir le centre de la fenetre */
	Context->ShiftX = Context->Width / 2 - ((Context->XStart + X) / 2 - Context->ShiftX) * Mult;
	Context->ShiftY = Context->Height / 2 - (Context->Height - (Context->YStart + Y) / 2 - Context->ShiftY) * Mult;

	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}


void MouseZoomOut(GtkWidget *Window, int X, int Y)
{
	double Mult;
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	if ((ABS(Context->XStart - X) < 10) || (ABS(Context->YStart - Y) < 10))
		return;

	/* on commence par calculer le nouveau facteur d'echelle */
	Mult = MIN(ABS((double)Context->Width / ((double)Context->XStart - X)),
		   ABS((double)Context->Height / ((double)Context->YStart - Y)));

	Context->Scale = Context->Scale / Mult;

	/* le centre de la fenetre doit devenir le centre de la zonee encadree */
	Context->ShiftX = (Context->XStart + X) / 2 - (Context->Width / 2 - Context->ShiftX) / Mult;
	Context->ShiftY = Context->Height - (Context->YStart + Y) / 2 - (Context->Height / 2 - Context->ShiftY) / Mult;

	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}


/****************************************************************************************************************/
void
Fit(GtkWidget *Window)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	/* la zone visualisee est 10% plus grande que la zone effective */
	Context->LeftWindow = Context->XMin - 0.1 * (Context->XMax - Context->XMin);
	Context->RightWindow = Context->XMax + 0.1 * (Context->XMax - Context->XMin);
	Context->BottomWindow = Context->YMin - 0.1 * (Context->YMax - Context->YMin);
	Context->TopWindow = Context->YMax + 0.1 * (Context->YMax - Context->YMin);
	if ((Context->XMax != Context->XMin) && (Context->YMax != Context->YMin))
		Context->Scale = MIN((double)Context->Width / (Context->RightWindow - Context->LeftWindow),
				     (double)Context->Height / (Context->TopWindow - Context->BottomWindow));
	else
		Context->Scale = 1;
	Context->ShiftX = (int)rint((Context->Width - (Context->RightWindow + Context->LeftWindow) * Context->Scale) / 2);
	Context->ShiftY = (int)rint((Context->Height - (Context->TopWindow + Context->BottomWindow) * Context->Scale) / 2);


	/* on remet a jour la fenetre de visu */
	UpdateViewWindow(Context);
	UpdateRulers(Window);
	UpdateAdjustment(Window);
	DrawDisplay(Window);
}



/**
 * initialisation d'une operation a la souris */
void
PressMouse(GtkWidget *Window, GdkEventButton *event)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	Context->XLast = Context->XStart = event->x;
	Context->YLast = Context->YStart = event->y;

	switch (event->button) {
	case 1:
		Context->Operation = MOUSE_ZOOM;
		break;

	case 2:
		Context->Operation = MOUSE_MEASURE;
		break;

	case 3:
		break;
	}
}



/**
 * gestion des operations realisees lors du deplacement de la souris */
void
MoveMouse(GtkWidget *Window, GdkEventButton *event)
{
	ContextStruct *Context;
	int X, Y;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;


	switch (Context->Operation) {
	case MOUSE_ZOOM:
		/* on efface le dernier rectangle de selection dessine */
		my_draw_rectangle(Context->DrawingArea->window, Context->ColorGC[XOR], 0,
				  Context->XStart, Context->YStart, Context->XLast, Context->YLast);

		/*  on redessine le nouveau rectangle */
		my_draw_rectangle(Context->DrawingArea->window, Context->ColorGC[XOR], 0,
				  Context->XStart, Context->YStart, event->x, event->y);
		break;

	case MOUSE_MEASURE:
		RefreshDisplay(Window);
		if (event->state & GDK_SHIFT_MASK) {                                                    /* si on shift, on deplace en horizontal ou vertical uniquement */
			if ((ABS(event->x - Context->XStart)) > (ABS((event->y - Context->YStart)))) {  /* on ne se deplace qu'en X */
				X = event->x;
				Y = Context->YStart;
			} else { /* on ne se deplace qu'en Y */
				X = Context->XStart;
				Y = event->y;
			}
		} else {
			X = event->x;
			Y = event->y;
		}
		DrawMeasure(Window, X, Y);
		break;
	}
	Context->XLast = event->x;
	Context->YLast = event->y;
}

/**
 * fin d'une operation a la souris */
void
ReleaseMouse(GtkWidget *Window, GdkEventButton *event)
{
	ContextStruct *Context;

	if (!(Context = (ContextStruct *)gtk_object_get_user_data(GTK_OBJECT(Window))))
		return;

	switch (Context->Operation) {
	case MOUSE_ZOOM:
		/* on efface le dernier rectangle de selection dessine */
		my_draw_rectangle(Context->DrawingArea->window, Context->ColorGC[XOR], 0,
				  Context->XStart, Context->YStart, Context->XLast, Context->YLast);
		if (event->x > Context->XStart)
			/* on a balaye de gauche a droite, c'est un zoom: la zone encadree va prendre toute la fenetre visible */
			MouseZoomIn(Window, event->x, event->y);
		else
			/* on a balaye de droite a gauche, c'est un dezoom: toute la fenetre visible va venir dans la zone encadree*/
			MouseZoomOut(Window, event->x, event->y);
		break;
	}
	Context->Operation = 0;
}



/**
 * creation et allocation d'une palette de couleurs pour la fenetre passee en parametre */
GdkGC **
ColorInit(GtkWidget *Window)
{
	GdkColormap *Palette = 0;
	GdkColor *Color;
	GdkGC **ColorGC;
	int i;
	static unsigned char STIPPLE[] = { 0x1, 0x2 };
	GdkPixmap *Stipple;

	/* creation de la palette de couleurs */
	ColorGC = (GdkGC **)malloc(TOTAL_COLORS * sizeof(GdkGC *));
	Palette = gdk_window_get_colormap(Window->window);
	/* ca devrait marcher avec la palette systeme, mais non... ??? */
	//Palette = gdk_colormap_get_system ();

	Stipple = gdk_bitmap_create_from_data(Window->window, (char *)STIPPLE, 2, 2);

	for (i = 0; i < NB_COLORS; i++) {
		Color = (GdkColor *)malloc(sizeof(GdkColor));
		/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
		 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
		Color->red = 0x1000 + (0xe000 / (RGB_STEP - 1)) * (i % RGB_STEP);
		Color->green = 0x1000 + (0xe000 / (RGB_STEP - 1)) * ((i / RGB_STEP) % RGB_STEP);
		Color->blue = 0x1000 + (0xe000 / (RGB_STEP - 1)) * ((i / (RGB_STEP * RGB_STEP)) % RGB_STEP);
		gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
		ColorGC[i] = gdk_gc_new(Window->window);
		gdk_gc_set_foreground(ColorGC[i], Color);
		/* plus besoin de la couleur */
		free(Color);
		Color = (GdkColor *)malloc(sizeof(GdkColor));
		Color->red = 0x5555;
		Color->green = 0x5555;
		Color->blue = 0x5555;
		gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
		gdk_gc_set_background(ColorGC[i], Color);
		gdk_gc_set_fill(ColorGC[i], GDK_STIPPLED);
		free(Color);
	}

	/* le noir */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0;
	Color->green = 0;
	Color->blue = 0;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[BLACK] = gdk_gc_new(Window->window);
	gdk_gc_set_foreground(ColorGC[BLACK], Color);
	/* plus besoin de la couleur */
	free(Color);

	/*le blanc */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0xffff;
	Color->green = 0xffff;
	Color->blue = 0xffff;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[WHITE] = gdk_gc_new(Window->window);
	gdk_gc_set_foreground(ColorGC[WHITE], Color);
	/* plus besoin de la couleur */
	free(Color);

	/* le rouge */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0xffff;
	Color->green = 0;
	Color->blue = 0;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[RED] = gdk_gc_new(Window->window);
	gdk_gc_set_foreground(ColorGC[RED], Color);
	gdk_gc_set_fill(ColorGC[RED], GDK_STIPPLED);
	/* plus besoin de la couleur */
	free(Color);

	/* le vert */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0;
	Color->green = 0xffff;
	Color->blue = 0;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[GREEN] = gdk_gc_new(Window->window);
	gdk_gc_set_foreground(ColorGC[GREEN], Color);
	gdk_gc_set_fill(ColorGC[GREEN], GDK_STIPPLED);
	/* plus besoin de la couleur */
	free(Color);

	/* le bleu */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0;
	Color->green = 0;
	Color->blue = 0xffff;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[BLUE] = gdk_gc_new(Window->window);
	gdk_gc_set_fill(ColorGC[BLUE], GDK_STIPPLED);
	gdk_gc_set_foreground(ColorGC[BLUE], Color);
	/* plus besoin de la couleur */
	free(Color);

	/* une couleur en mode xor */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0;
	Color->green = 0x7fff;
	Color->blue = 0xffff;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[XOR] = gdk_gc_new(Window->window);
	gdk_gc_set_function(ColorGC[XOR], GDK_XOR);
	gdk_gc_set_foreground(ColorGC[XOR], Color);
	/* plus besoin de la couleur */
	free(Color);


	/* une couleur verte en mode pointille */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0;
	Color->green = 0xffff;
	Color->blue = 0;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[GDASHED] = gdk_gc_new(Window->window);
	gdk_gc_set_line_attributes(ColorGC[GDASHED], 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
	gdk_gc_set_foreground(ColorGC[GDASHED], Color);
	gdk_gc_set_fill(ColorGC[GDASHED], GDK_STIPPLED);
	/* plus besoin de la couleur */
	free(Color);

	/* une couleur rouge en mode pointille */
	Color = (GdkColor *)malloc(sizeof(GdkColor));
	/* on prend RGB_STEP valeurs equitablement reparties entre 0x1000 et 0xf000 pour chaque composante
	 * et on construit les couleurs correspondant aux RGB_STEP^3 combinaisons */
	Color->red = 0xffff;
	Color->green = 0;
	Color->blue = 0;
	gdk_colormap_alloc_color(Palette, Color, FALSE, TRUE);
	ColorGC[RDASHED] = gdk_gc_new(Window->window);
	gdk_gc_set_line_attributes(ColorGC[RDASHED], 1, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
	gdk_gc_set_foreground(ColorGC[RDASHED], Color);
	gdk_gc_set_fill(ColorGC[RDASHED], GDK_STIPPLED);
	/* plus besoin de la couleur */
	free(Color);

	gtk_object_set_data_full(GTK_OBJECT(Window), "ColorGC", ColorGC, (GtkDestroyNotify)free);

	return ColorGC;
}

void
on_aboutbutton_clicked(GtkWidget *Window)
{
	GtkWidget *MainWindow;

	gtk_widget_destroy(Window);
	if (CFDWindow == 0) {
		MainWindow = CreateMainWindow();
		gtk_widget_show(MainWindow);
		CFDWindow = MainWindow;
	}
}


GtkWidget *
CreateAboutWindow()
{
	GtkWidget *window1;
	GtkWidget *vbox1;
	GtkWidget *pixmap1;
	GtkWidget *button1;

	window1 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(window1), "window1", window1);
	gtk_window_set_title(GTK_WINDOW(window1), ("A propos de CFD Watcher"));

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox1);
	gtk_object_set_data_full(GTK_OBJECT(window1), "vbox1", vbox1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(window1), vbox1);

	pixmap1 = mycreate_pixmap(window1, About_xpm);
	gtk_widget_ref(pixmap1);
	gtk_object_set_data_full(GTK_OBJECT(window1), "pixmap1", pixmap1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(pixmap1);
	gtk_box_pack_start(GTK_BOX(vbox1), pixmap1, TRUE, TRUE, 0);

	button1 = gtk_button_new_with_label(("Ok"));
	gtk_widget_ref(button1);
	gtk_object_set_data_full(GTK_OBJECT(window1), "button1", button1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button1);
	gtk_box_pack_start(GTK_BOX(vbox1), button1, FALSE, FALSE, 0);

	gtk_signal_connect_object(GTK_OBJECT(button1), "clicked",
				  GTK_SIGNAL_FUNC(on_aboutbutton_clicked),
				  GTK_OBJECT(window1));
	gtk_window_set_position((GtkWindow *)window1, GTK_WIN_POS_CENTER);
	gtk_widget_show_all(window1);
	return window1;
}





GtkWidget *
CreateMainWindow(void)
{
	GtkWidget *MainWindow;
	GtkWidget *DisplayTable;
	GtkWidget *DrawingArea;
	GtkWidget *VerticalRuler;
	GtkWidget *HorizontalRuler;
	GtkObject *VerticalAdjust;
	GtkObject *HorizontalAdjust;
	GtkWidget *VerticalScroll;
	GtkWidget *HorizontalScroll;
	GtkWidget *MainHBox;
	ContextStruct *Context;
	GtkWidget *MainVBox;
	GtkAccelGroup *accel_group;
	GtkItemFactory *item_factory;
	GtkWidget *MenuBar;
	GtkWidget *Logo;

	/********************************** les boutons *************************/
	GtkWidget *toolbar1;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	GtkWidget *button4;
	GtkWidget *button5;
	GtkWidget *button6;
	GtkWidget *button7;
	GtkWidget *button8;

	accel_group = gtk_accel_group_new();
	MainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(MainWindow, "MainWindow");
	gtk_object_set_data(GTK_OBJECT(MainWindow), "MainWindow", MainWindow);
	gtk_window_set_title(GTK_WINDOW(MainWindow), "CFD Watcher");
	gtk_window_set_default_size(GTK_WINDOW(MainWindow), 640, 480);
	gtk_window_set_policy(GTK_WINDOW(MainWindow), FALSE, TRUE, TRUE);

	MainVBox = gtk_vbox_new(FALSE, 0);
	gtk_widget_set_name(MainVBox, "MainVBox");
	gtk_widget_ref(MainVBox);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "MainVBox", MainVBox, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(MainVBox);
	gtk_container_add(GTK_CONTAINER(MainWindow), MainVBox);

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<Main>", accel_group);
	gtk_object_set_data(GTK_OBJECT(MainWindow), "ItemFactory", item_factory);

	/* ATTENTION : visiblement le dernier parametre de la fonction gtk_item_factory_new
	 * est passe en premier parametre de la fonction de callback...
	 * c'est bien ce qu'on veut, MAIS C'EST LE CONTRAIRE DE LA DOC !!!! */

	gtk_item_factory_create_items(item_factory, MENU_NUMBER, Menu, MainWindow);
	//  gtk_item_factory_parse_rc ("config.cfg");
	gtk_window_add_accel_group(GTK_WINDOW(MainWindow), accel_group);
	MenuBar = gtk_item_factory_get_widget(item_factory, "<Main>");
	gtk_widget_set_name(MenuBar, "MenuBar");
	gtk_widget_ref(MenuBar);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "MenuBar", MenuBar, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(MenuBar);
	gtk_box_pack_start(GTK_BOX(MainVBox), MenuBar, FALSE, FALSE, 0);

	MainHBox = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_name(MainHBox, "MainHBox");
	gtk_widget_ref(MainHBox);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "MainHBox", MainHBox, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(MainHBox);
	gtk_box_pack_start(GTK_BOX(MainVBox), MainHBox, TRUE, TRUE, 0);

	DisplayTable = gtk_table_new(3, 3, FALSE);
	gtk_widget_set_name(DisplayTable, "DisplayTable");
	gtk_widget_ref(DisplayTable);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "DisplayTable", DisplayTable, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DisplayTable);
	gtk_box_pack_start(GTK_BOX(MainHBox), DisplayTable, TRUE, TRUE, 0);

	DrawingArea = gtk_drawing_area_new();
	gtk_widget_set_name(DrawingArea, "DrawingArea");
	gtk_drawing_area_size((GtkDrawingArea *)DrawingArea, VIEW_WIDTH, VIEW_HEIGHT);
	gtk_widget_ref(DrawingArea);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "DrawingArea", DrawingArea, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DrawingArea);
	gtk_widget_add_events(DrawingArea,
			      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK |
			      GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
	GTK_WIDGET_SET_FLAGS(DrawingArea, GTK_CAN_FOCUS);


	gtk_table_attach(GTK_TABLE(DisplayTable), DrawingArea, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	VerticalRuler = gtk_vruler_new();
	gtk_widget_set_name(VerticalRuler, "VerticalRuler");
	gtk_widget_ref(VerticalRuler);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "VerticalRuler", VerticalRuler, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(VerticalRuler);
	gtk_table_attach(GTK_TABLE(DisplayTable), VerticalRuler, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	HorizontalRuler = gtk_hruler_new();
	gtk_widget_set_name(HorizontalRuler, "HorizontalRuler");
	gtk_widget_ref(HorizontalRuler);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "HorizontalRuler", HorizontalRuler, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(HorizontalRuler);
	gtk_table_attach(GTK_TABLE(DisplayTable), HorizontalRuler, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, 0);

	HorizontalAdjust = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "HorizontalAdjust", HorizontalAdjust, (GtkDestroyNotify)gtk_widget_unref);
	VerticalAdjust = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "VerticalAdjust", VerticalAdjust, (GtkDestroyNotify)gtk_widget_unref);
	HorizontalScroll = gtk_hscrollbar_new((GtkAdjustment *)HorizontalAdjust);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "HorizontalScroll", HorizontalScroll, (GtkDestroyNotify)gtk_widget_unref);
	VerticalScroll = gtk_vscrollbar_new((GtkAdjustment *)VerticalAdjust);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "VerticalScroll", VerticalScroll, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(HorizontalScroll);
	gtk_widget_show(VerticalScroll);
	gtk_table_attach(GTK_TABLE(DisplayTable), VerticalScroll, 2, 3, 1, 2,
			 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, 0);

	gtk_table_attach(GTK_TABLE(DisplayTable), HorizontalScroll, 1, 2, 2, 3,
			 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, 0);


	/***************************************** la boite de boutons a ajouter ********************************/

	toolbar1 = gtk_toolbar_new();
	gtk_widget_ref(toolbar1);
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar1), GTK_ORIENTATION_VERTICAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar1), GTK_TOOLBAR_BOTH);

	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "toolbar1", toolbar1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(toolbar1);
	gtk_box_pack_start(GTK_BOX(MainHBox), toolbar1, FALSE, FALSE, 0);

	Logo = mycreate_pixmap(toolbar1, logo_xpm);
	gtk_widget_ref(Logo);
	gtk_object_set_data_full(GTK_OBJECT(toolbar1), "Logo", Logo,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Logo);
	gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar1), Logo, NULL, NULL);


	button1 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Recadrer"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button1);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button1", button1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button1);
	gtk_widget_set_usize(button1, 80, 20);

	button2 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Zoomer"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button2);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button2", button2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button2);

	button3 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Dezoomer"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button3);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button3", button3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button3);


	button5 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Nord"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button5);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button5", button5,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button5);

	button6 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Sud"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button6);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button6", button6,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button6);

	button7 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Ouest"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button7);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button7", button7,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button7);

	button8 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Est"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button8);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button8", button8,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button8);




	button4 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
					     GTK_TOOLBAR_CHILD_BUTTON,
					     NULL,
					     ("Quitter"),
					     NULL, NULL,
					     NULL, NULL, NULL);
	gtk_widget_ref(button4);
	gtk_object_set_data_full(GTK_OBJECT(MainWindow), "button4", button4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(button4);




	gtk_signal_connect_object(GTK_OBJECT(button1), "clicked",
				  GTK_SIGNAL_FUNC(Fit),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button2), "clicked",
				  GTK_SIGNAL_FUNC(ZoomIn),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button3), "clicked",
				  GTK_SIGNAL_FUNC(ZoomOut),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button5), "clicked",
				  GTK_SIGNAL_FUNC(GoUp),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button6), "clicked",
				  GTK_SIGNAL_FUNC(GoDown),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button7), "clicked",
				  GTK_SIGNAL_FUNC(GoLeft),
				  GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(button8), "clicked",
				  GTK_SIGNAL_FUNC(GoRight),
				  GTK_OBJECT(MainWindow));

	gtk_signal_connect_object(GTK_OBJECT(button4), "clicked",
				  GTK_SIGNAL_FUNC(gtk_main_quit),
				  GTK_OBJECT(MainWindow));


	/***************************** fin de la boite de boutons ***********************************************/


	gtk_signal_connect(GTK_OBJECT(HorizontalAdjust), "value_changed", (GtkSignalFunc)HorizontalMoveDisplay, MainWindow);

	gtk_signal_connect(GTK_OBJECT(VerticalAdjust), "value_changed", (GtkSignalFunc)VerticalMoveDisplay, MainWindow);

	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "motion_notify_event",
				  (GtkSignalFunc)
				  GTK_WIDGET_CLASS(HorizontalRuler)->motion_notify_event, GTK_OBJECT(HorizontalRuler));
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "motion_notify_event",
				  (GtkSignalFunc)
				  GTK_WIDGET_CLASS(VerticalRuler)->motion_notify_event, GTK_OBJECT(VerticalRuler));

	/* il faut realizer la fenetre pour allouer les couleurs */
	gtk_widget_realize(MainWindow);
	ColorInit(MainWindow);

	/* on ajoute un context graphique */
	Context = (ContextStruct *)malloc(sizeof(ContextStruct));
	Context->Scale = (double)0;
	//Context->Background = BLACK;
	Context->Background = WHITE;
	Context->LeftWindow = 0;
	Context->BottomWindow = 0;
	Context->TopWindow = Context->RightWindow = 0;  /* pour l'instant on ne sait pas... */
	Context->ColorGC = (GdkGC **)gtk_object_get_data(GTK_OBJECT(MainWindow), "ColorGC");
	Context->DrawingArea = DrawingArea;
	Context->Pixmap = 0;


	/* dimensions initiales.... a modifier */
	Context->XMin = -2; Context->YMin = -2;
	Context->XMax = 2; Context->YMax = 2;




	/* pour retrouver rapidement le contexte ... */
	gtk_object_set_user_data(GTK_OBJECT(MainWindow), (gpointer)Context);

	/* cablage des callbacks de fermeture de fenetre */
	gtk_signal_connect_object(GTK_OBJECT(MainWindow), "delete_event",
				  GTK_SIGNAL_FUNC(gtk_main_quit), GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(MainWindow), "destroy_event",
				  GTK_SIGNAL_FUNC(gtk_main_quit), GTK_OBJECT(MainWindow));

	/* cablage des callbacks pour la zone de dessin */
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "expose_event", GTK_SIGNAL_FUNC(RefreshDisplay), GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "configure_event", GTK_SIGNAL_FUNC(ResetDisplay), GTK_OBJECT(MainWindow));

	/* commandes souris */
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "button_press_event", (GtkSignalFunc)PressMouse, GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "motion_notify_event", (GtkSignalFunc)MoveMouse, GTK_OBJECT(MainWindow));
	gtk_signal_connect_object(GTK_OBJECT(DrawingArea), "button_release_event",
				  (GtkSignalFunc)ReleaseMouse, GTK_OBJECT(MainWindow));

	return MainWindow;
}


gint
OptionMenuGetIndex(GtkWidget *omenu)
{
	GtkWidget *menu, *menu_item;
	GList *children, *child;

	if (omenu) {
		menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
		menu_item = GTK_OPTION_MENU(omenu)->menu_item;
		if (menu) {
			children = GTK_MENU_SHELL(menu)->children;
			child = g_list_find(children, menu_item);
			if (child)
				return g_list_position(children, child);
		}
	}

	return -1;
}




GtkWidget *
create_DistanceLibreWindow(void)
{
	GtkWidget *DistanceLibreWindow;
	GtkWidget *vbox3;
	GtkWidget *DlLabel;
	GtkWidget *vbox4;
	GtkWidget *hbox2;
	GtkWidget *Wpt1Label;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *hbox3;
	GtkWidget *BaliseProcheDepLabel;
	GtkWidget *BaliseDep;
	GtkWidget *DistDepLab;
	GtkWidget *entry31;
	GtkWidget *hbox5;
	GtkWidget *Wpt2Label;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *hbox4;
	GtkWidget *BaliseProcheArvLabel;
	GtkWidget *BaliseArv;
	GtkWidget *label41;
	GtkWidget *entry32;
	GtkWidget *hbox6;
	GtkWidget *DistLabel;
	GtkWidget *Dist;
	GtkWidget *KmLabel;
	GtkWidget *hbox7;
	GtkWidget *CoeffLabel;
	GtkWidget *ValeurCoefLabel;
	GtkWidget *hbox8;
	GtkWidget *PointLabel;
	GtkWidget *Points;

	DistanceLibreWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(DistanceLibreWindow), "DistanceLibreWindow", DistanceLibreWindow);
	gtk_window_set_title(GTK_WINDOW(DistanceLibreWindow), "Distance Libre");

	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox3);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "vbox3", vbox3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(DistanceLibreWindow), vbox3);

	DlLabel = gtk_label_new("Distance Libre");
	gtk_widget_ref(DlLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "DlLabel", DlLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DlLabel);
	gtk_box_pack_start(GTK_BOX(vbox3), DlLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DlLabel), 0, 6);

	vbox4 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox4);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "vbox4", vbox4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox4);
	gtk_box_pack_start(GTK_BOX(vbox3), vbox4, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox2);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox2", hbox2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox2);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox2, TRUE, TRUE, 0);

	Wpt1Label = gtk_label_new("Depart");
	gtk_widget_ref(Wpt1Label);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "Wpt1Label", Wpt1Label,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Wpt1Label);
	gtk_box_pack_start(GTK_BOX(hbox2), Wpt1Label, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(Wpt1Label), 41, 0);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox2), CoordXDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXDep), FALSE);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox2), CoordYDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYDep), FALSE);

	hbox3 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox3);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox3", hbox3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox3);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox3, TRUE, TRUE, 0);

	BaliseProcheDepLabel = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(BaliseProcheDepLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "BaliseProcheDepLabel", BaliseProcheDepLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseProcheDepLabel);
	gtk_box_pack_start(GTK_BOX(hbox3), BaliseProcheDepLabel, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(BaliseProcheDepLabel), 0, 0.5);

	BaliseDep = gtk_entry_new();
	gtk_widget_ref(BaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "BaliseDep", BaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox3), BaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseDep), FALSE);

	DistDepLab = gtk_label_new("Distance");
	gtk_widget_ref(DistDepLab);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "DistDepLab", DistDepLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistDepLab);
	gtk_box_pack_start(GTK_BOX(hbox3), DistDepLab, FALSE, FALSE, 0);

	entry31 = gtk_entry_new();
	gtk_widget_ref(entry31);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "entry31", entry31,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry31);
	gtk_box_pack_start(GTK_BOX(hbox3), entry31, TRUE, TRUE, 0);

	hbox5 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox5);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox5", hbox5,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox5);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox5, TRUE, TRUE, 0);

	Wpt2Label = gtk_label_new("Arrivee");
	gtk_widget_ref(Wpt2Label);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "Wpt2Label", Wpt2Label,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Wpt2Label);
	gtk_box_pack_start(GTK_BOX(hbox5), Wpt2Label, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(Wpt2Label), 38, 0);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox5), CoordXArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXArv), FALSE);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox5), CoordYArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYArv), FALSE);

	hbox4 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox4);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox4", hbox4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox4);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox4, TRUE, TRUE, 0);

	BaliseProcheArvLabel = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(BaliseProcheArvLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "BaliseProcheArvLabel", BaliseProcheArvLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseProcheArvLabel);
	gtk_box_pack_start(GTK_BOX(hbox4), BaliseProcheArvLabel, FALSE, FALSE, 0);

	BaliseArv = gtk_entry_new();
	gtk_widget_ref(BaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "BaliseArv", BaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox4), BaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseArv), FALSE);

	label41 = gtk_label_new("label41");
	gtk_widget_ref(label41);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "label41", label41,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label41);
	gtk_box_pack_start(GTK_BOX(hbox4), label41, FALSE, FALSE, 0);

	entry32 = gtk_entry_new();
	gtk_widget_ref(entry32);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "entry32", entry32,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry32);
	gtk_box_pack_start(GTK_BOX(hbox4), entry32, TRUE, TRUE, 0);

	hbox6 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox6);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox6", hbox6,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox6);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox6, TRUE, TRUE, 0);

	DistLabel = gtk_label_new("Distance parcourue");
	gtk_widget_ref(DistLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "DistLabel", DistLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistLabel);
	gtk_box_pack_start(GTK_BOX(hbox6), DistLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DistLabel), 4, 0);

	Dist = gtk_entry_new();
	gtk_widget_ref(Dist);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "Dist", Dist,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Dist);
	gtk_box_pack_start(GTK_BOX(hbox6), Dist, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Dist), FALSE);

	KmLabel = gtk_label_new("Km");
	gtk_widget_ref(KmLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "KmLabel", KmLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(KmLabel);
	gtk_box_pack_start(GTK_BOX(hbox6), KmLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(KmLabel), 4, 0);

	hbox7 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox7);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox7", hbox7,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox7);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox7, TRUE, TRUE, 0);

	CoeffLabel = gtk_label_new("Coefficient");
	gtk_widget_ref(CoeffLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "CoeffLabel", CoeffLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoeffLabel);
	gtk_box_pack_start(GTK_BOX(hbox7), CoeffLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(CoeffLabel), 30, 0);

	ValeurCoefLabel = gtk_label_new("1,0");
	gtk_widget_ref(ValeurCoefLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "ValeurCoefLabel", ValeurCoefLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(ValeurCoefLabel);
	gtk_box_pack_start(GTK_BOX(hbox7), ValeurCoefLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(ValeurCoefLabel), 155, 0);

	hbox8 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox8);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "hbox8", hbox8,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox8);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox8, TRUE, TRUE, 0);

	PointLabel = gtk_label_new("Points realises");
	gtk_widget_ref(PointLabel);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "PointLabel", PointLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(PointLabel);
	gtk_box_pack_start(GTK_BOX(hbox8), PointLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(PointLabel), 19, 0);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(DistanceLibreWindow), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_box_pack_start(GTK_BOX(hbox8), Points, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Points), FALSE);

	return DistanceLibreWindow;
}

GtkWidget * create_LoadWindow(void)
{
	GtkWidget *LoadWindow;
	GtkWidget *vbox5;
	GtkWidget *LoadLabel;

	LoadWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(LoadWindow), "LoadWindow", LoadWindow);
	gtk_window_set_title(GTK_WINDOW(LoadWindow), "Loading");

	vbox5 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox5);
	gtk_object_set_data_full(GTK_OBJECT(LoadWindow), "vbox5", vbox5,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox5);
	gtk_container_add(GTK_CONTAINER(LoadWindow), vbox5);

	LoadLabel = gtk_label_new("Operation en cours\nVeuillez patienter...");
	gtk_widget_ref(LoadLabel);
	gtk_object_set_data_full(GTK_OBJECT(LoadWindow), "LoadLabel", LoadLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(LoadLabel);
	gtk_box_pack_start(GTK_BOX(vbox5), LoadLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(LoadLabel), 0, 36);

	gtk_window_set_modal((GtkWindow *)LoadWindow, TRUE);
	gtk_window_set_position((GtkWindow *)LoadWindow, GTK_WIN_POS_CENTER);
	/* gtk_window_set_transient_for((GtkWindow *)LoadWindow, MainWindow); */

	return LoadWindow;
}

GtkWidget *
create_DLCWindow(void)
{
	GtkWidget *DLCWindow;
	GtkWidget *vbox6;
	GtkWidget *DlcLabel;
	GtkWidget *vbox7;
	GtkWidget *hbox9;
	GtkWidget *DepLabel;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXDep;
	GtkWidget *hbox12;
	GtkWidget *BaliseDepLab;
	GtkWidget *BaliseDep;
	GtkWidget *DistDep;
	GtkWidget *DistBaliseDep;
	GtkWidget *hbox10;
	GtkWidget *ContLabel;
	GtkWidget *CoordYCont;
	GtkWidget *CoordXCont;
	GtkWidget *hbox13;
	GtkWidget *BaliseContLab;
	GtkWidget *BaliseCont;
	GtkWidget *DistCont;
	GtkWidget *DistBaliseCont;
	GtkWidget *hbox11;
	GtkWidget *ArrLabel;
	GtkWidget *CoordYArv;
	GtkWidget *CoordXArv;
	GtkWidget *hbox14;
	GtkWidget *BaliseArvLab;
	GtkWidget *BaliseArv;
	GtkWidget *DistArv;
	GtkWidget *DistBaliseArv;
	GtkWidget *hbox15;
	GtkWidget *DistB1_B2;
	GtkWidget *Dist1Entry;
	GtkWidget *Km;
	GtkWidget *hbox20;
	GtkWidget *DistB2_B3;
	GtkWidget *Dist2Entry;
	GtkWidget *Km2;
	GtkWidget *hbox17;
	GtkWidget *DistTotalLab;
	GtkWidget *DistTotal;
	GtkWidget *Km3;
	GtkWidget *hbox18;
	GtkWidget *CoeffLab;
	GtkWidget *ValeurCoeffLab;
	GtkWidget *hbox19;
	GtkWidget *PointLabel;
	GtkWidget *Points;

	DLCWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(DLCWindow), "DLCWindow", DLCWindow);
	gtk_window_set_title(GTK_WINDOW(DLCWindow), "Distance Libre avec point de contournement");

	vbox6 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox6);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "vbox6", vbox6,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox6);
	gtk_container_add(GTK_CONTAINER(DLCWindow), vbox6);

	DlcLabel = gtk_label_new("Distance Libre avec point de contournement");
	gtk_widget_ref(DlcLabel);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DlcLabel", DlcLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DlcLabel);
	gtk_box_pack_start(GTK_BOX(vbox6), DlcLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DlcLabel), 0, 7);

	vbox7 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox7);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "vbox7", vbox7,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox7);
	gtk_box_pack_start(GTK_BOX(vbox6), vbox7, TRUE, TRUE, 0);

	hbox9 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox9);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox9", hbox9,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox9);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox9, TRUE, TRUE, 0);

	DepLabel = gtk_label_new("Depart");
	gtk_widget_ref(DepLabel);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DepLabel", DepLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DepLabel);
	gtk_box_pack_start(GTK_BOX(hbox9), DepLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DepLabel), 50, 0);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox9), CoordYDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYDep), FALSE);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox9), CoordXDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXDep), FALSE);

	hbox12 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox12);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox12", hbox12,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox12);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox12, TRUE, TRUE, 0);

	BaliseDepLab = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(BaliseDepLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseDepLab", BaliseDepLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDepLab);
	gtk_box_pack_start(GTK_BOX(hbox12), BaliseDepLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(BaliseDepLab), 9, 0);

	BaliseDep = gtk_entry_new();
	gtk_widget_ref(BaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseDep", BaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox12), BaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseDep), FALSE);

	DistDep = gtk_label_new("Distance");
	gtk_widget_ref(DistDep);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistDep", DistDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistDep);
	gtk_box_pack_start(GTK_BOX(hbox12), DistDep, FALSE, FALSE, 0);

	DistBaliseDep = gtk_entry_new();
	gtk_widget_ref(DistBaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistBaliseDep", DistBaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox12), DistBaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseDep), FALSE);

	hbox10 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox10);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox10", hbox10,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox10);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox10, TRUE, TRUE, 0);

	ContLabel = gtk_label_new("Point de contournement");
	gtk_widget_ref(ContLabel);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "ContLabel", ContLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(ContLabel);
	gtk_box_pack_start(GTK_BOX(hbox10), ContLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(ContLabel), 2, 0);

	CoordYCont = gtk_entry_new();
	gtk_widget_ref(CoordYCont);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordYCont", CoordYCont,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYCont);
	gtk_box_pack_start(GTK_BOX(hbox10), CoordYCont, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYCont), FALSE);

	CoordXCont = gtk_entry_new();
	gtk_widget_ref(CoordXCont);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordXCont", CoordXCont,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXCont);
	gtk_box_pack_start(GTK_BOX(hbox10), CoordXCont, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXCont), FALSE);

	hbox13 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox13);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox13", hbox13,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox13);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox13, TRUE, TRUE, 0);

	BaliseContLab = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(BaliseContLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseContLab", BaliseContLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseContLab);
	gtk_box_pack_start(GTK_BOX(hbox13), BaliseContLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(BaliseContLab), 9, 0);

	BaliseCont = gtk_entry_new();
	gtk_widget_ref(BaliseCont);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseCont", BaliseCont,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseCont);
	gtk_box_pack_start(GTK_BOX(hbox13), BaliseCont, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseCont), FALSE);

	DistCont = gtk_label_new("Distance");
	gtk_widget_ref(DistCont);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistCont", DistCont,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistCont);
	gtk_box_pack_start(GTK_BOX(hbox13), DistCont, FALSE, FALSE, 0);

	DistBaliseCont = gtk_entry_new();
	gtk_widget_ref(DistBaliseCont);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistBaliseCont", DistBaliseCont,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseCont);
	gtk_box_pack_start(GTK_BOX(hbox13), DistBaliseCont, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseCont), FALSE);

	hbox11 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox11);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox11", hbox11,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox11);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox11, TRUE, TRUE, 0);

	ArrLabel = gtk_label_new("Arrivee");
	gtk_widget_ref(ArrLabel);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "ArrLabel", ArrLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(ArrLabel);
	gtk_box_pack_start(GTK_BOX(hbox11), ArrLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(ArrLabel), 48, 0);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox11), CoordYArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYArv), FALSE);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox11), CoordXArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXArv), FALSE);

	hbox14 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox14);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox14", hbox14,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox14);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox14, TRUE, TRUE, 0);

	BaliseArvLab = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(BaliseArvLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseArvLab", BaliseArvLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArvLab);
	gtk_box_pack_start(GTK_BOX(hbox14), BaliseArvLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(BaliseArvLab), 9, 0);

	BaliseArv = gtk_entry_new();
	gtk_widget_ref(BaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "BaliseArv", BaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox14), BaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseArv), FALSE);

	DistArv = gtk_label_new("Distance");
	gtk_widget_ref(DistArv);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistArv", DistArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistArv);
	gtk_box_pack_start(GTK_BOX(hbox14), DistArv, FALSE, FALSE, 0);

	DistBaliseArv = gtk_entry_new();
	gtk_widget_ref(DistBaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistBaliseArv", DistBaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox14), DistBaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseArv), FALSE);

	hbox15 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox15);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox15", hbox15,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox15);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox15, TRUE, TRUE, 0);

	DistB1_B2 = gtk_label_new("Distance BD-B1");
	gtk_widget_ref(DistB1_B2);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistB1_B2", DistB1_B2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB1_B2);
	gtk_box_pack_start(GTK_BOX(hbox15), DistB1_B2, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DistB1_B2), 24, 0);

	Dist1Entry = gtk_entry_new();
	gtk_widget_ref(Dist1Entry);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Dist1Entry", Dist1Entry,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Dist1Entry);
	gtk_box_pack_start(GTK_BOX(hbox15), Dist1Entry, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Dist1Entry), FALSE);

	Km = gtk_label_new("km");
	gtk_widget_ref(Km);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Km", Km,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Km);
	gtk_box_pack_start(GTK_BOX(hbox15), Km, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(Km), 10, 0);

	hbox20 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox20);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox20", hbox20,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox20);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox20, TRUE, TRUE, 0);

	DistB2_B3 = gtk_label_new("Distance B1-BA");
	gtk_widget_ref(DistB2_B3);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistB2_B3", DistB2_B3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB2_B3);
	gtk_box_pack_start(GTK_BOX(hbox20), DistB2_B3, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DistB2_B3), 24, 0);

	Dist2Entry = gtk_entry_new();
	gtk_widget_ref(Dist2Entry);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Dist2Entry", Dist2Entry,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Dist2Entry);
	gtk_box_pack_start(GTK_BOX(hbox20), Dist2Entry, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Dist2Entry), FALSE);

	Km2 = gtk_label_new("km");
	gtk_widget_ref(Km2);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Km2", Km2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Km2);
	gtk_box_pack_start(GTK_BOX(hbox20), Km2, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(Km2), 10, 0);

	hbox17 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox17);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox17", hbox17,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox17);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox17, TRUE, TRUE, 0);

	DistTotalLab = gtk_label_new("Distance parcourue");
	gtk_widget_ref(DistTotalLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistTotalLab", DistTotalLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotalLab);
	gtk_box_pack_start(GTK_BOX(hbox17), DistTotalLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(DistTotalLab), 13, 0);

	DistTotal = gtk_entry_new();
	gtk_widget_ref(DistTotal);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "DistTotal", DistTotal,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotal);
	gtk_box_pack_start(GTK_BOX(hbox17), DistTotal, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistTotal), FALSE);

	Km3 = gtk_label_new("km");
	gtk_widget_ref(Km3);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Km3", Km3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Km3);
	gtk_box_pack_start(GTK_BOX(hbox17), Km3, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(Km3), 10, 0);

	hbox18 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox18);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox18", hbox18,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox18);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox18, TRUE, TRUE, 0);

	CoeffLab = gtk_label_new("Coefficient");
	gtk_widget_ref(CoeffLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "CoeffLab", CoeffLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoeffLab);
	gtk_box_pack_start(GTK_BOX(hbox18), CoeffLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(CoeffLab), 39, 0);

	ValeurCoeffLab = gtk_label_new("1,0");
	gtk_widget_ref(ValeurCoeffLab);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "ValeurCoeffLab", ValeurCoeffLab,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(ValeurCoeffLab);
	gtk_box_pack_start(GTK_BOX(hbox18), ValeurCoeffLab, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(ValeurCoeffLab), 221, 0);

	hbox19 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox19);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "hbox19", hbox19,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox19);
	gtk_box_pack_start(GTK_BOX(vbox7), hbox19, TRUE, TRUE, 0);

	PointLabel = gtk_label_new("Points realises");
	gtk_widget_ref(PointLabel);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "PointLabel", PointLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(PointLabel);
	gtk_box_pack_start(GTK_BOX(hbox19), PointLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(PointLabel), 28, 0);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(DLCWindow), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_box_pack_start(GTK_BOX(hbox19), Points, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Points), FALSE);

	return DLCWindow;
}



GtkWidget *
create_TRWindow(void)
{
	GtkWidget *TRWindow;
	GtkWidget *vbox8;
	GtkWidget *TriangleLabel;
	GtkWidget *hbox21;
	GtkWidget *label45;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXDep;
	GtkWidget *hbox22;
	GtkWidget *label46;
	GtkWidget *BaliseDep;
	GtkWidget *label47;
	GtkWidget *DistBaliseDep;
	GtkWidget *hbox23;
	GtkWidget *label48;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB1;
	GtkWidget *hbox27;
	GtkWidget *label49;
	GtkWidget *BaliseB1;
	GtkWidget *label50;
	GtkWidget *DistBaliseB1;
	GtkWidget *hbox24;
	GtkWidget *label51;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB2;
	GtkWidget *hbox28;
	GtkWidget *label52;
	GtkWidget *BaliseB2;
	GtkWidget *label53;
	GtkWidget *DistBaliseB2;
	GtkWidget *hbox25;
	GtkWidget *label54;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXB3;
	GtkWidget *hbox29;
	GtkWidget *label55;
	GtkWidget *BaliseB3;
	GtkWidget *label56;
	GtkWidget *DistBaliseB3;
	GtkWidget *hbox26;
	GtkWidget *label57;
	GtkWidget *CoordYArv;
	GtkWidget *CoordXArv;
	GtkWidget *hbox30;
	GtkWidget *label58;
	GtkWidget *BaliseArv;
	GtkWidget *label59;
	GtkWidget *DistBaliseArv;
	GtkWidget *hbox31;
	GtkWidget *label60;
	GtkWidget *DistB1_B2;
	GtkWidget *label64;
	GtkWidget *hbox33;
	GtkWidget *label61;
	GtkWidget *DistB2_B3;
	GtkWidget *label65;
	GtkWidget *hbox32;
	GtkWidget *label62;
	GtkWidget *DistB3_B1;
	GtkWidget *label66;
	GtkWidget *hbox34;
	GtkWidget *label63;
	GtkWidget *DistTotale;
	GtkWidget *label67;
	GtkWidget *hbox35;
	GtkWidget *label68;
	GtkWidget *label69;
	GtkWidget *hbox36;
	GtkWidget *label70;
	GtkWidget *Points;

	TRWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(TRWindow), "TRWindow", TRWindow);
	gtk_window_set_title(GTK_WINDOW(TRWindow), "Triangle Simple");

	vbox8 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox8);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "vbox8", vbox8,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox8);
	gtk_container_add(GTK_CONTAINER(TRWindow), vbox8);

	TriangleLabel = gtk_label_new("Triangle Simple");
	gtk_widget_ref(TriangleLabel);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "TriangleLabel", TriangleLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(TriangleLabel);
	gtk_box_pack_start(GTK_BOX(vbox8), TriangleLabel, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(TriangleLabel), 0, 7);

	hbox21 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox21);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox21", hbox21,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox21);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox21, TRUE, TRUE, 0);

	label45 = gtk_label_new("Depart");
	gtk_widget_ref(label45);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label45", label45,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label45);
	gtk_box_pack_start(GTK_BOX(hbox21), label45, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label45), 43, 0);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox21), CoordYDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYDep), FALSE);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox21), CoordXDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXDep), FALSE);

	hbox22 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox22);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox22", hbox22,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox22);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox22, TRUE, TRUE, 0);

	label46 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label46);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label46", label46,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label46);
	gtk_box_pack_start(GTK_BOX(hbox22), label46, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label46), 2, 0);

	BaliseDep = gtk_entry_new();
	gtk_widget_ref(BaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "BaliseDep", BaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox22), BaliseDep, TRUE, TRUE, 0);

	label47 = gtk_label_new("Distance");
	gtk_widget_ref(label47);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label47", label47,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label47);
	gtk_box_pack_start(GTK_BOX(hbox22), label47, FALSE, FALSE, 0);

	DistBaliseDep = gtk_entry_new();
	gtk_widget_ref(DistBaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistBaliseDep", DistBaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox22), DistBaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseDep), FALSE);

	hbox23 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox23);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox23", hbox23,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox23);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox23, TRUE, TRUE, 0);

	label48 = gtk_label_new("B1");
	gtk_widget_ref(label48);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label48", label48,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label48);
	gtk_box_pack_start(GTK_BOX(hbox23), label48, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label48), 53, 0);

	CoordYB1 = gtk_entry_new();
	gtk_widget_ref(CoordYB1);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordYB1", CoordYB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB1);
	gtk_box_pack_start(GTK_BOX(hbox23), CoordYB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB1), FALSE);

	CoordXB1 = gtk_entry_new();
	gtk_widget_ref(CoordXB1);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordXB1", CoordXB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB1);
	gtk_box_pack_start(GTK_BOX(hbox23), CoordXB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB1), FALSE);

	hbox27 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox27);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox27", hbox27,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox27);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox27, TRUE, TRUE, 0);

	label49 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label49);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label49", label49,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label49);
	gtk_box_pack_start(GTK_BOX(hbox27), label49, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label49), 2, 0);

	BaliseB1 = gtk_entry_new();
	gtk_widget_ref(BaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "BaliseB1", BaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox27), BaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB1), FALSE);

	label50 = gtk_label_new("Distance");
	gtk_widget_ref(label50);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label50", label50,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label50);
	gtk_box_pack_start(GTK_BOX(hbox27), label50, FALSE, FALSE, 0);

	DistBaliseB1 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistBaliseB1", DistBaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox27), DistBaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB1), FALSE);

	hbox24 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox24);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox24", hbox24,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox24);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox24, TRUE, TRUE, 0);

	label51 = gtk_label_new("B2");
	gtk_widget_ref(label51);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label51", label51,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label51);
	gtk_box_pack_start(GTK_BOX(hbox24), label51, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label51), 53, 0);

	CoordYB2 = gtk_entry_new();
	gtk_widget_ref(CoordYB2);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordYB2", CoordYB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB2);
	gtk_box_pack_start(GTK_BOX(hbox24), CoordYB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB2), FALSE);

	CoordXB2 = gtk_entry_new();
	gtk_widget_ref(CoordXB2);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordXB2", CoordXB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB2);
	gtk_box_pack_start(GTK_BOX(hbox24), CoordXB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB2), FALSE);

	hbox28 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox28);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox28", hbox28,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox28);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox28, TRUE, TRUE, 0);

	label52 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label52);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label52", label52,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label52);
	gtk_box_pack_start(GTK_BOX(hbox28), label52, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label52), 2, 0);

	BaliseB2 = gtk_entry_new();
	gtk_widget_ref(BaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "BaliseB2", BaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox28), BaliseB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB2), FALSE);

	label53 = gtk_label_new("Distance");
	gtk_widget_ref(label53);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label53", label53,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label53);
	gtk_box_pack_start(GTK_BOX(hbox28), label53, FALSE, FALSE, 0);

	DistBaliseB2 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistBaliseB2", DistBaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox28), DistBaliseB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB2), FALSE);

	hbox25 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox25);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox25", hbox25,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox25);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox25, TRUE, TRUE, 0);

	label54 = gtk_label_new("B3");
	gtk_widget_ref(label54);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label54", label54,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label54);
	gtk_box_pack_start(GTK_BOX(hbox25), label54, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label54), 53, 0);

	CoordYB3 = gtk_entry_new();
	gtk_widget_ref(CoordYB3);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordYB3", CoordYB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB3);
	gtk_box_pack_start(GTK_BOX(hbox25), CoordYB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB3), FALSE);

	CoordXB3 = gtk_entry_new();
	gtk_widget_ref(CoordXB3);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordXB3", CoordXB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB3);
	gtk_box_pack_start(GTK_BOX(hbox25), CoordXB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB3), FALSE);

	hbox29 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox29);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox29", hbox29,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox29);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox29, TRUE, TRUE, 0);

	label55 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label55);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label55", label55,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label55);
	gtk_box_pack_start(GTK_BOX(hbox29), label55, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label55), 2, 0);

	BaliseB3 = gtk_entry_new();
	gtk_widget_ref(BaliseB3);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "BaliseB3", BaliseB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB3);
	gtk_box_pack_start(GTK_BOX(hbox29), BaliseB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB3), FALSE);

	label56 = gtk_label_new("Distance");
	gtk_widget_ref(label56);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label56", label56,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label56);
	gtk_box_pack_start(GTK_BOX(hbox29), label56, FALSE, FALSE, 0);

	DistBaliseB3 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB3);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistBaliseB3", DistBaliseB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB3);
	gtk_box_pack_start(GTK_BOX(hbox29), DistBaliseB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB3), FALSE);

	hbox26 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox26);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox26", hbox26,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox26);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox26, TRUE, TRUE, 0);

	label57 = gtk_label_new("Arrivee");
	gtk_widget_ref(label57);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label57", label57,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label57);
	gtk_box_pack_start(GTK_BOX(hbox26), label57, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label57), 40, 0);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox26), CoordYArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYArv), FALSE);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox26), CoordXArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXArv), FALSE);

	hbox30 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox30);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox30", hbox30,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox30);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox30, TRUE, TRUE, 0);

	label58 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label58);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label58", label58,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label58);
	gtk_box_pack_start(GTK_BOX(hbox30), label58, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label58), 2, 0);

	BaliseArv = gtk_entry_new();
	gtk_widget_ref(BaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "BaliseArv", BaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox30), BaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseArv), FALSE);

	label59 = gtk_label_new("Distance");
	gtk_widget_ref(label59);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label59", label59,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label59);
	gtk_box_pack_start(GTK_BOX(hbox30), label59, FALSE, FALSE, 0);

	DistBaliseArv = gtk_entry_new();
	gtk_widget_ref(DistBaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistBaliseArv", DistBaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox30), DistBaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseArv), FALSE);

	hbox31 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox31);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox31", hbox31,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox31);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox31, TRUE, TRUE, 0);

	label60 = gtk_label_new("Distance B1-B2");
	gtk_widget_ref(label60);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label60", label60,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label60);
	gtk_box_pack_start(GTK_BOX(hbox31), label60, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label60), 17, 0);

	DistB1_B2 = gtk_entry_new();
	gtk_widget_ref(DistB1_B2);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistB1_B2", DistB1_B2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB1_B2);
	gtk_box_pack_start(GTK_BOX(hbox31), DistB1_B2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB1_B2), FALSE);

	label64 = gtk_label_new("km");
	gtk_widget_ref(label64);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label64", label64,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label64);
	gtk_box_pack_start(GTK_BOX(hbox31), label64, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label64), 10, 0);

	hbox33 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox33);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox33", hbox33,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox33);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox33, TRUE, TRUE, 0);

	label61 = gtk_label_new("Distance B2-B3");
	gtk_widget_ref(label61);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label61", label61,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label61);
	gtk_box_pack_start(GTK_BOX(hbox33), label61, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label61), 17, 0);

	DistB2_B3 = gtk_entry_new();
	gtk_widget_ref(DistB2_B3);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistB2_B3", DistB2_B3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB2_B3);
	gtk_box_pack_start(GTK_BOX(hbox33), DistB2_B3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB2_B3), FALSE);

	label65 = gtk_label_new("km");
	gtk_widget_ref(label65);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label65", label65,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label65);
	gtk_box_pack_start(GTK_BOX(hbox33), label65, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label65), 10, 0);

	hbox32 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox32);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox32", hbox32,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox32);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox32, TRUE, TRUE, 0);

	label62 = gtk_label_new("Distance B3-B1");
	gtk_widget_ref(label62);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label62", label62,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label62);
	gtk_box_pack_start(GTK_BOX(hbox32), label62, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label62), 17, 0);

	DistB3_B1 = gtk_entry_new();
	gtk_widget_ref(DistB3_B1);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistB3_B1", DistB3_B1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB3_B1);
	gtk_box_pack_start(GTK_BOX(hbox32), DistB3_B1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB3_B1), FALSE);

	label66 = gtk_label_new("km");
	gtk_widget_ref(label66);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label66", label66,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label66);
	gtk_box_pack_start(GTK_BOX(hbox32), label66, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label66), 10, 0);

	hbox34 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox34);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox34", hbox34,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox34);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox34, TRUE, TRUE, 0);

	label63 = gtk_label_new("Distance parcourue");
	gtk_widget_ref(label63);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label63", label63,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label63);
	gtk_box_pack_start(GTK_BOX(hbox34), label63, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label63), 6, 0);

	DistTotale = gtk_entry_new();
	gtk_widget_ref(DistTotale);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "DistTotale", DistTotale,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotale);
	gtk_box_pack_start(GTK_BOX(hbox34), DistTotale, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistTotale), FALSE);

	label67 = gtk_label_new("km");
	gtk_widget_ref(label67);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label67", label67,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label67);
	gtk_box_pack_start(GTK_BOX(hbox34), label67, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label67), 10, 0);

	hbox35 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox35);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox35", hbox35,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox35);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox35, TRUE, TRUE, 0);

	label68 = gtk_label_new("Coefficient");
	gtk_widget_ref(label68);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label68", label68,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label68);
	gtk_box_pack_start(GTK_BOX(hbox35), label68, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label68), 32, 0);

	label69 = gtk_label_new("1,3");
	gtk_widget_ref(label69);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label69", label69,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label69);
	gtk_box_pack_start(GTK_BOX(hbox35), label69, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label69), 228, 0);

	hbox36 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox36);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "hbox36", hbox36,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox36);
	gtk_box_pack_start(GTK_BOX(vbox8), hbox36, TRUE, TRUE, 0);

	label70 = gtk_label_new("Points realises");
	gtk_widget_ref(label70);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "label70", label70,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label70);
	gtk_box_pack_start(GTK_BOX(hbox36), label70, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label70), 20, 3);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(TRWindow), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_box_pack_start(GTK_BOX(hbox36), Points, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Points), FALSE);

	return TRWindow;
}

GtkWidget *
create_QDRLTWindow(void)
{
	GtkWidget *window6;
	GtkWidget *vbox9;
	GtkWidget *label71;
	GtkWidget *hbox37;
	GtkWidget *label72;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXDep;
	GtkWidget *hbox38;
	GtkWidget *label73;
	GtkWidget *BaliseDep;
	GtkWidget *label74;
	GtkWidget *DistBaliseDep;
	GtkWidget *hbox39;
	GtkWidget *label75;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB1;
	GtkWidget *hbox40;
	GtkWidget *label76;
	GtkWidget *BaliseB1;
	GtkWidget *label77;
	GtkWidget *DistBaliseB1;
	GtkWidget *hbox41;
	GtkWidget *label78;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB2;
	GtkWidget *hbox42;
	GtkWidget *label79;
	GtkWidget *BaliseB2;
	GtkWidget *label80;
	GtkWidget *DistBaliseB2;
	GtkWidget *hbox43;
	GtkWidget *label81;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXB3;
	GtkWidget *hbox44;
	GtkWidget *label82;
	GtkWidget *BaliseB3;
	GtkWidget *label83;
	GtkWidget *DistBaliseB3;
	GtkWidget *hbox53;
	GtkWidget *label98;
	GtkWidget *CoordYB4;
	GtkWidget *CoordXB4;
	GtkWidget *hbox54;
	GtkWidget *label99;
	GtkWidget *BaliseB4;
	GtkWidget *label100;
	GtkWidget *DistBaliseB4;
	GtkWidget *hbox45;
	GtkWidget *label84;
	GtkWidget *CoordYArv;
	GtkWidget *CoordXArv;
	GtkWidget *hbox46;
	GtkWidget *label85;
	GtkWidget *BaliseArv;
	GtkWidget *label86;
	GtkWidget *DistBaliseArv;
	GtkWidget *hbox47;
	GtkWidget *label87;
	GtkWidget *DistB1_B2;
	GtkWidget *label88;
	GtkWidget *hbox48;
	GtkWidget *label89;
	GtkWidget *DistB2_B3;
	GtkWidget *label90;
	GtkWidget *hbox49;
	GtkWidget *label91;
	GtkWidget *DistB3_B4;
	GtkWidget *label92;
	GtkWidget *hbox55;
	GtkWidget *label102;
	GtkWidget *DistB4_B1;
	GtkWidget *label101;
	GtkWidget *hbox50;
	GtkWidget *label93;
	GtkWidget *DistTotale;
	GtkWidget *label94;
	GtkWidget *hbox51;
	GtkWidget *label95;
	GtkWidget *label96;
	GtkWidget *hbox52;
	GtkWidget *label97;
	GtkWidget *Points;

	window6 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(window6), "window6", window6);
	gtk_window_set_title(GTK_WINDOW(window6), "Quadrilatere");

	vbox9 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox9);
	gtk_object_set_data_full(GTK_OBJECT(window6), "vbox9", vbox9,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox9);
	gtk_container_add(GTK_CONTAINER(window6), vbox9);

	label71 = gtk_label_new("Quadrilatere");
	gtk_widget_ref(label71);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label71", label71,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label71);
	gtk_box_pack_start(GTK_BOX(vbox9), label71, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label71), 0, 7);

	hbox37 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox37);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox37", hbox37,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox37);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox37, TRUE, TRUE, 0);

	label72 = gtk_label_new("Depart");
	gtk_widget_ref(label72);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label72", label72,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label72);
	gtk_box_pack_start(GTK_BOX(hbox37), label72, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label72), 43, 0);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox37), CoordYDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYDep), FALSE);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox37), CoordXDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXDep), FALSE);

	hbox38 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox38);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox38", hbox38,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox38);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox38, TRUE, TRUE, 0);

	label73 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label73);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label73", label73,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label73);
	gtk_box_pack_start(GTK_BOX(hbox38), label73, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label73), 2, 0);

	BaliseDep = gtk_entry_new();
	gtk_widget_ref(BaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseDep", BaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox38), BaliseDep, TRUE, TRUE, 0);

	label74 = gtk_label_new("Distance");
	gtk_widget_ref(label74);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label74", label74,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label74);
	gtk_box_pack_start(GTK_BOX(hbox38), label74, FALSE, FALSE, 0);

	DistBaliseDep = gtk_entry_new();
	gtk_widget_ref(DistBaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseDep", DistBaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox38), DistBaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseDep), FALSE);

	hbox39 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox39);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox39", hbox39,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox39);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox39, TRUE, TRUE, 0);

	label75 = gtk_label_new("B1");
	gtk_widget_ref(label75);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label75", label75,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label75);
	gtk_box_pack_start(GTK_BOX(hbox39), label75, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label75), 53, 0);

	CoordYB1 = gtk_entry_new();
	gtk_widget_ref(CoordYB1);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYB1", CoordYB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB1);
	gtk_box_pack_start(GTK_BOX(hbox39), CoordYB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB1), FALSE);

	CoordXB1 = gtk_entry_new();
	gtk_widget_ref(CoordXB1);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXB1", CoordXB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB1);
	gtk_box_pack_start(GTK_BOX(hbox39), CoordXB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB1), FALSE);

	hbox40 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox40);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox40", hbox40,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox40);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox40, TRUE, TRUE, 0);

	label76 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label76);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label76", label76,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label76);
	gtk_box_pack_start(GTK_BOX(hbox40), label76, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label76), 2, 0);

	BaliseB1 = gtk_entry_new();
	gtk_widget_ref(BaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseB1", BaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox40), BaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB1), FALSE);

	label77 = gtk_label_new("Distance");
	gtk_widget_ref(label77);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label77", label77,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label77);
	gtk_box_pack_start(GTK_BOX(hbox40), label77, FALSE, FALSE, 0);

	DistBaliseB1 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseB1", DistBaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox40), DistBaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB1), FALSE);

	hbox41 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox41);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox41", hbox41,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox41);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox41, TRUE, TRUE, 0);

	label78 = gtk_label_new("B2");
	gtk_widget_ref(label78);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label78", label78,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label78);
	gtk_box_pack_start(GTK_BOX(hbox41), label78, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label78), 53, 0);

	CoordYB2 = gtk_entry_new();
	gtk_widget_ref(CoordYB2);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYB2", CoordYB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB2);
	gtk_box_pack_start(GTK_BOX(hbox41), CoordYB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB2), FALSE);

	CoordXB2 = gtk_entry_new();
	gtk_widget_ref(CoordXB2);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXB2", CoordXB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB2);
	gtk_box_pack_start(GTK_BOX(hbox41), CoordXB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB2), FALSE);

	hbox42 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox42);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox42", hbox42,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox42);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox42, TRUE, TRUE, 0);

	label79 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label79);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label79", label79,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label79);
	gtk_box_pack_start(GTK_BOX(hbox42), label79, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label79), 2, 0);

	BaliseB2 = gtk_entry_new();
	gtk_widget_ref(BaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseB2", BaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox42), BaliseB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB2), FALSE);

	label80 = gtk_label_new("Distance");
	gtk_widget_ref(label80);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label80", label80,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label80);
	gtk_box_pack_start(GTK_BOX(hbox42), label80, FALSE, FALSE, 0);

	DistBaliseB2 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseB2", DistBaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox42), DistBaliseB2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB2), FALSE);

	hbox43 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox43);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox43", hbox43,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox43);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox43, TRUE, TRUE, 0);

	label81 = gtk_label_new("B3");
	gtk_widget_ref(label81);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label81", label81,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label81);
	gtk_box_pack_start(GTK_BOX(hbox43), label81, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label81), 53, 0);

	CoordYB3 = gtk_entry_new();
	gtk_widget_ref(CoordYB3);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYB3", CoordYB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB3);
	gtk_box_pack_start(GTK_BOX(hbox43), CoordYB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB3), FALSE);

	CoordXB3 = gtk_entry_new();
	gtk_widget_ref(CoordXB3);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXB3", CoordXB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB3);
	gtk_box_pack_start(GTK_BOX(hbox43), CoordXB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB3), FALSE);

	hbox44 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox44);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox44", hbox44,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox44);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox44, TRUE, TRUE, 0);

	label82 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label82);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label82", label82,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label82);
	gtk_box_pack_start(GTK_BOX(hbox44), label82, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label82), 2, 0);

	BaliseB3 = gtk_entry_new();
	gtk_widget_ref(BaliseB3);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseB3", BaliseB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB3);
	gtk_box_pack_start(GTK_BOX(hbox44), BaliseB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB3), FALSE);

	label83 = gtk_label_new("Distance");
	gtk_widget_ref(label83);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label83", label83,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label83);
	gtk_box_pack_start(GTK_BOX(hbox44), label83, FALSE, FALSE, 0);

	DistBaliseB3 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB3);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseB3", DistBaliseB3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB3);
	gtk_box_pack_start(GTK_BOX(hbox44), DistBaliseB3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB3), FALSE);

	hbox53 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox53);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox53", hbox53,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox53);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox53, TRUE, TRUE, 0);

	label98 = gtk_label_new("B4");
	gtk_widget_ref(label98);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label98", label98,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label98);
	gtk_box_pack_start(GTK_BOX(hbox53), label98, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label98), 53, 0);

	CoordYB4 = gtk_entry_new();
	gtk_widget_ref(CoordYB4);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYB4", CoordYB4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB4);
	gtk_box_pack_start(GTK_BOX(hbox53), CoordYB4, TRUE, TRUE, 0);

	CoordXB4 = gtk_entry_new();
	gtk_widget_ref(CoordXB4);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXB4", CoordXB4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB4);
	gtk_box_pack_start(GTK_BOX(hbox53), CoordXB4, TRUE, TRUE, 0);

	hbox54 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox54);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox54", hbox54,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox54);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox54, TRUE, TRUE, 0);

	label99 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label99);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label99", label99,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label99);
	gtk_box_pack_start(GTK_BOX(hbox54), label99, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label99), 2, 0);

	BaliseB4 = gtk_entry_new();
	gtk_widget_ref(BaliseB4);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseB4", BaliseB4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB4);
	gtk_box_pack_start(GTK_BOX(hbox54), BaliseB4, TRUE, TRUE, 0);

	label100 = gtk_label_new("Distance");
	gtk_widget_ref(label100);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label100", label100,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label100);
	gtk_box_pack_start(GTK_BOX(hbox54), label100, FALSE, FALSE, 0);

	DistBaliseB4 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB4);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseB4", DistBaliseB4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB4);
	gtk_box_pack_start(GTK_BOX(hbox54), DistBaliseB4, TRUE, TRUE, 0);

	hbox45 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox45);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox45", hbox45,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox45);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox45, TRUE, TRUE, 0);

	label84 = gtk_label_new("Arrivee");
	gtk_widget_ref(label84);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label84", label84,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label84);
	gtk_box_pack_start(GTK_BOX(hbox45), label84, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label84), 40, 0);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox45), CoordYArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYArv), FALSE);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(window6), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox45), CoordXArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXArv), FALSE);

	hbox46 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox46);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox46", hbox46,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox46);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox46, TRUE, TRUE, 0);

	label85 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label85);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label85", label85,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label85);
	gtk_box_pack_start(GTK_BOX(hbox46), label85, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label85), 2, 0);

	BaliseArv = gtk_entry_new();
	gtk_widget_ref(BaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(window6), "BaliseArv", BaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox46), BaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseArv), FALSE);

	label86 = gtk_label_new("Distance");
	gtk_widget_ref(label86);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label86", label86,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label86);
	gtk_box_pack_start(GTK_BOX(hbox46), label86, FALSE, FALSE, 0);

	DistBaliseArv = gtk_entry_new();
	gtk_widget_ref(DistBaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistBaliseArv", DistBaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox46), DistBaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseArv), FALSE);

	hbox47 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox47);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox47", hbox47,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox47);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox47, TRUE, TRUE, 0);

	label87 = gtk_label_new("Distance B1-B2");
	gtk_widget_ref(label87);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label87", label87,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label87);
	gtk_box_pack_start(GTK_BOX(hbox47), label87, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label87), 17, 0);

	DistB1_B2 = gtk_entry_new();
	gtk_widget_ref(DistB1_B2);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistB1_B2", DistB1_B2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB1_B2);
	gtk_box_pack_start(GTK_BOX(hbox47), DistB1_B2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB1_B2), FALSE);

	label88 = gtk_label_new("km");
	gtk_widget_ref(label88);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label88", label88,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label88);
	gtk_box_pack_start(GTK_BOX(hbox47), label88, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label88), 10, 0);

	hbox48 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox48);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox48", hbox48,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox48);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox48, TRUE, TRUE, 0);

	label89 = gtk_label_new("Distance B2-B3");
	gtk_widget_ref(label89);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label89", label89,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label89);
	gtk_box_pack_start(GTK_BOX(hbox48), label89, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label89), 17, 0);

	DistB2_B3 = gtk_entry_new();
	gtk_widget_ref(DistB2_B3);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistB2_B3", DistB2_B3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB2_B3);
	gtk_box_pack_start(GTK_BOX(hbox48), DistB2_B3, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB2_B3), FALSE);

	label90 = gtk_label_new("km");
	gtk_widget_ref(label90);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label90", label90,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label90);
	gtk_box_pack_start(GTK_BOX(hbox48), label90, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label90), 10, 0);

	hbox49 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox49);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox49", hbox49,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox49);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox49, TRUE, TRUE, 0);

	label91 = gtk_label_new("Distance B3-B4");
	gtk_widget_ref(label91);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label91", label91,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label91);
	gtk_box_pack_start(GTK_BOX(hbox49), label91, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label91), 17, 0);

	DistB3_B4 = gtk_entry_new();
	gtk_widget_ref(DistB3_B4);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistB3_B4", DistB3_B4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB3_B4);
	gtk_box_pack_start(GTK_BOX(hbox49), DistB3_B4, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB3_B4), FALSE);

	label92 = gtk_label_new("km");
	gtk_widget_ref(label92);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label92", label92,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label92);
	gtk_box_pack_start(GTK_BOX(hbox49), label92, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label92), 10, 0);

	hbox55 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox55);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox55", hbox55,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox55);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox55, TRUE, TRUE, 0);

	label102 = gtk_label_new("DistanceB4-B1");
	gtk_widget_ref(label102);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label102", label102,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label102);
	gtk_box_pack_start(GTK_BOX(hbox55), label102, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label102), 19, 0);

	DistB4_B1 = gtk_entry_new();
	gtk_widget_ref(DistB4_B1);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistB4_B1", DistB4_B1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB4_B1);
	gtk_box_pack_start(GTK_BOX(hbox55), DistB4_B1, TRUE, TRUE, 0);

	label101 = gtk_label_new("km");
	gtk_widget_ref(label101);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label101", label101,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label101);
	gtk_box_pack_start(GTK_BOX(hbox55), label101, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label101), 10, 0);

	hbox50 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox50);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox50", hbox50,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox50);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox50, TRUE, TRUE, 0);

	label93 = gtk_label_new("Distance parcourue");
	gtk_widget_ref(label93);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label93", label93,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label93);
	gtk_box_pack_start(GTK_BOX(hbox50), label93, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label93), 6, 0);

	DistTotale = gtk_entry_new();
	gtk_widget_ref(DistTotale);
	gtk_object_set_data_full(GTK_OBJECT(window6), "DistTotale", DistTotale,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotale);
	gtk_box_pack_start(GTK_BOX(hbox50), DistTotale, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistTotale), FALSE);

	label94 = gtk_label_new("km");
	gtk_widget_ref(label94);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label94", label94,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label94);
	gtk_box_pack_start(GTK_BOX(hbox50), label94, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label94), 10, 0);

	hbox51 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox51);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox51", hbox51,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox51);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox51, TRUE, TRUE, 0);

	label95 = gtk_label_new("Coefficient");
	gtk_widget_ref(label95);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label95", label95,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label95);
	gtk_box_pack_start(GTK_BOX(hbox51), label95, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label95), 32, 0);

	label96 = gtk_label_new("1,3");
	gtk_widget_ref(label96);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label96", label96,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label96);
	gtk_box_pack_start(GTK_BOX(hbox51), label96, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label96), 228, 0);

	hbox52 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox52);
	gtk_object_set_data_full(GTK_OBJECT(window6), "hbox52", hbox52,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox52);
	gtk_box_pack_start(GTK_BOX(vbox9), hbox52, TRUE, TRUE, 0);

	label97 = gtk_label_new("Points realises");
	gtk_widget_ref(label97);
	gtk_object_set_data_full(GTK_OBJECT(window6), "label97", label97,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label97);
	gtk_box_pack_start(GTK_BOX(hbox52), label97, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label97), 20, 3);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(window6), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_box_pack_start(GTK_BOX(hbox52), Points, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Points), FALSE);

	return window6;
}


GtkWidget *
create_ARWindow(void)
{
	GtkWidget *window8;
	GtkWidget *vbox12;
	GtkWidget *label103;
	GtkWidget *vbox13;
	GtkWidget *hbox57;
	GtkWidget *label104;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXDep;
	GtkWidget *hbox58;
	GtkWidget *label105;
	GtkWidget *BaliseDep;
	GtkWidget *label106;
	GtkWidget *DistBaliseDep;
	GtkWidget *hbox59;
	GtkWidget *label107;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB1;
	GtkWidget *hbox60;
	GtkWidget *label108;
	GtkWidget *BaliseB1;
	GtkWidget *label109;
	GtkWidget *DistBaliseB1;
	GtkWidget *hbox68;
	GtkWidget *label122;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB2;
	GtkWidget *hbox69;
	GtkWidget *label123;
	GtkWidget *BaliseB2;
	GtkWidget *label124;
	GtkWidget *DistBaliseB2;
	GtkWidget *hbox61;
	GtkWidget *label110;
	GtkWidget *CoordYArv;
	GtkWidget *CoordXArv;
	GtkWidget *hbox62;
	GtkWidget *label111;
	GtkWidget *BaliseArv;
	GtkWidget *label112;
	GtkWidget *DistBaliseArv;
	GtkWidget *hbox63;
	GtkWidget *label113;
	GtkWidget *DistB1_B2;
	GtkWidget *label114;
	GtkWidget *hbox65;
	GtkWidget *label117;
	GtkWidget *DistTotale;
	GtkWidget *label118;
	GtkWidget *hbox66;
	GtkWidget *label119;
	GtkWidget *label120;
	GtkWidget *hbox67;
	GtkWidget *label121;
	GtkWidget *Points;

	window8 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(window8), "window8", window8);
	gtk_window_set_title(GTK_WINDOW(window8), "Aller-Retour");

	vbox12 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox12);
	gtk_object_set_data_full(GTK_OBJECT(window8), "vbox12", vbox12,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox12);
	gtk_container_add(GTK_CONTAINER(window8), vbox12);

	label103 = gtk_label_new("Aller-Retour");
	gtk_widget_ref(label103);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label103", label103,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label103);
	gtk_box_pack_start(GTK_BOX(vbox12), label103, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label103), 0, 7);

	vbox13 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox13);
	gtk_object_set_data_full(GTK_OBJECT(window8), "vbox13", vbox13,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox13);
	gtk_box_pack_start(GTK_BOX(vbox12), vbox13, TRUE, TRUE, 0);

	hbox57 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox57);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox57", hbox57,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox57);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox57, TRUE, TRUE, 0);

	label104 = gtk_label_new("Depart");
	gtk_widget_ref(label104);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label104", label104,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label104);
	gtk_box_pack_start(GTK_BOX(hbox57), label104, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label104), 50, 0);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox57), CoordYDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYDep), FALSE);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox57), CoordXDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXDep), FALSE);

	hbox58 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox58);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox58", hbox58,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox58);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox58, TRUE, TRUE, 0);

	label105 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label105);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label105", label105,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label105);
	gtk_box_pack_start(GTK_BOX(hbox58), label105, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label105), 9, 0);

	BaliseDep = gtk_entry_new();
	gtk_widget_ref(BaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(window8), "BaliseDep", BaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox58), BaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseDep), FALSE);

	label106 = gtk_label_new("Distance");
	gtk_widget_ref(label106);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label106", label106,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label106);
	gtk_box_pack_start(GTK_BOX(hbox58), label106, FALSE, FALSE, 0);

	DistBaliseDep = gtk_entry_new();
	gtk_widget_ref(DistBaliseDep);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistBaliseDep", DistBaliseDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseDep);
	gtk_box_pack_start(GTK_BOX(hbox58), DistBaliseDep, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseDep), FALSE);

	hbox59 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox59);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox59", hbox59,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox59);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox59, TRUE, TRUE, 0);

	label107 = gtk_label_new("B1");
	gtk_widget_ref(label107);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label107", label107,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label107);
	gtk_box_pack_start(GTK_BOX(hbox59), label107, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label107), 61, 0);

	CoordYB1 = gtk_entry_new();
	gtk_widget_ref(CoordYB1);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordYB1", CoordYB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB1);
	gtk_box_pack_start(GTK_BOX(hbox59), CoordYB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYB1), FALSE);

	CoordXB1 = gtk_entry_new();
	gtk_widget_ref(CoordXB1);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordXB1", CoordXB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB1);
	gtk_box_pack_start(GTK_BOX(hbox59), CoordXB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXB1), FALSE);

	hbox60 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox60);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox60", hbox60,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox60);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox60, TRUE, TRUE, 0);

	label108 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label108);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label108", label108,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label108);
	gtk_box_pack_start(GTK_BOX(hbox60), label108, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label108), 9, 0);

	BaliseB1 = gtk_entry_new();
	gtk_widget_ref(BaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(window8), "BaliseB1", BaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox60), BaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseB1), FALSE);

	label109 = gtk_label_new("Distance");
	gtk_widget_ref(label109);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label109", label109,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label109);
	gtk_box_pack_start(GTK_BOX(hbox60), label109, FALSE, FALSE, 0);

	DistBaliseB1 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB1);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistBaliseB1", DistBaliseB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB1);
	gtk_box_pack_start(GTK_BOX(hbox60), DistBaliseB1, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseB1), FALSE);

	hbox68 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox68);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox68", hbox68,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox68);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox68, TRUE, TRUE, 0);

	label122 = gtk_label_new("B2");
	gtk_widget_ref(label122);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label122", label122,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label122);
	gtk_box_pack_start(GTK_BOX(hbox68), label122, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label122), 61, 0);

	CoordYB2 = gtk_entry_new();
	gtk_widget_ref(CoordYB2);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordYB2", CoordYB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB2);
	gtk_box_pack_start(GTK_BOX(hbox68), CoordYB2, TRUE, TRUE, 0);

	CoordXB2 = gtk_entry_new();
	gtk_widget_ref(CoordXB2);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordXB2", CoordXB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB2);
	gtk_box_pack_start(GTK_BOX(hbox68), CoordXB2, TRUE, TRUE, 0);

	hbox69 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox69);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox69", hbox69,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox69);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox69, TRUE, TRUE, 0);

	label123 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label123);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label123", label123,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label123);
	gtk_box_pack_start(GTK_BOX(hbox69), label123, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label123), 9, 0);

	BaliseB2 = gtk_entry_new();
	gtk_widget_ref(BaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(window8), "BaliseB2", BaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox69), BaliseB2, TRUE, TRUE, 0);

	label124 = gtk_label_new("Distance");
	gtk_widget_ref(label124);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label124", label124,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label124);
	gtk_box_pack_start(GTK_BOX(hbox69), label124, FALSE, FALSE, 0);

	DistBaliseB2 = gtk_entry_new();
	gtk_widget_ref(DistBaliseB2);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistBaliseB2", DistBaliseB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseB2);
	gtk_box_pack_start(GTK_BOX(hbox69), DistBaliseB2, TRUE, TRUE, 0);

	hbox61 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox61);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox61", hbox61,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox61);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox61, TRUE, TRUE, 0);

	label110 = gtk_label_new("Arrivee");
	gtk_widget_ref(label110);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label110", label110,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label110);
	gtk_box_pack_start(GTK_BOX(hbox61), label110, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label110), 48, 0);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox61), CoordYArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordYArv), FALSE);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(window8), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox61), CoordXArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(CoordXArv), FALSE);

	hbox62 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox62);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox62", hbox62,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox62);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox62, TRUE, TRUE, 0);

	label111 = gtk_label_new("Balise la plus proche");
	gtk_widget_ref(label111);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label111", label111,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label111);
	gtk_box_pack_start(GTK_BOX(hbox62), label111, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label111), 9, 0);

	BaliseArv = gtk_entry_new();
	gtk_widget_ref(BaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(window8), "BaliseArv", BaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(BaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox62), BaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(BaliseArv), FALSE);

	label112 = gtk_label_new("Distance");
	gtk_widget_ref(label112);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label112", label112,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label112);
	gtk_box_pack_start(GTK_BOX(hbox62), label112, FALSE, FALSE, 0);

	DistBaliseArv = gtk_entry_new();
	gtk_widget_ref(DistBaliseArv);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistBaliseArv", DistBaliseArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistBaliseArv);
	gtk_box_pack_start(GTK_BOX(hbox62), DistBaliseArv, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistBaliseArv), FALSE);

	hbox63 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox63);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox63", hbox63,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox63);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox63, TRUE, TRUE, 0);

	label113 = gtk_label_new("Distance B1-B2");
	gtk_widget_ref(label113);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label113", label113,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label113);
	gtk_box_pack_start(GTK_BOX(hbox63), label113, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label113), 24, 0);

	DistB1_B2 = gtk_entry_new();
	gtk_widget_ref(DistB1_B2);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistB1_B2", DistB1_B2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB1_B2);
	gtk_box_pack_start(GTK_BOX(hbox63), DistB1_B2, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistB1_B2), FALSE);

	label114 = gtk_label_new("km");
	gtk_widget_ref(label114);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label114", label114,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label114);
	gtk_box_pack_start(GTK_BOX(hbox63), label114, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label114), 10, 0);

	hbox65 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox65);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox65", hbox65,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox65);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox65, TRUE, TRUE, 0);

	label117 = gtk_label_new("Distance parcourue");
	gtk_widget_ref(label117);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label117", label117,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label117);
	gtk_box_pack_start(GTK_BOX(hbox65), label117, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label117), 13, 0);

	DistTotale = gtk_entry_new();
	gtk_widget_ref(DistTotale);
	gtk_object_set_data_full(GTK_OBJECT(window8), "DistTotale", DistTotale,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotale);
	gtk_box_pack_start(GTK_BOX(hbox65), DistTotale, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(DistTotale), FALSE);

	label118 = gtk_label_new("km");
	gtk_widget_ref(label118);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label118", label118,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label118);
	gtk_box_pack_start(GTK_BOX(hbox65), label118, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label118), 10, 0);

	hbox66 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox66);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox66", hbox66,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox66);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox66, TRUE, TRUE, 0);

	label119 = gtk_label_new("Coefficient");
	gtk_widget_ref(label119);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label119", label119,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label119);
	gtk_box_pack_start(GTK_BOX(hbox66), label119, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label119), 39, 0);

	label120 = gtk_label_new("1,3");
	gtk_widget_ref(label120);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label120", label120,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label120);
	gtk_box_pack_start(GTK_BOX(hbox66), label120, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label120), 221, 0);

	hbox67 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox67);
	gtk_object_set_data_full(GTK_OBJECT(window8), "hbox67", hbox67,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox67);
	gtk_box_pack_start(GTK_BOX(vbox13), hbox67, TRUE, TRUE, 0);

	label121 = gtk_label_new("Points realises");
	gtk_widget_ref(label121);
	gtk_object_set_data_full(GTK_OBJECT(window8), "label121", label121,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label121);
	gtk_box_pack_start(GTK_BOX(hbox67), label121, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label121), 28, 0);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(window8), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_box_pack_start(GTK_BOX(hbox67), Points, TRUE, TRUE, 0);
	gtk_entry_set_editable(GTK_ENTRY(Points), FALSE);

	return window8;
}



/** creation d'une fenetre d'attente avec une barre qui fait des allers-retours */
GtkWidget *
CreateWaitWindow(char *Text)
{
	GtkWidget *WaitWindow;
	GtkWidget *ProgressBar;
	GtkAdjustment *Adjust;
	GtkWidget *Message;
	GtkWidget *Box;
	GtkWidget *Frame;

	/* Création de la fenêtre */
	WaitWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(WaitWindow), GTK_WIN_POS_CENTER);
	//  gtk_container_set_border_width(GTK_CONTAINER(WaitWindow),10);

	Frame = gtk_frame_new(NULL);
	gtk_widget_ref(Frame);
	gtk_object_set_data_full(GTK_OBJECT(WaitWindow), "Frame", Frame, (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Frame);
	gtk_container_add(GTK_CONTAINER(WaitWindow), Frame);
	gtk_container_set_border_width(GTK_CONTAINER(Frame), 4);
	gtk_frame_set_shadow_type(GTK_FRAME(Frame), GTK_SHADOW_IN);
	gtk_window_set_policy((GtkWindow *)WaitWindow, FALSE, FALSE, FALSE);
	/* La fenêtre contient une boîte verticale
	 * qui contiendra les barres */
	Box = gtk_vbox_new(TRUE, 0);
	gtk_container_add(GTK_CONTAINER(Frame), Box);

	/* Création de l'ajustement pour les barres */
	Adjust = (GtkAdjustment *)gtk_adjustment_new(100, 100, 250, 0, 0, 0);

	Message = gtk_label_new(Text);
	gtk_widget_ref(Message);
	gtk_widget_show(Message);
	gtk_label_set_justify(GTK_LABEL(Message), GTK_JUSTIFY_CENTER);

	gtk_box_pack_start(GTK_BOX(Box), Message, FALSE, FALSE, 5);

	/* La deuxième barre utilise le mode activité */
	ProgressBar = gtk_progress_bar_new_with_adjustment(Adjust);
	gtk_progress_set_activity_mode(GTK_PROGRESS(ProgressBar), TRUE);
	gtk_progress_bar_set_activity_step(GTK_PROGRESS_BAR(ProgressBar), 5);
	gtk_progress_bar_set_activity_blocks(GTK_PROGRESS_BAR(ProgressBar), 3);
	gtk_box_pack_start(GTK_BOX(Box), ProgressBar, FALSE, FALSE, 5);

	gtk_widget_ref(ProgressBar);
	gtk_object_set_data((GtkObject *)WaitWindow, "Adjust", Adjust);
	gtk_object_set_data_full((GtkObject *)WaitWindow, "ProgressBar", ProgressBar, (GtkDestroyNotify)gtk_widget_unref);
	/* La valeur des barres de progression sera incrémentée
	 * par un timer */
	//  gtk_timeout_add(100, (GtkFunction)Timer, ProgressBar);




	/* On affiche le tout */
	gtk_widget_show_all(WaitWindow);
	gtk_window_set_transient_for((GtkWindow *)WaitWindow, (GtkWindow *)CFDWindow);
	return WaitWindow;

	gtk_signal_connect_object(GTK_OBJECT(WaitWindow), "delete_event",
				  GTK_SIGNAL_FUNC(gtk_widget_show), GTK_OBJECT(WaitWindow));
	gtk_signal_connect_object(GTK_OBJECT(WaitWindow), "destroy_event",
				  GTK_SIGNAL_FUNC(gtk_widget_show), GTK_OBJECT(WaitWindow));
}

GtkWidget *
create_DL2CWindow(void)
{
	GtkWidget *DL2CWindow;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *label20;
	GtkWidget *table1;
	GtkWidget *hbox2;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *hbox3;
	GtkWidget *entry22;
	GtkWidget *label35;
	GtkWidget *entry35;
	GtkWidget *hbox4;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *hbox5;
	GtkWidget *entry24;
	GtkWidget *label36;
	GtkWidget *entry33;
	GtkWidget *hbox6;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *hbox7;
	GtkWidget *entry26;
	GtkWidget *label37;
	GtkWidget *entry31;
	GtkWidget *hbox8;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *hbox9;
	GtkWidget *entry28;
	GtkWidget *label38;
	GtkWidget *entry29;
	GtkWidget *hbox10;
	GtkWidget *DistB1_B2;
	GtkWidget *label39;
	GtkWidget *hbox11;
	GtkWidget *DistB2_B3;
	GtkWidget *label40;
	GtkWidget *hbox12;
	GtkWidget *DistB3_B4;
	GtkWidget *label41;
	GtkWidget *hbox13;
	GtkWidget *DistTotale;
	GtkWidget *label42;
	GtkWidget *Points;
	GtkWidget *label21;
	GtkWidget *label22;
	GtkWidget *label23;
	GtkWidget *label24;
	GtkWidget *label25;
	GtkWidget *label26;
	GtkWidget *label27;
	GtkWidget *label28;
	GtkWidget *label29;
	GtkWidget *label30;
	GtkWidget *label31;
	GtkWidget *label32;
	GtkWidget *label33;
	GtkWidget *label34;
	GtkWidget *label43;

	DL2CWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(DL2CWindow), "DL2CWindow", DL2CWindow);
	gtk_window_set_title(GTK_WINDOW(DL2CWindow), ("Distance Libre avec 2 points de contournement"));

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_ref(vbox1);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "vbox1", vbox1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(DL2CWindow), vbox1);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox1);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox1", hbox1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 0);

	label20 = gtk_label_new(("Distance Libre avec 2 points de contournement"));
	gtk_widget_ref(label20);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label20", label20,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label20);
	gtk_box_pack_start(GTK_BOX(hbox1), label20, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label20), 100, 13);

	table1 = gtk_table_new(14, 2, FALSE);
	gtk_widget_ref(table1);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "table1", table1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(table1);
	gtk_box_pack_start(GTK_BOX(vbox1), table1, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox2);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox2", hbox2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox2);
	gtk_table_attach(GTK_TABLE(table1), hbox2, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	CoordXDep = gtk_entry_new();
	gtk_widget_ref(CoordXDep);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordXDep", CoordXDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXDep);
	gtk_box_pack_start(GTK_BOX(hbox2), CoordXDep, TRUE, TRUE, 0);

	CoordYDep = gtk_entry_new();
	gtk_widget_ref(CoordYDep);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordYDep", CoordYDep,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYDep);
	gtk_box_pack_start(GTK_BOX(hbox2), CoordYDep, TRUE, TRUE, 0);

	hbox3 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox3);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox3", hbox3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox3);
	gtk_table_attach(GTK_TABLE(table1), hbox3, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	entry22 = gtk_entry_new();
	gtk_widget_ref(entry22);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry22", entry22,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry22);
	gtk_box_pack_start(GTK_BOX(hbox3), entry22, TRUE, TRUE, 0);

	label35 = gtk_label_new(("Distance"));
	gtk_widget_ref(label35);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label35", label35,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label35);
	gtk_box_pack_start(GTK_BOX(hbox3), label35, FALSE, FALSE, 0);

	entry35 = gtk_entry_new();
	gtk_widget_ref(entry35);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry35", entry35,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry35);
	gtk_box_pack_start(GTK_BOX(hbox3), entry35, TRUE, TRUE, 0);

	hbox4 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox4);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox4", hbox4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox4);
	gtk_table_attach(GTK_TABLE(table1), hbox4, 1, 2, 2, 3,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	CoordXB1 = gtk_entry_new();
	gtk_widget_ref(CoordXB1);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordXB1", CoordXB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB1);
	gtk_box_pack_start(GTK_BOX(hbox4), CoordXB1, TRUE, TRUE, 0);

	CoordYB1 = gtk_entry_new();
	gtk_widget_ref(CoordYB1);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordYB1", CoordYB1,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB1);
	gtk_box_pack_start(GTK_BOX(hbox4), CoordYB1, TRUE, TRUE, 0);

	hbox5 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox5);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox5", hbox5,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox5);
	gtk_table_attach(GTK_TABLE(table1), hbox5, 1, 2, 3, 4,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	entry24 = gtk_entry_new();
	gtk_widget_ref(entry24);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry24", entry24,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry24);
	gtk_box_pack_start(GTK_BOX(hbox5), entry24, TRUE, TRUE, 0);

	label36 = gtk_label_new(("Distance"));
	gtk_widget_ref(label36);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label36", label36,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label36);
	gtk_box_pack_start(GTK_BOX(hbox5), label36, FALSE, FALSE, 0);

	entry33 = gtk_entry_new();
	gtk_widget_ref(entry33);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry33", entry33,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry33);
	gtk_box_pack_start(GTK_BOX(hbox5), entry33, TRUE, TRUE, 0);

	hbox6 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox6);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox6", hbox6,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox6);
	gtk_table_attach(GTK_TABLE(table1), hbox6, 1, 2, 4, 5,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	CoordXB2 = gtk_entry_new();
	gtk_widget_ref(CoordXB2);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordXB2", CoordXB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXB2);
	gtk_box_pack_start(GTK_BOX(hbox6), CoordXB2, TRUE, TRUE, 0);

	CoordYB2 = gtk_entry_new();
	gtk_widget_ref(CoordYB2);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordYB2", CoordYB2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYB2);
	gtk_box_pack_start(GTK_BOX(hbox6), CoordYB2, TRUE, TRUE, 0);

	hbox7 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox7);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox7", hbox7,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox7);
	gtk_table_attach(GTK_TABLE(table1), hbox7, 1, 2, 5, 6,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	entry26 = gtk_entry_new();
	gtk_widget_ref(entry26);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry26", entry26,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry26);
	gtk_box_pack_start(GTK_BOX(hbox7), entry26, TRUE, TRUE, 0);

	label37 = gtk_label_new(("Distance"));
	gtk_widget_ref(label37);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label37", label37,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label37);
	gtk_box_pack_start(GTK_BOX(hbox7), label37, FALSE, FALSE, 0);

	entry31 = gtk_entry_new();
	gtk_widget_ref(entry31);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry31", entry31,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry31);
	gtk_box_pack_start(GTK_BOX(hbox7), entry31, TRUE, TRUE, 0);

	hbox8 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox8);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox8", hbox8,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox8);
	gtk_table_attach(GTK_TABLE(table1), hbox8, 1, 2, 6, 7,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	CoordXArv = gtk_entry_new();
	gtk_widget_ref(CoordXArv);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordXArv", CoordXArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordXArv);
	gtk_box_pack_start(GTK_BOX(hbox8), CoordXArv, TRUE, TRUE, 0);

	CoordYArv = gtk_entry_new();
	gtk_widget_ref(CoordYArv);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "CoordYArv", CoordYArv,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(CoordYArv);
	gtk_box_pack_start(GTK_BOX(hbox8), CoordYArv, TRUE, TRUE, 0);

	hbox9 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox9);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox9", hbox9,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox9);
	gtk_table_attach(GTK_TABLE(table1), hbox9, 1, 2, 7, 8,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	entry28 = gtk_entry_new();
	gtk_widget_ref(entry28);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry28", entry28,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry28);
	gtk_box_pack_start(GTK_BOX(hbox9), entry28, TRUE, TRUE, 0);

	label38 = gtk_label_new(("Distance"));
	gtk_widget_ref(label38);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label38", label38,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label38);
	gtk_box_pack_start(GTK_BOX(hbox9), label38, FALSE, FALSE, 0);

	entry29 = gtk_entry_new();
	gtk_widget_ref(entry29);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "entry29", entry29,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(entry29);
	gtk_box_pack_start(GTK_BOX(hbox9), entry29, TRUE, TRUE, 0);

	hbox10 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox10);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox10", hbox10,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox10);
	gtk_table_attach(GTK_TABLE(table1), hbox10, 1, 2, 8, 9,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	DistB1_B2 = gtk_entry_new();
	gtk_widget_ref(DistB1_B2);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "DistB1_B2", DistB1_B2,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB1_B2);
	gtk_box_pack_start(GTK_BOX(hbox10), DistB1_B2, TRUE, TRUE, 0);

	label39 = gtk_label_new(("km"));
	gtk_widget_ref(label39);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label39", label39,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label39);
	gtk_box_pack_start(GTK_BOX(hbox10), label39, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label39), 10, 0);

	hbox11 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox11);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox11", hbox11,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox11);
	gtk_table_attach(GTK_TABLE(table1), hbox11, 1, 2, 9, 10,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	DistB2_B3 = gtk_entry_new();
	gtk_widget_ref(DistB2_B3);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "DistB2_B3", DistB2_B3,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB2_B3);
	gtk_box_pack_start(GTK_BOX(hbox11), DistB2_B3, TRUE, TRUE, 0);

	label40 = gtk_label_new(("km"));
	gtk_widget_ref(label40);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label40", label40,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label40);
	gtk_box_pack_start(GTK_BOX(hbox11), label40, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label40), 10, 0);

	hbox12 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox12);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox12", hbox12,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox12);
	gtk_table_attach(GTK_TABLE(table1), hbox12, 1, 2, 10, 11,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	DistB3_B4 = gtk_entry_new();
	gtk_widget_ref(DistB3_B4);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "DistB3_B4", DistB3_B4,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistB3_B4);
	gtk_box_pack_start(GTK_BOX(hbox12), DistB3_B4, TRUE, TRUE, 0);

	label41 = gtk_label_new(("km"));
	gtk_widget_ref(label41);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label41", label41,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label41);
	gtk_box_pack_start(GTK_BOX(hbox12), label41, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label41), 10, 0);

	hbox13 = gtk_hbox_new(FALSE, 0);
	gtk_widget_ref(hbox13);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "hbox13", hbox13,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(hbox13);
	gtk_table_attach(GTK_TABLE(table1), hbox13, 1, 2, 11, 12,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

	DistTotale = gtk_entry_new();
	gtk_widget_ref(DistTotale);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "DistTotale", DistTotale,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(DistTotale);
	gtk_box_pack_start(GTK_BOX(hbox13), DistTotale, TRUE, TRUE, 0);

	label42 = gtk_label_new(("km"));
	gtk_widget_ref(label42);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label42", label42,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label42);
	gtk_box_pack_start(GTK_BOX(hbox13), label42, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label42), 10, 0);

	Points = gtk_entry_new();
	gtk_widget_ref(Points);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "Points", Points,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Points);
	gtk_table_attach(GTK_TABLE(table1), Points, 1, 2, 13, 14,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label21 = gtk_label_new(("Depart"));
	gtk_widget_ref(label21);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label21", label21,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label21);
	gtk_table_attach(GTK_TABLE(table1), label21, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label22 = gtk_label_new(("Balise la plus proche"));
	gtk_widget_ref(label22);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label22", label22,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label22);
	gtk_table_attach(GTK_TABLE(table1), label22, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);
	gtk_misc_set_padding(GTK_MISC(label22), 5, 0);

	label23 = gtk_label_new(("B1"));
	gtk_widget_ref(label23);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label23", label23,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label23);
	gtk_table_attach(GTK_TABLE(table1), label23, 0, 1, 2, 3,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label24 = gtk_label_new(("Balise la plus proche"));
	gtk_widget_ref(label24);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label24", label24,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label24);
	gtk_table_attach(GTK_TABLE(table1), label24, 0, 1, 3, 4,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label25 = gtk_label_new(("B2"));
	gtk_widget_ref(label25);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label25", label25,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label25);
	gtk_table_attach(GTK_TABLE(table1), label25, 0, 1, 4, 5,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label26 = gtk_label_new(("Balise la plus proche"));
	gtk_widget_ref(label26);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label26", label26,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label26);
	gtk_table_attach(GTK_TABLE(table1), label26, 0, 1, 5, 6,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label27 = gtk_label_new(("Arrivee"));
	gtk_widget_ref(label27);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label27", label27,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label27);
	gtk_table_attach(GTK_TABLE(table1), label27, 0, 1, 6, 7,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label28 = gtk_label_new(("Balise la plus proche"));
	gtk_widget_ref(label28);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label28", label28,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label28);
	gtk_table_attach(GTK_TABLE(table1), label28, 0, 1, 7, 8,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label29 = gtk_label_new(("Distance BD-B1"));
	gtk_widget_ref(label29);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label29", label29,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label29);
	gtk_table_attach(GTK_TABLE(table1), label29, 0, 1, 8, 9,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label30 = gtk_label_new(("Distance B1-B2"));
	gtk_widget_ref(label30);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label30", label30,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label30);
	gtk_table_attach(GTK_TABLE(table1), label30, 0, 1, 9, 10,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label31 = gtk_label_new(("Distance B2-BA"));
	gtk_widget_ref(label31);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label31", label31,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label31);
	gtk_table_attach(GTK_TABLE(table1), label31, 0, 1, 10, 11,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label32 = gtk_label_new(("Distance Parcourue"));
	gtk_widget_ref(label32);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label32", label32,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label32);
	gtk_table_attach(GTK_TABLE(table1), label32, 0, 1, 11, 12,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label33 = gtk_label_new(("Coefficient"));
	gtk_widget_ref(label33);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label33", label33,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label33);
	gtk_table_attach(GTK_TABLE(table1), label33, 0, 1, 12, 13,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label34 = gtk_label_new(("Points realises"));
	gtk_widget_ref(label34);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label34", label34,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label34);
	gtk_table_attach(GTK_TABLE(table1), label34, 0, 1, 13, 14,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	label43 = gtk_label_new(("1.0"));
	gtk_widget_ref(label43);
	gtk_object_set_data_full(GTK_OBJECT(DL2CWindow), "label43", label43,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(label43);
	gtk_table_attach(GTK_TABLE(table1), label43, 1, 2, 12, 13,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);

	return DL2CWindow;
}



void
SaveTrackWindow(GtkWidget *Window)
{
	GtkFileSelection *Dialog;
	GtkWidget *Frame;
	GtkWidget *Table;
	GtkWidget *COMLabel;
	GtkWidget *GPSLabel;
	GtkWidget *GPS;
	GtkWidget *GPS_menu;
	GtkWidget *glade_menuitem;
	GtkWidget *Port;
	GtkWidget *Port_menu;

	Dialog = (GtkFileSelection *)gtk_file_selection_new("Sauvegardez le fichier trace");

/********************* boutons ajoutes dans la fenetre de dialogue **********************/
	Frame = gtk_frame_new(NULL);
	gtk_widget_ref(Frame);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "Frame", Frame,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Frame);
	gtk_container_add(GTK_CONTAINER(Dialog), Frame);
	gtk_widget_set_usize(Frame, 162, -2);
	gtk_container_set_border_width(GTK_CONTAINER(Frame), 4);

	Table = gtk_table_new(2, 2, FALSE);
	gtk_widget_ref(Table);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "Table", Table,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Table);
	gtk_container_add(GTK_CONTAINER(Frame), Table);
	gtk_container_set_border_width(GTK_CONTAINER(Table), 4);
	gtk_table_set_row_spacings(GTK_TABLE(Table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(Table), 8);

	COMLabel = gtk_label_new("Port serie");
	gtk_widget_ref(COMLabel);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "COMLabel", COMLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(COMLabel);
	gtk_table_attach(GTK_TABLE(Table), COMLabel, 0, 1, 0, 1,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(COMLabel), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap(GTK_LABEL(COMLabel), TRUE);
	gtk_misc_set_alignment(GTK_MISC(COMLabel), 0, 0.5);

	GPSLabel = gtk_label_new("GPS");
	gtk_widget_ref(GPSLabel);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "GPSLabel", GPSLabel,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(GPSLabel);
	gtk_table_attach(GTK_TABLE(Table), GPSLabel, 0, 1, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(GPSLabel), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap(GTK_LABEL(GPSLabel), TRUE);
	gtk_misc_set_alignment(GTK_MISC(GPSLabel), 0, 0.5);

	GPS = gtk_option_menu_new();
	gtk_widget_ref(GPS);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "GPS", GPS,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(GPS);
	gtk_table_attach(GTK_TABLE(Table), GPS, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);
	GPS_menu = gtk_menu_new();
	glade_menuitem = gtk_menu_item_new_with_label("Garmin");
	gtk_widget_show(glade_menuitem);
	gtk_menu_append(GTK_MENU(GPS_menu), glade_menuitem);
	glade_menuitem = gtk_menu_item_new_with_label("Magellan");
	gtk_widget_show(glade_menuitem);
	gtk_menu_append(GTK_MENU(GPS_menu), glade_menuitem);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(GPS), GPS_menu);

	Port = gtk_option_menu_new();
	gtk_widget_ref(Port);
	gtk_object_set_data_full(GTK_OBJECT(Dialog), "Port", Port,
				 (GtkDestroyNotify)gtk_widget_unref);
	gtk_widget_show(Port);
	gtk_table_attach(GTK_TABLE(Table), Port, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions)(0), 0, 0);
	Port_menu = gtk_menu_new();
	glade_menuitem = gtk_menu_item_new_with_label("COM1");
	gtk_widget_show(glade_menuitem);
	gtk_menu_append(GTK_MENU(Port_menu), glade_menuitem);
	glade_menuitem = gtk_menu_item_new_with_label("COM2");
	gtk_widget_show(glade_menuitem);
	gtk_menu_append(GTK_MENU(Port_menu), glade_menuitem);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(Port), Port_menu);
/****************************************************************************************/

	gtk_box_pack_start(GTK_BOX(Dialog->main_vbox), Frame, TRUE, TRUE, 0);






	/* on attache le dialogue a la fenetre principale */
	gtk_object_set_data(GTK_OBJECT(Window), "Dialog", Dialog);

	gtk_signal_connect_object(GTK_OBJECT(Dialog->ok_button), "clicked",
				  (GtkSignalFunc)gtk_widget_hide,
				  GTK_OBJECT(Dialog));

	gtk_signal_connect_object_after(GTK_OBJECT(Dialog->ok_button),
					"clicked",
					(GtkSignalFunc)LaunchReadTrack, GTK_OBJECT(Window));



	gtk_signal_connect_object(GTK_OBJECT(Dialog->cancel_button), "clicked",
				  (GtkSignalFunc)gtk_widget_destroy,
				  GTK_OBJECT(Dialog));

	gtk_window_set_modal(GTK_WINDOW(Dialog), TRUE);

	gtk_widget_show(GTK_WIDGET(Dialog));
}
