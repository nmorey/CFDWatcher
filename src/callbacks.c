#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string.h>
#include <malloc.h>
#define EARTH 6400
#define EARTHSIZE 40000
#define DELTA 0.15
#define FAI_DELTA 0.28
#define DELTA_MIN 10
#define FAI_COEFF 1.5
#define FAI_PARA_COEFF 1.4
#define DISTMIN 3.0
#define VMAX 40
#define MAXWPT 8000
#define STEPMAX 100
#define TAB(i, j) ((i > j)?(double)Tab[j][i - (j) - 1]:((i) == j)?-1:(double)Tab[i][j - (i) - 1])
#define WRITETAB(i, j) Tab[i][j - (i) - 1]
#ifndef MAX
#define MAX(i, j) (i < j)?j:i
#endif
#ifndef MIN
#define MIN(i, j) (i < j)?i:j
#endif
#define REVERT(i) ((i > 0)?i:(-(i + 1)))
#define BUFFERSIZE 256

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

double x[MAXWPT];
double y[MAXWPT];
double coordx[MAXWPT];
double coefx[MAXWPT];
double coordy[MAXWPT];
double coefy[MAXWPT];
int TableX[7] = { -1, -1, -1, -1, -1, -1, -1 };
int TableBorne[5] = { -1, -1, -1, -1, -1 };
int DepArv[3] = { -1, -1, -1 };
int DrawWpt[5] = { -1, -1, -1, -1, -1 };
double **Tab;
int Step;

/* Structure pour les points de bouclages*/
typedef struct _LocMin {
	int		X;
	int		Y;
	struct _LocMin *Next;
} LocMinStruct;


BaliseStruct *FirstBalise = 0;

LocMinStruct *First = 0;
double Xmid = 0;
double Ymid = 0;

int Waypoints = 0;
int NbrWpt = 0;
double BestDL = 0;
double BestDLC = 0;
double BestDL2C = 0;
double BestAR = 0;
double BestTR = 0;
double BestFAI = 0;
double BestTRPara = 0;
double BestFAIPara = 0;
double BestQDRLT = 0;
int DLWpt1, DLWpt2;
int DLCWpt1, DLCWpt2, DLCWpt3;
double DLCDist1, DLCDist2;
int ARWpt1, ARWpt2, ARDprt, ARArv;
int TRWpt1, TRWpt2, TRWpt3, TRDprt, TRArv;
double TRDist1, TRDist2, TRDist3;
int FAIWpt1, FAIWpt2, FAIWpt3, FAIDprt, FAIArv;
double FAIDist1, FAIDist2, FAIDist3;
int QDRLTWpt1, QDRLTWpt2, QDRLTWpt3, QDRLTWpt4, QDRLTDprt, QDRLTArv;
double QDRLTDist1, QDRLTDist2, QDRLTDist3, QDRLTDist4;


int TRParaWpt1, TRParaWpt2, TRParaWpt3, TRParaDprt, TRParaArv;
double TRParaDist1, TRParaDist2, TRParaDist3;
int FAIParaWpt1, FAIParaWpt2, FAIParaWpt3, FAIParaDprt, FAIParaArv;
double FAIParaDist1, FAIParaDist2, FAIParaDist3;
int DL2CDprt, DL2CWpt1, DL2CWpt2, DL2CArv;
double DL2CDist1, DL2CDist2, DL2CDist3;

/* ####################################################################### */
/* ########################### OUTILS #################################### */
/* ####################################################################### */

/* SplineSolve definit un tableau permettant de definir une courbe lisee passant pas tout les points */
void
SplineSolve(int n, double *y, double *Coef)
{                               /* calcul des coefficients de la courbe spline qui approxime les n points du tableau y
	                         * les coefficients sont ranges dans la table Coef qui doit etre allouee avec une taille
	                         * de n */
	double p, *u;
	gint i, k;

	u = g_malloc((n - 1) * sizeof(u[0]));

	Coef[0] = u[0] = 0.0;

	for (i = 1; i < n - 1; ++i) {
		p = Coef[i - 1] / 2 + 2.0;
		Coef[i] = -0.5 / p;
		u[i] = (double)(y[i + 1] - y[i]) - (double)(y[i] - y[i - 1]);
		u[i] = (3.0 * u[i] - 0.5 * u[i - 1]) / p;
	}

	Coef[n - 1] = 0.0;
	for (k = n - 2; k >= 0; --k)
		Coef[k] = Coef[k] * Coef[k + 1] + u[k];

	g_free(u);
}

/*SplineEval est appeler pour obtenir l'image d'un point par la fonction donne par le SplineSolve */
double
SplineEval(int n, double *y, double *Coef, double val)
{                               /* calcul d'une valeur intermediaire sur la courbe definie par les n points du tableau y,
	                         * les coefficients Coef ont du etre initialises par SplineSolve */
	gint k_lo, k_hi, k;
	double h, b, a;

	k_lo = 0;
	k_hi = n - 1;
	while (k_hi - k_lo > 1) {
		k = (k_hi + k_lo) / 2;
		if (k > val)
			k_hi = k;
		else
			k_lo = k;
	}

	h = (double)(k_hi - k_lo);
	g_assert(h > 0.0);

	a = (double)(k_hi - val) / h;
	b = (double)(val - k_lo) / h;
	return a * y[k_lo] + b * y[k_hi] +
	       ((a * a * a - a) * Coef[k_lo] +
		(b * b * b - b) * Coef[k_hi]) * (h * h) / 6.0;
}

/* Fenetre Graphique pour les messages d'erreurs */


void
GWarning(char *Message)
{
	GtkWidget *dialog, *hbox, *label, *button, *bbox;

	dialog = gtk_dialog_new();
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 20);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
			   TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(Message);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 10);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), bbox,
			   FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	button = gtk_button_new_with_label("OK");
	gtk_container_add(GTK_CONTAINER(bbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  GTK_OBJECT(dialog));

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_widget_show(dialog);
	gdk_window_raise(dialog->window);
}


/** met a jour la barre d'une fenetre d'attente. La fonction doit etre appelee regulierement */
void
UpdateWaitWindow(GtkWidget *WaitWindow)
{
	gfloat Valeur;
	GtkAdjustment *Adjust;
	GtkWidget *ProgressBar;

	ProgressBar = gtk_object_get_data((GtkObject *)WaitWindow, "ProgressBar");
	Adjust = (GtkAdjustment *)gtk_object_get_data((GtkObject *)WaitWindow, "Adjust");

	Valeur = Adjust->value + 1;
	if (Valeur > Adjust->upper)
		Valeur = Adjust->lower;
	gtk_progress_set_value(GTK_PROGRESS(ProgressBar), Valeur);
}

/* ####################################################################### */
/* ##################### OUVERTURE DES FICHIERS ########################## */
/* ####################################################################### */


/* Procedure recursive: Elle passe en negatif tout les distances d'une zone de point de bouclage */
/* Et elle cherche les points de cette zone tel que le nombre de point de la boucle soit el plus grand possible*/
int Marquer(int i, int j, int SizeMax, LocMinStruct *New)
{
	int Size;

	//printf("Marquer %d %d %d New:%d\n",i,j,NbrWpt,New);
	fflush(stdout);
	while (gtk_events_pending())
		gtk_main_iteration();
	if (REVERT(i - j) > SizeMax) {
		SizeMax = REVERT(i - j);
		New->X = i;
		New->Y = j;
	}

	WRITETAB(i, j) = -(TAB(i, j)) - 1;

	if ((j + 1) < NbrWpt) {
		if (((TAB(i, (j + 1))) > 0) && (TAB(i, (j + 1)) <= DISTMIN)) {
			Size = Marquer(i, j + 1, SizeMax, New);
			if (Size > SizeMax)
				SizeMax = Size;
		}
	}

	if (j > 0) {
		if (((TAB(i, (j - 1))) > 0) && (TAB(i, (j - 1)) <= DISTMIN)) {
			Size = Marquer(i, j - 1, SizeMax, New);
			if (Size > SizeMax)
				SizeMax = Size;
		}
	}


	if ((i + 1) < NbrWpt) {
		if (((TAB((i + 1), j)) > 0) && (TAB((i + 1), j) <= DISTMIN)) {
			Size = Marquer(i + 1, j, SizeMax, New);
			if (Size > SizeMax)
				SizeMax = Size;
		}
	}

	if (i > 0) {
		if (((TAB((i - 1), j)) > 0) && (TAB((i - 1), j) <= DISTMIN)) {
			Size = Marquer(i - 1, j, SizeMax, New);
			if (Size > SizeMax)
				SizeMax = Size;
		}
	}

	return SizeMax;
}

/* RollDown cherche le minimum absolu du tableau entre 0 et 3, tant que tout les points de bouclages n'ont pas ete trouves */
void RollDown(void)
{
	int i, j, Mark;
	LocMinStruct *Point = 0;
	LocMinStruct *Current = 0;

	First = 0;

	for (i = 0; i < NbrWpt - 1; i++) {
		while (gtk_events_pending())
			gtk_main_iteration();
		for (j = NbrWpt - 1; j > i; j--) {
			if ((TAB(i, j) < DISTMIN) && (TAB(i, j) >= 0)) {
				if (!First) {
					First = (LocMinStruct *)malloc(sizeof(LocMinStruct));
					First->X = i;
					First->Y = j;
					First->Next = 0;
					Current = First;
					Marquer(i, j, ABS(i - j), Current);
				} else {
					Mark = 1;
					for (Point = First; Point->Next != 0; Point = Point->Next) {
						if ((Point->X <= i) && (Point->Y >= j)) {
							Mark = 0;
							break;
						}
					}
					if (Mark) {
						Current->Next = (LocMinStruct *)malloc(sizeof(LocMinStruct));
						Current->Next->X = i;
						Current->Next->Y = j;
						Current->Next->Next = 0;
						Current = Current->Next;
						//printf("Current: %d Current->Next: %d\n",Current,Current->Next);
						Marquer(i, j, ABS(i - j), Current);
					}
				}
			}
		}
	}
}



/* Procedure de creation de la table des distance entre les waypoints */
void
MakeTable()
{
	int i, j;

	Tab = malloc(NbrWpt * sizeof(double *));
	for (i = 0; i < NbrWpt - 1; i++) {
		while (gtk_events_pending())
			gtk_main_iteration();
		Tab[i] = malloc((NbrWpt - i - 1) * sizeof(double));
		for (j = i + 1; j < NbrWpt; j++) {
			WRITETAB(i, j) =
				(double)sqrt((coordx[j] - coordx[i]) * (coordx[j] - coordx[i]) +
					     (coordy[j] - coordy[i]) * (coordy[j] - coordy[i]));
		}
	}

	/* On passe en negatif, tout les points consecutif a une distance de moins de 3km pour qu'ils ne soient pas considerer comme des points de bouclage */

	for (i = 0; i < NbrWpt - 1; i++) {
		j = 0;
		while ((Tab[i][j]) < DISTMIN) {
			while (gtk_events_pending())
				gtk_main_iteration();
			if (j < (NbrWpt - 2 - i)) {
				Tab[i][j] = -Tab[i][j] - 1;
				j++;
			} else {
				break;
			}
		}
	}
}

void CalculateBalise(GtkWidget *Window)
{
	BaliseStruct *Balise;

	for (Balise = FirstBalise; Balise; Balise = Balise->Next) {
		while (gtk_events_pending())
			gtk_main_iteration();
		Balise->PlaneX = EARTHSIZE * cos(Balise->Y * M_PI / 180) * (Balise->X - Xmid) / 360;
		Balise->PlaneY = EARTHSIZE * (Balise->Y - Ymid) / 360;
	}
	Fit(Window);
}

