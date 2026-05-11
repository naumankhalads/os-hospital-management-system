#include "hospital.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: patient_sim <pt_id> <pri> <bed_id>\n");
        return 1;
    }

    int pt_id  = atoi(argv[1]);
    int pri    = atoi(argv[2]);
    int bed_id = atoi(argv[3]);

    printf("[PATIENT %d] Arrived | Priority=%d Bed=%d\n", pt_id, pri, bed_id);
    fflush(stdout);

    int sleep_min, sleep_max;
    if (pri <= 2) {
        sleep_min = 5; sleep_max = 15;
    } else if (pri == 3) {
        sleep_min = 3; sleep_max = 10;
    } else {
        sleep_min = 2; sleep_max = 8;
    }

    srand(time(NULL) ^ getpid());
    int dur = sleep_min + rand() % (sleep_max - sleep_min + 1);

    printf("[PATIENT %d] Treatment started (duration=%ds)\n", pt_id, dur);
    fflush(stdout);

    sleep(dur);

    printf("[PATIENT %d] Treatment done — discharging\n", pt_id);
    fflush(stdout);

    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd < 0) {
        perror("patient: open fifo");
        return 1;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%d %d\n", pt_id, bed_id);
    write(fd, buf, strlen(buf));
    close(fd);

    return 0;
}
