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
} SharedData;

// Semaphore freigeben und beenden
void cleanup(int sem_id, SharedData* shared_data) {
    // Semaphore freigeben
    semctl(sem_id, 0, IPC_RMID);

    // Gemeinsamen Speicher detachen
    shmdt(shared_data);

    exit(0);
}

// Signalhandler-Funktion für SIGINT (Ctrl+C)
void signal_handler(int signum) {
    printf("Abbruch durch SIGINT (Ctrl+C)\n");
    
    // Zugriff auf den gemeinsam genutzten Speicher erhalten
    key_t key = ftok("shared_memory", 1234);
    int shmid = shmget(key, sizeof(SharedData), 0666);
    SharedData* shared_data = (SharedData*)shmat(shmid, NULL, 0);
    
    // Semaphore-ID aus dem gemeinsam genutzten Speicher lesen
    int sem_id = shared_data->sem_id;

    // Semaphore und gemeinsamen Speicher freigeben und beenden
    cleanup(sem_id, shared_data);
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
void semaphore_unlock (int sem_id) {
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
        semaphore_lock(sem_id);

        // Messwert im gemeinsamen Speicher aktualisieren
        shared_data->data = value; //-------------_>>>>>>>valau statt zahl
        shared_data->is_valid = 1;

        // Semaphore freigeben
        semaphore_unlock(sem_id);

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
        semaphore_lock(sem_id);

        // Messwert aus dem gemeinsamen Speicher lesen
        int value = shared_data->data;
        int is_valid = shared_data->is_valid;

        // Semaphore freigeben
        semaphore_unlock(sem_id);

        if (is_valid) {
            //semaphore sperren, um zugriff auf gemeinsamen speicher zu koordinieren
            semaphore_lock(sem_id);
            
            // Messwert in die Log-Datei schreiben
            fprintf(eingabedatei, "Messwert: %d\n", value);
        
        //is_valid auf 0 setzen, um anzuzeigen, dass der wert gelesen wurde
           semaphore_lock(sem_id);
           shared_data-> is_valid=0;
           semaphore_unlock(sem_id);  //semaphore freigeben 
        }

        usleep(100000); // Kurze Pause zwischen den Log-Einträgen
    }

    fclose(eingabedatei);
}

//stat funktion
void process_stat(SharedData* shared_data, int sem_id) {
    int sum = 0;
    int count = 0;

    while (1) {
        semaphore_lock(sem_id);   //semaphore sperren um Zugriff auf SM zu knotrollieren

        int value = shared_data->data;
        int is_valid = shared_data->is_valid;

        semaphore_unlock(sem_id);

        if (is_valid) {
    semaphore_lock(sem_id); // Semaphore sperren, um Zugriff auf den gemeinsamen Speicher zu kontrollieren

//stat daten aktualisiren
            sum += value;
            count++;
            //ausrechen
            double average = (double)sum / count;

       // Statistikdaten im gemeinsamen Speicher aktualisieren
            shared_data->sum = sum;
            shared_data->count = count;
            shared_data->average = average;

            semaphore_unlock(sem_id); // Semaphore freigeben
        }

        usleep(100000); // Kurze Pause zwischen den Statistikberechnungen
}
}

//report funktion
void process_report(SharedData* shared_data, int sem_id) {
    while (1) {
        semaphore_lock(sem_id);

        int sum = shared_data->sum;
        int count = shared_data->count;
        double average = shared_data->average;

        semaphore_unlock(sem_id);

        // Ausgabe der statistischen Daten
        printf("Summe: %d\n", sum);
        printf("Anzahl: %d\n", count);
        printf("Durchschnitt: %.2f\n", average);

        usleep(100000); // Kurze Pause zwischen den Report-Ausgaben
    }
}
//________________________MAin___________________________________________________

int main() {

// Shared Memory und Semaphore initialisieren
    
    //SM erstellen
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Fehler beim Erstellen des Shared Memory");
        exit(1);
    }
    //SM anhängen
    SharedData* shared_data = (SharedData*) shmat(shm_id, NULL, 0);
    if (shared_data == (void*) -1) {
        perror("Fehler beim Anhängen des Shared Memory");
        exit(1);
    }

// Prozess-IDs des die Kindprozesses
    pid_t conv_pid, log_pid, stat_pid, report_pid;



 // Semaphore initialisieren
    int sem_id = initialize_semaphore();

     // Signalhandler für SIGINT (Ctrl+C) registrieren
    signal(SIGINT, signal_handler);
    
    
    //SM und semaphore muss bereits initalisiert sein bevor kindprozess startet
    
    // KindProzesse erstellen
     conv_pid = fork();   // oben schon initialisiert ansosnren kann ich ptid conv pid=fork();
    if (conv_pid == 0) {
        process_conv(shared_data, sem_id);
        exit(0);
    } else if (conv_pid == -1) {
        perror("Fehler beim Erstellen des Konversionsprozesses");
        exit(1);
    }
    //log erzeugen
    log_pid = fork();
    if (log_pid == 0) {
        process_log(shared_data, sem_id);
        exit(0);
    } else if (log_pid == -1) {
        perror("Fehler beim Erstellen des Log-Prozesses");
        exit(1);
    }
     //stat erzeugen
    stat_pid = fork();
    if (stat_pid == 0) {
        process_stat(shared_data, sem_id);
        exit(0);
    } else if (stat_pid == -1) {
        perror("Fehler beim Erstellen des Statistikprozesses");
        exit(1);
    }
    //report erezuegen
     report_pid = fork();
    if (report_pid == 0) {
        process_report(shared_data, sem_id);
        exit(0);
    } else if (report_pid == -1) {
        perror("Fehler beim Erstellen des Report-Prozesses");
        exit(1);
    }

    // Warten, bis alle Prozesse beendet sind
    int status;
    waitpid(conv_pid, &status, 0);
    waitpid(log_pid, &status, 0);
    waitpid(stat_pid, &status, 0);
    waitpid(report_pid, &status, 0);

    // Semaphore und gemeinsamen Speicher freigeben und beenden
    cleanup(sem_id, shared_data);

    return 0;

    }


//fehlt
    //alle drei benutzen ps, pstree, top?
    // Programm  mit   Ctrl-C abgebrochen -> Signalhandler Signal SIGINT implementieren. Beachten Sie bitte, dass beim Abbruch des Programms alle von den Prozessen belegten Betriebsmittel (Pipes, Message Queues, gemeinsame Speicherberei- che, Semaphoren) freigegeben werden.
    //Überwachen Sie  Shared Memory-Bereiche und Semaphoren mit dem Kommando ipcs. Mit ipcrm können Sie Message Queues, Shared Memory-Bereiche und Semaphoren wieder freigeben, wenn Ihr Programm die- ses bei einer inkorrekten Beendigung versäumt hat.
    //löschen der SM
    //löschen der Semaphore 
    //muss ich den SM ganz löschen nach Ctrl+c?
