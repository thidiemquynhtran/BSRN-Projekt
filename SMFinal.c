# BSRN-Projekt
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>


#define SHARED_MEMORY_KEY 1400
#define SEMAPHORE_KEY 1463

// Globale Variablen für den gemeinsamen Speicherbereich und den Semaphor
int shared_memory_id = -1;
int sem_id = -1;
double* sharedmempointer = NULL;

// Funktionen für Semaphore-Lock und -Unlock

void semaphore_lock(int sem_id) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

void semaphore_unlock(int sem_id) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

// Kindprozess - conv - Funktion 
void conv_process(int sem_id, double* sharedmempointer) {
    while (1) {
        // Semaphore lock
        semaphore_lock(sem_id);

        // Zufällige Zahl generieren und im Speicher speichern
         double zufallszahlen = rand() % 100;  
        sharedmempointer[0] = zufallszahlen; // Speichern der Zufallszahl im Speichersegment

        // Semaphore unlock
        semaphore_unlock(sem_id);

        sleep(2);
    }
}

// Kindprozess - LOG - Funktion 
void log_process(int sem_id, double* sharedmempointer) {
    FILE* log_file = fopen("SMFinal.txt", "w");
    if (log_file == NULL) {
        perror("Fehler beim Öffnen der Log-Datei");
        exit(1);
    }

    while (1) {
        // Semaphore lock
        semaphore_lock(sem_id);

        // Lesen der generierten Zahl
        double number = sharedmempointer[0];

        // Semaphore unlock
        semaphore_unlock(sem_id);

        // Schreiben der Zahl in Logfile
        fprintf(log_file, "%.2lf\n", number);
        fflush(log_file);

        sleep(2);
    }

    fclose(log_file);
}

// Kindprozess - stat - Funktion 
void stat_process(int sem_id, double* sharedmempointer) {
    while (1) {
        // Semaphore lock
        semaphore_lock(sem_id);

       // Lesen der generierten Zahl
        double zufallszahlen = sharedmempointer[0];
         //_____-Berechnung___________
       int count = sharedmempointer[1] += 1;   //anzahl inkrememntireen 
        
        // Summe berechnens
            double sum=0;
            sum= sharedmempointer[2] += zufallszahlen; 
          
       //durchschnitt berechnen 
       double average;
        if (count >= 1) {
            average = sum / count;
        } else {
            average = 0.0;
        } 
        // Aktualisierung der Werte im gemeinsamen Speicher
        sharedmempointer[2] = sum;
        sharedmempointer[3] = average;

        // Semaphore unlock
        semaphore_unlock(sem_id);
        
        sleep(2);
    }
}

// Kindprozess - report - Funktion 
void report_process(int sem_id, double* shared_memory_data) {
    while (1) {
        // Semaphore lock
        semaphore_lock(sem_id);

        // Auslesen der statistischen Daten
        double sum = sharedmempointer[2];
        int count = sharedmempointer[1];
        double average = sharedmempointer[3];

        // Semaphore unlock
        semaphore_unlock(sem_id);

        // Berichterstattung
        printf("Summe: %.0f\n", sum);
        printf("Anzahl: %.d\n", count);
        printf("Durchschnitt: %.2lf\n", average);
        printf("-------------------\n");

        sleep(2);
    }
}

//Signalhandler
void sigint_handler(int signum) {
    printf("\nSIGINT-Signal empfangen. Beende das Programm...\n");

    // Semaphore entfernen
    if (sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            if (errno != EINVAL) {  // Unterdrücke die Fehlermeldung, wenn Semaphore bereits entfernt wurde
                perror("Fehler beim Entfernen des Semaphors");
            }
        } else {
            printf("Semaphore entfernt.\n");
        }
    }

    // Gemeinsames Speichersegment trennen
    if (shared_memory_id != -1) {
        if (sharedmempointer != NULL) {
            if (shmdt(sharedmempointer) == -1) {
                if (errno != EINVAL) {  // Unterdrücke die Fehlermeldung, wenn das Speichersegment bereits getrennt wurde
                    perror("Fehler beim Trennen des gemeinsamen Speichersegments");
                }
            } else {
                printf("Speicher Segment getrennt\n");
            }
        }

        if (shmctl(shared_memory_id, IPC_RMID, 0) == -1) {
            if (errno != EINVAL) {  // Unterdrücke die Fehlermeldung, wenn das Speichersegment bereits gelöscht wurde
                perror("Fehler beim Löschen des gemeinsamen Speichersegments");
            }
        } else {
            printf("Speicher Segment gelöscht\n");
        }
    }

    exit(0);
}


