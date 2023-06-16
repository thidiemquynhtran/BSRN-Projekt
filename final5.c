#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_MESS_SIZE 100

// Struktur für die Nachrichten zwischen Prozessen
struct message {
  long mtype;
  char mtext[MAX_MESS_SIZE];
};

// Globale Variablen für die Message Queues
int msgid_conv, msgid_log, msgid_stat, msgid_report;

// Signalhandler für SIGINT (Ctrl-C)
void sigintHandler(int signum) {
  // Freigabe von Message Queues
  if (msgctl(msgid_conv, IPC_RMID, NULL) == -1) {
    perror("msgctl - Conv");
    exit(1);
  } else {
    printf("\nConv-Message Queue mit ID %i wurde geloescht.\n", msgid_conv);
  }

  if (msgctl(msgid_log, IPC_RMID, NULL) == -1) {
    perror("msgctl - Log");
    exit(1);
  } else {
    printf("Log-Message Queue mit ID %i wurde geloescht.\n", msgid_log);
  }

  if (msgctl(msgid_stat, IPC_RMID, NULL) == -1) {
    perror("msgctl - Stat");
    exit(1);
  } else {
    printf("Stat-Message Queue mit ID %i wurde geloescht.\n", msgid_stat);
  }

  if (msgctl(msgid_report, IPC_RMID, NULL) == -1) {
    perror("msgctl - Report");
    exit(1);
  } else {
    printf("Report-Message Queue mit ID %i wurde geloescht.\n", msgid_report);
  }
  printf("Programm beendet.\n");
  exit(0);
}

