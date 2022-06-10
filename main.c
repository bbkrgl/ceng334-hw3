#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fat32.h"

#define BUFFER_SIZE 256

void cpstr_del(char* str1, char* str2, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		str1[i] = str2[i];
		str2[i] = 0;
	}
	str1[i] = '\0';
}

int main(int argc, char* argv[])
{	
	int i = 0;
	char curr;
	char strbuffer[BUFFER_SIZE];
	
	char* cmd = 0;
	char** args = 0;
	int cmd_argc = 0;

	char* CWD = "/";
	printf("/>");

	while ((curr = getchar()) != EOF && i < BUFFER_SIZE) {
		if (curr == ' ' || (curr == '\n' && i != 0)) {
			if (i == 0)
				continue;

			if (!cmd) {
				cmd = malloc((i + 1) * sizeof(char));
				cpstr_del(cmd, strbuffer, i);
			} else {
				args = realloc(args, (cmd_argc + 1) * sizeof(char*));
				args[cmd_argc] = malloc((i + 1) * sizeof(char));
				cpstr_del(args[cmd_argc], strbuffer, i);
			}

			i = 0;

			if (curr == ' ')
				continue;
		}

		if (curr == '\n') {
			if (!cmd) {
				printf("%s>", CWD);
				continue;
			}

			if (!strcmp(cmd, "exit")) {
				free(cmd);
				for (int j = 0; j < cmd_argc; j++)
					free(args[j]);
				free(args);

				return 0;
			} else if (!strcmp(cmd, "cd")) {
				printf("%s\n", cmd);
			} else if (!strcmp(cmd, "ls")) {
				printf("%s\n", cmd);
			} else if (!strcmp(cmd, "mkdir")) {
				printf("%s\n", cmd);
			} else if (!strcmp(cmd, "touch")) {
				printf("%s\n", cmd);
			} else if (!strcmp(cmd, "mv")) {
				printf("%s\n", cmd);
			} else if (!strcmp(cmd, "cat")) {
				printf("%s\n", cmd);
			} else {
				fprintf(stderr, "Unknown command\n");
			}

			i = 0;
			
			free(cmd);
			for (int j = 0; j < cmd_argc; j++)
				free(args[j]);
			free(args);

			cmd = 0;
			args = 0;
			cmd_argc = 0;

			printf("%s>", CWD);
		} else {
			strbuffer[i] = curr;
			i++;
		}
	}
	return 0;
}
