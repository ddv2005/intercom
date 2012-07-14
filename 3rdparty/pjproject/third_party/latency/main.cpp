#include "latency.h"
#include <signal.h>
#include <semaphore.h>

sem_t  exitSignal;

void termination_handler (int signum)
{
	printf("Shutdown Event %d\n",signum);
	switch(signum)
	{
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
			{
				sem_post(&exitSignal);
				break;
			}
	}
}

int main(int argc, const char * argv[])
{
	latency_checker l;
	latency_config_t c = create_default_latency_config();
	
	sem_init(&exitSignal,1,1);

	c.logs.level=10;
	c.clock_rate = 48000;
	c.min_frame_length = 10;
	if (l.start(c) == 0) {
		sem_wait(&exitSignal);
		signal(SIGINT, termination_handler);
		signal(SIGHUP, termination_handler);
		signal(SIGTERM, termination_handler);
		sem_wait(&exitSignal);
		l.stop();
	} else
		printf("Can't start system\n");
	sem_destroy(&exitSignal);
	return 0;
}