void OpenBalise(GtkWidget *Window)
{
	GtkFileSelection *Dialog;
	char Name[256];
	FILE *Input = 0;
	BaliseStruct *NewBalise;
	BaliseStruct *Temp;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = CreateWaitWindow("Chargement des balises en cours...");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	for (NewBalise = FirstBalise; NewBalise; NewBalise = Temp) {
		Temp = NewBalise->Next;
		free(NewBalise);
	}
	while (gtk_events_pending())
		gtk_main_iteration();
	FirstBalise = 0;
	NewBalise = 0;
	Dialog = gtk_object_get_data(GTK_OBJECT(Window), "Dialog");
	strcpy(Name, gtk_file_selection_get_filename(Dialog));
	Input = fopen(Name, "rt");
	if (Input == 0) {
		GWarning("erreur lors de la lecture du fichier!");
		return;
	}

	while (1) {
		while (gtk_events_pending())
			gtk_main_iteration();
		NewBalise = malloc(sizeof(BaliseStruct));
		if (fscanf(Input, "%lf %lf %[^\n]s", &NewBalise->Y, &NewBalise->X, NewBalise->Name) != 3) {
			if (!feof(Input))
				GWarning("Format du fichier incorrect!");
				//Ajouter un clean
			free(NewBalise);
			if ((Xmid != 0) || (Ymid != 0))
				CalculateBalise(Window);
			gtk_timeout_remove(Timer);
			gtk_widget_destroy(LoadWindow);
			return;
		}
		NewBalise->Next = FirstBalise;
		FirstBalise = NewBalise;
	}
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	return;
}






