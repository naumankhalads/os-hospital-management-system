#include "hospital.h"
#include "scheduler.h"

Ward       *ward  = NULL;
PtQueue     ptq   = {0};
pthread_mutex_t bed_mx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t q_mx   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  bed_free = PTHREAD_COND_INITIALIZER;
pthread_cond_t  q_ready  = PTHREAD_COND_INITIALIZER;
sem_t      *icu_sem = NULL;
sem_t      *iso_sem = NULL;
sem_t      *q_sem   = NULL;

static int   shm_id  = -1;
static int   running = 1;
static char  strat[16] = "best";
static Pt    hist[MAX_PTS];
static int   hist_n = 0;

static void reap(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void shutdown(int sig) {
    (void)sig;
    running = 0;
}

static void spawn_pt(Pt *p) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }
    if (pid == 0) {
        char id_s[16], pri_s[16], bed_s[16];
        snprintf(id_s,  sizeof(id_s),  "%d", p->pid);
        snprintf(pri_s, sizeof(pri_s), "%d", p->pri);
        snprintf(bed_s, sizeof(bed_s), "%d", p->bed_id);
        char *args[] = {"./patient_sim", id_s, pri_s, bed_s, NULL};
        execv("./patient_sim", args);
        perror("execv");
        exit(1);
    }
    ward->active++;
}

static int admit(Pt *p) {
    const char *type;
    int units;

    if (p->pri <= 2) {
        type  = "ICU";
        units = 3;
    } else if (p->pri == 3) {
        type  = "ISOLATION";
        units = 2;
    } else {
        type  = "GENERAL";
        units = 1;
    }
    p->units = units;

    int bed_id = -1;
    if (strcmp(strat, "first") == 0)
        bed_id = alloc_first(ward, units, type);
    else if (strcmp(strat, "worst") == 0)
        bed_id = alloc_worst(ward, units, type);
    else
        bed_id = alloc_best(ward, units, type);

    if (bed_id < 0) return -1;

    p->bed_id = bed_id;
    p->start  = time(NULL);
    page_report(ward, p->pid, units);

    if (hist_n < MAX_PTS) hist[hist_n++] = *p;

    spawn_pt(p);
    printf("[ADMIT] Patient %d (%s) -> %s bed %d\n",
           p->pid, p->name, type, bed_id);
    return bed_id;
}

static void *receptionist(void *arg) {
    (void)arg;
    int fd;
    char buf[256];
    static int id_cnt = 1;

    while (running) {
        fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0) { usleep(200000); continue; }

        while (read(fd, buf, sizeof(buf)-1) > 0) {
            Pt p = {0};
            p.pid = id_cnt++;
            p.arr = time(NULL);
            sscanf(buf, "%s %d %d", p.name, &p.age, &p.sev);
            p.pri   = (p.sev <= 2) ? 1 :
                      (p.sev <= 4) ? 2 :
                      (p.sev <= 6) ? 3 :
                      (p.sev <= 8) ? 4 : 5;
            p.units = (p.pri <= 2) ? 3 : (p.pri == 3) ? 2 : 1;

            sem_wait(q_sem);
            pthread_mutex_lock(&q_mx);
            enqueue(&ptq, p);
            pthread_cond_signal(&q_ready);
            pthread_mutex_unlock(&q_mx);
            printf("[RECV] Patient %s pri=%d queued\n", p.name, p.pri);
        }
        close(fd);
        usleep(300000);
    }
    return NULL;
}

static void *scheduler_thread(void *arg) {
    (void)arg;
    while (running) {
        pthread_mutex_lock(&q_mx);
        while (q_empty(&ptq) && running)
            pthread_cond_wait(&q_ready, &q_mx);
        if (!running) { pthread_mutex_unlock(&q_mx); break; }

        Pt p = dequeue(&ptq);
        sem_post(q_sem);
        pthread_mutex_unlock(&q_mx);

        if (p.pri <= 2)      sem_wait(icu_sem);
        else if (p.pri == 3) sem_wait(iso_sem);

        pthread_mutex_lock(&bed_mx);
        while (admit(&p) < 0 && running)
            pthread_cond_wait(&bed_free, &bed_mx);
        pthread_mutex_unlock(&bed_mx);
    }
    return NULL;
}

typedef struct { int type; } NurseArg;

