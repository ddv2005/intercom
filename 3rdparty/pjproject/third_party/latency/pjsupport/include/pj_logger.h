#ifndef __PJ_LOGGER_H
#define	__PJ_LOGGER_H

#include "pj_lock.h"

#define	PJ_LOG_MAX_LOGFILENAME		128
#define	PJ_LOG_DEF_MAX_LOGSIZE		100*1024*1024
#define	PJ_LOG_DEF_MAX_LOGFILES		3

pj_status_t pj_logging_init(pj_pool_t *pool);
pj_status_t pj_logging_start();
void pj_logging_shutdown(void);
void pj_logging_setMaxLogFileSize(pj_int64_t maxLogFileSize);
void pj_logging_setMaxLogFiles(pj_uint16_t maxLogFiles);
void pj_logging_setLogToConsole(pj_bool_t logToConsole);
void pj_logging_setFilename(const char *filename);
void pj_logging_destroy(void);

#endif	/* __PJ_LOGGER_H */
