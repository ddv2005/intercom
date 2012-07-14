#include "project_common.h"
#include "project_global.h"
#include "project_config.h"
#include "pjs_intercom.h"
#include "pjs_lua.h"
#include <pjsua-lib/pjsua.h>
#include <signal.h>
#include <semaphore.h>

sem_t exitSignal;

void termination_handler(int signum)
{
	printf("Shutdown Event %d\n", signum);
	switch (signum)
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
	pjs_config_t config;
	pjs_intercom_system pjsis;
	pjs_config_init(config);
	pjs_lua *lua_c;
	const char * config_file_name = "config.lua";

	if (argc > 1)
		config_file_name = argv[1];
	lua_c = new pjs_lua();
	if (lua_c->load_script(config_file_name) == 0)
	{
		lua_c->run();
		lua_getglobal(lua_c->get_state(), "config");
		if (lua_isfunction(lua_c->get_state(),-1))
		{
			lua_c->push_swig_object(&config, "pjs_config_t *");
			int Error = lua_pcall(lua_c->get_state(),1,LUA_MULTRET,0);
			if (Error)
			{
				printf("LUA Config error: %s\n", lua_tostring(lua_c->get_state(),-1));
				lua_pop(lua_c->get_state(), 1);
				exit(-1);
			}
		}
		else
		{
			printf("Can't find config function\n");
			exit(-3);
		}
	}
	else
	{
		printf("Can't load config file: %s\n", config_file_name);
		exit(-2);
	}

	delete lua_c;
	pjsis.set_config(config);

	sem_init(&exitSignal, 1, 1);

	if (pjsis.start() == RC_OK)
	{
		sem_wait(&exitSignal);
		signal(SIGINT, termination_handler);
		signal(SIGHUP, termination_handler);
		signal(SIGTERM, termination_handler);
		sem_wait(&exitSignal);
		pjsis.stop();
	}
	else
		printf("Can't start system\n");
	sem_destroy(&exitSignal);
	return 0;
}

