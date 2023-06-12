# BSRN-Projekt
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>

#define MAX_MEM_SIZE 1020

// Datenstruktur für die gemeinsamen Daten
typedef struct {
    int data;
    int is_valid;
    int sum;
    int count;
    double average;
} SharedData;

// Gemeinsame Variablen für das Shared Memory und die Semaphorgruppe
int shm_id;
int sem_id;
SharedData *shared_data;    //char atatt SharedData

// Definition der Semaphore-Operationen 
struct sembuf p_operation = {0, -1, 0};  // P-Operation
struct sembuf v_operation = {0, 1, 0};   // V-Operation

// Funktion für den Konversionsprozess
void process_conv(SharedData* shared_data, int sem_id) {
    while (1) {
        // Zufälligen Messwert erzeugen
        int value = rand() % 100;

        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semop(sem_id, &p_operation, 1);

        // Messwert im gemeinsamen Speicher aktualisieren
        shared_data->data = value;
        shared_data->is_valid = 1;

        // Semaphore freigeben
        semop(sem_id, &v_operation, 1);

        usleep(100000); // Kurze Pause zwischen den Messungen
    }
}

// Funktion für den Log-Prozess
void process_log(SharedData* shared_data, int sem_id) {
    FILE* eingabedatei = fopen("LokaleDatei.txt", "w");
    if (eingabedatei == NULL) {
        perror("Fehler beim Öffnen der (Log) Datei");
        exit(1);
    }

    while (1) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semop(sem_id, &p_operation, 1);

        // Messwert aus dem gemeinsamen Speicher lesen
        int value = shared_data->data;
        int is_valid = shared_data->is_valid;

        // Semaphore freigeben
        semop(sem_id, &v_operation, 1);

        if (is_valid) {
            // Messwert in die Log-Datei schreiben
            fprintf(eingabedatei, "Messwert: %d\n", value);
        }

        usleep(100000); // Kurze Pause zwischen den Log-Einträgen
    }

    fclose(eingabedatei);
}

// Funktion für den Statistik-Prozess
void process_stat(SharedData* shared_data, int sem_id) {
    int sum = 0;
    int count = 0;

    while (1) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semop(sem_id, &p_operation, 1);

        int value = shared_data->data;
        int is_valid = shared_data->is_valid;

        // Semaphore freigeben
        semop(sem_id, &v_operation, 1);

        if (is_valid) {
            // Statistikdaten aktualisieren
            sum += value;
            count++;
            double average = (double) sum / count;

            // Statistikdaten im gemeinsamen Speicher aktualisieren
            semop(sem_id, &p_operation, 1);
            shared_data->sum = sum;
            shared_data->count = count;
            shared_data->average = average;
            semop(sem_id, &v_operation, 1); // Semaphore freigeben
        }

        usleep(100000); // Kurze Pause zwischen den Statistikberechnungen
    }
}

// Funktion für den Report-Prozess
void process_report(SharedData* shared_data, int sem_id) {
    while (1) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semop(sem_id, &p_operation, 1);

        int sum = shared_data->sum;
        int count = shared_data->count;
        double average = shared_data->average;

        // Semaphore freigeben
        semop(sem_id, &v_operation, 1);

        // Ausgabe der statistischen Daten
        printf("Summe: %d\n", sum);
        printf("Anzahl: %d\n", count);
        printf("Durchschnitt: %.2f\n", average);

        usleep(100000); // Kurze Pause zwischen den Report-Ausgaben
    }
}

// Signalhandler für SIGINT (Ctrl-C)
void signalHandler(int signal) {
    // Gemeinsames Speichersegment und Semaphorgruppe freigeben
    if (shmdt(shared_data) == -1) {
        perror("Fehler beim Freigeben des gemeinsamen Speichersegments");
        exit(1);
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("1: Fehler beim Löschen des gemeinsamen Speichersegments");
        exit(1);
    }

    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Freigeben der Semaphorgruppe");
        exit(1);
    }

    exit(0);
}

int main() {
    // Schlüsselwerte für das Shared Memory und die Semaphorgruppe
    int shared_memory_key = 12345;
    int sem_key = 54321;
    
    // Gemeinsames Speichersegment erzeugen
    shm_id = shmget(shared_memory_key, sizeof(SharedData), IPC_CREAT | 0600);
    if (shm_id < 0) {
        perror("Fehler beim Erstellen des gemeinsamen Speichersegments");
        exit(1);
    }

    
    
    //struktur der shared memory - pointer trennen--> ich gebe den pointer in der shared memory struktur rein, das geht nicht ich muss dies ternnen  // Gemeinsames Speichersegment anhängen
    shared_data = shmat(shm_id, NULL, 0);
    if (shared_data == (SharedData*) -1) {
        perror("Fehler beim Anhängen des gemeinsamen Speichersegments");
        exit(1);
    }

    // Semaphorgruppe erstellen
    sem_id = semget(shared_memory_key, 1, IPC_CREAT | 0600);
    if (sem_id < 0) {
        perror("Fehler beim Erstellen der Semaphorgruppe");
        exit(1);
    }

    // Anfangswert der Semaphore auf 1 setzen
    semctl(sem_id, 0, SETVAL, 1);

    // Signalhandler für SIGINT (Ctrl-C) registrieren
    signal(SIGINT, signalHandler);

    // Prozess-IDs für die Kindprozesse
    pid_t conv_pid, log_pid, stat_pid, report_pid;

    // Kindprozesse erstellen
    conv_pid = fork();
    if (conv_pid == 0) {
        process_conv(shared_data, sem_id);
    } else if (conv_pid < 0) {
        perror("Fehler beim Erstellen des Konversionsprozesses");
        exit(1);
    }

    log_pid = fork();
    if (log_pid == 0) {
        process_log(shared_data, sem_id);
    } else if (log_pid < 0) {
        perror("Fehler beim Erstellen des Log-Prozesses");
        exit(1);
    }

    stat_pid = fork();
    if (stat_pid == 0) {
        process_stat(shared_data, sem_id);
    } else if (stat_pid < 0) {
        perror("Fehler beim Erstellen des Statistik-Prozesses");
        exit(1);
    }

    report_pid = fork();
    if (report_pid == 0) {
        process_report(shared_data, sem_id);
    } else if (report_pid < 0) {
        perror("Fehler beim Erstellen des Report-Prozesses");
        exit(1);
    }

    // Elternprozess
    while (1) {
        sleep(1); // Hauptprozess läuft weiter
    }

    return 0;
}