void
OpenFile(GtkWidget *Window)
{
	GtkFileSelection *Dialog;

	char Message[256];
	char Name[256];
	char Buffer[BUFFERSIZE];
	int i, k;

	GtkWidget *LoadWindow;
	guint Timer;
	FILE *Input = 0;
	LocMinStruct *Pointer;
	LocMinStruct *NextPointer;

	double Xmin, Xmax, Ymin, Ymax;
	int Time, Time1, Cpt;
	ContextStruct *Context = 0;



	LoadWindow = (GtkWidget *)CreateWaitWindow("Ouverture du fichier en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);







	Xmin = Xmax = Ymin = Ymax = 0;
	Xmid = 0;
	Ymid = 0;
	Context = gtk_object_get_user_data((GtkObject *)Window);

	/* On reset toutes les variables */
	if (NbrWpt > 0) {
		for (i = 0; i < NbrWpt - 1; i++) {
			while (gtk_events_pending())
				gtk_main_iteration();
			free(Tab[i]);
		}
		free(Tab);
		while (gtk_events_pending())
			gtk_main_iteration();
	}


	DepArv[0] = -1;
	DepArv[1] = -1;
	DepArv[2] = -1;
	DrawWpt[0] = -1;
	DrawWpt[1] = -1;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	DrawWpt[4] = -1;
	Step = 0;
	BestDL = BestDLC = BestTR = BestQDRLT = BestFAI = BestAR = BestTRPara = BestFAIPara = BestDL2C = 0;
	NbrWpt = 0;

	for (i = 0; i < ARRAY_SIZE(TableX); i++)
		TableX[i] = -1;


	for (i = 0; i < ARRAY_SIZE(TableBorne); i++)
		TableBorne[i] = -1;

	Waypoints = 0;
	Cpt = 0;





	/* On recupere le nom du fichier a ouvrir et on ferme les eventuels fichiers en cours d'edition */
	Dialog = gtk_object_get_data(GTK_OBJECT(Window), "Dialog");
	strcpy(Name, gtk_file_selection_get_filename(Dialog));
	Input = fopen(Name, "rt");

	if (!Input) {
		sprintf(Message, "ERROR : unable to open file %s\n", Name);
		GWarning(Message);
		goto error;
	}


	/* Tant qu'on est endessous du nombre de Waypoints et que le fichier n'est pas terminer, on lit. */
	/* On cherche le max et le min en X et en Y pour definir le milieu du vol */
	while ((!feof(Input)) && (NbrWpt <= MAXWPT)) {
		fgets(Buffer, BUFFERSIZE, Input);


		while (gtk_events_pending())
			gtk_main_iteration();

		/* On recupere les 2 premiers points pour definir le temps qui les separe
		 * et pouvoir calculer le pas de recherche dans les autres procedures */
		if ((Buffer[0] == 'B')) {
			// printf("Waypoints num:%d\n",NbrWpt);
			if (NbrWpt == 0) {
				sscanf(Buffer, "B%6d%7lf%*c%8lf%*c%*s", &Time, &y[0], &x[0]);
				x[0] = ((double)x[0]) / 100000;
				x[0] = x[0] * (1 / 0.60) - floor(x[0]) * 0.4 / 0.60;
				y[0] = ((double)y[0]) / 100000;
				y[0] = y[0] * (1 / 0.60) - floor(y[0]) * 0.4 / 0.60;
				Xmin = Xmax = x[0];
				Ymin = Ymax = y[0];
			} else if (NbrWpt == 1) {
				sscanf(Buffer, "B%6d%7lf%*c%8lf%*c%*s", &Time1, &y[1], &x[1]);
				x[1] = ((double)x[1]) / 100000;
				x[1] = x[1] * (1 / 0.60) - floor(x[1]) * 0.4 / 0.60;
				y[1] = ((double)y[1]) / 100000;
				y[1] = y[1] * (1 / 0.60) - floor(y[1]) * 0.4 / 0.60;

				if (Time == Time1) {
					GWarning("Echelle de temps erronee");
					goto error;
				}
			} else {
				sscanf(Buffer, "B%*6d%7lf%*c%8lf%*c%*s", &y[NbrWpt], &x[NbrWpt]);

				x[NbrWpt] = ((double)x[NbrWpt]) / 100000;
				x[NbrWpt] = x[NbrWpt] * (1 / 0.60) - floor(x[NbrWpt]) * 0.4 / 0.60;
				y[NbrWpt] = ((double)y[NbrWpt]) / 100000;
				y[NbrWpt] = y[NbrWpt] * (1 / 0.60) - floor(y[NbrWpt]) * 0.4 / 0.60;
				//	printf("x=%lf , y=%lf\n----\n",x[NbrWpt],y[NbrWpt]);

				if (x[Waypoints] < Xmin)
					Xmin = x[Waypoints];
				else if (x[Waypoints] > Xmax)
					Xmax = x[Waypoints];
				if (y[Waypoints] < Ymin)
					Ymin = y[Waypoints];
				else if (y[Waypoints] > Ymax)
					Ymax = y[Waypoints];
			}
			NbrWpt = ++Waypoints;
		}
	}

	if (Waypoints == 0) {
	    GWarning("Could not load any waypoints. Probably wrong file format");
	    goto error;
	}
	Xmid = (Xmin + Xmax) / 2;
	Ymid = (Ymin + Ymax) / 2;
	/* On projette les points sur un plan avec Pythagore, en utilisant le milieu calculer plus haut */
	for (k = 0; k < Waypoints; k++) {
		while (gtk_events_pending())
			gtk_main_iteration();
		coordx[k] = EARTHSIZE * cos(y[k] * M_PI / 180) * (x[k] - Xmid) / 360;
		coordy[k] = EARTHSIZE * (y[k] - Ymid) / 360;
/*       printf("%d %lf %lf\n",k,coordx[k],coordy[k]); */
/*        printf("x=%lf , y=%lf\n----\n",x[k],y[k]); */
	}

	/* On definit les coins de la zone a afficher */
	Context->XMin = (EARTHSIZE * cos(Ymin * M_PI / 180) * (Xmin - Xmid) / 360);
	Context->YMin = (EARTHSIZE * (Ymin - Ymid) / 360);
	Context->XMax = (EARTHSIZE * cos(Ymax * M_PI / 180) * (Xmax - Xmid) / 360);
	Context->YMax = (EARTHSIZE * (Ymax - Ymid) / 360);
	//  printf("%d %d %lf %lf %lf %lf\n",Waypoints,NbrWpt,Context->XMin,Context->XMax,Context->YMin,Context->YMax);



	/* On utilise SplineSolve sur les coord X et Y pour obtenir uen courbe lisee */
	SplineSolve(Waypoints, coordx, coefx);
	SplineSolve(Waypoints, coordy, coefy);

	/* On cree la table de distance puis on suprimes les eventuels points de bouclage d'un fichier precedents */
	MakeTable();
	for (Pointer = First; Pointer; Pointer = NextPointer) {
		NextPointer = Pointer->Next;
		free(Pointer);
	}


	while (gtk_events_pending())
		gtk_main_iteration();
	if (FirstBalise != 0)
		CalculateBalise(Window);


	/* On calcule le pas de recherche. On trouve les points de bouclages, et on calcule le zoom ideal pour ajuster le vol a la tailel de la fenetre */
	Step = (int)ceil(ABS(20 / ((double)(Time1 - Time))));
	//  printf("%d %d %d\n",Time,Time1,Step);
	RollDown();

 error:
	Fit(Window);
	fclose(Input);
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	/*   for(k=0;k<Waypoints;k++) */
	/*     printf("%lf %lf\n",coordx[k],coordy[k]); */
	return;
}


/* Procedure appelle par le menu File/Open */
/* Ouvre uen fenetre de dialogue pour selectionner le fichier */
void
on_MenuOpen_activate(GtkWidget *Window)
{
	GtkFileSelection *Dialog;

	Dialog = (GtkFileSelection *)gtk_file_selection_new("Select file");

	/* on attache le dialogue a la fenetre principale */
	gtk_object_set_data(GTK_OBJECT(Window), "Dialog", Dialog);

	gtk_signal_connect_object(GTK_OBJECT(Dialog->ok_button),
				  "clicked",
				  (GtkSignalFunc)OpenFile, GTK_OBJECT(Window));

	gtk_signal_connect_object(GTK_OBJECT(Dialog->ok_button), "clicked",
				  (GtkSignalFunc)gtk_widget_hide,
				  GTK_OBJECT(Dialog));

	gtk_signal_connect_object(GTK_OBJECT(Dialog->cancel_button), "clicked",
				  (GtkSignalFunc)gtk_widget_destroy,
				  GTK_OBJECT(Dialog));

	gtk_window_set_modal(GTK_WINDOW(Dialog), TRUE);
	gtk_widget_show(GTK_WIDGET(Dialog));
}



void
on_MenuOpenBalise_activate(GtkWidget *Window)
{
	GtkFileSelection *Dialog;

	Dialog = (GtkFileSelection *)gtk_file_selection_new("Select file");

	/* on attache le dialogue a la fenetre principale */
	gtk_object_set_data(GTK_OBJECT(Window), "Dialog", Dialog);

	gtk_signal_connect_object(GTK_OBJECT(Dialog->ok_button),
				  "clicked",
				  (GtkSignalFunc)OpenBalise, GTK_OBJECT(Window));

	gtk_signal_connect_object(GTK_OBJECT(Dialog->ok_button), "clicked",
				  (GtkSignalFunc)gtk_widget_hide,
				  GTK_OBJECT(Dialog));

	gtk_signal_connect_object(GTK_OBJECT(Dialog->cancel_button), "clicked",
				  (GtkSignalFunc)gtk_widget_destroy,
				  GTK_OBJECT(Dialog));

	gtk_window_set_modal(GTK_WINDOW(Dialog), TRUE);
	gtk_widget_show(GTK_WIDGET(Dialog));
}


void LaunchReadTrack(GtkWidget *Window)
{
	GWarning("GPS reader disable due to gpsbabel/old libUSB dependency!");
	return;

/*  GtkWidget *LoadWindow; */
/*   guint Timer; */

/*   GtkFileSelection *Dialog; */
/*   FILE* Serie=0; */
/*   char Name[256]; */
/*   char Port[14]; */
/*   char GPS[12]; */
/*   char Progr[9]; */
/*   char Opt1[3],Opt2[3],Opt3[3],Opt4[4],Opt5[3],Opt0[3]; */
/*   char ** Param; */
/*   FILE * Fichier=0; */
/*   int i; */
/*   Dialog = gtk_object_get_data (GTK_OBJECT (Window), "Dialog"); */
/*   pid_t Process=0; */
/*   int Status; */

/*   switch (OptionMenuGetIndex (gtk_object_get_data ((GtkObject*)Dialog,"Port"))) */
/*     { */
/*     case 0: /\* COM1 *\/  */
/*       strcpy(Port,"/dev/ttyS0"); */
/*       break; */
/*     case 1: /\* COM2 *\/  */
/*       strcpy(Port,"/dev/ttyS1"); */
/*       break; */
/*     } */
/*   Serie=fopen(Port,"r"); */
/*   if(!Serie){ */
/*     GWarning("Port serie invalide!"); */
/*     return; */
/*   } */
/*   fclose(Serie); */

/*   switch (OptionMenuGetIndex (gtk_object_get_data ((GtkObject*)Dialog,"GPS"))) */
/*     { */
/*     case 0: /\* GARMIN *\/  */
/*       strcpy(GPS,"garmin"); */
/*       break; */
/*     case 1: /\* MAGELLAN *\/  */
/*       strcpy(GPS,"magellan"); */
/*       break; */
/*       /\*     case 2: *\/ */
/*       /\* COMPEO *\/   */
/*       /\*       strcpy(GPS,); *\/ */
/*       /\*       break; *\/ */
/*     } */




/*   strcpy (Name, gtk_file_selection_get_filename (Dialog)); */
/*   Fichier = fopen(Name,"wt"); */
/*   if(!Fichier){ */
/*     GWarning("Impossible d'ecrire a cet endroit du disque!!"); */
/*     return; */
/*   } */
/*   fclose(Fichier); */
/*   strcpy(Progr,"gpsbabel"); */
/*   strcpy(Opt1,"-i"); */
/*   strcpy(Opt2,"-f"); */
/*   strcpy(Opt3,"-o"); */
/*   strcpy(Opt4,"igc"); */
/*   strcpy(Opt5,"-F"); */
/*   strcpy(Opt0,"-t"); */
/*   gtk_widget_hide(GTK_WIDGET (Dialog)); */
/*   Param=malloc(10*sizeof(char*)); */
/*   Param[0]=Progr; */
/*   Param[1]=Opt0; */
/*   Param[2]=Opt1; */
/*   Param[3]=GPS; */
/*   Param[4]=Opt2; */
/*   Param[5]=Port; */
/*   Param[6]=Opt3; */
/*   Param[7]=Opt4; */
/*   Param[8]=Opt5; */
/*   Param[9]=Name; */
/* /\*   for(i=0;i<10;i++) *\/ */
/* /\*     printf("%s ",Param[i]); *\/ */
/* /\*   printf("\n"); *\/ */
/*   LoadWindow = (GtkWidget *)CreateWaitWindow ("Lecture du GPS en cours.."); */
/*   Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow); */

/*   Process=fork(); */

/*   if(!Process){ */
/*     ReadTrack(10,Param); */
/*     _exit(12); */

/*   } else { */
/*     while(waitpid(Process,&Status,WNOHANG)<=0){ */
/*       while (gtk_events_pending ()) */
/* 	gtk_main_iteration(); */
/*       usleep(50000); */
/*     } */

/*     if(WEXITSTATUS(Status)!=12){ */
/* 	gtk_widget_destroy(GTK_WIDGET (Dialog)); */
/*       GWarning("Erreur de lecture du GPS: Verifiez le type du GPS, et le port de communication!"); */
/*       while (gtk_events_pending ()) */
/* 	gtk_main_iteration(); */
/*   gtk_timeout_remove (Timer); */
/*   gtk_widget_destroy (LoadWindow); */
/*       return; */
/*   } */
/*   gtk_timeout_remove (Timer); */
/*   gtk_widget_destroy (LoadWindow); */
/*     OpenFile(Window); */
/*   } */
}





/* ####################################################################### */
/* ########################### MODE COMMUN #### ########################## */
/* ####################################################################### */


BaliseStruct *
FindClosestBalise(int Waypoint, double *Dist)
{
	BaliseStruct *Balise;
	BaliseStruct *Closest;
	double Distance, Min;

	Min = DISTMIN;
	Closest = 0;
	for (Balise = FirstBalise; Balise; Balise = Balise->Next) {
		while (gtk_events_pending())
			gtk_main_iteration();

		Distance = 0;
		Distance = sqrt(pow((Balise->X - x[Waypoint]) * EARTHSIZE / 360, 2)
				+ pow((Balise->Y - y[Waypoint]) * EARTHSIZE / 360, 2));
		if (Distance < Min) {
			Min = Distance;
			Closest = Balise;
		}
	}
	*Dist = floor(Min * 10) / 10;
	return Closest;
}


int
IsConvexOrCross(int a, int b, int c, int d)
{
	double alpha0, alpha1, alpha2 = 0;
	double a2 = 0;
	double a0 = 0;
	double PI_2;



	PI_2 = M_PI * 2;

	alpha0 = alpha1 = atan2((double)(coordx[a] - coordx[d]), (double)(coordy[a] - coordy[d]));

	alpha2 = atan2((double)(coordx[b] - coordx[a]), (double)(coordy[b] - coordy[a]));

	a0 = alpha2 - alpha1;

	if (a0 > M_PI)
		a0 = a0 - PI_2;
	if (a0 <= -M_PI)
		a0 = a0 + PI_2;
	alpha1 = alpha2;

	alpha2 = atan2((double)(coordx[c] - coordx[b]), (double)(coordy[c] - coordy[b]));

	a2 = alpha2 - alpha1;

	if (a2 > M_PI)
		a2 = a2 - PI_2;
	if (a2 <= -M_PI)
		a2 = a2 + PI_2;

	if ((a2 * a0) < 0)
		return 0;

	a0 = a2;
	alpha1 = alpha2;

	alpha2 = atan2((double)(coordx[d] - coordx[c]), (double)(coordy[d] - coordy[c]));

	a2 = alpha2 - alpha1;

	if (a2 > M_PI)
		a2 = a2 - PI_2;
	if (a2 <= -M_PI)
		a2 = a2 + PI_2;

	if ((a2 * a0) < 0)
		return 0;

	a0 = a2;
	alpha1 = alpha2;



	a2 = alpha0 - alpha1;

	if (a2 > M_PI)
		a2 = a2 - PI_2;
	if (a2 <= -M_PI)
		a2 = a2 + PI_2;

	if ((a2 * a0) < 0)
		return 0;

	return 1;
}


/* Recherche de la meilleure distance libre */
void
FindBestDL()
{
	int i, j;
	double Dist = 0;


	BestDL = 0;
	DLWpt1 = 0;
	DLWpt2 = 0;

	for (i = 0; i <= (NbrWpt - 2); i += Step) {
		while (gtk_events_pending())
			gtk_main_iteration();

		for (j = (i + 1); (j <= NbrWpt - 1); j += Step) {
			Dist = REVERT(TAB(i, j));
			if (Dist > BestDL) {
				BestDL = Dist;
				DLWpt1 = i;
				DLWpt2 = j;
			}
		}
	}
	BestDL = (rint(10 * BestDL)) / 10;
}

/* recherche de la meilleure distance libre avec 1 point de contournement */
void
FindBestDLC()
{
	int i, j, k;
	double Dist1, Dist2, Dist;


	BestDLC = 0;
	DLCWpt1 = 0;
	DLCWpt2 = 0;
	DLCWpt3 = 0;
	DLCDist1 = 0;
	DLCDist2 = 0;
	for (i = 0; i <= (NbrWpt - 3); i += Step * 2) {
		for (j = (i + 1); j <= (NbrWpt - 2); j += Step * 2) {
			while (gtk_events_pending())
				gtk_main_iteration();

			Dist1 = REVERT(TAB(i, j));
			for (k = (j + 1); k <= (NbrWpt - 1); k += Step * 2) {
				Dist2 = REVERT(TAB(j, k));
				Dist = Dist1 + Dist2;
				if (Dist > BestDLC) {
					BestDLC = Dist;
					DLCWpt1 = i;
					DLCWpt2 = j;
					DLCWpt3 = k;
					DLCDist1 = Dist1;
					DLCDist2 = Dist2;
				}
			}
		}
	}
	DLCDist1 = (rint(10 * (DLCDist1))) / 10;
	DLCDist2 = (rint(10 * (DLCDist2))) / 10;
	BestDLC = (rint(10 * (DLCDist1 + DLCDist2))) / 10;
}

/* Recherche du meilleur Quadrilatere, en utilisant les points de boucl;age et un step*4 pour une vitesse d'execution decente */
void
FindBestQDRLT()
{
	int i, j, k, l;
	double Dist1, Dist2, Dist3, Dist4, Dist;
	LocMinStruct *Pointer;

	BestQDRLT = 0;
	QDRLTDprt = 0;
	QDRLTArv = 0;
	QDRLTWpt1 = 0;
	QDRLTWpt2 = 0;
	QDRLTWpt3 = 0;
	QDRLTWpt4 = 0;
	QDRLTDist1 = 0;
	QDRLTDist2 = 0;
	QDRLTDist3 = 0;
	QDRLTDist4 = 0;
	for (Pointer = First; Pointer; Pointer = Pointer->Next) {
		for (i = Pointer->X; i <= (Pointer->Y - 3); i += Step * 4) {
			for (j = (i + 1); j <= (Pointer->Y - 2); j += Step * 4) {
				Dist1 = REVERT(TAB(i, j));
				if (Dist1 >= 0.15 * BestQDRLT) {
					for (k = (j + 1); k <= (Pointer->Y - 1); k += Step * 4) {
						while (gtk_events_pending())
							gtk_main_iteration();

						Dist2 = REVERT(TAB(j, k));
						if (Dist2 >= 0.15 * BestQDRLT) {
							for (l = (k + 1); l <= Pointer->Y; l += Step * 4) {
								Dist3 = REVERT(TAB(k, l));
								Dist4 = REVERT(TAB(i, l));
								Dist = Dist1 + Dist2 + Dist3 + Dist4;
								if ((Dist > BestQDRLT)
								    && ((Dist1 >= (DELTA * Dist))
									&& (Dist2 >= (DELTA * Dist))
									&& (Dist3 >= (DELTA * Dist))
									&& (Dist4 >= (DELTA * Dist))) && IsConvexOrCross(i, j, k, l)) {
									QDRLTDprt = Pointer->X;
									QDRLTArv = Pointer->Y;
									QDRLTWpt1 = i;
									QDRLTWpt2 = j;
									QDRLTWpt3 = k;
									QDRLTWpt4 = l;
									QDRLTDist1 = (rint(10 * Dist1)) / 10;
									QDRLTDist2 = (rint(10 * Dist2)) / 10;
									QDRLTDist3 = (rint(10 * Dist3)) / 10;
									QDRLTDist4 = (rint(10 * Dist4)) / 10;
									BestQDRLT = (rint(10 * (QDRLTDist1 + QDRLTDist2 + QDRLTDist3 + QDRLTDist4))) / 10;
								}
							}
						}
					}
				}
			}
		}
	}
}

/* Recher du meilleur Aller-Retour */
void
FindBestAR()
{
	int i, j;
	double Dist = 0;
	LocMinStruct *Pointer;

	BestAR = 0;
	ARWpt1 = 0;
	ARWpt2 = 0;
	ARDprt = 0;
	ARArv = 0;
	for (Pointer = First; Pointer; Pointer = Pointer->Next) {
		for (i = Pointer->X; i <= ((Pointer->Y) - 1); i += Step * 1) {
			while (gtk_events_pending())
				gtk_main_iteration();

			for (j = (i + 1); (j <= Pointer->Y); j += Step * 1) {
				Dist = 2 * REVERT(TAB(i, j));
				if (Dist > BestAR && Dist > DELTA_MIN) {
					BestAR = Dist;
					ARWpt1 = i;
					ARWpt2 = j;
					ARDprt = Pointer->X;
					ARArv = Pointer->Y;
					break;
				}
			}
		}
	}
}

/* ####################################################################### */
/* ################################ MODE DELTA ########################### */
/* ####################################################################### */
/* Recherche du meilleur Triangle Simple/FAI */
/* Simple pour FAIorNot=0, FAI pour FAIorNot=1*/
/* On utilise la liste des points de bouclages */
/* Contrairement au parapente, un cote de triangle simple doit au moins faire 15% du total */
void
FindBestTR(int FAIorNot)
{
	int i, j, k;
	double Dist1, Dist2, Dist3, Dist;
	LocMinStruct *Pointer;

	if (FAIorNot == 0) {
		TRDprt = 0;
		TRArv = 0;
		TRWpt1 = 0;
		TRWpt2 = 0;
		TRWpt3 = 0;
		TRDist1 = 0;
		TRDist2 = 0;
		TRDist3 = 0;
	}
	if (FAIorNot == 1) {
		FAIDprt = 0;
		FAIArv = 0;
		BestFAI = 0;
		FAIWpt1 = 0;
		FAIWpt2 = 0;
		FAIWpt3 = 0;
		FAIDist1 = 0;
		FAIDist2 = 0;
		FAIDist3 = 0;
	}
	if (FAIorNot == 1)
		BestFAI = 0;
	else
		BestTR = 0;

	for (Pointer = First; Pointer; Pointer = Pointer->Next) {
		for (i = Pointer->X; i <= (Pointer->Y - 2); i += Step * 2) {
			for (j = (i + 1); j <= (Pointer->Y - 1); j += Step * 2) {
				while (gtk_events_pending())
					gtk_main_iteration();

				Dist1 = REVERT(TAB(i, j));
				if (((FAIorNot) && (Dist1 >= 0.28 * BestFAI)) || ((!FAIorNot) && (Dist1 >= 0.15 * BestTR))) {
					for (k = (j + 1); k <= (Pointer->Y); k += Step * 2) {
						;
						Dist2 = REVERT(TAB(j, k));
						Dist3 = REVERT(TAB(i, k));
						Dist = Dist1 + Dist2 + Dist3;
						if ((Dist > DELTA_MIN) && (((FAIorNot == 0) && (Dist > BestTR))
									   || ((FAIorNot == 1) && (Dist > BestFAI)))) {
							if ((Dist1 >= (DELTA * Dist)) && (Dist2 >= (DELTA * Dist))
							    && (Dist3 >= (DELTA * Dist))) {
								if (FAIorNot == 0) {
									TRDprt = Pointer->X;
									TRArv = Pointer->Y;
									BestTR = Dist;
									TRWpt1 = i;
									TRWpt2 = j;
									TRWpt3 = k;
									TRDist1 = Dist1;
									TRDist2 = Dist2;
									TRDist3 = Dist3;
									break;
								} else if ((FAIorNot == 1)
									   && (Dist1 >= (FAI_DELTA * Dist))
									   && (Dist2 >= (FAI_DELTA * Dist))
									   && (Dist3 >= (FAI_DELTA * Dist))) {
									FAIDprt = Pointer->X;
									FAIArv = Pointer->Y;
									BestFAI = Dist;
									FAIWpt1 = i;
									FAIWpt2 = j;
									FAIWpt3 = k;
									FAIDist1 = Dist1;
									FAIDist2 = Dist2;
									FAIDist3 = Dist3;
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	TRDist1 = (rint(10 * TRDist1)) / 10;
	TRDist2 = (rint(10 * TRDist2)) / 10;
	TRDist3 = (rint(10 * TRDist3)) / 10;
	BestTR = (rint(10 * BestTR)) / 10;
	FAIDist1 = (rint(10 * FAIDist1)) / 10;
	FAIDist2 = (rint(10 * FAIDist2)) / 10;
	FAIDist3 = (rint(10 * FAIDist3)) / 10;
	BestFAI = (rint(10 * BestFAI)) / 10;
}









/* ####################################################################### */
/* ############################ MODE PARAPENTE ########################### */
/* ####################################################################### */

/* Recherche de la meilleure distance libre avec 2 points de contournements */
/* Correspond a la recherche basique d'un quadrilatere sasn calculer le 4eme cote */
void
FindBestDL2C()
{
	int i, j, k, l;
	double Dist1, Dist2, Dist3, Dist;


	BestDL2C = 0;
	DL2CDprt = 0;
	DL2CWpt1 = 0;
	DL2CWpt2 = 0;
	DL2CArv = 0;
	DL2CDist1 = 0;
	DL2CDist2 = 0;
	DL2CDist3 = 0;
	for (i = 0; i <= (NbrWpt - 4); i += Step * 4) {
		for (j = (i + 1); j <= (NbrWpt - 3); j += Step * 4) {
			Dist1 = REVERT(TAB(i, j));
			for (k = (j + 1); k <= (NbrWpt - 2); k += Step * 4) {
				while (gtk_events_pending())
					gtk_main_iteration();

				Dist2 = REVERT(TAB(j, k));
				for (l = (k + 1); l <= (NbrWpt - 1); l += Step * 4) {
					Dist3 = REVERT(TAB(k, l));
					Dist = Dist1 + Dist2 + Dist3;;
					if (Dist > BestDL2C) {
						BestDL2C = Dist;
						DL2CDprt = i;
						DL2CWpt1 = j;
						DL2CWpt2 = k;
						DL2CArv = l;
						DL2CDist1 = Dist1;
						DL2CDist2 = Dist2;
						DL2CDist3 = Dist3;
					}
				}
			}
		}
	}
	DL2CDist1 = (rint(10 * (DL2CDist1))) / 10;
	DL2CDist2 = (rint(10 * (DL2CDist2))) / 10;
	DL2CDist3 = (rint(10 * (DL2CDist3))) / 10;
	BestDL2C = (rint(10 * (DL2CDist1 + DL2CDist2 + DL2CDist3))) / 10;
}

/* Recherche du meilleur Triangle Simple/FAI */
/* Simple pour FAIorNot=0, FAI pour FAIorNot=1*/
/* On utilise la liste des points de bouclages */
/* Aucune restriction sur les cotes du triangle simple */
void
FindBestTRPara(int FAIorNot)
{
	int i, j, k;
	double Dist1, Dist2, Dist3, Dist;
	LocMinStruct *Pointer;

	if (FAIorNot == 0) {
		TRParaDprt = 0;
		TRParaArv = 0;
		TRParaWpt1 = 0;
		TRParaWpt2 = 0;
		TRParaWpt3 = 0;
		TRParaDist1 = 0;
		TRParaDist2 = 0;
		TRParaDist3 = 0;
	}
	if (FAIorNot == 1) {
		FAIParaDprt = 0;
		FAIParaArv = 0;
		FAIParaWpt1 = 0;
		FAIParaWpt2 = 0;
		FAIParaWpt3 = 0;
		FAIParaDist1 = 0;
		FAIParaDist2 = 0;
		FAIParaDist3 = 0;
	}
	if (FAIorNot == 1)
		BestFAIPara = 0;
	else
		BestTRPara = 0;

	for (Pointer = First; Pointer; Pointer = Pointer->Next) {
		for (i = Pointer->X; i <= (Pointer->Y - 2); i += Step * 2) {
			for (j = (i + 1); j <= (Pointer->Y - 1); j += Step * 2) {
				while (gtk_events_pending())
					gtk_main_iteration();

				Dist1 = REVERT(TAB(i, j));
				if ((!FAIorNot) || ((FAIorNot) && (Dist1 >= FAI_DELTA * BestFAIPara))) {
					for (k = (j + 1); k <= (Pointer->Y); k += Step * 2) {
						;
						Dist2 = REVERT(TAB(j, k));
						Dist3 = REVERT(TAB(i, k));
						Dist = Dist1 + Dist2 + Dist3;
						if ((Dist > DELTA_MIN) && (((FAIorNot == 0) && (Dist > BestTRPara))
									   || ((FAIorNot == 1) && (Dist > BestFAIPara)))) {
							if (FAIorNot == 0) {
								TRParaDprt = Pointer->X;
								TRParaArv = Pointer->Y;
								BestTRPara = Dist;
								TRParaWpt1 = i;
								TRParaWpt2 = j;
								TRParaWpt3 = k;
								TRParaDist1 = Dist1;
								TRParaDist2 = Dist2;
								TRParaDist3 = Dist3;
								break;
							} else if ((FAIorNot == 1)
								   && (Dist1 >= (FAI_DELTA * Dist))
								   && (Dist2 >= (FAI_DELTA * Dist))
								   && (Dist3 >= (FAI_DELTA * Dist))) {
								FAIParaDprt = Pointer->X;
								FAIParaArv = Pointer->Y;
								BestFAIPara = Dist;
								FAIParaWpt1 = i;
								FAIParaWpt2 = j;
								FAIParaWpt3 = k;
								FAIParaDist1 = Dist1;
								FAIParaDist2 = Dist2;
								FAIParaDist3 = Dist3;
								break;
							}
						}
					}
				}
			}
		}
	}
	TRParaDist1 = (rint(10 * TRParaDist1)) / 10;
	TRParaDist2 = (rint(10 * TRParaDist2)) / 10;
	TRParaDist3 = (rint(10 * TRParaDist3)) / 10;
	BestTRPara = (rint(10 * BestTRPara)) / 10;
	FAIParaDist1 = (rint(10 * FAIParaDist1)) / 10;
	FAIParaDist2 = (rint(10 * FAIParaDist2)) / 10;
	FAIParaDist3 = (rint(10 * FAIParaDist3)) / 10;
	BestFAIPara = (rint(10 * BestFAIPara)) / 10;
}








/* ####################################################################### */
/* ############################## PROCEDURES GPS ######################### */
/* ####################################################################### */







/* ####################################################################### */
/* ##################### FIN DES PROC DE RECHERCHES ###################### */
/* ####################################################################### */


/*Les procedures Delta et Parapente marchent de la meme maniere, seuls les coeffs et les procedures de recherches appeles changent*/

/* Pour les fonctions on_auto(para)_activate */
/* Mode global de recherche, teste si chaque fonction n'a pas ete lancee, et la lance le cas echeant */
/* Puis trouve le max de points obtenus et appelle la procedure adequate */

/* Pour toutes les autres le fonctionnement est toujours le meme: */
/* Si la recherche de circuit n'a pas ete faite, on la lance */
/* En cas d'absence de circuit, on quiite la procedure avec un message d'erreur */
/* On recupere des pointeurs sur les entrees que l'on veut remplir */
/* On definit des chaines de caracteres contenant ce que va contenir l'entree*/
/* On attribue a chaque entree, la chaine qui lui correspond */

/* Enfin on stocke les coordonnes dans les tableaux pour les applications graphiques: */
/* TableX est  utilise pour dessiner le circuit calcules */
/* DepArv pour avoir les coordonnes du depart et de l'arrivee pour pouvoir placer une etiquette */
/* DrawWpt pour les etiquettes de chaque balise */
/* Puis on termien en rafraichissant la zone de dessin */




/* ####################################################################### */
/* ######################## PROCEDURES COMMUNES ########################## */
/* ####################################################################### */

/* Mode distance Libre */
void
on_distance_libre_activate(GtkWidget *Window)
{
	GtkWidget *DistanceLibreWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Distance;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	char Xdep[9], Ydep[9], Xarv[9], Yarv[9], Dstc[6];

	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;
	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;
	char Name1[256], Name2[256], DistanceBalise1[6], DistanceBalise2[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;
	guint Timer;
	int Hauteur;
	GtkWidget *LoadWindow;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'une distane libre en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestDL == 0)
		FindBestDL();

	if (BestDL == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	DistanceLibreWindow = (GtkWidget *)create_DistanceLibreWindow();
	gtk_widget_show(DistanceLibreWindow);
	CoordXDep =
		gtk_object_get_data((GtkObject *)DistanceLibreWindow, "CoordXDep");
	CoordYDep =
		gtk_object_get_data((GtkObject *)DistanceLibreWindow, "CoordYDep");
	CoordXArv =
		gtk_object_get_data((GtkObject *)DistanceLibreWindow, "CoordXArv");
	CoordYArv =
		gtk_object_get_data((GtkObject *)DistanceLibreWindow, "CoordYArv");
	Distance = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "Dist");
	Points = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "Points");

	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "entry31");
		BaliseArrivee = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)DistanceLibreWindow, "entry32");

		BalisePointer = FindClosestBalise(DLWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name1, BalisePointer->Name);
			sprintf(DistanceBalise1, "%lf", DistanceWpt);
		} else {
			sprintf(Name1, "Aucune");
			sprintf(DistanceBalise1, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name1);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise1);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(DLWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name2, BalisePointer->Name);
			sprintf(DistanceBalise2, "%lf", DistanceWpt);
		} else {
			sprintf(Name2, "Aucune");
			sprintf(DistanceBalise2, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name2);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise2);
	}

	sprintf(Xdep, "%lf", y[DLWpt1]);
	sprintf(Ydep, "%lf", x[DLWpt1]);
	sprintf(Xarv, "%lf", y[DLWpt2]);
	sprintf(Yarv, "%lf", x[DLWpt2]);
	sprintf(Dstc, "%lf", BestDL);
	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Distance, Dstc);
	gtk_entry_set_text((GtkEntry *)Points, Dstc);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = DLWpt1;
	TableX[1] = DLWpt2;
	TableX[2] = -1;
	TableBorne[0] = -1;
	DepArv[0] = DLWpt1;
	DepArv[1] = DLWpt2;
	DrawWpt[0] = -1;
	DrawWpt[1] = -1;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Mode Distance Libre avec point de contournement */
void on_distance_libre_avec_contournement_activate(GtkWidget *Window)
{
	GtkWidget *DLCWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYCont;
	GtkWidget *CoordXCont;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1Entry;
	GtkWidget *Dist2Entry;
	GtkWidget *DistTotale;
	GtkWidget *Points;

	GtkWidget *ZoneDrawingArea;

	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseContournement;
	GtkWidget *DistBaliseContournement;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name1[256], Name2[256], Name3[256], DistanceBalise1[6], DistanceBalise2[6], DistanceBalise3[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;
	guint Timer;
	char Xdep[9], Ydep[9], Xcont[9], Ycont[9], Xarv[9], Yarv[9], Dstc1[6],
	     Dstc2[6], Dstc3[6], Point[7];
	int Hauteur;
	GtkWidget *LoadWindow;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'une distance libre\n avec point de contournement en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestDLC == 0)
		FindBestDLC();


	if (BestDLC == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	DLCWindow = (GtkWidget *)create_DLCWindow();
	gtk_widget_show(DLCWindow);
	CoordXDep = gtk_object_get_data((GtkObject *)DLCWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)DLCWindow, "CoordYDep");
	CoordXCont = gtk_object_get_data((GtkObject *)DLCWindow, "CoordXCont");
	CoordYCont = gtk_object_get_data((GtkObject *)DLCWindow, "CoordYCont");
	CoordXArv = gtk_object_get_data((GtkObject *)DLCWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)DLCWindow, "CoordYArv");
	Dist1Entry = gtk_object_get_data((GtkObject *)DLCWindow, "Dist1Entry");
	Dist2Entry = gtk_object_get_data((GtkObject *)DLCWindow, "Dist2Entry");
	DistTotale = gtk_object_get_data((GtkObject *)DLCWindow, "DistTotal");
	Points = gtk_object_get_data((GtkObject *)DLCWindow, "Points");





	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)DLCWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)DLCWindow, "DistBaliseDep");

		BaliseContournement = gtk_object_get_data((GtkObject *)DLCWindow, "BaliseCont");
		DistBaliseContournement = gtk_object_get_data((GtkObject *)DLCWindow, "DistBaliseCont");

		BaliseArrivee = gtk_object_get_data((GtkObject *)DLCWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)DLCWindow, "DistBaliseArv");

		BalisePointer = FindClosestBalise(DLCWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name1, BalisePointer->Name);
			sprintf(DistanceBalise1, "%lf", DistanceWpt);
		} else {
			sprintf(Name1, "Aucune");
			sprintf(DistanceBalise1, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name1);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise1);
		BalisePointer = 0;

		BalisePointer = FindClosestBalise(DLCWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name2, BalisePointer->Name);
			sprintf(DistanceBalise2, "%lf", DistanceWpt);
		} else {
			sprintf(Name2, "Aucune");
			sprintf(DistanceBalise2, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseContournement, Name2);
		gtk_entry_set_text((GtkEntry *)DistBaliseContournement, DistanceBalise2);

		BalisePointer = FindClosestBalise(DLCWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name3, BalisePointer->Name);
			sprintf(DistanceBalise3, "%lf", DistanceWpt);
		} else {
			sprintf(Name3, "Aucune");
			sprintf(DistanceBalise3, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name3);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise3);
	}



	sprintf(Xdep, "%lf", x[DLCWpt1]);
	sprintf(Ydep, "%lf", y[DLCWpt1]);
	sprintf(Xcont, "%lf", x[DLCWpt2]);
	sprintf(Ycont, "%lf", y[DLCWpt2]);
	sprintf(Xarv, "%lf", x[DLCWpt3]);
	sprintf(Yarv, "%lf", y[DLCWpt3]);
	sprintf(Dstc1, "%lf", DLCDist1);
	sprintf(Dstc2, "%lf", DLCDist2);
	sprintf(Dstc3, "%lf", BestDLC);
	sprintf(Point, "%lf", BestDLC);

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXCont, Xcont);
	gtk_entry_set_text((GtkEntry *)CoordYCont, Ycont);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1Entry, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2Entry, Dstc2);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc3);
	gtk_entry_set_text((GtkEntry *)Points, Point);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = DLCWpt1;
	TableX[1] = DLCWpt2;
	TableX[2] = DLCWpt3;
	TableX[3] = -1;
	TableBorne[0] = -1;

	DepArv[0] = DLCWpt1;
	DepArv[1] = DLCWpt3;
	DrawWpt[0] = DLCWpt2;
	DrawWpt[1] = -1;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}


