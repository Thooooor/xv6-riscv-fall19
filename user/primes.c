#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv) {
    if (argc != 1) {
        printf("Please enter No PARAMETERS.\n");
    }
    int list[34];

    for (int i = 0; i <= 33; i++) {
        list[i] = i + 2;
    }

    for (int i = 0; i < sizeof(list); i++) {

    }

    exit();
}

int getleft(int *list) {
    
}