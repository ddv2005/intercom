#include "pjs_emulator_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

pjs_emulator_controller::pjs_emulator_controller(pjs_global &global,pjs_external_controller_callback &callback):
	pjs_external_controller(global,callback),m_event()
{
	m_exit = false;
	m_thread = NULL;
	m_lock = NULL;
	m_pool = pj_pool_create(m_global.get_pool_factory(),"pjs_emulator_controller",128,128,NULL);
	if(m_pool)
	{
		m_lock = new PPJ_MutexLock(m_pool,NULL);
		if(m_lock)
		{
			pj_status_t status = pj_thread_create(m_pool, "pjs_emulator_controller thread", (pj_thread_proc*)&s_thread_proc,
						  this,
						  PJ_THREAD_DEFAULT_STACK_SIZE,
						  0,
						  &m_thread);
			if (status == PJ_SUCCESS)
			{

			}
			else
				m_thread = NULL;
		}
	}
}

void* pjs_emulator_controller::thread_proc()
{
    struct termios oldSettings, newSettings;

    tcgetattr( fileno( stdin ), &oldSettings );
    newSettings = oldSettings;
    newSettings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr( fileno( stdin ), TCSANOW, &newSettings );

	PJ_LOG_( INFO_LEVEL,(__FILE__,"pjs_emulator_controller thread start"));
	while(!m_exit)
	{
        fd_set set;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO( &set );
        FD_SET( fileno( stdin ), &set );

        int res = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

        if( res > 0 )
        {
        	pjs_intercom_message_t message;
            char c;
            read( fileno( stdin ), &c, 1 );
            PJ_LOG_(INFO_LEVEL,(__FILE__,"got char %c",c));
            switch(c)
            {
            case '1':
            {
        		memset(&message,0,sizeof(message));
        		message.message = IM_MAIN_BUTTON;
        		message.param1 = IBS_DOWN;

        		fire_on_message(message);

        		memset(&message,0,sizeof(message));
        		message.message = IM_MAIN_BUTTON;
        		message.param1 = IBS_UP;

        		fire_on_message(message);

            	break;
            }
            case '2':
            {
        		memset(&message,0,sizeof(message));
        		message.message = IM_INPUT1;
        		message.param1 = IBS_DOWN;

        		fire_on_message(message);

        		memset(&message,0,sizeof(message));
        		message.message = IM_INPUT1;
        		message.param1 = IBS_UP;

        		fire_on_message(message);

            	break;
            }
            case '3':
            {
        		memset(&message,0,sizeof(message));
        		message.message = IM_INPUT2;
        		message.param1 = IBS_DOWN;

        		fire_on_message(message);
            	break;
            }
            case '4':
            {
        		memset(&message,0,sizeof(message));
        		message.message = IM_INPUT2;
        		message.param1 = IBS_UP;

        		fire_on_message(message);

            	break;
            }

            }
        }
        else if( res < 0 )
        {
            break;
        }
		m_event.wait(5);
	}

    tcsetattr( fileno( stdin ), TCSANOW, &oldSettings );
	PJ_LOG_( INFO_LEVEL,(__FILE__,"pjs_emulator_controller thread end"));
	return NULL;
}

pjs_emulator_controller::~pjs_emulator_controller()
{
	m_exit = true;
	m_event.signal();
	if(m_thread)
	{
		pj_thread_join(m_thread);
		pj_thread_destroy(m_thread);
	}
	if(m_lock)
		delete m_lock;
	if(m_pool)
		pj_pool_release(m_pool);

}

pj_int32_t pjs_emulator_controller::set_output(pj_uint8_t  output, pj_uint8_t value)
{
	pj_int32_t result = RC_OK;
	return result;
}



