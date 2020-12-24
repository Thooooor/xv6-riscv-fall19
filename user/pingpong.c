#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXOPBLOCKS 10
#define BSIZE 1024
#define BUFSZ  (MAXOPBLOCKS+2)*BSIZE
char buf[BUFSZ];

int main(int argc, char *argv[]) {
    if (argc != 1) {    // 判断参数数量
        printf("Please don't add any PARAMTERS!\n");
    } else {
        int p1[2], p2[2];
        int pid, child_pid, parent_pid;
        // 创建两个管道
        if (pipe(p1) != 0 || pipe(p2) != 0) {
            printf("Pipe failed.\n");
            exit();
        }
        // 创建子进程
        if ((pid = fork()) < 0 ) {
            printf("Fork error!\n");
            exit();
        } 
        // 子进程
        if (pid == 0) {
            child_pid = getpid();
            read(p1[0], buf, sizeof(buf));
            close(p1[0]);
            printf("%d: received %s\n", child_pid, buf);
            write(p2[1], "pong", 4);
            close(p2[1]);
            exit();
        } else {    // 父进程
            parent_pid = getpid();
            write(p1[1], "ping", 4);
            close(p1[1]);
            read(p2[0], buf, sizeof(buf));
            close(p2[0]);
            printf("%d: received %s\n", parent_pid ,buf);
            exit();
        }
    }
    exit();
}