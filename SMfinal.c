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

#define SHARED_MEMORY_KEY 1234
#define SEMAPHORE_KEY 5678

typedef struct {
    int data;
    int is_valid;
    int sum;
    int count;
    float average;
} SharedData;

int flag = 1;
int resource_cleanup_count = 0; // Zählervariable für Ressourcenfreigabe


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

// Semaphore sperren
void semaphore_lock(int sem_id) {
    struct sembuf sem_lock = {0, -1, 0};
    if (semop(sem_id, &sem_lock, 1) == -1) {
        perror("Fehler beim Sperren des Semaphors");
        exit(1);
    }
}

// Semaphore freigeben
void semaphore_unlock(int sem_id) {
    struct sembuf sem_unlock = {0, 1, 0};
    if (semop(sem_id, &sem_unlock, 1) == -1) {
        perror("Fehler beim Freigeben des Semaphors");
        exit(1);
    }
}

// Signal-Handler für SIGINT
void sigint_handler(int signum) {
    printf("\nSIGINT-Signal empfangen. Beende das Programm...\n");
    flag = 0;
}

int main() {
    // SIGINT-Signal-Handler registrieren
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("Fehler beim Registrieren des SIGINT-Signal-Handlers");
        exit(1);
    }

    // Gemeinsamen Speicher erstellen
    int shm_id = shmget(SHARED_MEMORY_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Fehler beim Erstellen des gemeinsamen Speichersegments");
        exit(1);
    }
    printf("Shared Memory ID: %d\n", shm_id);

    // Gemeinsamen Speicher anzeigen
    SharedData* shared_data = (SharedData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("Fehler beim Anhängen an den gemeinsamen Speicher");
        exit(1);
    }

    // Initialisierung des gemeinsamen Speichers
    shared_data->data = 0;
    shared_data->is_valid = 0;
    shared_data->sum = 0;
    shared_data->count = 0;
    shared_data->average = 0;

    // Semaphore initialisieren
    int sem_id = initialize_semaphore();
    printf("Semaphore ID: %d\n", sem_id);

    // Prozess-IDs der Kindprozesse
    pid_t conv_pid, log_pid, stat_pid, report_pid;

    // Kindprozess 'conv' erstellen
    conv_pid = fork();
    if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konversionsprozesses");
        exit(1);
    } else if (conv_pid == 0) {
        // Kindprozess 'conv' hat Zugriff auf shared_data
        while (flag) {
            // Zufälligen Messwert erzeugen
            int value = rand() % 100;

            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphore_lock(sem_id);

            // Messwert im gemeinsamen Speicher aktualisieren
            shared_data->data = value;
            shared_data->is_valid = 1;

            // Semaphore freigeben
            semaphore_unlock(sem_id);

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
        // Kindprozess 'log' hat Zugriff auf shared_data
        FILE* eingabedatei = fopen("SMsemaphore.txt", "w");
        if (eingabedatei == NULL) {
            perror("Fehler beim Öffnen der (Log) Datei");
            exit(1);
        }

        while (flag) {
            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphore_lock(sem_id);

            // Messwert aus dem gemeinsamen Speicher lesen
            int value = shared_data->data;
            int is_valid = shared_data->is_valid;

            // Semaphore freigeben
            semaphore_unlock(sem_id);

            if (is_valid) {
                // Messwert in die Log-Datei schreiben
                fprintf(eingabedatei, "Messwert: %d\n", value);

                // Semaphore sperren, um is_valid auf 0 zu setzen
                semaphore_lock(sem_id);
                shared_data->is_valid = 0;
                semaphore_unlock(sem_id);
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
        // Kindprozess 'stat' hat Zugriff auf shared_data
        int sum = 0;
        int count = 0;

        while (flag) {
            semaphore_lock(sem_id); // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren

            int value = shared_data->data;
            int is_valid = shared_data->is_valid;

            semaphore_unlock(sem_id); // Semaphore freigeben

            if (is_valid) {
                sum += value;
                count++;

                semaphore_lock(sem_id);
                shared_data->is_valid = 0;
                semaphore_unlock(sem_id);
            }

            if (count > 0) {
                double average = (double)sum / count;

                semaphore_lock(sem_id);
                shared_data->sum = sum;
                shared_data->count = count;
                shared_data->average = average;
                semaphore_unlock(sem_id);
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
        // Kindprozess 'report' hat Zugriff auf shared_data
        while (flag) {
            semaphore_lock(sem_id); // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren

            int sum = shared_data->sum;
            int count = shared_data->count;
            double average = shared_data->average;

            semaphore_unlock(sem_id); // Semaphore freigeben

            // Shell-Ausgabe
            printf("Summe: %d\tAnzahl: %d\tDurchschnitt: %.2f\n", sum, count, average);

            sleep(5); // Kurze Pause zwischen den Berichten
        }

        exit(0);
    }

    // Auf Kindprozesse warten
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

resource_cleanup_count++; // Zählervariable inkrementieren
printf("Ressourcenfreigabe durchlaufen: %d Mal\n", resource_cleanup_count);

    // Gemeinsamen Speicher freigeben
    if (shmdt(shared_data) == -1) {
        perror("Fehler beim Trennen des gemeinsamen Speichers");
        exit(1);
    } else {
        printf("Shared Memory getrennt.\n");
    }

    // Gemeinsamen Speicher entfernen
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des gemeinsamen Speichers");
    }else {
        printf("Shared Memory entfernt.\n");
    }

    // Semaphore entfernen
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
    }else {
        printf("Semaphore entfernt.\n");
    }

    return 0;
}
