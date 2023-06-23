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

  //Bei 'CTRl-C'  wird Signal-Handler aktiviert
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

      int id = fork ();

      if (id == 0)
	{
	  //Conv-Prozess
	  int werte[5];

	  for (int i = 0; i < 5; i++)
	    {
	      werte[i] = rand () % 101;
	    }

	  close (fd[0]);
	  write (fd[1], werte, sizeof (int) * 5);
	  close (fd[1]);
	}
      else
	{

	  //Stat-Prozess
	  close (fd[1]);
	  int werte[5];
	  read (fd[0], werte, sizeof (int) * 5);
	  close (fd[0]);

	  int summe = 0;

	  for (int i = 0; i < 5; i++)
	    {
	      summe += werte[i];
	    }

	  int mittelwert = summe / 5;

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

	  //Log-Prozess
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
	      fprintf (fp, "Messwert:  %.2d\n", werte[i]);

	    }
	  fclose (fp);

	  //Warten auf Conv-Prozess
	  wait (NULL);

	  int id1 = fork ();

	  if (id1 == 0)
	    {

	      //Summe reinschreiben
	      close (fd1[0]);
	      write (fd1[1], &summe, sizeof (int));
	      close (fd1[1]);

	      //Mittelwert reinschreiben
	      close (fd2[0]);
	      write (fd2[1], &mittelwert, sizeof (int));
	      close (fd2[1]);

	    }
	  else
	    {
	      //Report-Prozess
	      //Summe auslesen
	      int summe;
	      close (fd1[1]);
	      read (fd1[0], &summe, sizeof (int));
	      close (fd1[0]);

	      //Mittelwert auslesen
	      int mittelwert;
	      close (fd2[1]);
	      read (fd2[0], &mittelwert, sizeof (int));
	      close (fd2[0]);

	      //Summe und Mittelwert ausgeben
	      printf ("\nSumme: %d", summe);
	      printf ("\nMittelwert: %d", mittelwert);
	      printf ("\n");

	      //Warten auf Stat-Prozess 
	      wait (NULL);
	    }

	}


    }
  return 0;
}