/* ####################################################################### */
/* ######################## PROCEDURES DELTA ############################# */
/* ####################################################################### */



/* Mode auto Delta */
void
on_auto_activate(GtkWidget *Window)
{
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche du meilleur circuit en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);

	if (BestDL == 0)
		FindBestDL();
	if (BestDLC == 0)
		FindBestDLC();
	if (BestAR == 0)
		FindBestAR();
	if (BestTR == 0)
		FindBestTR(0);
	if (BestFAI == 0)
		FindBestTR(1);
	if (BestQDRLT == 0)
		FindBestQDRLT();
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);


	if (BestDL > BestDLC && BestDL > (BestAR * 1.3) && BestDL > (BestTR * 1.3)
	    && BestDL > (BestFAI * 1.5) && BestDL > BestQDRLT)
		on_distance_libre_activate(Window);
	if (BestDLC > BestDL && BestDLC > (BestAR * 1.3) && BestDLC > (BestTR * 1.3)
	    && BestDLC > (BestFAI * 1.5) && BestDLC > (BestQDRLT * 1.3))
		on_distance_libre_avec_contournement_activate(Window);
	if ((BestAR * 1.3) > BestDL && (BestAR * 1.3) > BestDLC
	    && (BestAR * 1.3) > (BestTR * 1.3) && (BestAR * 1.3) > (BestFAI * 1.5)
	    && (BestAR * 1.3) > (BestQDRLT * 1.3))
		on_aller_retour_activate(Window);
	if ((BestTR * 1.3) > BestDL && (BestTR * 1.3) > BestDLC
	    && (BestTR * 1.3) > (BestAR * 1.3) && (BestTR * 1.3) > (BestFAI * 1.5)
	    && (BestTR * 1.3) > (BestQDRLT * 1.3))
		on_triangle_activate(Window);
	if ((BestFAI * 1.5) > BestDLC && (BestFAI * 1.5) > (BestAR * 1.3)
	    && (BestFAI * 1.5) > (BestTR * 1.3) && (BestFAI * 1.5) > BestDL
	    && (BestFAI * 1.5) > (BestQDRLT * 1.3))
		on_triangle_fai_activate(Window);
	if ((BestQDRLT * 1.3) > BestDLC && (BestQDRLT * 1.3) > (BestAR * 1.3)
	    && (BestQDRLT * 1.3) > (BestTR * 1.3)
	    && (BestQDRLT * 1.3) > (BestFAI * 1.5) && (BestQDRLT * 1.3) > BestDL)
		on_quadrilatere_activate(Window);
	if ((BestDL == 0) && (BestDLC == 0) && (BestFAI == 0) && (BestTR == 0) && (BestAR == 0) && (BestQDRLT == 0)) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		return;
	}
}



