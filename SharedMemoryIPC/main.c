#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#define SHM_SIZE 1024
#define ITERATIONS 100000
#define SEMAPHORE_NAME "/my_semaphore"

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void writer_process(int shmid, sem_t *sem) {
    char *shm_ptr = (char *)shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (size_t i = 0; i < ITERATIONS; i++) {
        for (int i = 0; i < SHM_SIZE; i++) {
            shm_ptr[i] = '0' + (rand() % 10);
        }
        sem_post(sem);
        sem_wait(sem);
    }

    gettimeofday(&end, NULL);

    double elapsed = elapsed_time(start, end);
    double latency = elapsed / ITERATIONS;
    double throughput = ((double)(SHM_SIZE * ITERATIONS) / elapsed) * 1e6;
    printf("Shared memory Elapsed Time: %d microseconds, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
           (int)elapsed, latency, throughput / 1024 / 1024);

    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
        exit(EXIT_FAILURE);
    }
}

void reader_process(int shmid, sem_t *sem) {
    char *shm_ptr = (char *)shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < ITERATIONS; i++) {
        sem_wait(sem);
        char buffer[SHM_SIZE];
        memcpy(buffer, shm_ptr, SHM_SIZE);
        sem_post(sem);
    }

    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    int shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    sem_t *sem = sem_open(SEMAPHORE_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        writer_process(shmid, sem);
        exit(0);
    } else {
        sleep(1);
        reader_process(shmid, sem);
        wait(NULL);
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failed");
        exit(EXIT_FAILURE);
    }

    sem_close(sem);
    sem_unlink(SEMAPHORE_NAME);

    return 0;
}
