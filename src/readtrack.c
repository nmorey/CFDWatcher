#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <string.h>

#ifndef WINDOWS
#include <unistd.h>
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "callbacks.h"
#include "interface.h"

#include <gps/gps.h>


typedef struct _Track {
	time_t		Time;
	int		Index;
	int		NbPoints;
	int		Ref;
	struct _Track * Next;
} TrackStruct;

TrackStruct *New;
TrackStruct *FirstGPS = 0;
int NbTracks = 0;
GPS_PTrack *trk;







static void Print_Error(const int32 err)
{
	switch (err) {
	case FRAMING_ERROR:
		GWarning("Framing error (high level serial communication)\n");
		break;
	case PROTOCOL_ERROR:
		GWarning("Unknown protocol. Maybe undocumented by Garmin\n");
		break;
	case HARDWARE_ERROR:
		GWarning("Unix system call error\n");
		break;
	case SERIAL_ERROR:
		GWarning("Error reading/writing to serial port\n");
		break;
	case MEMORY_ERROR:
		GWarning("Ran out of memory or other memory error\n");
		break;
	case GPS_UNSUPPORTED:
		GWarning("Your GPS doesn't support this protocol\n");
		break;
	case INPUT_ERROR:
		GWarning("Corrupt or wrong format input file\n");
		break;
	default:
		GWarning("Unknown library error\n");
		break;
	}

	return;
}




/* Procedure d'extinction du GPS */
static void Do_Off(const char *port)
{
	int32 ret;

	if ((ret = GPS_Command_Off(port)) < 0) {
		//	GWarning("Error turning off GPS\n");
		//	Print_Error(ret);
	}

	return;
}



void Print_Track(int32 n, GtkWidget *MainWindow)
{
	int32 i;
	GtkWidget *TrackList;
	char *ListContent[3];
	GtkWidget *TrackWindow;
	struct tm *Date;

	/* creation de la liste */
	TrackList = gtk_clist_new(3); /* nb de colonnes */

	/* recherche des differentes track */
	for (i = 0; i < n; ++i) {
		if (trk[i]->tnew) {
			if (FirstGPS) /* ce n'est pas le premier */
				/* on va calculer le nombre de points */
				FirstGPS->NbPoints = i - FirstGPS->Index;

			New = (TrackStruct *)malloc(sizeof(TrackStruct));
			New->Index = i;
			New->Next = FirstGPS;
			New->Time = trk[i]->Time;
			New->Ref = NbTracks + 1;
			FirstGPS = New;
			NbTracks++;
		}
	}
	if (FirstGPS) { /* on a au moins une trace */
		/* on va calculer le nombre de points */
		FirstGPS->NbPoints = i - FirstGPS->Index;
	} else {
		GWarning("Aucun vol sur votre GPS..\n");
		return;
	}
	/* On definit les elements de la liste pour la fenetre de selection*/
	ListContent[0] = g_strdup_printf("0");
	ListContent[1] = g_strdup_printf("All");
	ListContent[2] = g_strdup_printf("All");
	gtk_clist_append((GtkCList *)TrackList, ListContent);
	g_free(ListContent[0]);
	g_free(ListContent[1]);
	g_free(ListContent[2]);
	for (New = FirstGPS; New; New = New->Next) {
		Date = localtime(&New->Time);
		/* pour chaque track */
		/* ajout de la track */
		ListContent[0] = g_strdup_printf("%d", New->Ref);
		ListContent[1] = g_strdup_printf("%02d/%02d/%4d", Date->tm_mday, Date->tm_mon + 1, Date->tm_year + 1900);
		ListContent[2] = g_strdup_printf("%d", New->NbPoints);
		gtk_clist_append((GtkCList *)TrackList, ListContent);
		g_free(ListContent[0]);
		g_free(ListContent[1]);
		g_free(ListContent[2]);
	}

	TrackWindow = CreateTrackWindow(TrackList, MainWindow);
	gtk_widget_show(TrackWindow);
}

/* Procedure d'ecriture destracks sur le disque */
void CreateTrackFiles(char *VolNum)
{
	int i, j, Selection;
	struct tm *Date;
	TrackStruct *Next;

	/* On convertit en nombre, le numero selectionne */
	Selection = 0;
	for (i = 0; VolNum[i] != NULL; i++)
		Selection = Selection * 10 + VolNum[i] - '0';

	/* Pour chaque track, si elle a ete selectionne, ou le mode all l'a ete, on l'ecrit sur le disque */
	for (New = FirstGPS; New; New = New->Next) {
		if ((Selection == 0) || (New->Ref == Selection)) {
			char Name[256];
			FILE *outf;

			Date = localtime(&New->Time);
			sprintf(Name, "vol_%02d_%02d-%02d-%4d.txt",
				New->Ref,
				Date->tm_mday, Date->tm_mon + 1, Date->tm_year + 1900);
			if (!(outf = fopen(Name, "w"))) {
				GWarning("Impossible de creer le fichier\n");
				return;
			}
			fprintf(outf, "Version,212\n\nWGS 1984 (GPS),217, 6378137, 298.2572236, 0, 0, 0\nUSER GRID,0,0,0,0,0\n\n");
			j = 1;
			for (i = New->Index; i < (New->Index + New->NbPoints); ++i) {
				Date = localtime(&trk[i]->Time);
				fprintf(outf, "t,d,%.6f,%.6f,%02d/%02d/%d,%02d:%02d:%02d,0,%d\n",
					trk[i]->lat,
					trk[i]->lon, Date->tm_mon + 1, Date->tm_mday, Date->tm_year + 1900, Date->tm_hour, Date->tm_min, Date->tm_sec, j);
				j = 0;
			}
			fprintf(outf, "n,Track 0001,255,1");
			fclose(outf);
		}
	}
	/* On libere les variables utilisees */
	for (New = FirstGPS; New; New = Next) {
		Next = New->Next;
		free(New);
	}

	free(trk);
	return;
}

static void Do_Trkrec(const char *port, GtkWidget *MainWindow)
{
	int32 n;
	//    int32 i;
	GtkWidget *LoadWindow;
	guint Timer;

	(GtkObject *)LoadWindow = CreateWaitWindow("Telechargements de vols en cours..");
	Timer = gtk_timeout_add(50, (GtkFunction)UpdateWaitWindow, LoadWindow);

	while (gtk_events_pending())
		gtk_main_iteration();
	if ((n = GPS_Command_Get_Track(port, &trk)) < 0) {
		Print_Error(n);
		gtk_timeout_remove(Timer);
		gtk_widget_destroy(LoadWindow);
		return;
	}
	Do_Off(port);
	gtk_timeout_remove(Timer);
	gtk_widget_destroy(LoadWindow);
	Print_Track(n, MainWindow);


	return;
}







/* Procedure principale*/
void Readtrack(char *port, GtkWidget *MainWindow)
{
	if (GPS_Init(port) < 0) {
		GWarning("Not a Garmin GPS, GPS off/disconnected/supervisor mode\n");
		return;
	}
	Do_Trkrec(port, MainWindow);

	return;
}