/* Mode Aller-Retour Delta (coeff 1.3) */
void
on_aller_retour_activate(GtkWidget *Window)
{
	GtkWidget *ARWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;

	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;
	guint Timer;
	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;


	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], Dstc1[6], Dstc4[6],
	     Point[7], Xarv[9], Yarv[9];
	int Hauteur;
	double Aller, PointTotal;
	GtkWidget *LoadWindow;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un aller-retour en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestAR == 0)
		FindBestAR(0);

	if (BestAR == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	ARWindow = (GtkWidget *)create_ARWindow();
	gtk_widget_show(ARWindow);

	CoordXDep = gtk_object_get_data((GtkObject *)ARWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)ARWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)ARWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)ARWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)ARWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)ARWindow, "CoordYB2");
	CoordXArv = gtk_object_get_data((GtkObject *)ARWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)ARWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)ARWindow, "DistB1_B2");
	DistTotale = gtk_object_get_data((GtkObject *)ARWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)ARWindow, "Points");
	PointTotal = (rint(13 * BestAR)) / 10;
	Aller = BestAR / 2;



	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)ARWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)ARWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)ARWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseB2");

		BaliseArrivee = gtk_object_get_data((GtkObject *)ARWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}



	sprintf(Xdep, "%lf", x[ARDprt]);
	sprintf(Ydep, "%lf", y[ARDprt]);
	sprintf(XB1, "%lf", x[ARWpt1]);
	sprintf(YB1, "%lf", y[ARWpt1]);
	sprintf(XB2, "%lf", x[ARWpt2]);
	sprintf(YB2, "%lf", y[ARWpt2]);
	sprintf(Xarv, "%lf", x[ARArv]);
	sprintf(Yarv, "%lf", y[ARArv]);
	sprintf(Dstc1, "%lf", Aller);
	sprintf(Dstc4, "%lf", BestAR);
	sprintf(Point, "%lf", PointTotal);

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = ARWpt1;
	TableX[1] = ARWpt2;
	TableX[2] = -1;

	TableBorne[0] = ARDprt;
	TableBorne[1] = ARWpt1;
	TableBorne[2] = ARWpt2;
	TableBorne[3] = ARArv;
	TableBorne[4] = -1;

	DepArv[0] = ARDprt;
	DepArv[1] = ARArv;
	DrawWpt[0] = ARWpt1;
	DrawWpt[1] = ARWpt2;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Mode triangle simple delta (coef 1.3 et cote>15% du total) */