int main() {
    
    // Signalhandler für SIGINT (Ctrl+C) einrichten
    signal(SIGINT, sigint_handler);
    
    // Initialisierung des Zufallsgenerators
    srand(time(NULL));

    // Erzeugen des gemeinsamen Speichers
    shared_memory_id = shmget(SHARED_MEMORY_KEY, 4 * sizeof(double), IPC_CREAT | 0666);
    if (shared_memory_id == -1) {
        perror("Fehler beim Erzeugen des gemeinsamen Speichers");
        return 1;
    }else {
        printf("Shared Memory ID: %d\n", shared_memory_id);
    }

    // Anhängen des gemeinsamen Speichers
    sharedmempointer = (double*)shmat(shared_memory_id, NULL, 0);
    if (sharedmempointer == (double*)-1) {
        perror("Fehler beim Anhängen des gemeinsamen Speichers");
        return 1;
    }

    // Initialisierung der Werte im gemeinsamen Speicher
    sharedmempointer[0] = 0.0; // Zufallszahl
    sharedmempointer[1] = 0; // Anzahl der Zufallszahlen
    sharedmempointer[2] = 0.0; // Summe der Zufallszahlen
    sharedmempointer[3] = 0.0; // Durchschnitt der Zufallszahlen
    
    // Erzeugen des Semaphors
    sem_id = semget(SEMAPHORE_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Fehler beim Erzeugen des Semaphors");
        return 1;
    }else {
        printf("Semaphore ID: %d\n", sem_id);
    }

    // Initialisierung des Semaphors auf den Wert 1
     if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Fehler beim Initialisieren des Semaphors");
        exit(1);
    }
    

   // Prozess zur Konvertierung 
    pid_t conv_pid = fork();
    if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konvertierungsprozesses");
        exit(1);
    } else if (conv_pid == 0) {
        printf("Conv Prozess - PID: %d,    ||| PPID: %d\n", getpid(), getppid());
        conv_process(sem_id, sharedmempointer);
        exit(0);
    }

    // Prozess zum Logging 
    pid_t log_pid = fork();
    if (log_pid == -1) {
        perror("Fehler beim Erstellen des Logging-Prozesses");
        exit(1);
    } else if (log_pid == 0) {
        printf("Log Prozess - PID: %d,     ||| PPID: %d\n", getpid(), getppid());
        log_process(sem_id, sharedmempointer);
        exit(0);
    }


     // Prozess zur Berechnung 
    pid_t stat_pid = fork();
    if (stat_pid == -1) {
        perror("Fehler beim Erstellen des Statistik-Prozesses");
        exit(1);
    } else if (stat_pid == 0) {
        printf("Stat Prozess - PID: %d,     ||| PPID: %d\n", getpid(), getppid());
        stat_process(sem_id, sharedmempointer);
        exit(0);
    }
    
// Prozess zur Berichterstattung 
    pid_t report_pid = fork();
    if (report_pid == -1) {
        perror("Fehler beim Erstellen des Berichterstattungs-Prozesses");
        exit(1);
    } else if (report_pid == 0) {
        printf("Report Prozess - PID: %d,   ||| PPID: %d\n", getpid(), getppid());
        report_process(sem_id, sharedmempointer);
        exit(0);
    }

    // Warten auf Beendigung der Kindprozesse
    waitpid(conv_pid, NULL, 0);
    waitpid(log_pid, NULL, 0);
    waitpid(stat_pid, NULL, 0);
    waitpid(report_pid, NULL, 0);

     //SEMAPHROE LÖSCHEN
     if (sem_id != -1) {
    struct semid_ds sem_info;
    if (semctl(sem_id, 0, IPC_STAT, &sem_info) == -1) {
        perror("Fehler beim Überprüfen des Semaphors");
    } else {
        if (sem_info.sem_otime != 0) {
            printf("Semaphore wurde bereits entfernt.\n");
        } else {
            if (semctl(sem_id, 0, IPC_RMID) == -1) {
                perror("Fehler beim Entfernen des Semaphors");
            } else {
                printf("Semaphore entfernt.\n");
            }
        }
    }
}else {
    perror("Fehler beim Abrufen des Semaphors");
}

    // Gemeinsames Speichersegment trennen
    if (shared_memory_id != -1) {
        if (sharedmempointer != NULL) {
            if (shmdt(sharedmempointer) == 0) {
                printf("Speicher Segment getrennt\n");
            } else {
                printf("Speicher Segment nicht getrennt\n");
                perror("shmdt");
            }
        }
        //Gemeinsames Speichersegment löschen
        if (shmctl(shared_memory_id, IPC_RMID, 0) == -1) {
            printf("Speicher Segment nicht gelöscht\n");
            perror("shmctl");
        } else {
            printf("Speicher Segment gelöscht\n");
        }
    }    

    return 0;
}
