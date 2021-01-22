#ifndef STRING
#define STRING

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "string.h"
#include "getch.h"

void str_trim_lf (char*, int);
void str_overwrite_stdout();

void clear_stdin(void);
char get_cmd(char start,char end);

#endif // STRING