/*
 * NE PAS ÉDITER CE FICHIER - il est généré par Glade.
 */

typedef struct _Context {       /* decription du context graphique */
	                        /* echelle d'affichage */
	double		Scale;

	/* decalage entre affichage reel et graphique */
	int		ShiftX;
	int		ShiftY;

	/* dimensions de la fenetre */
	int		Width;
	int		Height;

	/* couleur de fond : 0 = noir, NB_COLORS = blanc */
	int		Background;

	/* limites de la fenetre visible (en coordonnees reelles) */
	double		LeftWindow;
	double		RightWindow;
	double		TopWindow;
	double		BottomWindow;

	/* dimension de la figure a dessiner (en coordonnees reelles) */
	double		XMin;
	double		YMin;
	double		XMax;
	double		YMax;

	/* coordonnees du dernier point de saisie lors d'un deplacement de souris */
	int		XLast;
	int		YLast;
	int		XStart;
	int		YStart;

	unsigned char	Operation;          /* type d'operation faite a la souris */

	/* pointeurs sur les widgets de la fenetre */
	GtkWidget *	DrawingArea;
	GdkPixmap *	Pixmap;
	GdkGC **	ColorGC;
} ContextStruct;

void SaveTrackWindow(GtkWidget *Window);

void DrawDisplay(GtkWidget *Window);
void my_draw_rectangle(GdkDrawable *Drawable, GdkGC *GC, gint Fill, gint X1, gint Y1, gint X2, gint Y2);
double XGraphic2Real(ContextStruct *Context, int X);
double YGraphic2Real(ContextStruct *Context, int Y);
int XReal2Graphic(ContextStruct *Context, double X);
int YReal2Graphic(ContextStruct *Context, double Y);
void CenterString(GtkWidget *Window, gint XMin, gint YMin, gint XMax, gint YMax, char *String);
gint OptionMenuGetIndex(GtkWidget *omenu);
void NameWaypoint(GtkWidget *Window, gint XMin, gint YMin, char *String, int Color);
void DrawMeasure(GtkWidget *Window, int X, int Y);
void UpdateViewWindow(ContextStruct *Context);
void UpdateRulers(GtkWidget *Window);
void UpdateHorizontalAdjustment(GtkWidget *Window);
void UpdateVerticalAdjustment(GtkWidget *Window);
void UpdateAdjustment(GtkWidget *Window);
void RefreshDisplay(GtkWidget *Window);
void ResetDisplay(GtkWidget *Window);
void HorizontalMoveDisplay(GtkAdjustment *Adjust, GtkWidget *Window);
void VerticalMoveDisplay(GtkAdjustment *Adjust, GtkWidget *Window);
void ZoomIn(GtkWidget *Window);
void ZoomOut(GtkWidget *Window);
void GoRight(GtkWidget *Window);
void GoLeft(GtkWidget *Window);
void GoUp(GtkWidget *Window);
void GoDown(GtkWidget *Window);
void MouseZoomIn(GtkWidget *Window, int X, int Y);
void MouseZoomOut(GtkWidget *Window, int X, int Y);
void Fit(GtkWidget *Window);
void PressMouse(GtkWidget *Window, GdkEventButton *event);
void MoveMouse(GtkWidget *Window, GdkEventButton *event);
void ReleaseMouse(GtkWidget *Window, GdkEventButton *event);
GdkGC ** ColorInit(GtkWidget *Window);
GtkWidget * CreateAboutWindow();
GtkWidget * CreateMainWindow(void);
void DoubleClickOnTrack(GtkWidget *TrackWindow, GdkEvent *Event);
GtkWidget * CreateTrackWindow(GtkWidget *TrackList, GtkWidget *Window);
GtkWidget * create_DistanceLibreWindow(void);
GtkWidget * create_LoadWindow(void);
GtkWidget * create_DLCWindow(void);
GtkWidget * create_TRWindow(void);
GtkWidget * create_QDRLTWindow(void);
GtkWidget * create_window7(void);
GtkWidget * create_ARWindow(void);
GtkWidget * CreateWaitWindow(char *Text);
GtkWidget * create_DL2CWindow(void);
