#include "pj_logger.h"
#include <pj/file_io.h>
#if PJ_ANDROID
#include <android/log.h>
#endif
#ifndef __WINDOWS
#define sprintf_s snprintf
#endif


struct _pj_log_data_t
{
	pj_oshandle_t 	logFile;
	pj_lock_t *		lock;
	pj_bool_t		logToConsole;
	pj_bool_t		flushLogFile;
	pj_char_t		tmpFilename[PJ_LOG_MAX_LOGFILENAME+1];
	pj_char_t		tmp1Filename[PJ_LOG_MAX_LOGFILENAME+1];
	pj_char_t		logFilename[PJ_LOG_MAX_LOGFILENAME+1];
	pj_char_t		logFilenameActual[PJ_LOG_MAX_LOGFILENAME+1];
	pj_off_t		maxLogFileSize;
	pj_uint16_t		maxLogFiles;
	pj_log_func *	old_writer;
};

static _pj_log_data_t pj_log_data;

void pj_log_check_file();

void pj_logging_setMaxLogFileSize(pj_off_t maxLogFileSize)
{
	pj_lock_acquire(pj_log_data.lock);
	pj_log_data.maxLogFileSize = maxLogFileSize;
	pj_lock_release(pj_log_data.lock);
}

void pj_logging_setMaxLogFiles(pj_uint16_t maxLogFiles)
{
	pj_lock_acquire(pj_log_data.lock);
	if(maxLogFiles<2)
			maxLogFiles = 2;
	pj_log_data.maxLogFiles = maxLogFiles;
	pj_lock_release(pj_log_data.lock);
}

void pj_logging_setLogToConsole(pj_bool_t logToConsole)
{
	pj_lock_acquire(pj_log_data.lock);
	pj_log_data.logToConsole = logToConsole;
	pj_lock_release(pj_log_data.lock);
}

void pj_logging_setFilename(const char *filename)
{
	pj_lock_acquire(pj_log_data.lock);
	if(pj_log_data.logFile)
	{
		pj_file_close(pj_log_data.logFile);
		pj_log_data.logFile = NULL;
	}

	strncpy(pj_log_data.logFilename,filename,PJ_LOG_MAX_LOGFILENAME);
	strncpy(pj_log_data.logFilenameActual,filename,PJ_LOG_MAX_LOGFILENAME);
	pj_log_data.logFilenameActual[PJ_LOG_MAX_LOGFILENAME] = 0;
	pj_log_data.logFilename[PJ_LOG_MAX_LOGFILENAME] = 0;

	pj_log_check_file();
	pj_lock_release(pj_log_data.lock);
}

