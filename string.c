#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "string.h"
#include "getch.h"

void str_trim_lf (char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void clear_stdin(void)
{
	stdin->_IO_read_ptr = stdin->_IO_read_end;//清理输入缓冲区
}

char get_cmd(char start,char end)
{
	clear_stdin();

	printf("请输入指令:");
	while(true)
	{
		char val = getch();
		if(val >= start && val <= end)
		{
			printf("%c\n",val);
			return val;
		}
	}
}
