#include <stdio.h>
#include<stdlib.h> // standard library 
#include<time.h>
#include <unistd.h> //nachscheun was die BiB bedeutet, ab hier
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "shared_memory.h"


void convProcess(){
    int shmID= shmget(SHARED_MEMORY_KEY,SHARED_MEMORY_SIZE,IPC_CREAT | 0666);
if (shmID == -1){
perror("shmget() failed");
exit(1);  //return EXIT_FAILURE 
}

 // wenn es funktionert dann liegt der SM irgednwo im speicgher, deshalb müssen wir es atachen
int*shared_memory= (int*) shmat(shmID,NULL,0);
if(shared_memory==(int*)-1){
    perror("fehler beim anhängen des SM");
    exit(1);
}


printf("es wurden folgende zahlen generiert: \n"); 
int zahl, i; 
time_t zeit = time(NULL); 
srand(zeit);


for(i=0;i<=5;i++){
zahl= rand()% 15;   
//printf("%d\n",zahl);
shared_memory[i] = zahl;//zahl in sM rein
//generation verlangsem?
printf("Conv: Zahl = %d\n", zahl);
}

shmdt(shared_memory);
}

int main(){
    convProcess();
    return 0;
}