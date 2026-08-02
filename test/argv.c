#include <stdio.h>

int main(int argc, char** argv, char** envp) {
	for (int i = 0; i < argc; i++) {
		printf("argv[%d]=%s\n", i, argv[i]);
	}

	printf("\n\n");

	size_t index = 0;
	for (char **env = envp; *env != 0; env++) {
		char *thisEnv = *env;
		printf("envp[%zu]=%s\n", index, thisEnv);
		index++;
  	}

	return 0;
}
