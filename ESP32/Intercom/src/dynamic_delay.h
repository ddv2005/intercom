#ifndef DYNAMIC_DELAY_H_
#define DYNAMIC_DELAY_H_
#include <Arduino.h>
#include "unilib/uni_mutex.h"
#include "unilib/uni_event.h"

class dynamicDelayWithSignal_c
{
protected:
  uni_shared_event m_event;
  uint32_t m_lastTime;
public:
  dynamicDelayWithSignal_c()
  {
    m_lastTime = 0;
  }
  
  void start()
  {
    m_lastTime = millis();
  }
  
  void delay(uint16_t ms)
  {
    uint32_t now = millis();
    if((m_lastTime!=0)&&(m_lastTime<=now))
    {
      uint32_t diff = now-m_lastTime;
      if(diff>=ms)
        ms = 0;
      else
        ms -= diff;
    }
    if(ms>0)
    {
      m_event.wait(ms);
    }
    else
      taskYIELD();
    m_lastTime = millis();
  }
  
  void signal()
  {
    m_event.signal();
  }
   
};

class dynamicDelay_c
{
protected:
  uint32_t m_lastTime;
public:
  dynamicDelay_c()
  {
    m_lastTime = 0;
  }
  
  void start()
  {
    m_lastTime = millis();
  }
  
  void delay(uint16_t ms)
  {
    uint32_t now = millis();
    if((m_lastTime!=0)&&(m_lastTime<=now))
    {
      uint32_t diff = now-m_lastTime;
      if(diff>=ms)
        ms = 0;
      else
        ms -= diff;
    }
    if(ms>0)
    {
      usleep(ms * 1000);
    }
    else
      taskYIELD();
    m_lastTime = millis();
  }
};


#endif /* DYNAMIC_DELAY_H_ */
