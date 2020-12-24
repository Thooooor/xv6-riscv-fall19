#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void my_close(int port, int pd[]) {
    close(port);
    dup(pd[port]);
    close(pd[0]);
    close(pd[1]);
}

void pipe_number(int number) {
    int recieved;
    // 从管道中读出数字，写入recieved
    while (read(0, &recieved, sizeof(recieved))) {
        if (recieved % number != 0) {   // 过滤掉能被number整除的数
            write(1, &recieved, sizeof(recieved));
        }
    }
}

void primes() {
    int pid;
    int pd[2];
    int number;
    // 读入管道中的第一个数，为素数
    if (read(0, &number, sizeof(number))) {
        printf("prime %d\n", number);
    } else {
        exit();
    }
    // 创建管道
    if (pipe(pd) != 0) {
        printf("Pipe failed.\n");
        exit();
    }
    // 创建子进程
    if ((pid = fork()) < 0) {
        printf("Fork error!\n");
        exit();
    }
    // 子进程筛选素数
    if (pid == 0) {
        my_close(1, pd);    // 关闭当前子进程的文件和管道
        pipe_number(number);
    } else {    // 父进程继续调用primes()
        my_close(0, pd);    // 关闭当前父进程的文件和管道
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

    if (pid == 0) { // 对子进程输入2-35
        my_close(1, pd);
        for (int i = 2; i <= 35; i++) {
            write(1, &i, sizeof(i));
        }
    } else {
        my_close(0, pd);
        primes();
    }
    exit();
}
