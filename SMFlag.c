# BSRN-Projekt
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <string.h>
#include <math.h> // Hinzugefügte Bibliothek

#define SHARED_MEMORY_KEY 1111
#define SEMAPHORE_KEY 9999
#define NUM_RANDOM_NUMBERS 1

int flag = 1;

// Semaphore initialisieren
int initialize_semaphore() {
    int sem_id = semget(SEMAPHORE_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Fehler beim Erstellen des Semaphors");
        exit(1);
    }

    // Semaphore auf 1 setzen
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Fehler beim Initialisieren des Semaphors");
        exit(1);
    }

    return sem_id;
}

// Semaphore lock
void semaphore_lock(int sem_id) {
    struct sembuf sem_lock = {0, -1, 0};
    if (semop(sem_id, &sem_lock, 1) == -1) {
        perror("Fehler beim Sperren des Semaphors");
        exit(1);
    }
}

// Semaphore unlock
void semaphore_unlock(int sem_id) {
    struct sembuf sem_unlock = {0, 1, 0};
    if (semop(sem_id, &sem_unlock, 1) == -1) {
        perror("Fehler beim Freigeben des Semaphors");
        exit(1);
    }
}

// SIGINT-Signal-Handler
void sigint_handler(int signum) {
    printf("\nSIGINT-Signal empfangen. Beende das Programm...\n");
    flag = 0;
}

int main(int argc, char **argv) {
    int shared_memory_id;
    int sem_id;
    int returncode_shmctl;
    int returncode_shmdt;
    double* shared_memory_data;  // Zeiger auf gemeinsamen Speicher
    pid_t conv_pid, log_pid, stat_pid, report_pid;

    // Gemeinsames Speichersegment erstellen oder abrufen
    shared_memory_id = shmget(SHARED_MEMORY_KEY, sizeof(double) * (NUM_RANDOM_NUMBERS + 3), IPC_CREAT | 0600);
    if (shared_memory_id < 0) {
        perror("Das Segment konnte nicht erstellt oder abgerufen werden");
        exit(1);
    } else {
        printf("Shared Memory ID: %d\n", shared_memory_id);
    }

    // Gemeinsames Speichersegment anhängen
    shared_memory_data = shmat(shared_memory_id, NULL, 0);
    if (shared_memory_data == (double *)-1) {
        perror("Das Segment konnte nicht angehängt werden");
        exit(1);
    } else {
        printf("Shared Memory angehängt.\n");
    }

    // SIGINT-Signal-Handler registrieren
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("Fehler beim Registrieren des SIGINT-Signal-Handlers");
        exit(1);
    }

    // Semaphore initialisieren
    sem_id = initialize_semaphore();
    printf("Semaphore ID: %d\n", sem_id);

    conv_pid = fork();
    if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konversionsprozesses");
        exit(1);
    } else if (conv_pid == 0) {
        while (flag) {
            // Semaphore locken
            semaphore_lock(sem_id);

            int random_number = rand() % 100;  // Generiere eine Zufallszahl zwischen 0 und 99
        
        // Zufallszahlen in das gemeinsame Speichersegment schreiben
            for (int i = 0; i < NUM_RANDOM_NUMBERS; i++) {
                shared_memory_data[i] = (double) random_number;
            }

            // Semaphore freigeben
            semaphore_unlock(sem_id);

            // Kurze Pause
            sleep(1);
        }

        // Kindprozess 'conv' beenden
        exit(0);
    }

    log_pid = fork();
    if (log_pid == -1) {
        perror("Fehler beim Erstellen des Log-Prozesses");
        exit(1);
    } else if (log_pid == 0) {
        // Kindprozess 'log' hat Zugriff auf shared_memory_data
        FILE* log_file = fopen("SMFlag.txt", "w");
        if (log_file == NULL) {
            perror("Fehler beim Öffnen der (Log) Datei");
            exit(1);
        }

        while (flag) {
            // Semaphore locken
            semaphore_lock(sem_id);

            // Zahlen aus dem gemeinsamen Speicher lesen und in die Datei schreiben
            for (int i = 0; i < NUM_RANDOM_NUMBERS; i++) {
                fprintf(log_file, "Zufällig generierte Zahl:  %.2f\n", shared_memory_data[i]);

            }

            // Semaphore freigeben
            semaphore_unlock(sem_id);

            // Kurze Pause zwischen den Log-Einträgen
            sleep(1);
        }

        fclose(log_file);
        exit(0);
    }

    stat_pid = fork();
    if (stat_pid == -1) {
        perror("Fehler beim Erstellen des Statistik-Prozesses");
        exit(1);
    } else if (stat_pid == 0) {
        // Kindprozess 'stat' hat Zugriff auf shared_memory_data
        double sum = 0;
        int count = 0;
        double average = 0.0;

        while (flag) {
            // Semaphore locken
            semaphore_lock(sem_id);

            // Zahlen aus dem gemeinsamen Speicher lesen und Statistiken berechnen
            for (int i = 0; i < NUM_RANDOM_NUMBERS; i++) {
                sum += shared_memory_data[i];
                count++;
            }

            // Mittelwert berechnen
            if (count > 0) {
                average = sum / count;
            }

            // Statistische Daten in das gemeinsame Speichersegment schreiben
            shared_memory_data[NUM_RANDOM_NUMBERS] = sum;
            shared_memory_data[NUM_RANDOM_NUMBERS + 1] = count;
            shared_memory_data[NUM_RANDOM_NUMBERS + 2] = average;

            // Semaphore freigeben
            semaphore_unlock(sem_id);

            // Pause zwischen den Statistikberechnungen
            sleep(1);
        }

        exit(0);
    }

    report_pid = fork();
    if (report_pid == -1) {
        perror("Fehler beim Erstellen des Report-Prozesses");
        exit(1);
    } else if (report_pid == 0) {
        while (flag) {
            // Semaphore locken
            semaphore_lock(sem_id);

            // Statistische Daten aus dem gemeinsamen Speicher lesen
            double sum = shared_memory_data[NUM_RANDOM_NUMBERS];
            int count = shared_memory_data[NUM_RANDOM_NUMBERS + 1];
            double average = shared_memory_data[NUM_RANDOM_NUMBERS + 2];

            // Statistische Daten ausgeben
            printf("Summe: %.2f, Anzahl: %d, Durchschnitt: %.2f\n", sum, count, average);

            // Semaphore freigeben
            semaphore_unlock(sem_id);

            // Pause zwischen den Ausgaben
            sleep(1);
        }

        exit(0);
    }

    // Warten auf Beendigung der Kindprozesse
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

    // Semaphore entfernen
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
    } else {
        printf("Semaphore entfernt.\n");
    }

    // Gemeinsames Speichersegment trennen
    returncode_shmdt = shmdt(shared_memory_data);
    if (returncode_shmdt < 0) {
        printf("Das Segment konnte nicht getrennt werden.\n");
        perror("shmdt");
        exit(1);
    } else {
        printf("Das Segment wurde vom Prozess getrennt.\n");
    }

    // Gemeinsames Speichersegment löschen
    returncode_shmctl = shmctl(shared_memory_id, IPC_RMID, 0);
    if (returncode_shmctl == -1) {
        printf("Das Segment konnte nicht gelöscht werden.\n");
        perror("shmctl");
        exit(1);
    } else {
        printf("Das Segment wurde gelöscht.\n");
    }

    return 0;
}
