#ifndef DYNAMIC_DELAY_H_
#define DYNAMIC_DELAY_H_
#include <Arduino.h>

class dynamicDelayWithSignal_c
{
protected:
  CondVar  m_cp;
  Mutex    m_mp;
  uint32_t m_lastTime;
public:
  dynamicDelayWithSignal_c()
  {
    m_lastTime = 0;
    chCondInit(&m_cp);
    chMtxInit(&m_mp);
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
      chMtxLock(&m_mp);
      if(chCondWaitTimeout(&m_cp,MS2ST(ms))!=RDY_TIMEOUT)
        chMtxUnlock();
    }
    else
      chThdYield();
    m_lastTime = millis();
  }
  
  void signal()
  {
    chCondSignal(&m_cp);
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
      chThdSleepMilliseconds(ms);
    }
    else
      chThdYield();
    m_lastTime = millis();
  }
};


#endif /* DYNAMIC_DELAY_H_ */
