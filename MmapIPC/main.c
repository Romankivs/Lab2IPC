#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>

#define FILE_PATH "mmap_test_file"
#define SIZE (1024L * 1024L)
#define ITERATIONS 100000
#define BLOCK_SIZE 1024

#define SEM_NAME "/mmap_semaphore"

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void test_mmap(const char *type, int mmap_flags, int fd, int locked) {
    struct timeval start, end;
    void *addr;
    pid_t pid;
    size_t i;

    char *buffer = malloc(BLOCK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    addr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, mmap_flags, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    if (locked && mlock(addr, SIZE) != 0) {
        perror("mlock failed");
        munmap(addr, SIZE);
        exit(EXIT_FAILURE);
    }

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        munmap(addr, SIZE);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Read from shared memory
        for (i = 0; i < ITERATIONS; i++) {
            sem_wait(sem);

            for (int i = 0; i < BLOCK_SIZE; i++) {
                buffer[i] = '0' + (rand() % 10);
            }

            memcpy(buffer, (char *)addr + (i % (SIZE - BLOCK_SIZE)), BLOCK_SIZE);
            sem_post(sem);
        }
        exit(0);
    } else {
        // Write to shared memory
        gettimeofday(&start, NULL);

        for (i = 0; i < ITERATIONS; i++) {
            memcpy((char *)addr + (i % (SIZE - BLOCK_SIZE)), buffer, BLOCK_SIZE);
            sem_post(sem);
            sem_wait(sem);
        }

        gettimeofday(&end, NULL);

        double elapsed = elapsed_time(start, end);
        double latency = elapsed / ITERATIONS;
        double throughput = ((double)(BLOCK_SIZE * ITERATIONS) / elapsed) * 1e6;
        printf("%s - Elapsed Time: %d microseconds, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
               type, (int)elapsed, latency, throughput / 1024 / 1024);
    }

    // Cleanup
    munmap(addr, SIZE);
    if (locked) {
        munlock(addr, SIZE);
    }

    free(buffer);
    sem_close(sem);
    sem_unlink(SEM_NAME);
}

int main() {
    int fd;

    fd = open(FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open failed");
        return EXIT_FAILURE;
    }
    if (ftruncate(fd, SIZE) != 0) {
        perror("ftruncate failed");
        close(fd);
        return EXIT_FAILURE;
    }

    test_mmap("File-backed mmap", MAP_SHARED, fd, 0);
    test_mmap("Anonymous mmap", MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    test_mmap("Locked mmap", MAP_SHARED, fd, 1);

    close(fd);
    unlink(FILE_PATH);

    return 0;
}
