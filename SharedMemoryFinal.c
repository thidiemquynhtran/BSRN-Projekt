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

// Funktion für den Conv-Prozess
void process_conv(SharedData* shared_data, int sem_id) {
    while (flag) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semaphore_lock(sem_id);

        // Daten konvertieren (Beispiel: Verdoppeln)
        shared_data->data *= 2;
        shared_data->is_valid = 1;

        // Statistik aktualisieren
        shared_data->sum += shared_data->data;
        shared_data->count++;
        shared_data->average = (float)shared_data->sum / shared_data->count;

        // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
        semaphore_unlock(sem_id);

        // Kurze Pause zwischen den Konvertierungen
        sleep(1);
    }
}

// Funktion für den Log-Prozess
void process_log(SharedData* shared_data, int sem_id) {
    while (flag) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semaphore_lock(sem_id);

        // Log-Nachricht ausgeben
        printf("Aktueller Messwert (Log): %d\n", shared_data->data);

        // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
        semaphore_unlock(sem_id);

        // Kurze Pause zwischen den Ausgaben
        sleep(2);
    }
}

// Funktion für den Statistik-Prozess
void process_stat(SharedData* shared_data, int sem_id) {
    while (flag) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semaphore_lock(sem_id);

        // Statistik ausgeben
        printf("Anzahl der Werte: %d\n", shared_data->count);
        printf("Summe der Werte: %d\n", shared_data->sum);
        printf("Durchschnitt: %.2f\n", shared_data->average);

        // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
        semaphore_unlock(sem_id);

        // Kurze Pause zwischen den Ausgaben
        sleep(3);
    }
}

// Funktion für den Report-Prozess
void process_report(SharedData* shared_data, int sem_id) {
    while (flag) {
        // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
        semaphore_lock(sem_id);

        // Report erstellen
        printf("Report:\n");
        printf("Aktueller Messwert: %d\n", shared_data->data);
        printf("Anzahl der Werte: %d\n", shared_data->count);
        printf("Summe der Werte: %d\n", shared_data->sum);
        printf("Durchschnitt: %.2f\n", shared_data->average);

        // Semaphore freigeben, um anderen Prozessen Zugriff zu ermöglichen
        semaphore_unlock(sem_id);

        // Kurze Pause zwischen den Ausgaben
        sleep(4);
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

    // Gemeinsamen Speicher anzeigen
    SharedData* shared_data = (SharedData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("Fehler beim Anhängen an den gemeinsamen Speicher");
        exit(1);
    }

    

    // Initialisierung des gemeinsamen Speichers
    shared_data->data = 10;
    shared_data->is_valid = 0;
    shared_data->sum = 0;
    shared_data->count = 0;
    shared_data->average = 0;

    // Semaphore initialisieren
    int sem_id = initialize_semaphore();

    // Kindprozess (conv) erstellen
    pid_t conv_pid = fork();
    if (conv_pid == 0) {
        process_conv(shared_data, sem_id);
        exit(0);
    } else if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Kindprozesses (conv)");
        exit(1);
    }

    // Log-Prozess erstellen
    pid_t log_pid = fork();
    if (log_pid == 0) {
        process_log(shared_data, sem_id);
        exit(0);
    } else if (log_pid == -1) {
        perror("Fehler beim Erstellen des Log-Prozesses");
        exit(1);
    }

    // Statistik-Prozess erstellen
    pid_t stat_pid = fork();
    if (stat_pid == 0) {
        process_stat(shared_data, sem_id);
        exit(0);
    } else if (stat_pid == -1) {
        perror("Fehler beim Erstellen des Statistik-Prozesses");
        exit(1);
    }

    // Report-Prozess erstellen
    pid_t report_pid = fork();
    if (report_pid == 0) {
        process_report(shared_data, sem_id);
        exit(0);
    } else if (report_pid == -1) {
        perror("Fehler beim Erstellen des Report-Prozesses");
        exit(1);
    }

    // Auf Kindprozesse warten
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

    // Gemeinsamen Speicher freigeben
    if (shmdt(shared_data) == -1) {
        perror("Fehler beim Trennen des gemeinsamen Speichers");
        exit(1);
    }

    // Gemeinsamen Speicher entfernen
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des gemeinsamen Speichers");
    }

    // Semaphore entfernen
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
    }

    return 0;
}
