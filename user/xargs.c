#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char buf2[512];
    char buf[32][32];
    char *pass[32];

    for (int i = 0; i < 32; i++) pass[i] = buf[i];

    // 依次读入
    for (int i = 1; i < argc; i++) strcpy(buf[i - 1], argv[i]);
    // 依次处理
    int n;
    while ((n = read(0, buf2, sizeof(buf2))) > 0) {
        int pos = argc - 1; // 参数数量
        char *c = buf[pos];
        // 根据空行或者换行符分割参数和指令
        for (char *p = buf2; *p; p++) {
            if (*p == ' ' || *p == '\n') {
                *c = '\0';
                pos++;
                c = buf[pos];
            } else *c++ = *p;
        }

        *c = '\0';
        pos++;
        pass[pos] = 0;

        if (fork()) wait(); // 父进程等待子进程执行结束
        else exec(pass[0], pass);   // 子进程执行命令
    }

    if (n < 0) {
        printf("xargs: read error\n");
        exit();
    }

    exit();
}
