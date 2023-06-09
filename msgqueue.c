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

// Signalhandler für SIGINT (Ctrl-C)
void sigintHandler(int sig) {
  // Freigabe von Ressourcen, z.B. Message Queues
  msgctl(123, IPC_RMID, NULL);
  msgctl(123, IPC_RMID, NULL);
  msgctl(123, IPC_RMID, NULL);
  msgctl(123, IPC_RMID, NULL);
  exit(0);
}
int conv_pid, log_pid, stat_pid, report_pid;

int main() {
  // Erstellen der Message Queues
  key_t key_conv = ftok(".", 'C');
  int msgid_conv = msgget(key_conv, IPC_CREAT | 0666);

  key_t key_log = ftok(".", 'L');
  int msgid_log = msgget(key_log, IPC_CREAT | 0666);

  key_t key_stat = ftok(".", 'S');
  int msgid_stat = msgget(key_stat, IPC_CREAT | 0666);

  key_t key_report = ftok(".", 'R');
  int msgid_report = msgget(key_report, IPC_CREAT | 0666);

  // Signalhandler für SIGINT registrieren
  signal(SIGINT, sigintHandler);

  // Fork-Prozess für Conv
  conv_pid = fork();
  if (conv_pid == 0) {
    // Conv-Prozess
    while (1) {
      // for (int i = 0; i < 10; ++i) {
      //  Messwerte erzeugen und in die Conv-Message Queue schreiben
      int value = rand() % 100; // Zufälliger Messwert (Beispiel)
      struct message msg;
      msg.mtype = 1; // Beispielhafter Nachrichtentyp
      sprintf(msg.mtext, "%d",
              value); // Messwert in den Textbereich der Nachricht schreiben
      // msgsnd(msgid_conv, &msg, sizeof(msg), 0);
      if (msgsnd(msgid_conv, &msg, sizeof(msg.mtext), IPC_NOWAIT) == -1) {
        printf("Das Schreiben des Werts ist fehlgeschlagen. \n");
        perror("msgsnd");
        exit(1);
      }
      sleep(1); // Zeitverzögerung für den Beispielzweck
    }
  }

  // Fork-Prozess für Log
  log_pid = fork();
  if (log_pid == 0) {
    // Log-Prozess
    while (1) {
      // for (int i = 0; i < 10; ++i) {
      //  Messwerte aus der Conv-Message Queue lesen und in eine lokale Datei
      //  schreiben
      struct message msg;
      int rc_msgrcv =
          msgrcv(msgid_conv, &msg, sizeof(msg), 1, MSG_NOERROR | IPC_NOWAIT);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Log fehlgeschlagen.\n");
        perror("msgrcv");
        exit(1);
      }

      FILE *file =
          fopen("btryy2.txt", "a"); // Beispielhafte Datei zum Schreiben öffnen
      fprintf(file, "%s\n", msg.mtext); // Messwert in die Datei schreiben
      fclose(file);
    }
  }

  // Fork-Prozess für Stat
  stat_pid = fork();
  if (stat_pid == 0) {
    // Stat-Prozess
    int sum = 0;
    int count = 0;
    while (1) {
      // for (int i = 0; i < 10; ++i) {
      // Messwerte aus der Conv-Message Queue lesen und statistische Daten
      // berechnen
      struct message msg;

      int rc_msgrcv = msgrcv(msgid_conv, &msg, sizeof(msg.mtext), msg.mtype,
                             MSG_NOERROR | IPC_NOWAIT);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Stat fehlgeschlagen.\n");
        perror("msgrcv");
        exit(1);
      }

      int value = atoi(
          msg.mtext); // Messwert aus dem Textbereich der Nachricht extrahieren
      sum += value;
      count++;
      float average = (float)sum / count;
      // Ergebnisse in die Stat-Message Queue schreiben
      sprintf(msg.mtext, "Sum: %d, Average: %.2f", sum, average);

      if (msgsnd(msgid_stat, &msg, sizeof(msg.mtext), IPC_NOWAIT) == -1) {
        printf("Das Senden der Nachricht bei Stat ist fehlgeschlagen.\n");
        perror("msgsnd");
        exit(1);
      }
    }
  }

  // Fork-Prozess für Report
  report_pid = fork();
  if (report_pid == 0) {
    // Report-Prozess
    while (1) {
      // Statistikdaten aus der Stat-Message Queue lesen und in der Shell
      // ausgeben
      struct message msg;

      int rc_msgrcv = msgrcv(msgid_stat, &msg, sizeof(msg.mtext), msg.mtype,
                             MSG_NOERROR | IPC_NOWAIT);
      if (rc_msgrcv < 0) {
        printf("Lesen der Nachricht bei Report fehlgeschlagen.\n");
        perror("msgrcv");
        exit(1);
      } else {
        printf("Statistische Daten:: %s\n", msg.mtext);

        // printf("Statistical Data: %s\n", msg.mtext);
        sleep(5); // Zeitverzögerung für den Beispielzweck
      }
      //}
    }

    // Hauptprozess
    // Warten auf die Beendigung der Kindprozesse
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

    // Freigabe von Ressourcen, z.B. Message Queues
    msgctl(msgid_conv, IPC_RMID, 0);
    msgctl(msgid_log, IPC_RMID, 0);
    msgctl(msgid_stat, IPC_RMID, 0);
    msgctl(msgid_report, IPC_RMID, 0);

    return 0;
  }
}