void pj_log_check_file()
{
	if(pj_log_data.logFilename[0]!=0)
	{
		if(!pj_log_data.logFile)
		{
			pj_file_open(NULL,pj_log_data.logFilenameActual,PJ_O_WRONLY | PJ_O_APPEND,&pj_log_data.logFile);
			int i=1;
			while((pj_log_data.logFile==NULL)&&(i<10))
			{
				strncpy(pj_log_data.logFilenameActual,pj_log_data.logFilename,PJ_LOG_MAX_LOGFILENAME);
				pj_log_data.logFilenameActual[PJ_LOG_MAX_LOGFILENAME] = 0;
				int cnt = strlen(pj_log_data.logFilenameActual);
				if(cnt>PJ_LOG_MAX_LOGFILENAME)
					break;
				sprintf_s(&pj_log_data.logFilenameActual[cnt],PJ_LOG_MAX_LOGFILENAME-cnt,"%u.log",i);
				pj_log_data.logFilenameActual[PJ_LOG_MAX_LOGFILENAME] = 0;

				const char *pp = strchr(pj_log_data.logFilename,'.');
				if(pp!=NULL)
				{
					int cnt=pp-pj_log_data.logFilename;
					if((cnt>=0)&&(cnt<PJ_LOG_MAX_LOGFILENAME))
					{
						sprintf_s(&pj_log_data.logFilenameActual[cnt],PJ_LOG_MAX_LOGFILENAME-cnt,"_%u%s",i,pp);
						pj_log_data.logFilenameActual[PJ_LOG_MAX_LOGFILENAME] = 0;
					}
				}
				pj_file_open(NULL,pj_log_data.logFilenameActual,PJ_O_WRONLY | PJ_O_APPEND,&pj_log_data.logFile);
			}
		}
		if(pj_log_data.logFile)
			pj_file_setpos(pj_log_data.logFile,0,PJ_SEEK_END);
	}
	if(pj_log_data.logFile && (pj_log_data.maxLogFileSize>0))
	{
		pj_off_t size=0;
		pj_file_getpos(pj_log_data.logFile,&size);
		if(size>pj_log_data.maxLogFileSize)
		{
			if(pj_log_data.maxLogFiles<2)
				pj_log_data.maxLogFiles = 2;
			sprintf_s(pj_log_data.tmpFilename,PJ_LOG_MAX_LOGFILENAME,"%s.%u",pj_log_data.logFilenameActual,pj_log_data.maxLogFiles-1);
			pj_log_data.tmpFilename[PJ_LOG_MAX_LOGFILENAME] = 0;
			pj_file_delete(pj_log_data.tmpFilename);
			for(unsigned int i=pj_log_data.maxLogFiles-2;i>0; i--)
			{
				sprintf_s(pj_log_data.tmpFilename,PJ_LOG_MAX_LOGFILENAME,"%s.%u",pj_log_data.logFilenameActual,i);
				sprintf_s(pj_log_data.tmp1Filename,PJ_LOG_MAX_LOGFILENAME,"%s.%u",pj_log_data.logFilenameActual,i+1);
				pj_log_data.tmpFilename[PJ_LOG_MAX_LOGFILENAME] = 0;
				pj_log_data.tmp1Filename[PJ_LOG_MAX_LOGFILENAME] = 0;
				pj_file_move(pj_log_data.tmpFilename,pj_log_data.tmp1Filename);
				pj_file_delete(pj_log_data.tmpFilename);
			}
			pj_file_close(pj_log_data.logFile);
			pj_log_data.logFile = NULL;
			sprintf_s(pj_log_data.tmpFilename,PJ_LOG_MAX_LOGFILENAME,"%s.1",pj_log_data.logFilenameActual);
			pj_log_data.tmpFilename[PJ_LOG_MAX_LOGFILENAME] = 0;
			pj_file_move(pj_log_data.logFilenameActual,pj_log_data.tmpFilename);
			pj_log_check_file();
		}
	}
}

static void pj_log_writer_ex(int level, const char *buffer, int len)
{
	if(pj_log_data.logToConsole)
	{
#if PJ_ANDROID
		__android_log_print(ANDROID_LOG_DEBUG, "AG", "%s", buffer);
#else
		pj_log_write(level, buffer, len);
#endif
	}

	pj_lock_acquire(pj_log_data.lock);
	pj_log_check_file();
	if(pj_log_data.logFile)
	{
		pj_ssize_t size = len;
		pj_file_write(pj_log_data.logFile,buffer, &size);
		if(pj_log_data.flushLogFile)
			pj_file_flush(pj_log_data.logFile);
	}
	pj_lock_release(pj_log_data.lock);
}

pj_status_t pj_logging_init(pj_pool_t *pool)
{
	memset(&pj_log_data,0,sizeof(pj_log_data));
	PJS_ASSERT(pj_lock_create_simple_mutex(pool,NULL,&pj_log_data.lock)==PJ_SUCCESS);
	pj_log_data.logToConsole = true;
	pj_log_data.maxLogFiles = PJ_LOG_DEF_MAX_LOGFILES;
	pj_log_data.maxLogFileSize = PJ_LOG_DEF_MAX_LOGSIZE;
	pj_log_data.flushLogFile = true;

    return PJ_SUCCESS;
}

pj_status_t pj_logging_start()
{
	pj_log_check_file();
	pj_log_data.old_writer = pj_log_get_log_func();
    pj_log_set_log_func( &pj_log_writer_ex );

    return PJ_SUCCESS;
}


void pj_logging_shutdown(void)
{
	pj_log_set_log_func( pj_log_data.old_writer );


	pj_lock_acquire(pj_log_data.lock);
	if(pj_log_data.logFile)
	{
		pj_file_close(pj_log_data.logFile);
		pj_log_data.logFile = NULL;
	}
	pj_lock_release(pj_log_data.lock);
}

void pj_logging_destroy(void)
{
	pj_lock_destroy(pj_log_data.lock);
}