void
on_triangle_activate(GtkWidget *Window)
{
	GtkWidget *TRWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;


	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;


	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Point[7], Xarv[9], Yarv[9];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un triangle en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestTR == 0) {
		FindBestTR(0);
		if ((TRDist1 >= FAI_DELTA * BestTR)
		    && (TRDist2 >= FAI_DELTA * BestTR) && (TRDist3 >= FAI_DELTA * BestTR) && (BestTR != 0)) {
			GWarning
				("Le meilleur triangle simple est FAI, mais a ete considere comme un triangle simple pour les points!");
			FAIDprt = TRDprt;
			FAIArv = TRArv;
			BestFAI = BestTR;
			FAIWpt1 = TRWpt1;
			FAIWpt2 = TRWpt2;
			FAIWpt3 = TRWpt3;
			FAIDist1 = TRDist1;
			FAIDist2 = TRDist2;
			FAIDist3 = TRDist3;
		}
	}


	if (BestTR == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	TRWindow = (GtkWidget *)create_TRWindow();
	gtk_widget_show(TRWindow);





	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB3");

		BaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}






	CoordXDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB3");
	CoordXArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)TRWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)TRWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)TRWindow, "DistB3_B1");
	DistTotale = gtk_object_get_data((GtkObject *)TRWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)TRWindow, "Points");
	PointTotal = (rint(13 * BestTR)) / 10;
	sprintf(Xdep, "%lf", x[TRDprt]);
	sprintf(Ydep, "%lf", y[TRDprt]);
	sprintf(XB1, "%lf", x[TRWpt1]);
	sprintf(YB1, "%lf", y[TRWpt1]);
	sprintf(XB2, "%lf", x[TRWpt2]);
	sprintf(YB2, "%lf", y[TRWpt2]);
	sprintf(XB3, "%lf", x[TRWpt3]);
	sprintf(YB3, "%lf", y[TRWpt3]);
	sprintf(Xarv, "%lf", x[TRArv]);
	sprintf(Yarv, "%lf", y[TRArv]);
	sprintf(Dstc1, "%lf", TRDist1);
	sprintf(Dstc2, "%lf", TRDist2);
	sprintf(Dstc3, "%lf", TRDist3);
	sprintf(Dstc4, "%lf", BestTR);
	sprintf(Point, "%lf", PointTotal);

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = TRWpt1;
	TableX[1] = TRWpt2;
	TableX[2] = TRWpt3;
	TableX[3] = TRWpt1;
	TableX[4] = -1;

	TableBorne[0] = TRDprt;
	TableBorne[1] = TRWpt1;
	TableBorne[2] = TRWpt3;
	TableBorne[3] = TRArv;
	TableBorne[4] = -1;


	DepArv[0] = TRDprt;
	DepArv[1] = TRArv;
	DrawWpt[0] = TRWpt1;
	DrawWpt[1] = TRWpt2;
	DrawWpt[2] = TRWpt3;
	DrawWpt[3] = -1;

	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Triangle FAI Delta (coef 1.5) */
void
on_triangle_fai_activate(GtkWidget *Window)
{
	GtkWidget *TRWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	GtkWidget *Coeff;
	GtkWidget *UnderTitle;

	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;

	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Point[7], Xarv[9], Yarv[9],
	     Coeffs[10], Subtitle[15];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un triangle FAI en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestFAI == 0)
		FindBestTR(1);



	if (BestFAI == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	TRWindow = (GtkWidget *)create_TRWindow();
	gtk_widget_show(TRWindow);





	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB3");

		BaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}








	CoordXDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB3");
	CoordXArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)TRWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)TRWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)TRWindow, "DistB3_B1");
	DistTotale = gtk_object_get_data((GtkObject *)TRWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)TRWindow, "Points");
	Coeff = gtk_object_get_data((GtkObject *)TRWindow, "label69");
	UnderTitle = gtk_object_get_data((GtkObject *)TRWindow, "TriangleLabel");
	PointTotal = (rint(15 * BestFAI)) / 10;
	sprintf(Xdep, "%lf", x[FAIDprt]);
	sprintf(Ydep, "%lf", y[FAIDprt]);
	sprintf(XB1, "%lf", x[FAIWpt1]);
	sprintf(YB1, "%lf", y[FAIWpt1]);
	sprintf(XB2, "%lf", x[FAIWpt2]);
	sprintf(YB2, "%lf", y[FAIWpt2]);
	sprintf(XB3, "%lf", x[FAIWpt3]);
	sprintf(YB3, "%lf", y[FAIWpt3]);
	sprintf(Xarv, "%lf", x[FAIArv]);
	sprintf(Yarv, "%lf", y[FAIArv]);
	sprintf(Dstc1, "%lf", FAIDist1);
	sprintf(Dstc2, "%lf", FAIDist2);
	sprintf(Dstc3, "%lf", FAIDist3);
	sprintf(Dstc4, "%lf", BestFAI);
	sprintf(Point, "%lf", PointTotal);
	sprintf(Coeffs, "%.1f", FAI_COEFF);
	sprintf(Subtitle, "Triangle FAI");

	gtk_window_set_title((GtkWindow *)TRWindow, "Triangle FAI");
	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);
	gtk_label_set_text((GtkLabel *)Coeff, Coeffs);
	gtk_label_set_text((GtkLabel *)UnderTitle, Subtitle);

	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = FAIWpt1;
	TableX[1] = FAIWpt2;
	TableX[2] = FAIWpt3;
	TableX[3] = FAIWpt1;
	TableX[4] = -1;

	TableBorne[0] = FAIDprt;
	TableBorne[1] = FAIWpt1;
	TableBorne[2] = FAIWpt3;
	TableBorne[3] = FAIArv;
	TableBorne[4] = -1;


	DepArv[0] = FAIDprt;
	DepArv[1] = FAIArv;
	DrawWpt[0] = FAIWpt1;
	DrawWpt[1] = FAIWpt2;
	DrawWpt[2] = FAIWpt3;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Mode Quadrilatere Delta (coef 1.3) */
void
on_quadrilatere_activate(GtkWidget *Window)
{
	GtkWidget *QDRLTWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXB4;
	GtkWidget *CoordYB4;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *Dist4;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;


	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseB4;
	GtkWidget *DistBaliseB4;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;



	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     XB4[9], YB4[9], Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Dstc5[6],
	     Point[7], Xarv[9], Yarv[9];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un quadrilatere en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestQDRLT == 0)
		FindBestQDRLT();


	if (BestQDRLT == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	QDRLTWindow = (GtkWidget *)create_QDRLTWindow();
	gtk_widget_show(QDRLTWindow);




	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB3");

		BaliseB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB4");
		DistBaliseB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB4");

		BaliseArrivee = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt4, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB4, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB4, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}


	CoordXDep = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB3");
	CoordXB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB4");
	CoordYB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB4");
	CoordXArv = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB3_B4");
	Dist4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB4_B1");
	DistTotale = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)QDRLTWindow, "Points");
	PointTotal = (rint(13 * BestQDRLT)) / 10;
	sprintf(Xdep, "%lf", x[QDRLTDprt]);
	sprintf(Ydep, "%lf", y[QDRLTDprt]);
	sprintf(XB1, "%lf", x[QDRLTWpt1]);
	sprintf(YB1, "%lf", y[QDRLTWpt1]);
	sprintf(XB2, "%lf", x[QDRLTWpt2]);
	sprintf(YB2, "%lf", y[QDRLTWpt2]);
	sprintf(XB3, "%lf", x[QDRLTWpt3]);
	sprintf(YB3, "%lf", y[QDRLTWpt3]);
	sprintf(XB4, "%lf", x[QDRLTWpt4]);
	sprintf(YB4, "%lf", y[QDRLTWpt4]);
	sprintf(Xarv, "%lf", x[QDRLTArv]);
	sprintf(Yarv, "%lf", y[QDRLTArv]);
	sprintf(Dstc1, "%lf", QDRLTDist1);
	sprintf(Dstc2, "%lf", QDRLTDist2);
	sprintf(Dstc3, "%lf", QDRLTDist3);
	sprintf(Dstc4, "%lf", QDRLTDist4);
	sprintf(Dstc5, "%lf", BestQDRLT);
	sprintf(Point, "%lf", PointTotal);

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXB4, XB4);
	gtk_entry_set_text((GtkEntry *)CoordYB4, YB4);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)Dist4, Dstc4);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc5);
	gtk_entry_set_text((GtkEntry *)Points, Point);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = QDRLTWpt1;
	TableX[1] = QDRLTWpt2;
	TableX[2] = QDRLTWpt3;
	TableX[3] = QDRLTWpt4;
	TableX[4] = QDRLTWpt1;
	TableX[5] = -1;

	TableBorne[0] = QDRLTDprt;
	TableBorne[1] = QDRLTWpt1;
	TableBorne[2] = QDRLTWpt4;
	TableBorne[3] = QDRLTArv;
	TableBorne[4] = -1;


	DepArv[0] = QDRLTDprt;
	DepArv[1] = QDRLTArv;
	DrawWpt[0] = QDRLTWpt1;
	DrawWpt[1] = QDRLTWpt2;
	DrawWpt[2] = QDRLTWpt3;
	DrawWpt[3] = QDRLTWpt4;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}


/* ####################################################################### */
/* ########################## MODE PARAPENTE ############################# */
/* ####################################################################### */

/* Les gtk_label_set_text sont utilises pour changer l'affichage du coefficient dans la fenetre et ainsi eviter d'avoir a en creer une nouvelle */


