#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "structs.h"

#define FREQ_CLOCK 500
#define MAX_LINE_LENGTH 100
#define G_QUANTUM 4

#define SCHED_T 6
#define LOADER_T 2

#define LD '0'
#define ST '1'
#define ADD '2'

#define CPUS 2
#define CORES 4
#define THREADS 4

// Definición de funciones
void *scheduler_timer();
void scheduler();
void *loader_timer();
void loader();
void loader();
void push(processQueue *queue, pcb *pcb);
pcb* pull(processQueue *queue);

// Lista de procesos
struct processQueue *main_processQueue;
struct mac *machine;

// Semáforos:
// mutex - para los timers principales.
// threads_mutex - para las acciones realizadas a nivel de threads.
// clock_mutex - para la ejecución del reloj de la máquina.
pthread_mutex_t mutex;
pthread_mutex_t clock_mutex;
pthread_cond_t all;
pthread_cond_t next;

// Program counter
int PC = 0;

// Conteo de programas leídos
int progs = 0;

// Conteo de procesos creados desde el arranque del sistema
int pid = 0;

// Ticks de la máquina
int ticks = 0;

// Control de los timers
int timers_running = 0;

/**
 * Reloj de la máquina
*/
void *mac_clock() {
    int i, j, k;
    while (1) {
        pthread_mutex_lock(&clock_mutex);
        ticks++;
        printf("\nClock: %d.\n", ticks);
        while(timers_running < 2){
            pthread_cond_wait(&next, &clock_mutex);
        }

        pthread_mutex_lock(&mutex);
        for(i = 0; i < CPUS; i++){
            for(j = 0; j < CORES; j++){
                struct processQueue *internal_pq = machine->cpu[i].core[j].internal_queue;
                
                // Si hay procesos en la cola, se procede a cargar en el primer thread disponible
                if( internal_pq->num_processes != 0 ){

                    struct pcb *pcb = pull(internal_pq);

                    for(k = 0; k < THREADS; k++){
                        if(machine->cpu[i].core[j].thread[k].pc == 0){
                            machine->cpu[i].core[j].thread[k].pcb = pcb;

                            // Se comprueba el quantum del proceso
                            if( pcb->quantum > G_QUANTUM && pcb->quantum > 0){
                                pcb->quantum = pcb->quantum - G_QUANTUM;
                                
                                // Queda vida restante del proceso
                                if( pcb->quantum > 0){
                                    push(internal_pq, pcb);
                                    pcb->state = RUNNING; 
                                    printf("CORE %d - THREAD %d ejecutado, sin finalizar. Quantum restante: %d.\n", j, k, pcb->quantum);
                                } else
                                // El proceso ha finalizado
                                {
                                    pcb->state = FINISHED; 
                                    printf("CORE %d - THREAD %d ejecutado y finalizado.\n", j, k); 
                                }
                            }
                        }
                    }    
                }
            }
        }
        pthread_mutex_unlock(&mutex);

        timers_running = 0;
        pthread_cond_broadcast(&all);
        pthread_mutex_unlock(&clock_mutex);

    }
    sleep(1000);
}

/**
 * Timer del Scheduler
*/
void *scheduler_timer(){
    pthread_mutex_lock(&clock_mutex);
    while(1)
    {
        timers_running++;
        if( ticks%SCHED_T == 0 ){
            pthread_mutex_lock(&mutex);
            scheduler();
            pthread_mutex_unlock(&mutex);
        }
        pthread_cond_signal(&next);
        pthread_cond_wait(&all, &clock_mutex);
    }
}

/**
 * Función que simula el funcionamiento del Scheduler del sistema, con
 * una implementación LIFO (Last In, First Out)
*/
void scheduler(){
    int i, j, k, count, n_proc = MAX_PROCESSES;

    int p = main_processQueue->num_processes;
    int busy = 0;

    // Mientras haya procesos a repartir o los cores no estén llenos, se reparten los procesos.
    while(p != 0 || !busy){
       for( i = 0; i < CPUS && n_proc != 0; i++){
        int full_cores = 0;
        for( j = 0; j < CORES; j ++){
            struct processQueue *internal_processQueue = machine->cpu[i].core[j].internal_queue;

            // Se comprueba que el número de procesos de la lista del core no haya alcanzado el máximo
            if( internal_processQueue->num_processes < MAX_QUEUE_LEN ){
                struct pcb *pcb = pull(main_processQueue);
                push(internal_processQueue, pcb);
                printf("CPU %d - CORE %d: proceso %d repartido.\n", i, j, pcb->pid);
                pcb->attached = 1;
                p--;
            } else {
                printf("CPU %d - CORE %d: lista de procesos llena.\n", i, j);
                full_cores++;
            }
        } 
        if( full_cores == CORES ){
            busy = 1;
        }
       }
    } 
}

/**
 * Timer del Loader
*/
void *loader_timer(){
    pthread_mutex_lock(&clock_mutex);
    while(1)
    {
        timers_running++;
        if( ticks%LOADER_T == 0 ){
            pthread_mutex_lock(&mutex);
            loader();
            pthread_mutex_unlock(&mutex);
        }
        pthread_cond_signal(&next);
        pthread_cond_wait(&all, &clock_mutex);
    }
}

