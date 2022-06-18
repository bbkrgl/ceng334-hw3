#include "cmd.h"

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
	open_fs(argv[1]);

	int i = 0;
	char curr;
	char strbuffer[BUFFER_SIZE];
	
	char* cmd = 0;
	char** args = 0;
	int cmd_argc = 0;

	printf("%s> ", CWD);

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
				cmd_argc++;
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

			if (!strcmp(cmd, "quit")) {
				free(cmd);
				for (int j = 0; j < cmd_argc; j++)
					free(args[j]);
				free(args);

				return 0;
			} else if (!strcmp(cmd, "cd")) {
				if (cmd_argc > 0)
					cd(args[0]);
			} else if (!strcmp(cmd, "ls")) {
				if (cmd_argc > 0) {
					if (!strcmp(args[0], "-l")) {
						if (cmd_argc > 1)
							ls(args[1], 1);
						else
							ls(CWD, 1);
					} else {
						ls(args[0], 0);
					}
				} else {
					ls(CWD, 0);
				}
			} else if (!strcmp(cmd, "mkdir")) {
				if (cmd_argc > 0)
					mkdir(args[0]);
			} else if (!strcmp(cmd, "touch")) {
				if (cmd_argc > 0)
					touch(args[0]);
			} else if (!strcmp(cmd, "mv")) {
				if (cmd_argc > 1)
					mv(args[0], args[1]);
			} else if (!strcmp(cmd, "cat")) {
				if (cmd_argc > 0)
					cat(args[0]);
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

			printf("%s> ", CWD);
		} else {
			strbuffer[i] = curr;
			i++;
		}
	}
	return 0;
}
