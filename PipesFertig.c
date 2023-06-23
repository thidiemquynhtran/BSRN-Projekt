#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wait.h>
#include <signal.h>

//Wert fC<r while-Schleife
static int keepRunning = 1;

//Signal-Handler
void
intHandler (int signal)
{
  keepRunning = 0;
}

int
main ()
{

  //Bei 'CTRl-C' wird Signal-Handler aktiviert
  signal (SIGINT, intHandler);

  //Endlosprozesse werden gestartet
  while (keepRunning == 1)
    {

      //erste Pipe fC<r Messwerte
      int fd[2];
      if (pipe (fd) == -1)
	{
	  printf ("\nerror");
	  exit (1);
	}

      //zweite Pipe fC<r Summe
      int fd1[2];
      if (pipe (fd1) == -1)
	{
	  printf ("\nerror");
	  exit (1);
	}

      //dritte Pipe fC<r Mittelwert
      int fd2[2];
      if (pipe (fd2) == -1)
	{
	  printf ("\nerror");
	  exit (1);
	}

      int id = fork ();

      if (id == 0)
	{
	  //Conv-Prozess
	  int werte[5];

	  //Zufalls-Messwerte werden erzeugt
	  for (int i = 0; i < 5; i++)
	    {
	      werte[i] = rand () % 101;
	    }

	  //Messwerte in Pipe schreiben
	  close (fd[0]);
	  write (fd[1], werte, sizeof (int) * 5);
	  close (fd[1]);
	}
      else
	{

	  //Log-Prozess

	  //Messwerte von Conv aus Pipe lesen
	  close (fd[1]);
	  int werte1[5];
	  read (fd[0], werte1, sizeof (int) * 5);
	  close (fd[0]);

	  FILE *fp;

	  //Lokale Datei C6ffnen
	  fp = fopen ("log.txt", "w");

	  if (fp == NULL)
	    {
	      printf ("\nDatei konnte nicht geoeffnet werden!");
	    }
	  //Messwerte aus Conv reinschreiben
	  for (int i = 0; i < 5; i++)
	    {
	      fprintf (fp, "Messwert:  %.2d\n", werte1[i]);

	    }
	  //Lokale Datei schliessen
	  fclose (fp);

	  //Warten auf Conv-Prozess
	  wait (NULL);

	  int id1 = fork ();

	  if (id1 == 0)
	    {
	      //Stat-Prozess

	      //Messwerte von Conv aus Pipe lesen
	      close (fd[1]);
	      int werte2[5];
	      read (fd[0], werte2, sizeof (int) * 5);
	      close (fd[0]);

	      //Summe berechnen
	      int summe = 0;

	      for (int i = 0; i < 5; i++)
		{
		  summe += werte2[i];
		}

	      //Mittrlwert berechnen
	      int mittelwert = summe / 5;



	      //Summe in Pipe schreiben
	      close (fd1[0]);
	      write (fd1[1], &summe, sizeof (int));
	      close (fd1[1]);

	      //Mittelwert in Pipe schreiben
	      close (fd2[0]);
	      write (fd2[1], &mittelwert, sizeof (int));
	      close (fd2[1]);

	    }
	  else
	    {
	      //Report-Prozess
	      //Summe auslesen
	      int sum;
	      close (fd1[1]);
	      read (fd1[0], &sum, sizeof (int));
	      close (fd1[0]);

	      //Mittelwert auslesen
	      int mw;
	      close (fd2[1]);
	      read (fd2[0], &mw, sizeof (int));
	      close (fd2[0]);

	      //Summe und Mittelwert ausgeben
	      printf ("\nSumme: %d", sum);
	      printf ("\nMittelwert: %d", mw);
	      printf ("\n");

	      //Warten auf Stat-Prozess 
	      wait (NULL);
	    }

	}

    }
  return 0;
}
