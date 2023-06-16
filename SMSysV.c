#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>

#define SHARED_MEMORY_KEY 1234
#define SEMAPHORE_KEY 5678

typedef struct {
    int data;
    int sum;
    int count;
    float average;
} SharedData;

int flag = 1;  // Globaler Flag für die Schleifenbeendigung
int cleanup_called = 0;  // Variable, um den Status des Signalhandlers zu überprüfen

int shm_id;     // Globale Variable für die Shared Memory-ID
int sem_id;     // Globale Variable für die Semaphore-ID
SharedData* shared_data;  // Globale Variable für den gemeinsamen Speicherbereich

void cleanup() {
    // Überprüfen, ob die Ressourcen bereits freigegeben wurden
    if (shm_id != -1) {
        // Gemeinsamen Speicher freigeben
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Fehler beim Entfernen des gemeinsamen Speichers");
        } else {
            printf("Shared Memory entfernt.\n");
        }
        shm_id = -1;  // Setze die ID auf -1, um anzuzeigen, dass die Freigabe erfolgt ist
    }

    if (sem_id != -1) {
        // Semaphore entfernen
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            perror("Fehler beim Entfernen des Semaphors");
        } else {
            printf("Semaphore entfernt.\n");
        }
        sem_id = -1;  // Setze die ID auf -1, um anzuzeigen, dass die Freigabe erfolgt ist
    }

    flag = 0;  // Setze den Flag auf 0, um die Schleifenbeendigung anzuzeigen

    exit(0);
}

void sigint_handler(int signal) {
    printf("SIGINT empfangen. Beende das Programm...\n");
    if (cleanup_called == 0) {
        cleanup_called = 1;
        cleanup();
    }
}

// Rest des Codes bleibt unverändert


// Rest des Codes bleibt unverändert


// Rest des Codes bleibt unverändert

// Rest des Codes bleibt unverändert


// Rest des Codes bleibt unverändert


// Semaphore sperren
void semaphore_lock() {
    struct sembuf semaphore_op;
    semaphore_op.sem_num = 0;
    semaphore_op.sem_op = -1;
    semaphore_op.sem_flg = 0;
    semop(sem_id, &semaphore_op, 1);
}

// Semaphore freigeben
void semaphore_unlock() {
    struct sembuf semaphore_op;
    semaphore_op.sem_num = 0;
    semaphore_op.sem_op = 1;
    semaphore_op.sem_flg = 0;
    semop(sem_id, &semaphore_op, 1);
}

int main() {
    // Signalhandler für SIGINT (Strg+C) registrieren
    signal(SIGINT, sigint_handler);

    // Gemeinsamen Speicher erstellen
    shm_id = shmget(SHARED_MEMORY_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        printf("Fehler beim Erstellen des gemeinsamen Speichers");
        exit(1);
    }
    printf("Shared Memory erstellt.\n");

    // Gemeinsamen Speicher anzeigen
    SharedData* shared_data = (SharedData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("Fehler beim Anhängen an den gemeinsamen Speicher");
        exit(1);
    }
    printf("Shared Memory angehängt.\n");

    // Semaphore erstellen und initialisieren
    sem_id = semget(SEMAPHORE_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        printf("Fehler beim Erstellen des Semaphors");
        exit(1);
    }
    semctl(sem_id, 0, SETVAL, 1);  // Initialisierung des Semaphors auf 1
    printf("Semaphore erstellt und initialisiert.\n");

    // Kindprozess: Konvertierungsprozess
    pid_t conv_pid = fork();
    if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konvertierungs-Prozesses");
        exit(1);
    } else if (conv_pid == 0) {
        // Kindprozess: Konvertierungsprozess
        while (flag) {
            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphore_lock();

            // Konvertierung durchführen
            shared_data->data = (rand() % 100) + 1;  // Zufällige Zahl von 1 bis 100 generieren
            shared_data->sum += shared_data->data;
            shared_data->count++;
            shared_data->average = (float) shared_data->sum / shared_data->count;

            // Semaphore freigeben
            semaphore_unlock();

            sleep(1);  // 1 Sekunde warten
        }

        exit(0);
    }

    // Kindprozess: Ausgabeprozess
    pid_t print_pid = fork();
    if (print_pid == -1) {
        perror("Fehler beim Erstellen des Ausgabe-Prozesses");
        exit(1);
    } else if (print_pid == 0) {
        // Kindprozess: Ausgabeprozess
        while (flag) {
            // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren
            semaphore_lock();

            // Ausgabe der Daten
            printf("Daten: %d\tSumme: %d\tAnzahl: %d\tDurchschnitt: %.2f\n",
                   shared_data->data, shared_data->sum, shared_data->count, shared_data->average);

            // Semaphore freigeben
            semaphore_unlock();

            sleep(1);  // 1 Sekunde warten
        }

        exit(0);
    }

    // Warten auf die Beendigung der Kindprozesse
    int status;
    waitpid(conv_pid, &status, 0);
    waitpid(print_pid, &status, 0);

    // Gemeinsamen Speicher freigeben
    if (shmdt(shared_data) == -1) {
        perror("Fehler beim Trennen des gemeinsamen Speichers");
        exit(1);
    } else {
        printf("Shared Memory getrennt.\n");
    }

    // Semaphore entfernen
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Fehler beim Entfernen des Semaphors");
    } else {
        printf("Semaphore entfernt.\n");
    }

    // Gemeinsamen Speicher entfernen
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("Fehler beim Entfernen des gemeinsamen Speichers");
    } else {
        printf("Shared Memory entfernt.\n");
    }

    return 0;
}

//SYsV
