# BSRN-Projekt
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>

// Struktur für den gemeinsam genutzten Speicher
typedef struct {
    int data; // Messwert
    int is_valid; // Flag für Plausibilität
    int sum; // Summe der Messwerte
    int count; // Anzahl der Messwerte
    double average; // Durchschnitt der Messwerte
    int sem_id; // Semaphore-ID
    int shm_id;
} SharedData;

// Funktion, um den gemeinsamen Speicher zurückzugeben
SharedData* get_shared_data() {
    static SharedData shared_data;
    return &shared_data;
}

// Funktion, um Semaphore zu erstellen
int initialize_semaphore() {
    int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Fehler beim Erstellen des Semaphors");
        exit(1);
    }

    // Semaphore auf 1 erstellen
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Fehler beim Initialisieren des Semaphors");
        exit(1);
    }
    return sem_id;
}
// Funktion, um Semaphore zu sperren
void semaphore_lock(int sem_id) {
    struct sembuf p_operation = {0, -1, 0}; // Semaphor-Operation: P
    if (semop(sem_id, &p_operation, 1) == -1) {
        perror("Fehler beim Sperren des Semaphors");
        exit(1);
    }
}

// Funktion, um Semaphore freizugeben
void semaphore_unlock(int sem_id) {
    struct sembuf v_operation = {0, 1, 0}; // Semaphor-Operation: V
    if (semop(sem_id, &v_operation, 1) == -1) {
        perror("Fehler beim Freigeben des Semaphors");
        exit(1);
    }
}
// Funktion der Kindprozesse (conv)
void process_conv(SharedData* shared_data, int sem_id) {
    while (1) {
        // Zufälligen Messwert erzeugen
        int value = rand() % 100;

        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        struct sembuf p_operation = {0, -1, 0}; // Semaphor-Operation: P
        if (semop(sem_id, &p_operation, 1) == -1) {
            perror("Fehler beim Sperren des Semaphors");
            exit(1);
        }

        // Messwert im gemeinsamen Speicher aktualisieren
        shared_data->data = value;
        shared_data->is_valid = 1;
        shared_data->sum += value;
        shared_data->count++;
        shared_data->average = (double)shared_data->sum / shared_data->count;

        printf("Messwert: %d\n", value);
        printf("Durchschnitt: %.2f\n", shared_data->average);

        // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
        struct sembuf v_operation = {0, 1, 0}; // Semaphor-Operation: V
        if (semop(sem_id, &v_operation, 1) == -1) {
            perror("Fehler beim Freigeben des Semaphors");
            exit(1);
        }

        // Kurze Pause zwischen den Messungen
        sleep(1);
    }
}

// Signalhandler für SIGINT (Ctrl+C)
void sigint_handler(int signal) {
    // Zugriff auf den gemeinsamen Speicher synchronisieren
    static int flag = 0;
    if (flag) {
        // Bereits im Signalhandler, beende das Programm
        exit(0);
    }
    flag = 1;

    printf("\nSIGINT-Signal empfangen. Beende das Programm...\n");

    // Gemeinsamen Speicher freigeben
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Fehler beim Erstellen des gemeinsamen Speichersegments");
        exit(1);
    }

    SharedData* shared_data = (SharedData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("Fehler beim Anhängen an den gemeinsamen Speicher");
        exit(1);
    }

    // Semaphore entfernen
    int sem_id = shared_data->sem_id;
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
        exit(1);
    }

    // Gemeinsamen Speicher freigeben
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des gemeinsamen Speichersegments");
        exit(1);
    }

    // Beende das Programm
    exit(0);
}

int main() {
    // Signalhandler für SIGINT (Ctrl+C) registrieren
    signal(SIGINT, sigint_handler);

    // Semaphore erstellen
    int sem_id = initialize_semaphore();

    // Gemeinsamen Speicher erstellen
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Fehler beim Erstellen des gemeinsamen Speichersegments");
        exit(1);
    }

    SharedData* shared_data = (SharedData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("Fehler beim Anhängen an den gemeinsamen Speicher");
        exit(1);
    }

    // Semaphore-ID im gemeinsamen Speicher speichern
    shared_data->sem_id = sem_id;

    // Kindprozesse erzeugen
    pid_t pid = fork();
    if (pid == 0) {
        // Kindprozess (conv)
        process_conv(shared_data, sem_id);
        exit(0);
    } else if (pid > 0) {
        // Elternprozess (main)
        while (1) {
            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            struct sembuf p_operation = {0, -1, 0}; // Semaphor-Operation: P
            if (semop(sem_id, &p_operation, 1) == -1) {
                perror("Fehler beim Sperren des Semaphors");
                exit(1);
            }

            // Prüfen, ob ein gültiger Messwert vorhanden ist
            if (shared_data->is_valid) {
                printf("Aktueller Messwert: %d\n", shared_data->data);
            }

            // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
            struct sembuf v_operation = {0, 1, 0}; // Semaphor-Operation: V
            if (semop(sem_id, &v_operation, 1) == -1) {
                perror("Fehler beim Freigeben des Semaphors");
                exit(1);
            }

            // Kurze Pause zwischen den Ausgaben
            sleep(2);
        }
    } else {
        perror("Fehler beim Erzeugen des Kindprozesses");
        exit(1);
    }

    // Warten auf den Kindprozess
    wait(NULL);

    // Semaphore entfernen
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
        exit(1);
    }

    // Gemeinsamen Speicher freigeben
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des gemeinsamen Speichersegments");
        exit(1);
    }

    return 0;
}



//fehlt