/* Mode Auto Para*/
void
on_autopara_activate(GtkWidget *Window)
{
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche du meilleur circuit en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestDL == 0)
		FindBestDL();
	if (BestDLC == 0)
		FindBestDLC();
	if (BestDL2C == 0)
		FindBestDL2C();
	if (BestAR == 0)
		FindBestAR();
	if (BestTR == 0)
		FindBestTRPara(0);
	if (BestFAI == 0)
		FindBestTRPara(1);
	if (BestQDRLT == 0)
		FindBestQDRLT();
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	if (BestDL > BestDLC && BestDL > (BestAR * 1.2) && BestDL > (BestTRPara * 1.2)
	    && BestDL > (BestFAIPara * 1.2) && BestDL > (BestQDRLT * 1.2) && BestDL > BestDL2C)
		on_distance_libre_activate(Window);
	if (BestDLC > BestDL && BestDLC > (BestAR * 1.2) && BestDLC > (BestTRPara * 1.2)
	    && BestDLC > (BestFAIPara * 1.4) && BestDLC > (BestQDRLT * 1.2) && BestDLC > BestDL2C)
		on_distance_libre_avec_contournement_activate(Window);
	if ((BestAR * 1.2) > BestDL && (BestAR * 1.2) > BestDLC
	    && (BestAR * 1.2) > (BestTRPara * 1.2) && (BestAR * 1.2) > (BestFAIPara * 1.4)
	    && (BestAR * 1.2) > (BestQDRLT * 1.2) && (BestAR * 1.2) > BestDL2C)
		on_aller_retourpara_activate(Window);
	if ((BestTRPara * 1.2) > BestDL && (BestTRPara * 1.2) > BestDLC
	    && (BestTRPara * 1.2) > (BestAR * 1.2) && (BestTRPara * 1.2) > (BestFAIPara * 1.4)
	    && (BestTRPara * 1.2) > (BestQDRLT * 1.2) && (BestTRPara * 1.2) > BestDL2C)
		on_trianglepara_activate(Window);
	if ((BestFAIPara * 1.4) > BestDLC && (BestFAIPara * 1.4) > (BestAR * 1.2)
	    && (BestFAIPara * 1.4) > (BestTRPara * 1.2) && (BestFAIPara * 1.4) > BestDL && (BestFAIPara * 1.4) > (BestQDRLT * 1.2) && (BestFAIPara * 1.4) > BestDL2C)
		on_trianglepara_fai_activate(Window);
	if ((BestQDRLT * 1.2) > BestDLC && (BestQDRLT * 1.2) > (BestAR * 1.2)
	    && (BestQDRLT * 1.2) > (BestTRPara * 1.2)
	    && (BestQDRLT * 1.2) > (BestFAIPara * 1.4) && (BestQDRLT * 1.2) > BestDL && (BestQDRLT * 1.2) > BestDL2C)
		on_quadrilaterepara_activate(Window);
	if (BestDL2C > BestDL && BestDL2C > BestDLC && BestDL2C > (BestAR * 1.2) && BestDL2C > (BestTRPara * 1.2) && BestDL2C > (BestFAIPara * 1.4) && BestDL2C > (BestQDRLT * 1.2))
		on_distance_libre_avec_deux_contournement_activate(Window);
	if ((BestDL == 0) && (BestDLC == 0) && (BestFAIPara == 0) && (BestTRPara == 0) && (BestAR == 0) && (BestQDRLT == 0) && (BestDL2C == 0)) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		return;
	}
}

/* Mode triangle Simple (coef 1.2) */
void
on_trianglepara_activate(GtkWidget *Window)
{
	GtkWidget *TRWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	GtkWidget *Coeff;


	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;



	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Point[7], Xarv[9], Yarv[9], Coefficient[4];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un triangle en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestTRPara == 0) {
		FindBestTRPara(0);
		if ((TRParaDist1 >= FAI_DELTA * BestTRPara)
		    && (TRParaDist2 >= FAI_DELTA * BestTRPara) && (TRParaDist3 >= FAI_DELTA * BestTRPara) && (BestTRPara != 0)) {
			GWarning
				("Le meilleur triangle simple est FAI, mais a ete considere comme un triangle simple pour les points!");
			FAIParaDprt = TRParaDprt;
			FAIParaArv = TRParaArv;
			BestFAIPara = BestTRPara;
			FAIParaWpt1 = TRParaWpt1;
			FAIParaWpt2 = TRParaWpt2;
			FAIParaWpt3 = TRParaWpt3;
			FAIParaDist1 = TRParaDist1;
			FAIParaDist2 = TRParaDist2;
			FAIParaDist3 = TRParaDist3;
		}
	}


	if (BestTRPara == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	TRWindow = (GtkWidget *)create_TRWindow();
	gtk_widget_show(TRWindow);





	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB3");

		BaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRParaDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRParaWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRParaWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRParaWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(TRParaArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}







	CoordXDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB3");
	CoordXArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)TRWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)TRWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)TRWindow, "DistB3_B1");
	DistTotale = gtk_object_get_data((GtkObject *)TRWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)TRWindow, "Points");
	Coeff = gtk_object_get_data((GtkObject *)TRWindow, "label69");
	PointTotal = (rint(12 * BestTRPara)) / 10;
	sprintf(Xdep, "%lf", x[TRParaDprt]);
	sprintf(Ydep, "%lf", y[TRParaDprt]);
	sprintf(XB1, "%lf", x[TRParaWpt1]);
	sprintf(YB1, "%lf", y[TRParaWpt1]);
	sprintf(XB2, "%lf", x[TRParaWpt2]);
	sprintf(YB2, "%lf", y[TRParaWpt2]);
	sprintf(XB3, "%lf", x[TRParaWpt3]);
	sprintf(YB3, "%lf", y[TRParaWpt3]);
	sprintf(Xarv, "%lf", x[TRParaArv]);
	sprintf(Yarv, "%lf", y[TRParaArv]);
	sprintf(Dstc1, "%lf", TRParaDist1);
	sprintf(Dstc2, "%lf", TRParaDist2);
	sprintf(Dstc3, "%lf", TRParaDist3);
	sprintf(Dstc4, "%lf", BestTRPara);
	sprintf(Point, "%lf", PointTotal);
	strcpy(Coefficient, "1,2");

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);
	gtk_label_set_text((GtkLabel *)Coeff, Coefficient);

	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = TRParaWpt1;
	TableX[1] = TRParaWpt2;
	TableX[2] = TRParaWpt3;
	TableX[3] = TRParaWpt1;
	TableX[4] = -1;

	TableBorne[0] = TRParaDprt;
	TableBorne[1] = TRParaWpt1;
	TableBorne[2] = TRParaWpt3;
	TableBorne[3] = TRParaArv;
	TableBorne[4] = -1;

	DepArv[0] = TRParaDprt;
	DepArv[1] = TRParaArv;
	DrawWpt[0] = TRParaWpt1;
	DrawWpt[1] = TRParaWpt2;
	DrawWpt[2] = TRParaWpt3;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/*Mode Triangle FAI para (coef 1.4) */
void
on_trianglepara_fai_activate(GtkWidget *Window)
{
	GtkWidget *TRWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	GtkWidget *Coeff;
	GtkWidget *UnderTitle;



	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;


	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Point[7], Xarv[9], Yarv[9],
	     Coeffs[10], Subtitle[15];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un triangle FAI en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestFAIPara == 0)
		FindBestTRPara(1);



	if (BestFAIPara == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	TRWindow = (GtkWidget *)create_TRWindow();
	gtk_widget_show(TRWindow);




	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseB3");

		BaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)TRWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIParaDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIParaWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIParaWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIParaWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(FAIParaArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}



	CoordXDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)TRWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)TRWindow, "CoordYB3");
	CoordXArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)TRWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)TRWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)TRWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)TRWindow, "DistB3_B1");
	DistTotale = gtk_object_get_data((GtkObject *)TRWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)TRWindow, "Points");
	Coeff = gtk_object_get_data((GtkObject *)TRWindow, "label69");
	UnderTitle = gtk_object_get_data((GtkObject *)TRWindow, "TriangleLabel");
	PointTotal = (rint(FAI_PARA_COEFF * 10 * BestFAIPara)) / 10;
	sprintf(Xdep, "%lf", x[FAIParaDprt]);
	sprintf(Ydep, "%lf", y[FAIParaDprt]);
	sprintf(XB1, "%lf", x[FAIParaWpt1]);
	sprintf(YB1, "%lf", y[FAIParaWpt1]);
	sprintf(XB2, "%lf", x[FAIParaWpt2]);
	sprintf(YB2, "%lf", y[FAIParaWpt2]);
	sprintf(XB3, "%lf", x[FAIParaWpt3]);
	sprintf(YB3, "%lf", y[FAIParaWpt3]);
	sprintf(Xarv, "%lf", x[FAIParaArv]);
	sprintf(Yarv, "%lf", y[FAIParaArv]);
	sprintf(Dstc1, "%lf", FAIParaDist1);
	sprintf(Dstc2, "%lf", FAIParaDist2);
	sprintf(Dstc3, "%lf", FAIParaDist3);
	sprintf(Dstc4, "%lf", BestFAIPara);
	sprintf(Point, "%lf", PointTotal);
	sprintf(Coeffs, "%.1f", FAI_PARA_COEFF);
	sprintf(Subtitle, "Triangle FAI");

	gtk_window_set_title((GtkWindow *)TRWindow, "Triangle FAI");
	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);
	gtk_label_set_text((GtkLabel *)Coeff, Coeffs);
	gtk_label_set_text((GtkLabel *)UnderTitle, Subtitle);

	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = FAIParaWpt1;
	TableX[1] = FAIParaWpt2;
	TableX[2] = FAIParaWpt3;
	TableX[3] = FAIParaWpt1;
	TableX[4] = -1;

	TableBorne[0] = FAIParaDprt;
	TableBorne[1] = FAIParaWpt1;
	TableBorne[2] = FAIParaWpt3;
	TableBorne[3] = FAIParaArv;
	TableBorne[4] = -1;

	DepArv[0] = FAIParaDprt;
	DepArv[1] = FAIParaArv;
	DrawWpt[0] = FAIParaWpt1;
	DrawWpt[1] = FAIParaWpt2;
	DrawWpt[2] = FAIParaWpt3;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Mode Distance Libre avec 2 pt de contournement (coef 1.0) */
void
on_distance_libre_avec_deux_contournement_activate(GtkWidget *Window)
{
	GtkWidget *DL2CWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;


	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;


	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6],
	     Point[7], Xarv[9], Yarv[9];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'une distance libre avec\n deux points de contournement en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestDL2C == 0)
		FindBestDL2C();


	if (BestDL2C == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	DL2CWindow = (GtkWidget *)create_DL2CWindow();
	gtk_widget_show(DL2CWindow);







	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)DL2CWindow, "entry22");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)DL2CWindow, "entry35");

		BaliseB1 = gtk_object_get_data((GtkObject *)DL2CWindow, "entry24");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)DL2CWindow, "entry33");

		BaliseB2 = gtk_object_get_data((GtkObject *)DL2CWindow, "entry26");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)DL2CWindow, "entry31");

		BaliseArrivee = gtk_object_get_data((GtkObject *)DL2CWindow, "entry28");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)DL2CWindow, "entry29");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(DL2CDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(DL2CWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(DL2CWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(DL2CArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}



	CoordXDep = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordYB2");
	CoordXArv = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)DL2CWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)DL2CWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)DL2CWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)DL2CWindow, "DistB3_B4");
	DistTotale = gtk_object_get_data((GtkObject *)DL2CWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)DL2CWindow, "Points");
	PointTotal = (rint(10 * BestDL2C)) / 10;
	sprintf(Xdep, "%lf", x[DL2CDprt]);
	sprintf(Ydep, "%lf", y[DL2CDprt]);
	sprintf(XB1, "%lf", x[DL2CWpt1]);
	sprintf(YB1, "%lf", y[DL2CWpt1]);
	sprintf(XB2, "%lf", x[DL2CWpt2]);
	sprintf(YB2, "%lf", y[DL2CWpt2]);
	sprintf(Xarv, "%lf", x[DL2CArv]);
	sprintf(Yarv, "%lf", y[DL2CArv]);
	sprintf(Dstc1, "%lf", DL2CDist1);
	sprintf(Dstc2, "%lf", DL2CDist2);
	sprintf(Dstc3, "%lf", DL2CDist3);
	sprintf(Dstc4, "%lf", BestDL2C);
	sprintf(Point, "%lf", PointTotal);

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = DL2CDprt;
	TableX[1] = DL2CWpt1;
	TableX[2] = DL2CWpt2;
	TableX[3] = DL2CArv;
	TableX[4] = -1;


	TableBorne[0] = -1;

	DepArv[0] = DL2CDprt;
	DepArv[1] = DL2CArv;
	DrawWpt[0] = DL2CWpt1;
	DrawWpt[1] = DL2CWpt2;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}
