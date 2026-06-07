#include <stdio.h>
#include <elf.h>

int main(int argc, char** argv, char** envp) {
	for (int i = 0; i < argc; i++) {
		printf("argv[%d]=%s\n", i, argv[i]);
	}

	printf("\n\n");

	for (char **env = envp; *env != 0; env++) {
    		char *thisEnv = *env;
    		printf("%s\n", thisEnv);    
  	}

	return 0;
}