static void *nurse(void *arg) {
    NurseArg *na = (NurseArg*)arg;
    int type = na->type;
    free(na);

    while (running) {
        int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0) { sleep(1); continue; }

        char buf[64];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
            buf[n] = '\0';
            int pt_id, bed_id;
            if (sscanf(buf, "%d %d", &pt_id, &bed_id) == 2) {
                if (bed_id >= 0 && bed_id < ward->total) {
                    const char *bt = ward->beds[bed_id].type;
                    int match = (type == 0 && strcmp(bt, "ICU") == 0) ||
                                (type == 1 && strcmp(bt, "ISOLATION") == 0) ||
                                (type == 2 && strcmp(bt, "GENERAL") == 0);
                    if (match) {
                        pthread_mutex_lock(&bed_mx);
                        free_bed(ward, bed_id);
                        if (type == 0)      sem_post(icu_sem);
                        else if (type == 1) sem_post(iso_sem);
                        pthread_cond_broadcast(&bed_free);
                        pthread_mutex_unlock(&bed_mx);
                        ward->active--;
                        printf("[NURSE%d] Freed bed %d for patient %d\n",
                               type, bed_id, pt_id);
                        Pt *h = &hist[hist_n > 0 ? hist_n-1 : 0];
                        h->done = time(NULL);
                    }
                }
            }
        }
        close(fd);
        sleep(1);
    }
    return NULL;
}

static void discharge_listener() {
    int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;
    char buf[64];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        int pt_id, bed_id;
        if (sscanf(buf, "%d %d", &pt_id, &bed_id) == 2) {
            pthread_mutex_lock(&bed_mx);
            free_bed(ward, bed_id);
            pthread_cond_broadcast(&bed_free);
            pthread_mutex_unlock(&bed_mx);
            if (bed_id < ICU_CAP)                   sem_post(icu_sem);
            else if (bed_id < ICU_CAP + ISO_CAP)    sem_post(iso_sem);
            ward->active--;
            printf("[DISCHG] Patient %d left bed %d\n", pt_id, bed_id);
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--strategy", 10) == 0 && i+1 < argc)
            strncpy(strat, argv[++i], sizeof(strat)-1);
    }

    signal(SIGCHLD, reap);
    signal(SIGTERM, shutdown);
    signal(SIGINT,  shutdown);

    shm_id = shmget(SHM_KEY, sizeof(Ward), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); return 1; }
    ward = (Ward*)shmat(shm_id, NULL, 0);
    if (ward == (Ward*)-1) { perror("shmat"); return 1; }
    init_ward(ward);

    mkfifo(FIFO_PATH, 0666);

    sem_unlink(SEM_ICU); sem_unlink(SEM_ISO); sem_unlink(SEM_Q);
    icu_sem = sem_open(SEM_ICU, O_CREAT, 0666, ICU_CAP);
    iso_sem = sem_open(SEM_ISO, O_CREAT, 0666, ISO_CAP);
    q_sem   = sem_open(SEM_Q,   O_CREAT, 0666, MAX_PTS);

    printf("=== HOSPITAL STARTED (strategy=%s) ===\n", strat);
    printf("ICU=%d ISO=%d GENERAL=%d\n", ICU_CAP, ISO_CAP, GEN_CAP);

    pthread_t recv_t, sched_t, nurse_t[MAX_NURSES];
    pthread_create(&recv_t,  NULL, receptionist,    NULL);
    pthread_create(&sched_t, NULL, scheduler_thread, NULL);
    for (int i = 0; i < MAX_NURSES; i++) {
        NurseArg *na = malloc(sizeof(NurseArg));
        na->type = i;
        pthread_create(&nurse_t[i], NULL, nurse, na);
    }

    while (running) {
        discharge_listener();
        sleep(1);
    }

    pthread_cancel(recv_t);
    pthread_cancel(sched_t);
    for (int i = 0; i < MAX_NURSES; i++) pthread_cancel(nurse_t[i]);

    if (hist_n > 0) run_sched_sim(hist, hist_n);

    sem_close(icu_sem); sem_close(iso_sem); sem_close(q_sem);
    sem_unlink(SEM_ICU); sem_unlink(SEM_ISO); sem_unlink(SEM_Q);
    shmdt(ward);
    shmctl(shm_id, IPC_RMID, NULL);
    unlink(FIFO_PATH);

    printf("=== HOSPITAL SHUTDOWN. Patients served: %d ===\n", hist_n);
    return 0;
}
