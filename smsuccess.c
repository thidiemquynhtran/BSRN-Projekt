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
} SharedData;

int semaphoreID; // Semaphore-ID
int sharedMemoryID; // Shared Memory ID
SharedData* sharedData; // Globaler Zeiger auf den gemeinsamen Speicher


// Signalhandler für SIGINT (Ctrl-C)
void signalHandler(int signum) {
    
    printf("Betriebsmittel wurden freigegeben. Das Programm wird beendet.\n");
    
    //sm löschen 
    // Shared Memory entfernen
    if (shmctl(sharedMemoryID, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des Shared Memory");
        exit(1);
    } else {
        printf("Shared Memory entfernt.\n");
    }

    // Prüfen, ob Shared Memory erfolgreich entfernt wurde
    if (shmget(sharedMemoryID, sizeof(SharedData), 0) != -1) {
        perror("Shared Memory wurde nicht erfolgreich entfernt");
        exit(1);
    } else {
        printf("Shared Memory erfolgreich entfernt.\n");
    }

    // Semaphore entfernen
    if (semctl(semaphoreID, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
        exit(1);
    } else {
        printf("Semaphore entfernt.\n");
    }

    // Prüfen, ob Semaphore erfolgreich entfernt wurde
    if (semget(semaphoreID, 1, IPC_CREAT | IPC_EXCL) != -1) {
        perror("Semaphore wurde nicht erfolgreich entfernt");
        exit(1);
    } else {
        printf("Semaphore erfolgreich entfernt.\n");
    }

    

    exit(0);
}

// Funktion, um Semaphore zu erstellen
int initializeSemaphore() {
    int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Fehler beim Erstellen des Semaphors");
        exit(1);
    } else {
        printf("Semaphor erstellt (ID: %d).\n", sem_id);
    }

    // Semaphore auf 1 setzen
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Fehler beim Initialisieren des Semaphors");
        exit(1);
    }
    return sem_id;
}

// Funktion, um Semaphore zu sperren
void semaphoreLock(int sem_id) {
    struct sembuf p_operation = {0, -1, 0}; // Semaphor-Operation: P
    if (semop(sem_id, &p_operation, 1) == -1) {
        perror("Fehler beim Sperren des Semaphors");
        exit(1);
    }
}

// Funktion, um Semaphore freizugeben
void semaphoreUnlock(int sem_id) {
    struct sembuf v_operation = {0, 1, 0}; // Semaphor-Operation: V
    if (semop(sem_id, &v_operation, 1) == -1) {
        perror("Fehler beim Freigeben des Semaphors");
        exit(1);
    }
}

int main() {
    // Shared Memory und Semaphore erstellen
    sharedMemoryID = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (sharedMemoryID == -1) {
        perror("Fehler beim Erstellen des Shared Memory");
        exit(1);
    } else {
        printf("Shared Memory erstellt (ID: %d).\n", sharedMemoryID);
    }

    // Shared Memory anhängen
    SharedData* sharedData = (SharedData*)shmat(sharedMemoryID, NULL, 0);
    if (sharedData == (void*)-1) {
        perror("Fehler beim Anhängen des Shared Memory");
        exit(1);
    }

    // Prozess-IDs der Kindprozesse
    pid_t conv_pid, log_pid, stat_pid, report_pid;

    // Semaphore initialisieren
    semaphoreID = initializeSemaphore();

    // Signalhandler für SIGINT (Ctrl+C) registrieren
    signal(SIGINT, signalHandler);

    // Kindprozess 'conv' erstellen
    conv_pid = fork();
    if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konversionsprozesses");
        exit(1);
    } else if (conv_pid == 0) {
        while (1) {
            // Zufälligen Messwert erzeugen
            int value = rand() % 100;

            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphoreLock(semaphoreID);

            // Messwert im gemeinsamen Speicher aktualisieren
            sharedData->data = value;
            sharedData->is_valid = 1;

            // Semaphore freigeben
            semaphoreUnlock(semaphoreID);

            sleep(1); // Kurze Pause zwischen den Messungen
        }

        exit(0);
    }

    // Kindprozess 'log' erstellen
    log_pid = fork();
    if (log_pid == -1) {
        perror("Fehler beim Erstellen des Log-Prozesses");
        exit(1);
    } else if (log_pid == 0) {
        FILE* eingabedatei = fopen("SMsemaphore.txt", "w");
        if (eingabedatei == NULL) {
            perror("Fehler beim Öffnen der (Log) Datei");
            exit(1);
        }

        while (1) {
            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphoreLock(semaphoreID);

            // Messwert aus dem gemeinsamen Speicher lesen
            int value = sharedData->data;
            int is_valid = sharedData->is_valid;

            // Semaphore freigeben
            semaphoreUnlock(semaphoreID);

            if (is_valid) {
                // Messwert in die Log-Datei schreiben
                fprintf(eingabedatei, "Messwert: %d\n", value);

                // Semaphore sperren, um is_valid auf 0 zu setzen
                semaphoreLock(semaphoreID);
                sharedData->is_valid = 0;
                semaphoreUnlock(semaphoreID);
            }

            sleep(2); // Kurze Pause zwischen den Log-Einträgen
        }

        fclose(eingabedatei);
        exit(0);
    }

    // Kindprozess 'stat' erstellen
    stat_pid = fork();
    if (stat_pid == -1) {
        perror("Fehler beim Erstellen des Statistik-Prozesses");
        exit(1);
    } else if (stat_pid == 0) {
        int sum = 0;
        int count = 0;

        while (1) {
            semaphoreLock(semaphoreID); // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren

            int value = sharedData->data;
            int is_valid = sharedData->is_valid;

            semaphoreUnlock(semaphoreID); // Semaphore freigeben

            if (is_valid) {
                sum += value;
                count++;

                semaphoreLock(semaphoreID);
                sharedData->is_valid = 0;
                semaphoreUnlock(semaphoreID);
            }

            if (count > 0) {
                double average = (double)sum / count;

                semaphoreLock(semaphoreID);
                sharedData->sum = sum;
                sharedData->count = count;
                sharedData->average = average;
                semaphoreUnlock(semaphoreID);
            }

            sleep(3); // Kurze Pause zwischen den Statistiken
        }

        exit(0);
    }

    // Kindprozess 'report' erstellen
    report_pid = fork();
    if (report_pid == -1) {
        perror("Fehler beim Erstellen des Report-Prozesses");
        exit(1);
    } else if (report_pid == 0) {
        while (1) {
            semaphoreLock(semaphoreID); // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren

            int sum = sharedData->sum;
            int count = sharedData->count;
            double average = sharedData->average;

            semaphoreUnlock(semaphoreID); // Semaphore freigeben

            // Shell-Ausgabe
            printf("Summe: %d\tAnzahl: %d\tDurchschnitt: %.2f\n", sum, count, average);

            sleep(5); // Kurze Pause zwischen den Berichten
        }

        exit(0);
    }

    // Warten, bis alle Kindprozesse beendet sind
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

    // Semaphore entfernen
    if (semctl(semaphoreID, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
        exit(1);
    } else {
        printf("Semaphore entfernt.\n");
    }

    // Shared Memory trennen
    if (shmdt(sharedData) == -1) {
        perror("Fehler beim Trennen des Shared Memory");
        exit(1);
    } else {
        printf("Shared Memory getrennt.\n");
    }

    // Shared Memory entfernen
    if (shmctl(sharedMemoryID, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des Shared Memory");
        exit(1);
    } else {
        printf("Shared Memory entfernt.\n");
    }

    return 0;
}
