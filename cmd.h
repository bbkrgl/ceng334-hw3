#ifndef __HW3_CMD__
#define __HW3_CMD__

#include "filesystem.h"
#include <time.h>

void ls(char*, int pp);
void cd(char* newcwd);
void cat(char* file);
void touch(char* file);
void mkdir(char* file);

#endif
