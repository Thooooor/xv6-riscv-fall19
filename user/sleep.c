#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    // 判断参数数量是否符合
    if (argc != 2) {
        printf("Please enter only ONE INT PARAMETER!\n");
    } else {
        int time = atoi(argv[1]);   // 将char转换为int
        if (time) {
            printf("Sleep time = %d\n", time);
            sleep(time);
        } else {
            printf("Please enter An INT > 0.\n");
        }
    }
    exit();
}