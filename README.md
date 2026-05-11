# os-hospital-management-system
---

## Build & Run

```bash
make all          # compile everything
make run          # start hospital (default: best-fit)
make test         # run stress test (20 patients)
make clean        # remove binaries and IPC artifacts
```

### Start with specific strategy
```bash
./scripts/start_hospital.sh --strategy best    # Best-Fit (default)
./scripts/start_hospital.sh --strategy first   # First-Fit
./scripts/start_hospital.sh --strategy worst   # Worst-Fit
```

### Admit a patient
```bash
./scripts/triage.sh <name> <age> <severity 1-10>
./scripts/triage.sh Ali 25 8
```

### Shutdown
```bash
./scripts/stop_hospital.sh
```

---

## OS Concepts Demonstrated

| Concept | Where |
|---|---|
| fork() + execv() | admissions.c spawns patient_sim per patient |
| SIGCHLD + waitpid(WNOHANG) | Zombie reaping in admissions.c |
| Anonymous Pipe | triage.sh → admissions (patient data) |
| Named FIFO | patient_sim → admissions (discharge) |
| Shared Memory (shmget) | Ward bitmap shared across processes |
| POSIX Threads | Receptionist, Scheduler, 3 Nurse threads |
| Mutex | Protects bed bitmap from race conditions |
| Condition Variable | Scheduler waits on bed_freed signal |
| Semaphores | Enforces ICU=4, ISO=4 capacity limits |
| Producer-Consumer | Receptionist enqueues, Scheduler dequeues |
| Best-Fit Allocator | Selects smallest fitting bed partition |
| Coalescing | Merges adjacent free beds after discharge |
| Fragmentation Report | Logged after every alloc/dealloc |
| Paging Simulation | Internal fragmentation per patient |

---

## File Structure

```
hospital_project/
├── src/
│   ├── hospital.h       # shared structs, constants, prototypes
│   ├── admissions.c     # main process + all threads
│   ├── patient_sim.c    # per-patient child process
│   ├── bed_allocator.c  # memory management + queue
│   ├── scheduler.c      # FCFS, SJF, Priority, RR simulation
│   └── scheduler.h
├── scripts/
│   ├── triage.sh        # patient intake script
│   ├── start_hospital.sh
│   ├── stop_hospital.sh
│   └── stress_test.sh
├── logs/
│   ├── schedule_log.txt
│   └── memory_log.txt
├── Makefile
└── README.md
```

---

## Dependencies
- Linux (Ubuntu 20.04+)
- gcc with pthread support
- POSIX shared memory (librt)

---

## Notes
- All IPC is cleaned up automatically on shutdown
- Valgrind: `valgrind --leak-check=full ./admissions`
