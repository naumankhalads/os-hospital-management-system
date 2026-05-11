#include "hospital.h"

static void log_gantt(FILE *f, Pt *pts, int n, const char *algo) {
    fprintf(f, "\n=== %s ===\n", algo);
    fprintf(f, "%-5s %-15s %-6s %-6s %-6s %-8s %-10s\n",
            "ID", "Name", "Arr", "Start", "Done", "Wait", "Turnaround");
    double tw = 0, tt = 0;
    for (int i = 0; i < n; i++) {
        double w = difftime(pts[i].start, pts[i].arr);
        double ta = difftime(pts[i].done, pts[i].arr);
        tw += w; tt += ta;
        fprintf(f, "%-5d %-15s %-6ld %-6ld %-6ld %-8.0f %-10.0f\n",
                pts[i].pid, pts[i].name,
                (long)pts[i].arr, (long)pts[i].start, (long)pts[i].done,
                w, ta);
    }
    fprintf(f, "Avg Wait=%.2f  Avg TA=%.2f\n", tw/n, tt/n);
    printf("[SCHED] %s => AvgWait=%.2f AvgTA=%.2f\n", algo, tw/n, tt/n);
}

static int cmp_fcfs(const void *a, const void *b) {
    return (int)(((Pt*)a)->arr - ((Pt*)b)->arr);
}

static int cmp_sjf(const void *a, const void *b) {
    int da = ((Pt*)a)->units;
    int db = ((Pt*)b)->units;
    return da - db;
}

static int cmp_pri(const void *a, const void *b) {
    return ((Pt*)a)->pri - ((Pt*)b)->pri;
}

void run_sched_sim(Pt *pts, int n) {
    if (n == 0) return;

    FILE *f = fopen(SCHED_LOG, "a");
    if (!f) return;

    Pt tmp[MAX_PTS];

    memcpy(tmp, pts, n * sizeof(Pt));
    qsort(tmp, n, sizeof(Pt), cmp_fcfs);
    time_t cur = tmp[0].arr;
    for (int i = 0; i < n; i++) {
        if (cur < tmp[i].arr) cur = tmp[i].arr;
        tmp[i].start = cur;
        tmp[i].done  = cur + tmp[i].units * 2;
        cur = tmp[i].done;
    }
    log_gantt(f, tmp, n, "FCFS");

    memcpy(tmp, pts, n * sizeof(Pt));
    qsort(tmp, n, sizeof(Pt), cmp_sjf);
    cur = time(NULL);
    for (int i = 0; i < n; i++) {
        tmp[i].start = cur;
        tmp[i].done  = cur + tmp[i].units * 2;
        cur = tmp[i].done;
    }
    log_gantt(f, tmp, n, "SJF");

    memcpy(tmp, pts, n * sizeof(Pt));
    qsort(tmp, n, sizeof(Pt), cmp_pri);
    cur = time(NULL);
    for (int i = 0; i < n; i++) {
        tmp[i].start = cur;
        tmp[i].done  = cur + tmp[i].units * 2;
        cur = tmp[i].done;
    }
    log_gantt(f, tmp, n, "PRIORITY");

    int quantum = 3;
    memcpy(tmp, pts, n * sizeof(Pt));
    int rem[MAX_PTS];
    for (int i = 0; i < n; i++) rem[i] = tmp[i].units * 2;
    cur = time(NULL);
    int done_cnt = 0;
    fprintf(f, "\n=== ROUND ROBIN (q=%d) ===\n", quantum);
    while (done_cnt < n) {
        for (int i = 0; i < n; i++) {
            if (rem[i] <= 0) continue;
            if (tmp[i].start == 0) tmp[i].start = cur;
            int run = rem[i] < quantum ? rem[i] : quantum;
            rem[i] -= run;
            cur += run;
            if (rem[i] == 0) {
                tmp[i].done = cur;
                done_cnt++;
                fprintf(f, "  P%d done at t=%ld\n", tmp[i].pid, (long)cur);
            }
        }
    }
    fprintf(f, "Round Robin complete\n");

    fclose(f);
}
