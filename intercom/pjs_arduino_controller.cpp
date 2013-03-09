#include "pjs_arduino_controller.h"
#include <unistd.h>

#define MAX_PACKET_SIZE	32
//Arduino commands
#define  ACC_VER    1
#define  ACC_ISTAT  2
#define  ACC_SET_RELAY    3
#define  ACC_OSTAT        4
#define  ACC_SET_BTN_LED  5
#define  ACC_SET_FAN_SPEED  6
#define  ACC_SET_FAN_MODE   7
#define  ACC_SET_FAN_TEMP   8
#define  ACC_FAN_DATA       9
#define  ACC_GET_ISTAT      10
#define  ACC_GET_OSTAT      11
#define  ACC_PING           12
#define  ACC_SET_BTN2_LED  	13
#define  ACC_ERROR  0xFF

//Input mask
#define  AIM_BTN    0x1
#define  AIM_INP1   0x2
#define  AIM_INP2   0x4

//FAN MODES
#define AFM_MANUAL  0
#define AFM_AUTO    1

pjs_arduino_controller::pjs_arduino_controller(pjs_global &global,pjs_external_controller_callback &callback):
	pjs_external_controller(global,callback),m_event(),parser(cp_prefix_t(0x55,0xBB))
{
	m_exit = false;
	m_thread = NULL;
	m_lock = NULL;
	serial = NULL;
	just_opened = false;
	m_pool = pj_pool_create(m_global.get_pool_factory(),"pjs_arduino_controller",128,128,NULL);
	if(m_pool)
	{
		m_lock = new PPJ_MutexLock(m_pool,NULL);
		if(m_lock)
		{
			pj_status_t status = pj_thread_create(m_pool, "pjs_arduino_controller thread", (pj_thread_proc*)&s_thread_proc,
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
	m_last_input = m_input = m_relay = m_btn_led =  m_btn2_led = 0;
}

void pjs_arduino_controller::check_input()
{
	if(m_input!=m_last_input)
	{
		if((m_input & AIM_BTN) != (m_last_input & AIM_BTN))
		{
			pjs_intercom_message_t message;
			memset(&message,0,sizeof(message));
			message.message = IM_MAIN_BUTTON;
			message.param1 = (m_input & AIM_BTN)?IBS_DOWN:IBS_UP;

			fire_on_message(message);
		}
		if((m_input & AIM_INP1) != (m_last_input & AIM_INP1))
		{
			pjs_intercom_message_t message;
			memset(&message,0,sizeof(message));
			message.message = IM_INPUT1;
			message.param1 = (m_input & AIM_INP1)?IBS_DOWN:IBS_UP;

			fire_on_message(message);
		}
		if((m_input & AIM_INP2) != (m_last_input & AIM_INP2))
		{
			pjs_intercom_message_t message;
			memset(&message,0,sizeof(message));
			message.message = IM_INPUT2;
			message.param1 = (m_input & AIM_INP2)?IBS_DOWN:IBS_UP;

			fire_on_message(message);
		}
		m_last_input = m_input;
	}
}

#define sendPacket(P,V) serial->Write(newPacketBuffer,parser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),P,&V,sizeof(V)))
#define sendPacketN(P) serial->Write(newPacketBuffer,parser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),P,NULL,0))

void* pjs_arduino_controller::thread_proc()
{
	pjs_controller_config_t config = m_global.get_config().controller;
	pj_char_t temp_sensor_path[sizeof(pjs_system_config_t::temp_sensor)];
	pj_uint8_t newPacketBuffer[MAX_PACKET_SIZE+4];
	pj_uint8_t buffer[MAX_PACKET_SIZE+4];
	pj_timestamp last_activity_time;
	pj_timestamp last_temp_time;
	pj_timestamp last_response_time;
	pj_timestamp open_time;
	pj_timestamp last_check_time;
	bool has_temp_sensor = true;
	just_opened = false;

	strncpy(temp_sensor_path,m_global.get_config().system.temp_sensor,sizeof(temp_sensor_path));
	last_check_time.u64 = last_temp_time.u64 = 0;
	PJ_LOG_( INFO_LEVEL,(__FILE__,"arduino controller thread start"));
	while(!m_exit)
	{
		if(serial)
		{
			pj_timestamp now;
			pj_get_timestamp(&now);

			if(pj_elapsed_msec(&last_check_time,&now)>=1000)
			{
				last_check_time.u64 = now.u64;
				if(!serial->IsSerialAlive())
				{
					m_lock->acquire();
					PJ_LOG_( ERROR_LEVEL,(__FILE__,"serial port %s is closed",config.port));
					serial->Close();
					delete serial;
					serial = NULL;
					m_event.wait(1000);
					m_lock->release();
				}
			}
		}
		if(!serial)
		{
			m_lock->acquire();
			PJ_LOG_( WARN_LEVEL,(__FILE__,"opening serial port %s",config.port));
			serial = new serialib();
			if(serial->Open(config.port,config.speed)==1)
			{
				PJ_LOG_( WARN_LEVEL,(__FILE__,"serial port %s opened",config.port));
				serial->setDTR();
				parser.reset();
				pj_get_timestamp(&last_activity_time);
				open_time = last_response_time = last_activity_time;
				last_temp_time.u64 = 0;
				just_opened = true;
				sendPacketN(ACC_VER);
			}
			else
			{
				PJ_LOG_( ERROR_LEVEL,(__FILE__,"serial port %s can not be opened",config.port));
				delete serial;
				serial = NULL;
			}
			m_lock->release();
		}
		if(serial)
		{
			pj_timestamp now;
			pj_get_timestamp(&now);

			if(just_opened)
			{
				if(pj_elapsed_msec(&open_time,&now)>=10500)
				{
					PJ_LOG_( ERROR_LEVEL,(__FILE__,"serial port %s closed because no response",config.port));
					m_lock->acquire();
					serial->clearDTR();
					serial->setDTR();
					serial->Close();
					delete serial;
					serial = NULL;
					m_lock->release();
					m_event.wait(1000);
					continue;
				}

				if(pj_elapsed_msec(&last_response_time,&now)>=2000)
				{
					PJ_LOG_( WARN_LEVEL,(__FILE__,"Arduino. Resend version request"));
					sendPacketN(ACC_VER);
					last_response_time = now;
				}
			}
			else
			{
				if(pj_elapsed_msec(&last_response_time,&now)>=10000)
				{
					PJ_LOG_( ERROR_LEVEL,(__FILE__,"serial port %s closed because no responses too long",config.port));
					m_lock->acquire();
					serial->clearDTR();
					serial->setDTR();
					serial->Close();
					delete serial;
					serial = NULL;
					m_lock->release();
					m_event.wait(1000);
					continue;
				}

				if(pj_elapsed_msec(&last_activity_time,&now)>=5000)
				{
					pj_get_timestamp(&last_activity_time);
					sendPacketN(ACC_PING);
				}
				if(has_temp_sensor&&(pj_elapsed_msec(&last_temp_time,&now)>=1000)&&temp_sensor_path[0])
				{
					pj_get_timestamp(&last_temp_time);
					int tf=open(temp_sensor_path,O_RDONLY);
					if(tf>=0)
					{
						char temp[32];
						unsigned s=read(tf,temp,sizeof(temp));
						close(tf);
						if(s>=sizeof(temp))
							s=sizeof(temp)-1;
						temp[s]=0;
						int itemp = atoi(temp);
						if(itemp>1000)
							itemp/=1000;
						if((itemp>30)&&(itemp<100))
						{
							byte_t bt=itemp;
							sendPacket(ACC_SET_FAN_TEMP,bt);
							pj_get_timestamp(&last_activity_time);
						}
					}
					else
					{
						PJ_LOG_( ERROR_LEVEL,(__FILE__,"arduino can't open %s. Error %d",temp_sensor_path, (int)errno));
						has_temp_sensor = false;
					}
				}
			}
			if(serial->WaitForInput(50)>0)
			{
				byte_t bytes = parser.get_expected_data_size();
				if(bytes<=sizeof(buffer))
				{
					byte_t ret = serial->RawRead(buffer,bytes);
					if(ret>0)
					{
						if(parser.post_data(buffer,ret) == CP_RESULT_PACKET_READY)
						{
							byte_t size = parser.get_packet(buffer,sizeof(buffer));
							parser.reset();
							last_response_time = now;
							if(size>=1)
							{
								byte_t command = buffer[0];
								if(just_opened)
								{
									if((command!=ACC_VER) && (command!=ACC_ISTAT))
										continue;
									else
										just_opened = false;
									if(!just_opened)
									{
										send_relay();
										send_btn_led();
										send_btn2_led();
									}
								}
								switch (command) {
								case ACC_VER:
								{
									buffer[size] = 0;
									PJ_LOG_( INFO_LEVEL,(__FILE__,"arduino version: %s",&buffer[1]));
									break;
								}

								case ACC_ISTAT:
								{
									if(size>=2)
									{
										m_input = buffer[1];
										check_input();
									}
									break;
								}

								}
							}
						}
					}
					else
						m_event.wait(1);
				}
				else
					parser.reset();
			}
			else
				m_event.wait(5);
		}
		else
			m_event.wait(5000);
	}
	PJ_LOG_( INFO_LEVEL,(__FILE__,"arduino controller thread end"));
	return NULL;
}

pjs_arduino_controller::~pjs_arduino_controller()
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

void pjs_arduino_controller::send_relay()
{
	pj_uint8_t buffer[MAX_PACKET_SIZE+4];
	send_buffer(buffer,parser.create_packet(buffer,sizeof(buffer),ACC_SET_RELAY,&m_relay,sizeof(m_relay)));
}

void pjs_arduino_controller::update_inputs()
{
	pj_uint8_t buffer[MAX_PACKET_SIZE+4];
	send_buffer(buffer,parser.create_packet(buffer,sizeof(buffer),ACC_GET_ISTAT,NULL,0));
}

void pjs_arduino_controller::send_btn_led()
{
	pj_uint8_t buffer[MAX_PACKET_SIZE+4];
	send_buffer(buffer,parser.create_packet(buffer,sizeof(buffer),ACC_SET_BTN_LED,&m_btn_led,sizeof(m_btn_led)));
}

void pjs_arduino_controller::send_btn2_led()
{
	pj_uint8_t buffer[MAX_PACKET_SIZE+4];
	send_buffer(buffer,parser.create_packet(buffer,sizeof(buffer),ACC_SET_BTN2_LED,&m_btn2_led,sizeof(m_btn2_led)));
}


void pjs_arduino_controller::send_buffer(void *data, pj_uint16_t size)
{
	PPJ_WaitAndLock lock(*m_lock);
	if(serial && !just_opened)
		serial->Write(data,size);
}
pj_int32_t pjs_arduino_controller::set_output(pj_uint8_t  output, pj_uint8_t value)
{
	pj_int32_t result = RC_OK;
	switch(output)
	{
	case ECO_BELL:
	{
		m_relay = value;
		send_relay();
		break;
	}

	case ECO_MAIN_LED:
	{
		m_btn_led = value;
		send_btn_led();
		break;
	}

	case ECO_SECOND_LED:
	{
		m_btn2_led = value;
		send_btn2_led();
		break;
	}

	default:
		result = RC_ERROR;
		break;
	}
	return result;
}
