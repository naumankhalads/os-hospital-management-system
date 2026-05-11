#include "hospital.h"

static FILE *mlog = NULL;

static void open_mlog() {
    if (!mlog) mlog = fopen(MEM_LOG, "a");
}

static void log_mem(const char *msg) {
    open_mlog();
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    fprintf(mlog, "[%s] %s\n", ts, msg);
    fflush(mlog);
}

void init_ward(Ward *w) {
    int idx = 0;
    for (int i = 0; i < ICU_CAP; i++) {
        w->beds[idx].id    = idx;
        w->beds[idx].start = i * 3;
        w->beds[idx].sz    = 3;
        w->beds[idx].free  = 1;
        w->beds[idx].pt_id = -1;
        strcpy(w->beds[idx].type, "ICU");
        for (int j = 0; j < 3; j++) w->ward[i*3 + j] = -1;
        idx++;
    }
    int base = ICU_CAP * 3;
    for (int i = 0; i < ISO_CAP; i++) {
        w->beds[idx].id    = idx;
        w->beds[idx].start = base + i * 2;
        w->beds[idx].sz    = 2;
        w->beds[idx].free  = 1;
        w->beds[idx].pt_id = -1;
        strcpy(w->beds[idx].type, "ISOLATION");
        for (int j = 0; j < 2; j++) w->ward[base + i*2 + j] = -1;
        idx++;
    }
    base += ISO_CAP * 2;
    for (int i = 0; i < GEN_CAP; i++) {
        w->beds[idx].id    = idx;
        w->beds[idx].start = base + i;
        w->beds[idx].sz    = 1;
        w->beds[idx].free  = 1;
        w->beds[idx].pt_id = -1;
        strcpy(w->beds[idx].type, "GENERAL");
        w->ward[base + i] = -1;
        idx++;
    }
    w->total    = idx;
    w->pt_count = 0;
    w->active   = 0;
    log_mem("Ward initialized");
}

static int do_alloc(Ward *w, int units, const char *type, int mode) {
    int best = -1;
    int best_sz = 999999;
    int worst_sz = -1;

    for (int i = 0; i < w->total; i++) {
        Bed *b = &w->beds[i];
        if (!b->free) continue;
        if (strcmp(b->type, type) != 0) continue;
        if (b->sz < units) continue;

        if (mode == 0) {
            if (b->sz < best_sz) { best = i; best_sz = b->sz; }
        } else if (mode == 1) {
            best = i; break;
        } else {
            if (b->sz > worst_sz) { best = i; worst_sz = b->sz; }
        }
    }
    return best;
}

static int mark_alloc(Ward *w, int idx, int pt_id) {
    Bed *b = &w->beds[idx];
    b->free  = 0;
    b->pt_id = pt_id;
    for (int j = b->start; j < b->start + b->sz; j++)
        w->ward[j] = pt_id;
    char msg[128];
    snprintf(msg, sizeof(msg), "Allocated bed %d (%s) to patient %d", b->id, b->type, pt_id);
    log_mem(msg);
    frag_report(w);
    return b->id;
}

int alloc_best(Ward *w, int units, const char *type) {
    int i = do_alloc(w, units, type, 0);
    if (i < 0) return -1;
    return mark_alloc(w, i, w->pt_count++);
}

int alloc_first(Ward *w, int units, const char *type) {
    int i = do_alloc(w, units, type, 1);
    if (i < 0) return -1;
    return mark_alloc(w, i, w->pt_count++);
}

int alloc_worst(Ward *w, int units, const char *type) {
    int i = do_alloc(w, units, type, 2);
    if (i < 0) return -1;
    return mark_alloc(w, i, w->pt_count++);
}

void free_bed(Ward *w, int bed_id) {
    if (bed_id < 0 || bed_id >= w->total) return;
    Bed *b = &w->beds[bed_id];

    printf("\n[WARD BEFORE COALESCE]\n");
    for (int i = 0; i < w->total; i++)
        printf("  Bed %2d [%s] %s pt=%d\n", w->beds[i].id, w->beds[i].type,
               w->beds[i].free ? "FREE" : "BUSY", w->beds[i].pt_id);

    b->free  = 1;
    b->pt_id = -1;
    for (int j = b->start; j < b->start + b->sz; j++)
        w->ward[j] = -1;

    coalesce(w);

    printf("[WARD AFTER COALESCE]\n");
    for (int i = 0; i < w->total; i++)
        printf("  Bed %2d [%s] %s pt=%d\n", w->beds[i].id, w->beds[i].type,
               w->beds[i].free ? "FREE" : "BUSY", w->beds[i].pt_id);

    char msg[64];
    snprintf(msg, sizeof(msg), "Freed bed %d", bed_id);
    log_mem(msg);
    frag_report(w);
}

void coalesce(Ward *w) {
    for (int i = 0; i < w->total - 1; i++) {
        Bed *a = &w->beds[i];
        Bed *b = &w->beds[i+1];
        if (a->free && b->free && strcmp(a->type, b->type) == 0
            && a->start + a->sz == b->start) {
            a->sz += b->sz;
            for (int k = i+1; k < w->total - 1; k++)
                w->beds[k] = w->beds[k+1];
            w->total--;
            i--;
            log_mem("Coalesced two adjacent free beds");
        }
    }
}

void frag_report(Ward *w) {
    int total_free = 0, largest = 0;
    for (int i = 0; i < w->total; i++) {
        if (w->beds[i].free) {
            total_free += w->beds[i].sz;
            if (w->beds[i].sz > largest) largest = w->beds[i].sz;
        }
    }
    double ext = (total_free > 0)
        ? (1.0 - (double)largest / total_free) * 100.0 : 0.0;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "FreeUnits=%d LargestBlock=%d ExtFrag=%.1f%%",
             total_free, largest, ext);
    log_mem(msg);
    printf("[MEM] Free=%d Largest=%d ExtFrag=%.1f%%\n", total_free, largest, ext);
}

void page_report(Ward *w __attribute__((unused)), int pt_id, int units) {
    int pages = (units + PAGE_SZ - 1) / PAGE_SZ;
    int internal = pages * PAGE_SZ - units;
    printf("[PAGE] Patient %d: units=%d pages=%d internal_frag=%d\n",
           pt_id, units, pages, internal);
    char msg[128];
    snprintf(msg, sizeof(msg),
             "Patient %d pages=%d internal_frag=%d", pt_id, pages, internal);
    log_mem(msg);
}

void enqueue(PtQueue *q, Pt p) {
    if (q->cnt >= MAX_PTS) return;
    q->q[q->tail] = p;
    q->tail = (q->tail + 1) % MAX_PTS;
    q->cnt++;
    int n = q->cnt;
    int i = q->tail - 1;
    if (i < 0) i += MAX_PTS;
    while (n > 1) {
        int par = ((i - q->head + MAX_PTS) % MAX_PTS - 1) / 2;
        int pi  = (q->head + par) % MAX_PTS;
        if (q->q[pi].pri > q->q[i].pri) {
            Pt tmp   = q->q[pi];
            q->q[pi] = q->q[i];
            q->q[i]  = tmp;
            i = pi;
            n--;
        } else break;
    }
}

Pt dequeue(PtQueue *q) {
    Pt top = q->q[q->head];
    q->head = (q->head + 1) % MAX_PTS;
    q->cnt--;
    return top;
}

int q_empty(PtQueue *q) {
    return q->cnt == 0;
}
