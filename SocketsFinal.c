#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/sem.h>

#define _XOPEN_SOURCE
#define PORT 12345
#define BUFFER_SIZE 1024


pid_t conv_pid, log_pid, stat_pid, report_pid;
    int conv_sockets[2], log_sockets[2], stat_sockets[2], report_sockets[2];

struct stat {
    int sum;
    int average;
    
} ;

void conv_process(int socket) {
    // Logik des Conv-Prozesses
   
    // Zufallszahlen generieren und über den Socket senden
    
    srand(getpid()); // Zufallszahlengenerator mit der Prozess-ID initialisieren

    while (1) {
      
        int random_number = rand() % 100;

        send(socket, &random_number, sizeof(random_number), 0);

       
        sleep(1); // 1 Sekunde warten zwischen dem Senden von Zahlen

        
    }
        

}

void log_process(int socket) {
    // Logik des Log-Prozesses
   
    int received_number; // Werte vom Conv-Prozess über den Socket empfangen und ausgeben

    while (1) {

        recv(socket, &received_number, sizeof(received_number), 0);

        FILE *file =
        fopen("sockets.txt", "a"); // Beispielhafte Datei zum Schreiben öffnen
      
        if (file == NULL) {
        perror("fopen - Log");
        exit(1);
      
            
        }
        
      fprintf(file, "%d\n",received_number); // Messwert in die Datei schreiben
     
      fclose(file);
     
        send(log_sockets[1], &received_number, sizeof(received_number), 0);
       
       
      

    }
}

void stat_process(int socket) {
    // Logik des Stat-Prozesses
    // Werte vom Conv-Prozess über den Socket empfangen und statistische Berechnungen durchführen
    int received_number;
    int sum = 0;
    int count = 0;

    while (1) {

        recv(log_sockets[0], &received_number, sizeof(received_number), 0);
        
        sum += received_number;
        count++;
        double average = (double) sum / count;
       
        // Summe und Durschnitt über den Socket senden
        struct stat st;
        st.sum=sum;
        st.average=average;
        
        send(stat_sockets[1], &st, sizeof(st), 0);
        



        sleep(1);

    }
}

void report_process(int socket) {
    // Logik des Report-Prozesses
    // Werte vom Stat-Prozess über den Socket empfangen und Berichte generieren
    int received_number;
    struct stat st;

    while (1) {

        recv(socket, &st, sizeof(st), 0);
        printf("Empfangene Zahlen Report : summe: %d - durchshnitt: %d\n", st.sum, st.average);
        
    }
}

void sigint_handler(int signum) {
    // Signalhandler für SIGINT (Strg+C)
    // Ressourcen freigeben und das Programm ordnungsgemäß beenden
    // Hier den Code zum Schließen von Sockets, Freigeben von Speicher usw. hinzufügen

    // Sockets schließen
    close(conv_sockets[0]);
    close(conv_sockets[1]);
    close(log_sockets[0]);
    close(log_sockets[1]);
    close(stat_sockets[0]);
    close(stat_sockets[1]);
    close(report_sockets[0]);
    close(report_sockets[1]);

    // Kindprozesse beenden
    kill(conv_pid, SIGKILL);
    kill(log_pid, SIGKILL);
    kill(stat_pid, SIGKILL);
    kill(report_pid, SIGKILL);

    // Auf das Beenden der Kindprozesse warten
    int status;
    pid_t pid;
    while ((pid = wait(&status)) > 0) {
        printf("Kindprozess mit PID %d beendet.\n", pid);
    }

    // Speicher freigeben, falls zutreffend

    exit(0);
}


 
int main() {
    signal(SIGINT, sigint_handler); // SIGINT-Signalhandler registrieren

  
    // Sockets für die Kommunikation zwischen Prozessen erstellen
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, conv_sockets) < 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, log_sockets) < 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, stat_sockets) < 0 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, report_sockets) < 0) {
        perror("Fehler beim Erstellen der Sockets");
        exit(1);
    }

    // Conv-Prozess starten
    conv_pid = fork();
    if (conv_pid == 0) {
        // Kindprozess: Conv
        close(conv_sockets[0]);
        conv_process(conv_sockets[1]);
        exit(0);
    } else if (conv_pid > 0) {
        // Elternprozess
        printf("Conv-Prozess gestartet mit PID: %d\n", conv_pid);
        close(conv_sockets[1]);
    } else {
        // Fork fehlgeschlagen
        perror("Fehler beim Starten des Conv-Prozesses");
        exit(1);
    }

    // Log-Prozess starten
    log_pid = fork();
    if (log_pid == 0) {
        // Kindprozess: Log
        close(log_sockets[0]);
        log_process(conv_sockets[0]);
        exit(0);
    } else if (log_pid > 0) {
        // Elternprozess
        printf("Log-Prozess gestartet mit PID: %d\n", log_pid);
        close(log_sockets[1]);
       // kill(log_pid,SIGSTOP);
    } else {
        // Fork fehlgeschlagen
        perror("Fehler beim Starten des Log-Prozesses");
        exit(1);
    }

    // Stat-Prozess starten
    stat_pid = fork();
    if (stat_pid == 0) {
        // Kindprozess: Stat
        close(stat_sockets[0]);
        stat_process(conv_sockets[0]);
        exit(0);
    } else if (stat_pid > 0) {
        // Elternprozess
        printf("Stat-Prozess gestartet mit PID: %d\n", stat_pid);
        close(stat_sockets[1]);
        //kill(stat_pid,SIGSTOP);
    } else {
        // Fork fehlgeschlagen
        perror("Fehler beim Starten des Stat-Prozesses");
        exit(1);
    }

    // Report-Prozess starten
    report_pid = fork();
    if (report_pid == 0) {
        // Kindprozess: Report
        close(report_sockets[0]);
        report_process(stat_sockets[0]);
        exit(0);
    } else if (report_pid > 0) {
        // Elternprozess
        printf("Report-Prozess gestartet mit PID: %d\n", report_pid);
        close(report_sockets[1]);

    } else {
        // Fork fehlgeschlagen
        perror("Fehler beim Starten des Report-Prozesses");
        exit(1);
    }

    // Auf Beendigung aller Kindprozesse warten
    int status;
    pid_t pid;
    while ((pid = wait(&status)) > 0) {
        printf("Kindprozess mit PID %d beendet.\n", pid);
    }

    return 0;
}
