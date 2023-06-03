#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {

  int conv_pid, log_pid, stat_pid, report_pid;

  // Starte den Conv-Prozess

  conv_pid = fork();

  if (conv_pid > 0) {
    printf("Hier laeuft Elternprozess\n");
    printf("[Eltern] Eigene PID: %d\n", getpid());

  } else if (conv_pid == 0) {
    printf("Hier laeuft Kindprozess\n");
    printf("[Kind] PID: %d\n", getpid());
    printf("[Kind] PID des Vaters: %d\n", getppid());
    execl("/Users/diemquynh02/Projekt Demo/Conv", "Conv", "-u", NULL);
    perror("Fehler beim Starten von Conv");
    exit(1);
  } else if (conv_pid == -1) {
    perror("\n Es kam bei fork() zu einem Fehler!");
  }

  // Starte den Log-Prozess
  log_pid = fork();
  if (log_pid > 0) {
    printf("Hier laeuft Elternprozess\n");
    printf("[Eltern] Eigene PID: %d\n", getpid());

  } else if (log_pid == 0) {
    printf("Hier laeuft Kindprozess\n");
    printf("[Kind] PID: %d\n", getpid());
    printf("[Kind] PID des Vaters: %d\n", getppid());
    execl("/Users/diemquynh02/Projekt Demo/Log", "Log", "-u", NULL);
    perror("Fehler beim Starten von Log");
    exit(1);
  } else if (log_pid == -1) {
    perror("\n Es kam bei fork() zu einem Fehler!");
  }

  // Starte den Stat-Prozess

  stat_pid = fork();

  if (stat_pid > 0) {
    printf("Hier laeuft Elternprozess\n");
    printf("[Eltern] Eigene PID: %d\n", getpid());

  } else if (stat_pid == 0) {
    printf("Hier laeuft Kindprozess\n");
    printf("[Kind] PID: %d\n", getpid());
    printf("[Kind] PID des Vaters: %d\n", getppid());
    execl("/Users/diemquynh02/Projekt Demo/Stat", "Stat", "-u", NULL);
    perror("Fehler beim Starten von Stat");
    exit(1);
  } else if (stat_pid == -1) {
    perror("\n Es kam bei fork() zu einem Fehler!");
  }

  // Starte den Report-Prozess

  report_pid = fork();
  if (conv_pid > 0) {
    printf("Hier laeuft Elternprozess\n");
    printf("[Eltern] Eigene PID: %d\n", getpid());

  } else if (report_pid == 0) {
    printf("Hier laeuft Kindprozess\n");
    printf("[Kind] PID: %d\n", getpid());
    printf("[Kind] PID des Vaters: %d\n", getppid());
    execl("/Users/diemquynh02/Projekt Demo/Report", "Report", "-u", NULL);
    perror("Fehler beim Starten von Report");
    exit(1);
  } else if (report_pid == -1) {
    perror("\n Es kam bei fork() zu einem Fehler!");
  }

  int status;
  waitpid(conv_pid, &status, 0);
  waitpid(log_pid, &status, 0);
  waitpid(stat_pid, &status, 0);
  waitpid(report_pid, &status, 0);
  return 0;
}
