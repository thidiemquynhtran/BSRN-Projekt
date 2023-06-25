#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int conv_log[2];		// Pipe zwischen Conv und Log
int conv_stat[2];		// Pipe zwischen Conv und Stat
int stat_report[2];		// Pipe zwischen Stat und Report

// Signal-Handler
void
sigintHandler (int signum)
{
  printf ("Programm beendet.\n");
  exit (signum);
}

int
main ()
{
  // Pipes erzeugen
  if (pipe (conv_log) == -1)
    {
      perror ("Fehler beim Erstellen der conv_log Pipe!");
      exit (1);
    }
  if (pipe (conv_stat) == -1)
    {
      perror ("Fehler beim Erstellen der conv_stat Pipe!");
      exit (1);
    }
  if (pipe (stat_report) == -1)
    {
      perror ("Fehler beim Erstellen der stat_report Pipe!");
      exit (1);
    }

  // SIGINT als Signal fuer Signal-Handler festlegen 
  signal (SIGINT, sigintHandler);

  // Prozesse werden gestartet
  pid_t convPID, logPID, statPID, reportPID;

  convPID = fork ();

  if (convPID == -1)
    {
      perror ("Fehler bei fork (Conv)!");
      exit (1);
    }

  if (convPID == 0)
    {
      // PID's von Conv und Elternprozess ausgeben
      printf ("\n Conv: PID: %i, PPID: %i\n", getpid (), getppid ());

      // Conv-Prozess

      close (conv_log[0]);	// Lese-Ende schliessen 
      close (conv_stat[0]);	// Lese-Ende schliessen
      close (stat_report[0]);	// Lese-Ende schliessen

      // Seed initialisieren fuer Wertegenerierung
      srand (time (NULL));

      // While-Schleife starten
      while (1)
	{
	  // Zufallszahl zwischen 0 und 99 generieren und speichern
	  int value = rand () % 100;

	  // Messwert fuer Log reinschreiben
	  write (conv_log[1], &value, sizeof (int));

	  // Messwert fuer Stat reinschreiben
	  write (conv_stat[1], &value, sizeof (int));

	  // Kurze Pause einbauen
	  usleep (500000);
	}
    }
  else if (convPID > 0)
    {

      logPID = fork ();

      if (logPID == -1)
	{
	  perror ("Fehler bei fork (Log)!");
	  exit (1);
	}

      if (logPID == 0)
	{
	  // PID's von Log und Elternprozess ausgeben
	  printf ("\n Log: PID: %i, PPID: %i\n", getpid (), getppid ());

	  // Log-Prozess
	  close (conv_log[1]);	// Schreib-Ende schliessen

	  // Lokale Datei oeffnen
	  FILE *fp = fopen ("log.txt", "w");
	  if (fp == NULL)
	    {
	      perror ("Fehler beim oeffnen der log.txt Datei");
	      exit (1);
	    }
	  // While-Schleife
	  while (1)
	    {
	      int zahl;

	      // Messwerte aus Conv auslesen
	      read (conv_log[0], &zahl, sizeof (int));

	      // Messwerte in Datei schreiben
	      fprintf (fp, "Messwert: %d\n", zahl);

	      // Flushe den Dateipuffer
	      fflush (fp);
	    }
	  // Datei schliessen
	  fclose (fp);
	}
      else if (logPID > 0)
	{

	  statPID = fork ();

	  if (statPID == -1)
	    {
	      perror ("Fehler bei fork (Stat)!");
	      exit (1);
	    }

	  if (statPID == 0)
	    {
	      // PID's von Stat und Elternprozess ausgeben
	      printf ("\n Stat: PID: %i, PPID: %i\n", getpid (), getppid ());

	      // Stat-Prozess
	      close (conv_stat[1]);	// Schreib-Ende schliessen
	      close (stat_report[0]);	// Lese-Ende schliessen

	      int sum = 0;
	      int zaehler = 0;

	      // While-Schleife
	      while (1)
		{
		  int messwert;

		  // Messwerte von Conv auslesen
		  read (conv_stat[0], &messwert, sizeof (int));

		  // Summe berechnen und Anzahl um 1 erhoehen
		  sum += messwert;
		  zaehler++;

		  // Stat-Werte fC<r Report reinschreiben
		  write (stat_report[1], &sum, sizeof (int));
		  write (stat_report[1], &zaehler, sizeof (int));
		}
	    }
	  else if (statPID > 0)
	    {

	      reportPID = fork ();

	      if (reportPID == -1)
		{
		  perror ("Fehler bei fork (Report)!");
		  exit (1);
		}

	      if (reportPID == 0)
		{
		  // PID's von Report und Elternprozess ausgeben
		  printf ("\n Report: PID: %i, PPID: %i\n", getpid (),
			  getppid ());

		  // Report-Prozess

		  close (stat_report[1]);	// Schreib-Ende schliessen

		  // While-Schleife
		  while (1)
		    {
		      int sum;
		      int zaehler;

		      // Stat-Daten auslesen
		      read (stat_report[0], &sum, sizeof (int));
		      read (stat_report[0], &zaehler, sizeof (int));

		      // Mittelwert berechnen
		      double mw = (double) sum / zaehler;

		      // Ausgabe der Stat-Daten
		      printf ("Summe: %d, Anzahl: %d, Mittelwert: %.2f\n",
			      sum, zaehler, mw);
		    }
		}
	      else if (reportPID > 0)
		{
		  // Elternprozess

		  // Warten auf Beendigung der Prozesse
		  wait (NULL);
		  wait (NULL);
		  wait (NULL);
		  wait (NULL);
		}
	    }
	}
    }

  return 0;
}
