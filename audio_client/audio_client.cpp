#include <stdlib.h>
#include <signal.h>
#include "zeromq/include/zmq.h"
#include <pthread.h>
#include <sys/time.h>
#include "pjs_zeromq_common.h"
using namespace std;

extern const unsigned char pjmedia_linear2alaw_tab[16384];
#define pjmedia_linear2alaw(pcm_val)	\
	    pjmedia_linear2alaw_tab[(((short)pcm_val) >> 2) & 0x3fff]

extern const unsigned char pjmedia_linear2ulaw_tab[16384];
#define pjmedia_linear2ulaw(pcm_val)	\
	    pjmedia_linear2ulaw_tab[(((short)pcm_val) >> 2) & 0x3fff]

#define HEADER_SIZE (sizeof(pjs_audio_monitor_zeromq_packet_t)+PJSAMZ_TYPE_SIZE)

class pjs_event
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_triggered;
public:
	pjs_event()
	{
		pthread_mutex_init(&m_mutex, 0);
		pthread_cond_init(&m_cond, 0);
		m_triggered = false;
	}
	~pjs_event()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	void wait()
	{
		pthread_mutex_lock(&m_mutex);
		while (!m_triggered)
			pthread_cond_wait(&m_cond, &m_mutex);
		m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
	}

	int wait(int timeout)
	{
		pthread_mutex_lock(&m_mutex);
		struct timespec abs_time;
		struct timeval now;

		gettimeofday(&now, NULL);
		abs_time.tv_nsec = (now.tv_usec + 1000UL * (timeout % 1000)) * 1000UL;
		abs_time.tv_sec = now.tv_sec + timeout / 1000
				+ abs_time.tv_nsec / 1000000000UL;
		abs_time.tv_nsec %= 1000000000UL;

		int err = 0;
		while (!m_triggered)
		{
			err = pthread_cond_timedwait(&m_cond, &m_mutex, &abs_time);
			if (err == 0 || err == ETIMEDOUT)
				break;
			if (err == EINVAL)
			{
				err = 0;
				break;
			}
		}
		if (!err)
			m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
		return err;
	}

	void signal()
	{
		pthread_mutex_lock(&m_mutex);
		m_triggered = true;
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}
};

pjs_event exitSignal;

void termination_handler(int signum)
{
	switch (signum)
	{
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
		{
			exitSignal.signal();
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("No connect URI \n");
		return -1;
	}
	void *context = zmq_ctx_new();

	void *subscriber = zmq_socket(context, ZMQ_SUB);
	int rc = zmq_connect(subscriber, argv[1]);
	if (rc == 0)
	{
		rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, NULL, 0);
		if (rc == 0)
		{
			bool f_exit = false;
			signal(SIGINT, termination_handler);
			signal(SIGHUP, termination_handler);
			signal(SIGTERM, termination_handler);

			unsigned char * buffer = (unsigned char*) calloc(4096,1);
			zmq_pollitem_t items[] =
			{
			{ subscriber, 0, ZMQ_POLLIN, 0 } };
			while (!f_exit && exitSignal.wait(10))
			{
				zmq_msg_t message;
				zmq_poll(items, 1, 100);
				if (items[0].revents & ZMQ_POLLIN)
				{
					zmq_msg_init(&message);
					zmq_msg_recv(&message, subscriber, 0);

					size_t size = zmq_msg_size(&message);
					char *data = (char *) zmq_msg_data(&message);
					if (size > HEADER_SIZE)
					{
						data += HEADER_SIZE;
						int count = (size - HEADER_SIZE)/2;
						for(int i=0; i<count;i++)
						{
							buffer[i] = pjmedia_linear2ulaw(*(short*)data);
							data+=sizeof(short);
						}
						f_exit = fwrite(buffer, 1, count, stdout)!=count;
					}
					zmq_msg_close(&message);
				}
			}
			free(buffer);
		}
		else
			printf("Connect error %d\n", rc);
	}
	else
		printf("Context error %d\n", rc);

	zmq_close(subscriber);
	zmq_ctx_destroy(context);
	return 0;
}
