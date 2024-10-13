#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define ITERATIONS 100000
#define BLOCK_SIZE 1024

struct message {
    long mtype;
    char mtext[BLOCK_SIZE];
};

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void test_queue_ipc(void) {
    struct timeval start, end;
    pid_t pid;
    size_t i;
    int msgqid;
    key_t key = 1234;

    msgqid = msgget(key, IPC_CREAT | 0666);
    if (msgqid < 0) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Read from the message queue
        struct message msg;
        gettimeofday(&start, NULL);

        for (i = 0; i < ITERATIONS; i++) {
            if (msgrcv(msgqid, &msg, BLOCK_SIZE, 1, 0) < 0) {
                perror("msgrcv failed");
                exit(EXIT_FAILURE);
            }
        }

        gettimeofday(&end, NULL);

        double elapsed = elapsed_time(start, end);
        double latency = elapsed / ITERATIONS;
        double throughput = ((double)(BLOCK_SIZE * ITERATIONS) / elapsed) * 1e6;
        printf("Message queue Elapsed Time: %d microseconds, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
               (int)elapsed, latency, throughput / 1024 / 1024);

        exit(0);
    } else {
        // Write data to the message queue
        struct message msg;
        msg.mtype = 1;

        for (i = 0; i < ITERATIONS; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                msg.mtext[j] = '0' + (rand() % 10);
            }
            if (msgsnd(msgqid, &msg, BLOCK_SIZE, 0) < 0) {
                perror("msgsnd failed");
                exit(EXIT_FAILURE);
            }
        }

        wait(NULL);
    }

    msgctl(msgqid, IPC_RMID, NULL);
}

int main(void) {
    test_queue_ipc();
    return 0;
}
