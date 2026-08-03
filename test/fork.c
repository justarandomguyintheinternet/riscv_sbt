#include <unistd.h>
#include <stdio.h>

int main() {
    int pid = fork();

    if (pid < 0) {
        printf("Failed to fork\n");
        return 1;
    }

    if (pid == 0) {
        printf("child\n");
    } else {
        printf("parent of %d\n", pid);
    }

    return 0;
}
