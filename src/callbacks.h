#include <gtk/gtk.h>

typedef struct _Balise {
	char		Name[256];
	double		X;
	double		Y;
	double		PlaneX;
	double		PlaneY;
	struct _Balise *Next;
} BaliseStruct;



void  GWarning(char *Message);

void
on_MenuOpen_activate(GtkWidget *Window);


void
on_auto_activate(GtkWidget *Window);

void
on_distance_libre_activate(GtkWidget *Window);

void
on_distance_libre_avec_contournement_activate(GtkWidget *Window);

void
on_aller_retour_activate(GtkWidget *Window);

void
on_triangle_activate(GtkWidget *Window);

void
on_triangle_fai_activate(GtkWidget *Window);

void
on_quadrilatere_activate(GtkWidget *Window);

void
ReadFile(FILE *Input, int TrackNumber, GtkWidget *Window);

void
OpenFile(GtkWidget *Window);

void
on_MenuOpenBalise_activate(GtkWidget *Window);

double
SplineEval(int n, double *y, double *Coef, double val);

/* void */
/* SelectTrack (GtkWidget * TrackWindow); */

/* void */
/* ChooseTrack (GtkWidget * TrackWindow); */

/* void */
/* SelectPort (GtkWidget * TrackWindow); */
/* void */
/* SelectTrackFromGPS (GtkWidget * TrackWindow); */
void
AnimeWaitWindow(GtkWidget *Window);

void
UpdateWaitWindow(GtkWidget *WaitWindow);

BaliseStruct *
FindClosestBalise(int Waypoint, double *Dist);

void
on_autopara_activate(GtkWidget *Window);

void
on_distance_libre_avec_deux_contournement_activate(GtkWidget *Window);


void
on_trianglepara_activate(GtkWidget *Window);
void
on_trianglepara_fai_activate(GtkWidget *Window);
void
on_aller_retourpara_activate(GtkWidget *Window);
void
on_quadrilaterepara_activate(GtkWidget *Window);

void LaunchReadTrack(GtkWidget *Window);