/* Mode Quadrilatere para (coef 1.2) */
void
on_quadrilaterepara_activate(GtkWidget *Window)
{
	GtkWidget *QDRLTWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXB3;
	GtkWidget *CoordYB3;
	GtkWidget *CoordXB4;
	GtkWidget *CoordYB4;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *Dist2;
	GtkWidget *Dist3;
	GtkWidget *Dist4;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	GtkWidget *Coef;


	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseB3;
	GtkWidget *DistBaliseB3;

	GtkWidget *BaliseB4;
	GtkWidget *DistBaliseB4;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;


	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], XB3[9], YB3[9],
	     XB4[9], YB4[9], Dstc1[6], Dstc2[6], Dstc3[6], Dstc4[6], Dstc5[6],
	     Point[7], Xarv[9], Yarv[9], Coefs[4];
	int Hauteur;
	double PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un quadrilatere en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestQDRLT == 0)
		FindBestQDRLT();


	if (BestQDRLT == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	QDRLTWindow = (GtkWidget *)create_QDRLTWindow();
	gtk_widget_show(QDRLTWindow);








	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB2");

		BaliseB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB3");
		DistBaliseB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB3");

		BaliseB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseB4");
		DistBaliseB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseB4");

		BaliseArrivee = gtk_object_get_data((GtkObject *)QDRLTWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt3, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB3, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB3, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTWpt4, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB4, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB4, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(QDRLTArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}


	CoordXDep = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB2");
	CoordXB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB3");
	CoordYB3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB3");
	CoordXB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXB4");
	CoordYB4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYB4");
	CoordXArv = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)QDRLTWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB1_B2");
	Dist2 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB2_B3");
	Dist3 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB3_B4");
	Dist4 = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistB4_B1");
	DistTotale = gtk_object_get_data((GtkObject *)QDRLTWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)QDRLTWindow, "Points");
	Coef = gtk_object_get_data((GtkObject *)QDRLTWindow, "label96");
	PointTotal = (rint(12 * BestQDRLT)) / 10;
	sprintf(Xdep, "%lf", x[QDRLTDprt]);
	sprintf(Ydep, "%lf", y[QDRLTDprt]);
	sprintf(XB1, "%lf", x[QDRLTWpt1]);
	sprintf(YB1, "%lf", y[QDRLTWpt1]);
	sprintf(XB2, "%lf", x[QDRLTWpt2]);
	sprintf(YB2, "%lf", y[QDRLTWpt2]);
	sprintf(XB3, "%lf", x[QDRLTWpt3]);
	sprintf(YB3, "%lf", y[QDRLTWpt3]);
	sprintf(XB4, "%lf", x[QDRLTWpt4]);
	sprintf(YB4, "%lf", y[QDRLTWpt4]);
	sprintf(Xarv, "%lf", x[QDRLTArv]);
	sprintf(Yarv, "%lf", y[QDRLTArv]);
	sprintf(Dstc1, "%lf", QDRLTDist1);
	sprintf(Dstc2, "%lf", QDRLTDist2);
	sprintf(Dstc3, "%lf", QDRLTDist3);
	sprintf(Dstc4, "%lf", QDRLTDist4);
	sprintf(Dstc5, "%lf", BestQDRLT);
	sprintf(Point, "%lf", PointTotal);
	strcpy(Coefs, "1.2");
	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXB3, XB3);
	gtk_entry_set_text((GtkEntry *)CoordYB3, YB3);
	gtk_entry_set_text((GtkEntry *)CoordXB4, XB4);
	gtk_entry_set_text((GtkEntry *)CoordYB4, YB4);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)Dist2, Dstc2);
	gtk_entry_set_text((GtkEntry *)Dist3, Dstc3);
	gtk_entry_set_text((GtkEntry *)Dist4, Dstc4);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc5);
	gtk_entry_set_text((GtkEntry *)Points, Point);
	gtk_label_set_text((GtkLabel *)Coef, Coefs);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = QDRLTWpt1;
	TableX[1] = QDRLTWpt2;
	TableX[2] = QDRLTWpt3;
	TableX[3] = QDRLTWpt4;
	TableX[4] = QDRLTWpt1;
	TableX[5] = -1;

	TableBorne[0] = QDRLTDprt;
	TableBorne[1] = QDRLTWpt1;
	TableBorne[2] = QDRLTWpt4;
	TableBorne[3] = QDRLTArv;
	TableBorne[4] = -1;

	DepArv[0] = QDRLTDprt;
	DepArv[1] = QDRLTArv;
	DrawWpt[0] = QDRLTWpt1;
	DrawWpt[1] = QDRLTWpt2;
	DrawWpt[2] = QDRLTWpt3;
	DrawWpt[3] = QDRLTWpt4;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}

/* Mode Aller-Retour Para (coef 1.2) */
void
on_aller_retourpara_activate(GtkWidget *Window)
{
	GtkWidget *ARWindow;
	GtkWidget *CoordXDep;
	GtkWidget *CoordYDep;
	GtkWidget *CoordXB1;
	GtkWidget *CoordYB1;
	GtkWidget *CoordXB2;
	GtkWidget *CoordYB2;
	GtkWidget *CoordXArv;
	GtkWidget *CoordYArv;
	GtkWidget *Dist1;
	GtkWidget *DistTotale;
	GtkWidget *Points;
	GtkWidget *ZoneDrawingArea;
	GtkWidget *Coef;

	GtkWidget *BaliseDepart;
	GtkWidget *DistBaliseDepart;

	GtkWidget *BaliseB1;
	GtkWidget *DistBaliseB1;

	GtkWidget *BaliseB2;
	GtkWidget *DistBaliseB2;

	GtkWidget *BaliseArrivee;
	GtkWidget *DistBaliseArrivee;

	char Name[256], DistanceBalise[6];
	double DistanceWpt;
	BaliseStruct *BalisePointer = 0;

	char Xdep[9], Ydep[9], XB1[9], YB1[9], XB2[9], YB2[9], Dstc1[6], Dstc4[6],
	     Point[7], Xarv[9], Yarv[9], Coefs[4];
	int Hauteur;
	double Aller, PointTotal;
	GtkWidget *LoadWindow;
	guint Timer;

	LoadWindow = (GtkWidget *)CreateWaitWindow("Recherche d'un aller-retour en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);
	if (BestAR == 0)
		FindBestAR(0);


	if (BestAR == 0) {
		GWarning("Aucun circuit declarable de ce type n'a ete trouve");
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	ARWindow = (GtkWidget *)create_ARWindow();
	gtk_widget_show(ARWindow);





	if (FirstBalise != 0) {
		BaliseDepart = gtk_object_get_data((GtkObject *)ARWindow, "BaliseDep");
		DistBaliseDepart = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseDep");

		BaliseB1 = gtk_object_get_data((GtkObject *)ARWindow, "BaliseB1");
		DistBaliseB1 = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseB1");

		BaliseB2 = gtk_object_get_data((GtkObject *)ARWindow, "BaliseB2");
		DistBaliseB2 = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseB2");

		BaliseArrivee = gtk_object_get_data((GtkObject *)ARWindow, "BaliseArv");
		DistBaliseArrivee = gtk_object_get_data((GtkObject *)ARWindow, "DistBaliseArv");

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARDprt, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseDepart, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseDepart, DistanceBalise);


		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARWpt1, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB1, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB1, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARWpt2, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseB2, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseB2, DistanceBalise);

		BalisePointer = 0;
		BalisePointer = FindClosestBalise(ARArv, &DistanceWpt);
		if (BalisePointer != 0) {
			strcpy(Name, BalisePointer->Name);
			sprintf(DistanceBalise, "%lf", DistanceWpt);
		} else {
			sprintf(Name, "Aucune");
			sprintf(DistanceBalise, "0");
		}
		gtk_entry_set_text((GtkEntry *)BaliseArrivee, Name);
		gtk_entry_set_text((GtkEntry *)DistBaliseArrivee, DistanceBalise);
	}



	CoordXDep = gtk_object_get_data((GtkObject *)ARWindow, "CoordXDep");
	CoordYDep = gtk_object_get_data((GtkObject *)ARWindow, "CoordYDep");
	CoordXB1 = gtk_object_get_data((GtkObject *)ARWindow, "CoordXB1");
	CoordYB1 = gtk_object_get_data((GtkObject *)ARWindow, "CoordYB1");
	CoordXB2 = gtk_object_get_data((GtkObject *)ARWindow, "CoordXB2");
	CoordYB2 = gtk_object_get_data((GtkObject *)ARWindow, "CoordYB2");
	CoordXArv = gtk_object_get_data((GtkObject *)ARWindow, "CoordXArv");
	CoordYArv = gtk_object_get_data((GtkObject *)ARWindow, "CoordYArv");
	Dist1 = gtk_object_get_data((GtkObject *)ARWindow, "DistB1_B2");
	DistTotale = gtk_object_get_data((GtkObject *)ARWindow, "DistTotale");
	Points = gtk_object_get_data((GtkObject *)ARWindow, "Points");
	Coef = gtk_object_get_data((GtkObject *)ARWindow, "label120");
	PointTotal = (rint(12 * BestAR)) / 10;
	Aller = BestAR / 2;
	sprintf(Xdep, "%lf", x[ARDprt]);
	sprintf(Ydep, "%lf", y[ARDprt]);
	sprintf(XB1, "%lf", x[ARWpt1]);
	sprintf(YB1, "%lf", y[ARWpt1]);
	sprintf(XB2, "%lf", x[ARWpt2]);
	sprintf(YB2, "%lf", y[ARWpt2]);
	sprintf(Xarv, "%lf", x[ARArv]);
	sprintf(Yarv, "%lf", y[ARArv]);
	sprintf(Dstc1, "%lf", Aller);
	sprintf(Dstc4, "%lf", BestAR);
	sprintf(Point, "%lf", PointTotal);
	strcpy(Coefs, "1.2");

	gtk_entry_set_text((GtkEntry *)CoordXDep, Xdep);
	gtk_entry_set_text((GtkEntry *)CoordYDep, Ydep);
	gtk_entry_set_text((GtkEntry *)CoordXB1, XB1);
	gtk_entry_set_text((GtkEntry *)CoordYB1, YB1);
	gtk_entry_set_text((GtkEntry *)CoordXB2, XB2);
	gtk_entry_set_text((GtkEntry *)CoordYB2, YB2);
	gtk_entry_set_text((GtkEntry *)CoordXArv, Xarv);
	gtk_entry_set_text((GtkEntry *)CoordYArv, Yarv);
	gtk_entry_set_text((GtkEntry *)Dist1, Dstc1);
	gtk_entry_set_text((GtkEntry *)DistTotale, Dstc4);
	gtk_entry_set_text((GtkEntry *)Points, Point);
	gtk_label_set_text((GtkLabel *)Coef, Coefs);


	ZoneDrawingArea = gtk_object_get_data((GtkObject *)Window, "DrawingArea");
	Hauteur = ZoneDrawingArea->allocation.height;

	TableX[0] = ARWpt1;
	TableX[1] = ARWpt2;
	TableX[2] = -1;

	TableBorne[0] = ARDprt;
	TableBorne[1] = ARWpt1;
	TableBorne[2] = ARWpt2;
	TableBorne[3] = ARArv;
	TableBorne[4] = -1;

	DepArv[0] = ARDprt;
	DepArv[1] = ARArv;
	DrawWpt[0] = ARWpt1;
	DrawWpt[1] = ARWpt2;
	DrawWpt[2] = -1;
	DrawWpt[3] = -1;
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	DrawDisplay(Window);
}
