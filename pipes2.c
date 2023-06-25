#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int convToLogPipe[2];   // Pipe zwischen Conv und Log
int convToStatPipe[2];  // Pipe zwischen Conv und Stat
int statToReportPipe[2];  // Pipe zwischen Stat und Report

// Signalhandler für SIGINT
void sigintHandler(int signum)
{
    printf("Program beendet.\n");
    // Schließe und entferne die Pipes
    close(convToLogPipe[0]);
    close(convToLogPipe[1]);
    close(convToStatPipe[0]);
    close(convToStatPipe[1]);
    close(statToReportPipe[0]);
    close(statToReportPipe[1]);
    exit(signum);
}

// Conv-Prozess
void convProcess()
{
    close(convToLogPipe[0]);  // Schließe den Lese-Ende der Pipe für Log
    close(convToStatPipe[0]); // Schließe den Lese-Ende der Pipe für Stat
    close(statToReportPipe[0]); // Schließe den Lese-Ende der Pipe für Report

    // Seed für die Zufallszahlgenerierung initialisieren
    srand((unsigned int)time(NULL));

    while (1) {
        // Generiere eine Zufallszahl
        int value = rand() % 100;

        // Schreibe die Zufallszahl in die Pipe für Log
        write(convToLogPipe[1], &value, sizeof(int));

        // Schreibe die Zufallszahl in die Pipe für Stat
        write(convToStatPipe[1], &value, sizeof(int));

        // Kurze Pause, um das Echtzeitverhalten zu simulieren
        usleep(100000);
    }
}

// Log-Prozess
void logProcess()
{
    close(convToLogPipe[1]);  // Schließe den Schreib-Ende der Pipe von Conv

    FILE *file = fopen("pipes2.txt", "w");
    if (file == NULL) {
        perror("Failed to open log file");
        exit(1);
    }

    while (1) {
        int value;

        // Lies die Messwerte von Conv aus der Pipe
        read(convToLogPipe[0], &value, sizeof(int));

        // Schreibe den Messwert in die Datei
        fprintf(file, "%d\n", value);

        // Flushe den Dateipuffer
        fflush(file);
    }

    fclose(file);
}

// Stat-Prozess
void statProcess()
{
    close(convToStatPipe[1]);   // Schließe den Schreib-Ende der Pipe von Conv
    close(statToReportPipe[0]); // Schließe den Lese-Ende der Pipe von Report

    int sum = 0;
    int count = 0;

    while (1) {
        int value;

        // Lies die Messwerte von Conv aus der Pipe
        read(convToStatPipe[0], &value, sizeof(int));

        // Berechne die Statistik
        sum += value;
        count++;

        // Schreibe die Statistik in die Pipe für Report
        write(statToReportPipe[1], &sum, sizeof(int));
        write(statToReportPipe[1], &count, sizeof(int));
    }
}

// Report-Prozess
void reportProcess()
{
    close(statToReportPipe[1]); // Schließe den Schreib-Ende der Pipe von Stat

    while (1) {
        int sum;
        int count;

        // Lies die Statistikdaten von Stat aus der Pipe
        read(statToReportPipe[0], &sum, sizeof(int));
        read(statToReportPipe[0], &count, sizeof(int));

        // Berechne den Durchschnitt
        double average = (double)sum / count;

        // Gib die statistischen Daten in der Shell aus
        printf("Sum: %d, Count: %d, Average: %.2f\n", sum, count, average);
    }
}

int main()
{
    // Erzeuge die Pipes
    if (pipe(convToLogPipe) == -1) {
        perror("Failed to create pipe for Conv and Log");
        exit(1);
    }
    if (pipe(convToStatPipe) == -1) {
        perror("Failed to create pipe for Conv and Stat");
        exit(1);
    }
    if (pipe(statToReportPipe) == -1) {
        perror("Failed to create pipe for Stat and Report");
        exit(1);
    }

    // Signalhandler für SIGINT registrieren
    signal(SIGINT, sigintHandler);

    // Starte die Endlosprozesse
    pid_t convPID, logPID, statPID, reportPID;

    convPID = fork();
    if (convPID == 0) {
        // Conv-Prozess
        convProcess();
    } else if (convPID > 0) {
        logPID = fork();
        if (logPID == 0) {
            // Log-Prozess
            logProcess();
        } else if (logPID > 0) {
            statPID = fork();
            if (statPID == 0) {
                // Stat-Prozess
                statProcess();
            } else if (statPID > 0) {
                reportPID = fork();
                if (reportPID == 0) {
                    // Report-Prozess
                    reportProcess();
                } else if (reportPID > 0) {
                    // Elternprozess
                    // Warte auf das Ende aller Kindprozesse
                    wait(NULL);
                    wait(NULL);
                    wait(NULL);
                    wait(NULL);
                } else {
                    perror("Failed to fork Report process");
                    exit(1);
                }
            } else {
                perror("Failed to fork Stat process");
                exit(1);
            }
        } else {
            perror("Failed to fork Log process");
            exit(1);
        }
    } else {
        perror("Failed to fork Conv process");
        exit(1);
    }

    return 0;
}
