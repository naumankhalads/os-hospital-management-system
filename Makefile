CC     = gcc
CFLAGS = -Wall -Wextra -pthread -I./src
LFLAGS = -lpthread -lrt

SRC    = src/bed_allocator.c src/scheduler.c
ADM    = src/admissions.c
SIM    = src/patient_sim.c

all: admissions patient_sim

admissions: $(ADM) $(SRC)
	$(CC) $(CFLAGS) -o admissions $(ADM) $(SRC) $(LFLAGS)

patient_sim: $(SIM)
	$(CC) $(CFLAGS) -o patient_sim $(SIM) $(LFLAGS)

run: all
	@mkdir -p logs
	./start_hospital.sh

test: all
	@mkdir -p logs
	./scripts/stress_test.sh

clean:
	rm -f admissions patient_sim
	rm -f /tmp/discharge_fifo
	rm -f logs/*.txt
	ipcrm -M 0xBEDF00D 2>/dev/null || true
	sem_unlink /sem_icu_limit 2>/dev/null || true
	sem_unlink /sem_iso_limit 2>/dev/null || true
	sem_unlink /sem_queue_bound 2>/dev/null || true

.PHONY: all run test clean
