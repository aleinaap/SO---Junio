#ifndef STRUCTS_H
#define STRUCTS_H

#define MAX_PROCESSES 32
#define MAX_QUEUE_LEN 16

#define PAGE_SIZE 256                               // tamaño de página en bytes
#define PAGE_TABLE_SIZE 32                          // tamaño de la tabla de páginas
#define TLB_SIZE 8                                  // tamaño de la TLB

enum process_state {
    READY,
    RUNNING,
    FINISHED
};
typedef enum process_state process_state;

struct pcb {
    int pid;                                        // process ID
    enum process_state state;                       // process state (e.g., ready, blocked, running)
    int attached;                                   // process attached to queue (e.g., 1 = attached, 0 = detached)
    int quantum;                                    // process quantum
    struct pcb *next;
    struct mmu *mm;
};
typedef struct pcb pcb;

struct processQueue {
    int num_processes;                               // number of processes in queue
    struct pcb *head;                                     // list's head
    struct pcb *tail;                                     // list's tail
};
typedef struct processQueue processQueue;

struct mmu {
    int text;
    int data;
    struct tlb *pgb;
};
typedef struct mmu mmu;

struct tlb {
    int pid;
    int physic;
    int virtual;
};
typedef struct tlb tlb;

struct thread {
    int id;
    int pc;                                         // program counter
    int ri;                                         // registro de instrucción
    struct pcb *pcb;                                     
    struct mmu *mmu;
};
typedef struct thread thread;

struct core {
    int id;
    struct thread *thread;
    struct processQueue *internal_queue;                 // internal queue of scheduled processes to be dispatched
};
typedef struct core core;

struct CPU {
    int id;
    struct core *core;
};
typedef struct CPU CPU;

struct mac {
    struct CPU *cpu;
};
typedef struct mac mac;

#endif