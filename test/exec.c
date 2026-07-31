#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv, char** envp) {
    execve(argv[1], &argv[1], envp);

    printf("Failed to execute\n");

    return 1;
}
