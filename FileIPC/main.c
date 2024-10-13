#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>

#define FILE_PATH "ipc_test_file"
#define ITERATIONS 100000
#define BLOCK_SIZE 1024

#define SEM_NAME "/my_semaphore"

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void test_file_ipc(const char *type) {
    struct timeval start, end;
    pid_t pid;
    int fd;
    size_t i;

    char *buffer = malloc(BLOCK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    fd = open(FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open failed");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);

    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        close(fd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Read from the file
        for (i = 0; i < ITERATIONS; i++) {
            sem_wait(sem);
            lseek(fd, 0, SEEK_SET);
            read(fd, buffer, BLOCK_SIZE);
            sem_post(sem);
        }
        close(fd);
        exit(0);
    } else {
        // Write to the file
        gettimeofday(&start, NULL);

        for (i = 0; i < ITERATIONS; i++) {
            lseek(fd, 0, SEEK_SET);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                buffer[i] = '0' + (rand() % 10);
            }
            write(fd, buffer, BLOCK_SIZE);
            sem_post(sem);
            sem_wait(sem);
        }

        gettimeofday(&end, NULL);

        double elapsed = elapsed_time(start, end);
        double latency = elapsed / ITERATIONS;
        double throughput = ((double)(BLOCK_SIZE * ITERATIONS) / elapsed) * 1e6;
        printf("File Elapsed Time: %d microseconds, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
               (int)elapsed, latency, throughput / 1024 / 1024);
    }

    close(fd);
    free(buffer);
    sem_close(sem);
    sem_unlink(SEM_NAME);
}

int main(void) {
    test_file_ipc("File-based IPC");
    unlink(FILE_PATH);
    return 0;
}
