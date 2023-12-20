#include "uni_log.h"
#include <stdio.h>
#include "../config.h"

static void uni_log_console(const char * text)
{
	DEBUG_MSG("%s\n",text);
}

int itmp = uni_logger::uni_log_add_func(&uni_log_console);






