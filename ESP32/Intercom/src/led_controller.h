#ifndef LED_CONTROLLER_H_
#define LED_CONTROLLER_H_
#include <Arduino.h>
#include <stdint.h>
#include "unilib/uni_mutex.h"
#include "config.h"

#ifndef __packed
#define __packed  __attribute__((packed))
#endif

#ifndef byte_t
#define byte_t  uint8_t
#endif
typedef struct ledScript_t
{
  byte_t  size;
  byte_t  id;
  byte_t* data;
} __packed ledScript_t;

typedef struct ledScripts_t
{
  byte_t count;
  ledScript_t *scripts;
} __packed ledScripts_t;

typedef enum ledModes_t
{
  LM_OFF,
  LM_ON,
  LM_SCRIPT
} ledModes_t;

// Script Commands
#define LCSC_OFF    0x00
#define LCSC_ON     0x01
// Wait, 2 bytes ms
#define LCSC_WAIT   0x02
// Set Value, 1 byte - value
#define LCSC_SET    0x03
// 1 byte - value, 2 bytes - time in ms
#define LCSC_FADE   0x04
//  byte addr
#define LCSC_GOTO   0x05

#define LCS_TIME_MS(A) (byte_t)A & 0xFF, (byte_t)(((uint16_t)A)>>8)
#define LCS_GETTIME_MS(A, B) (((uint16_t)A & 0xFF) | (((uint16_t)B)<<8))

#define LCV_OFF 0
#define LCV_ON  255

class ledController_c
{
protected:
  byte_t      m_pin;
  byte_t      m_pwmChannel;

  ledModes_t  m_mode;
  ledScripts_t *m_scripts;
  ledScript_t  *m_currentScript;
  byte_t        m_currentScriptId;
  byte_t        m_currentScriptPos;
  uint32_t      m_scriptTimer;
  byte_t      m_startValue;
  byte_t      m_lastValue;
  uni_simple_mutex m_mutex;
  
  void  setValue(byte_t value)
  {
    m_lastValue = value;
    ledcWrite(m_pwmChannel,value); 
  }

  byte_t getCmdSize(byte_t cmd)
  {
    switch(cmd)
    {
      case LCSC_OFF:
      case LCSC_ON:
        return 1;

      case LCSC_SET:
      case LCSC_GOTO:
        return 2;

      case LCSC_WAIT:
        return 3;

      case LCSC_FADE:
        return 4;
    };
    return 0;
  } 

  byte_t approximateValue(byte_t value,byte_t start,uint32_t now,uint32_t time)
  {
  
    int16_t nv = start+((((int32_t)value-(int32_t)start)*(int32_t)(now-m_scriptTimer))/(int32_t)time);
    if(nv<0)
      nv = 0;
    else
      if(nv>255)
        nv = 255;
    return nv;
  }  

public:
  ledController_c(byte_t pin, byte_t pwmChannel, ledScripts_t *scripts):m_scripts(scripts)
  {
    m_pin = pin;
    m_pwmChannel = pwmChannel;
    m_currentScript = NULL;
    m_currentScriptId = 0;
    m_mode = LM_OFF;
  }

  void begin()
  {
    uni_locker lock(m_mutex);
    ledcAttachPin(m_pin, m_pwmChannel);
    ledcSetup(m_pwmChannel,4000,8);
    setValue(LCV_OFF);
  }
  
  void setON()
  {
    uni_locker lock(m_mutex);
    if(m_mode != LM_ON)
    {
      m_mode = LM_ON;
      setValue(LCV_ON);
    }
  };
  
  void setOFF()
  {
    uni_locker lock(m_mutex);
    if(m_mode != LM_OFF)
    {
      m_mode = LM_OFF;
      setValue(LCV_OFF);
    }
  };
  
  void runScript(byte_t  id)
  {
    uni_locker lock(m_mutex);
    if(m_mode==LM_SCRIPT && m_currentScriptId==id) return;
    m_mode = LM_SCRIPT;
    m_currentScriptId = id;
    m_currentScript = NULL;
    m_currentScriptPos = 0;
    m_scriptTimer = 0;
    if(m_scripts)
    {
      for(byte_t i=0; i<m_scripts->count; i++)
      {
        if(m_scripts->scripts[i].id == id)
        {
          m_currentScript = &m_scripts->scripts[i];
        }
      }
    }
  }
 
  void loop()
  {
        uni_locker lock(m_mutex);
        if(m_mode==LM_SCRIPT)
        {
          if(m_currentScript)
          {
            while(true)
            {
              if(m_currentScriptPos<m_currentScript->size)
              {
                byte_t cmd = m_currentScript->data[m_currentScriptPos];
                byte_t cmd_size = getCmdSize(cmd);
                if((cmd_size>0)&&((m_currentScriptPos+cmd_size)<=m_currentScript->size))
                {
                  byte_t cont = 3;
                  uint16_t time;
                  uint32_t now;
                  switch(cmd)
                  {
                    case LCSC_OFF:
                      setValue(LCV_OFF);
                      break;
                      
                    case LCSC_ON:
                      setValue(LCV_ON);
                      break;

                    case LCSC_SET:
                      setValue(m_currentScript->data[m_currentScriptPos+1]);
                      break;
                    
                    case LCSC_GOTO:
                      m_currentScriptPos = m_currentScript->data[m_currentScriptPos+1];
                      cont = 0;
                      break;
              
                    case LCSC_WAIT:
                      {
                        time = LCS_GETTIME_MS(m_currentScript->data[m_currentScriptPos+1], m_currentScript->data[m_currentScriptPos+2]);
                        if(m_scriptTimer==0)
                        {
                          m_scriptTimer = millis();
                          cont = 2;
                        }
                        else
                        {
                          now = millis();
                          if((m_scriptTimer>now) || ((now-m_scriptTimer)>=time))
                          {
                            m_scriptTimer = 0;
                            cont = 1;
                          }
                          else
                            cont = 2;
                        }
                        break;
                      }
              
                    case LCSC_FADE:
                      {
                        time = LCS_GETTIME_MS(m_currentScript->data[m_currentScriptPos+2], m_currentScript->data[m_currentScriptPos+3]);
                        if(m_scriptTimer==0)
                        {
                          m_scriptTimer = millis();
                          m_startValue = m_lastValue;
                          cont = 2;
                        }
                        else
                        {
                          byte_t value = m_currentScript->data[m_currentScriptPos+1];
                          now = millis();
                          if((m_scriptTimer>now) || ((now-m_scriptTimer)>=time))
                          {
                            setValue(value);
                            m_scriptTimer = 0;
                            cont = 1;
                          }
                          else
                          {
                            int16_t nv = m_startValue+((((int32_t)value-(int32_t)m_startValue)*(int32_t)(now-m_scriptTimer))/(int32_t)time);
                            if(nv<0)
                              nv = 0;
                            else
                            if(nv>255)
                              nv = 255;
                            setValue(nv);
                            cont = 2;
                          }
                        }
                        break;
                      }                    
                      break;
                  };
                  if(cont&1)
                  {
                    m_currentScriptPos += cmd_size;
                    if(m_currentScriptPos>=m_currentScript->size)
                      m_currentScriptPos = 0;
                  }
                  else
                  {
                    if(cont&2)
                    {
                      break;
                    }
                  }
                }
                else
                {
                  m_currentScriptPos = 0;
                  m_mode = LM_OFF;
                  setValue(LCV_OFF);
                  break;
                }
              }
              else
              {
                m_currentScriptPos = 0;
                break;
              }
            }
          }
        }
  }
};

#endif /* LED_CONTROLLER_H_ */