int main() {
  // Erstellen der Message Queues
  key_t key_conv = ftok(".", 'C');
  msgid_conv = msgget(key_conv, IPC_CREAT | 0666);
  if (msgid_conv == -1) {
    perror("msgget - Conv");
    exit(1);
  }

  key_t key_log = ftok(".", 'L');
  msgid_log = msgget(key_log, IPC_CREAT | 0666);
  if (msgid_log == -1) {
    perror("msgget - Log");
    exit(1);
  }

  key_t key_stat = ftok(".", 'S');
  msgid_stat = msgget(key_stat, IPC_CREAT | 0666);
  if (msgid_stat == -1) {
    perror("msgget - Stat");
    exit(1);
  }

  key_t key_report = ftok(".", 'R');
  msgid_report = msgget(key_report, IPC_CREAT | 0666);
  if (msgid_report == -1) {
    perror("msgget - Report");
    exit(1);
  }

  int conv_pid, log_pid, stat_pid, report_pid;

  // Starte den Conv-Prozess mit fork-Systemaufruf
  conv_pid = fork();
  if (conv_pid == 0) {
    printf("\n Conv: PID: %i", getpid());
    printf("\n Conv: PPID: %i", getppid());
    // Conv-Prozess
    while (1) {
      // Messwerte (Zufallszahlen) erzeugen und in die Conv-Message Queue
      // schreiben
      int wert = rand() % 100; 
      struct message msg;
      msg.mtype = 1; // Nachrichtentyp festlegen
      sprintf(msg.mtext, "%d",
              wert); // Messwert in den Textbereich der Nachricht schreiben
      if (msgsnd(msgid_conv, &msg, sizeof(msg.mtext), IPC_NOWAIT) == -1) {
        printf("Das Schreiben des Werts ist fehlgeschlagen.\n");
        perror("msgsnd - Conv");
        exit(1);
      }
      sleep(1); // Zeitverzögerung 
    }
  } else if (conv_pid == -1) {
    perror("\nEs kam bei fork() zu einem Fehler!");
  }

  // Starte den Log-Prozess mit fork-Systemaufruf
  log_pid = fork();
  if (log_pid == 0) {
    printf("\n Log: PID: %i", getpid());
    printf("\n Log: PPID: %i", getppid());
    //  Log-Prozess
    while (1) {
      // Messwerte aus der Conv-Message Queue lesen und in eine lokale Datei
      // schreiben
      struct message msg;
      int rc_msgrcv =
          msgrcv(msgid_conv, &msg, sizeof(msg.mtext), 1, MSG_NOERROR);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Log fehlgeschlagen.\n");
        perror("msgrcv - Log");
        exit(1);
      }
      FILE *file =
          fopen("final5.txt", "a"); // Beispielhafte Datei zum Schreiben öffnen
      if (file == NULL) {
        perror("fopen - Log");
        exit(1);
      }
      fprintf(file, "%s\n", msg.mtext); // Messwert in die Datei schreiben
      fclose(file);
    }
  } else if (log_pid == -1) {
    perror("\nEs kam bei fork() zu einem Fehler!");
  }

  // Starte den Stat-Prozess mit fork-Systemaufruf
  stat_pid = fork();
 
  if (stat_pid == 0) {
    printf("\n Stat: PID: %i", getpid());
    printf("\n Stat: PPID: %i", getppid());
    // Stat-Prozess
    int sum = 0;
    int counter = 0;
    while (1) {
      // Messwerte aus der Conv-Message Queue lesen und statistische Daten
      // berechnen
      struct message msg;
      int rc_msgrcv =
          msgrcv(msgid_conv, &msg, sizeof(msg.mtext), msg.mtype, MSG_NOERROR);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Stat fehlgeschlagen.\n");
        perror("msgrcv - Stat");
        exit(1);
      }
      int wert = atoi(
          msg.mtext); // Messwert aus dem Textbereich der Nachricht extrahieren
      sum += wert;
      counter++;
      float mittelwert = (float)sum / counter;
      // Ergebnisse in die Stat-Message Queue schreiben
      sprintf(msg.mtext,
              "\nAnzahl der Werte:%d\n Summe: %d\n Mittelwert: %.2f\n", counter,
              sum, mittelwert);
      if (msgsnd(msgid_stat, &msg, sizeof(msg.mtext), IPC_NOWAIT) == -1) {
        printf("Das Senden der Nachricht bei Stat ist fehlgeschlagen.\n");
        perror("msgsnd - Stat");
        exit(1);
      }
    }
  } else if (stat_pid == -1) {
    perror("\nEs kam bei fork() zu einem Fehler!");
  }

  // Starte den Report-Prozess mit fork-Systemaufruf
  report_pid = fork();
  if (report_pid == 0) {
    printf("\n Report: PID: %i", getpid());
    printf("\n Report: PPID: %i", getppid());
    // Report-Prozess
    while (1) {
      // Statistikdaten aus der Stat-Message Queue lesen und in der Shell
      // ausgeben
      struct message msg;
      int rc_msgrcv =
          msgrcv(msgid_stat, &msg, sizeof(msg.mtext), msg.mtype, MSG_NOERROR);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Report fehlgeschlagen.\n");
        perror("msgrcv - Report");
        exit(1);
      } else {
        printf("\nStatistische Daten: %s\n", msg.mtext);
        sleep(1);
      }
    }
  } else if (report_pid == -1) {
    perror("\nEs kam bei fork() zu einem Fehler!");
  }

  // Signalhandler für SIGINT registrieren
  struct sigaction sa;
  sa.sa_handler = sigintHandler;
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }
  // Warten auf die Beendigung der Kindprozesse
  waitpid(conv_pid, NULL, 0);
  waitpid(log_pid, NULL, 0);
  waitpid(stat_pid, NULL, 0);
  waitpid(report_pid, NULL, 0);

  // Freigabe von Message Queues
  if (msgctl(msgid_conv, IPC_RMID, NULL) == -1) {
    perror("msgctl - Conv");
    exit(1);
  }
  if (msgctl(msgid_log, IPC_RMID, NULL) == -1) {
    perror("msgctl - Log");
    exit(1);
  }
  if (msgctl(msgid_stat, IPC_RMID, NULL) == -1) {
    perror("msgctl - Stat");
    exit(1);
  }
  if (msgctl(msgid_report, IPC_RMID, NULL) == -1) {
    perror("msgctl - Report");
    exit(1);
  }

  return 0;
}
