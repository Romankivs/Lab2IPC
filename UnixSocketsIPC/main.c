#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#define SOCKET_PATH "/tmp/unix_socket_temp"
#define ITERATIONS 100000
#define BLOCK_SIZE 1024

double elapsed_time(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
}

void reader_process(void) {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char *buffer = (char *)malloc(BLOCK_SIZE);

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (size_t i = 0; i < ITERATIONS; i++) {
        ssize_t bytes_received = recv(client_fd, buffer, BLOCK_SIZE, 0);
        if (bytes_received < 0) {
            perror("recv failed");
            free(buffer);
            close(client_fd);
            close(server_fd);
            exit(EXIT_FAILURE);
        }
    }

    gettimeofday(&end, NULL);

    double elapsed = elapsed_time(start, end);
    double latency = elapsed / ITERATIONS;
    double throughput = ((double)(BLOCK_SIZE * ITERATIONS) / elapsed) * 1e6;
    printf("Unix socket Elapsed Time: %d microseconds, Latency: %.2f microseconds, Throughput: %.2f MB/s\n",
           (int)elapsed, latency, throughput / 1024 / 1024);

    free(buffer);
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
}

void writer_process(void) {
    int sock_fd;
    struct sockaddr_un addr;
    char *buffer = (char *)malloc(BLOCK_SIZE);

    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        free(buffer);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < ITERATIONS; i++) {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            buffer[i] = '0' + (rand() % 10);
        }
        if (send(sock_fd, buffer, BLOCK_SIZE, 0) < 0) {
            perror("send failed");
            free(buffer);
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
    }

    free(buffer);
    close(sock_fd);
}

int main(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        reader_process();
    } else {
        sleep(1);
        writer_process();
        wait(NULL);
    }

    return 0;
}
