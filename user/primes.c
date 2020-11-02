#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void closepipe(int port, int pd[]) {
    close(port);
    dup(pd[port]);
    close(pd[0]);
    close(pd[1]);
}

void pipenumber(int number) {
    int recieved;
    while (read(0, &recieved, sizeof(recieved))) {
        if (recieved % number != 0) {
            write(1, &recieved, sizeof(recieved));
        }
    }
}

void primes() {
    int pid;
    int pd[2];
    int number;

    if (read(0, &number, sizeof(number))) {
        printf("prime %d\n", number);
    } else {
        exit();
    }

    if (pipe(pd) != 0) {
        printf("Pipe failed.\n");
        exit();
    }

    if ((pid = fork()) < 0) {
        printf("Fork error!\n");
        exit();
    }

    if (pid == 0) {
        closepipe(1, pd);
        pipenumber(number);
    } else {
        closepipe(0, pd);
        primes();
    }
}

int main(int argc, char *argv) {
    if (argc != 1) {
        printf("Please enter No PARAMETERS.\n");
    }
    int pd[2];
    int pid;

    if (pipe(pd) != 0) {
        printf("Pipe failed.\n");
        exit();
    }

    if ((pid = fork()) < 0) {
        printf("Fork error!\n");
        exit();
    }

    if (pid == 0) {
        closepipe(1, pd);
        for (int i = 2; i <= 35; i++) {
            write(1, &i, sizeof(i));
        }
    } else {
        closepipe(0, pd);
        primes();
    }
    exit();
}
