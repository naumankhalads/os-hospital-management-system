#ifndef HOSPITAL_H
#define HOSPITAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define MAX_BEDS       20
#define ICU_CAP        4
#define ISO_CAP        4
#define GEN_CAP        12
#define MAX_PTS        50
#define MAX_NURSES     3
#define PAGE_SZ        2
#define WARD_UNITS     (ICU_CAP*3 + ISO_CAP*2 + GEN_CAP*1)

#define SHM_KEY        0xBEDF00D
#define FIFO_PATH      "/tmp/discharge_fifo"
#define SEM_ICU        "/sem_icu_limit"
#define SEM_ISO        "/sem_iso_limit"
#define SEM_Q          "/sem_queue_bound"
#define SCHED_LOG      "logs/schedule_log.txt"
#define MEM_LOG        "logs/memory_log.txt"

typedef struct {
    int   pid;
    char  name[64];
    int   age;
    int   sev;
    int   pri;
    int   units;
    int   bed_id;
    time_t arr;
    time_t start;
    time_t done;
} Pt;

typedef struct {
    int   id;
    int   start;
    int   sz;
    int   free;
    int   pt_id;
    char  type[16];
} Bed;

typedef struct {
    Bed   beds[MAX_BEDS];
    int   ward[WARD_UNITS];
    int   total;
    int   pt_count;
    int   active;
} Ward;

typedef struct {
    Pt    q[MAX_PTS];
    int   head;
    int   tail;
    int   cnt;
} PtQueue;

extern Ward       *ward;
extern PtQueue     ptq;
extern pthread_mutex_t bed_mx;
extern pthread_mutex_t q_mx;
extern pthread_cond_t  bed_free;
extern pthread_cond_t  q_ready;
extern sem_t      *icu_sem;
extern sem_t      *iso_sem;
extern sem_t      *q_sem;

void  init_ward(Ward *w);
int   alloc_best(Ward *w, int units, const char *type);
int   alloc_first(Ward *w, int units, const char *type);
int   alloc_worst(Ward *w, int units, const char *type);
void  free_bed(Ward *w, int bed_id);
void  coalesce(Ward *w);
void  frag_report(Ward *w);
void  page_report(Ward *w, int pt_id, int units);
void  enqueue(PtQueue *q, Pt p);
Pt    dequeue(PtQueue *q);
int   q_empty(PtQueue *q);

#endif