void loader(){
    int quantum = 0, textCodei, dataCodei;
    FILE *file;
    struct pcb *process;
    char filename[12];
    char line[MAX_LINE_LENGTH];
    char textCode[9];
    char dataCode[9];


    // Si no existe el archivo 000, significa que no hay ningún archivo para cargar
    if((file = fopen("prog000.elf", "r")) == NULL){
        printf("No hay archivos.\n");
    } else {
        fclose(file);
    };

    // Se cargarán como mucho 10 archivos
    if(progs == 10){
        progs = 0;
    };

    sprintf(filename, "prog%03d.elf", progs);
    file = fopen(filename, "r");
    if((file = fopen(filename, "r"))== NULL){
        printf("Error al abrir el archivo %s.\n", filename);
    };

    printf("Archivo %s abierto.\n\n", filename);

    // Leer la línea .text
    if (fgets(line, sizeof(line), file) != NULL) {
        strncpy(textCode, line + 6, sizeof(textCode) - 1); // Omitir los primeros 6 caracteres
    }

    // Leer la línea .data
    if (fgets(line, sizeof(line), file) != NULL) {
        strncpy(dataCode, line + 6, sizeof(dataCode) - 1); // Omitir los primeros 6 caracteres
    }

    // Eliminar el texto ".text" y ".data" de los códigos capturados
    memmove(textCode, textCode + 6, strlen(textCode) - 6);
    memmove(dataCode, dataCode + 6, strlen(dataCode) - 6);

    textCodei = atoi(textCode);
    dataCodei = atoi(dataCode);

    // Procesar las líneas restantes
    while (fgets(line, sizeof(line), file) != NULL) {
        char opcode[2];
        strncpy(opcode, line, 1);
        opcode[1] = '\0';

        // Verificar el tipo de instrucción y actualizar el quantum
        switch (opcode[0]) {
            // Operación LD - incrementar quantum en 2
            case LD:
                quantum += 2;
                break;
            // Operación LD - incrementar quantum en 3
            case ST:
                quantum += 3;
                break;
            // Operación LD - incrementar quantum en 1
            case ADD:
                quantum += 1;
                break;
            default:
                break;
        }
    }
    printf("Quantum: %d\n\n", quantum);


    pid++;

    process = (pcb *) malloc(sizeof(struct pcb));
    process->mm = malloc(sizeof(struct mmu));
    process->pid = pid;
    process->state = READY;
    process->quantum = quantum;
    process->attached = 0;
    process->mm->data = dataCodei;
    process->mm->text = textCodei;

    progs++;

    fclose(file);    
}

/**
 * Función para añadir un proceso a una determinada cola
 * @param queue la cola a la que se debe añadir el proceso
 * @param pcb proceso a añadir
*/
void push(processQueue *queue, pcb *pcb){
    int length = queue->num_processes;

    if( length == 0 ){
        queue->tail = pcb;
    } else {
        queue->head->next = pcb;
    }

    queue->head = pcb;
    pcb->next = NULL;

    queue->num_processes++;
}

/**
 * Función para recuperar un proceso de una determinada cola
 * @param queue la cola a la que pertenece el proceso deseado
 * @param pcb proceso a recuperar
*/
pcb* pull(processQueue *queue){
    int length = queue->num_processes;
    struct pcb *pcb_ptr = queue->tail;

    if( length == 1 ){
        queue->tail = NULL;
        queue->head = NULL;
    } else if( length > 1 ){
        queue->tail = pcb_ptr->next;
    }

    queue->num_processes--;
    return pcb_ptr;
}

int main()
{
    printf("\n\n-----------------------------------------------------------------------\n");
    printf("----------- Pelipian Alexandra Aleina - Sistemas Operativos -----------\n");
    printf("-------------------------- Kernel Simulator ---------------------------\n");
    printf("-----------------------------------------------------------------------\n\n\n");

    int i, j, k;

    main_processQueue = malloc(sizeof(processQueue));
    main_processQueue->num_processes = 0;
    main_processQueue->head = malloc(sizeof(pcb));
    main_processQueue->tail = malloc(sizeof(pcb));

    machine = malloc(sizeof(mac));

    struct CPU *CPUs = (CPU *)malloc(CPUS * sizeof(CPU));
    machine->cpu = CPUs;

    for(i = 0; i < CPUS; i++){
        struct CPU cpu;
        machine->cpu[i] = cpu;
        machine->cpu[i].core = (core *)malloc(CORES * sizeof(core));

        for(j = 0; j < CORES; j++){
            struct core core;
            machine->cpu[i].core[j] = core;
            machine->cpu[i].core[j].thread = (thread *)malloc(THREADS * sizeof(thread));

            machine->cpu[i].core[j].internal_queue = malloc(sizeof(processQueue));
            machine->cpu[i].core[j].internal_queue->num_processes = 0;
            machine->cpu[i].core[j].internal_queue->head = malloc(sizeof(pcb));
            machine->cpu[i].core[j].internal_queue->tail = malloc(sizeof(pcb));

            for(k = 0; k < THREADS; k++){
                printf("CPU %d, CORE %d, THREAD %d's MEMORY ALLOCATED.\n", i+1, j+1, k+1);
                pcb *pcb = malloc(sizeof(struct pcb));
                pcb->pid = 0;
                pcb->state = READY;
                pcb->attached = 0;
                pcb->quantum = 0;

                struct thread thread;
                machine->cpu[i].core[j].thread[j] = thread;

                thread.pcb = pcb;
            }
        }
    }
    printf("\n");

    pthread_cond_init(&all, NULL);
    pthread_cond_init(&next, NULL);
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&clock_mutex, NULL);

    pthread_t threads[3];
    pthread_create(&threads[0], NULL, mac_clock, NULL);
    pthread_create(&threads[2], NULL, loader_timer,NULL);
    pthread_create(&threads[1], NULL, scheduler_timer,NULL);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);

}