#ifndef FAN_H_
#define FAN_H_
#include <Arduino.h>
#include "fan_common.h"

#define __packed  __attribute__((packed))

typedef struct fan_speed_item_t
{
  uint8_t  speed;
  uint8_t  value;
} __packed fan_speed_item_t;

// Speeds MUST be sorted from 0 -> 100
typedef struct fan_speed_t
{
  uint8_t  count;
  uint8_t  min_speed;
  fan_speed_item_t  *data;
} __packed fan_speed_t;

typedef struct fan_data_t
{
  uint8_t  mode;
  uint8_t  speed;
  uint8_t  value;
} __packed fan_data_t;

typedef struct fan_temp_data_t
{
  uint16_t  target;
  uint8_t   threshold;
  uint16_t  full_speed_temp;
} __packed fan_temp_data_t;


class fan_c
{
protected:
  uint8_t  m_pin;
  fan_speed_t &m_speed_data;
  fan_temp_data_t &m_temp_data;
  uint8_t  m_mode;
  uint16_t  m_temp;
  uint8_t  m_speed;
  uint8_t  m_value;
  Mutex    m_lock;
  int32_t  m_start_time;
  
  uint16_t getLinearValue(uint8_t hspeed, uint8_t lspeed, uint8_t speed, uint8_t hvalue,uint8_t lvalue)
  {
    return (uint16_t)lvalue + (uint16_t)(speed-lspeed)*(uint16_t)(hvalue-lvalue)/(uint16_t)(hspeed-lspeed);
  }
  
  void setRawSpeed(uint8_t  speed)
  {
    uint16_t value = (uint16_t)speed*(uint16_t)255/(uint16_t)100;
    if(value>255)
      value = 255;
    m_speed = speed;
    for(uint8_t i = 0; i<m_speed_data.count; i++)
    {
      if(m_speed_data.data[i].speed==speed)
      {
        value = m_speed_data.data[i].value;
        break;
      }
      else
        if(m_speed_data.data[i].speed>speed)
        {
          if(i>0)
            value = getLinearValue(m_speed_data.data[i].speed,m_speed_data.data[i-1].speed,speed,m_speed_data.data[i].value,m_speed_data.data[i-1].value);
          else
            value = getLinearValue(m_speed_data.data[i].speed,0,speed,m_speed_data.data[i].value,0);
           break;
        }
        else
        if(i==(m_speed_data.count-1))
        {
          value = getLinearValue(100,m_speed_data.data[i].speed,speed,255,m_speed_data.data[i].value);
        }
    }
    m_value = value;
    analogWrite(m_pin,m_value);
  }
  
  void updateTemp()
  {
    if(m_mode == AFM_AUTO)
    {
      uint16_t target = m_temp_data.target;
      if(m_speed>=m_speed_data.min_speed)
      {
        target = m_temp_data.target-m_temp_data.threshold;
      }
      if(m_temp>target)
      {
        int32_t now = millis();
        if(!m_start_time || (m_start_time>now))
          m_start_time = now;
        if((now-m_start_time)<2000)
          setRawSpeed(100);
        else
        {
          if(m_temp>=m_temp_data.full_speed_temp)
          {
            setRawSpeed(100);
          }
          else
          {
            int32_t speed = m_speed_data.min_speed+(m_temp-target)*(100-m_speed_data.min_speed)/(m_temp_data.full_speed_temp-target);
            if(speed<0)
              speed = 0;
            else
            if(speed>100)
              speed = 100;
            setRawSpeed(speed);
          }
        }
      }
      else
      {
        setRawSpeed(0);
        m_start_time = 0 ;
      }
    }
  }
  
public:
  fan_c(uint8_t  pin, fan_speed_t &speed_data, fan_temp_data_t &temp_data):m_pin(pin),m_speed_data(speed_data),m_temp_data(temp_data)
  {
    chMtxInit(&m_lock);
    m_mode = AFM_AUTO;
    m_temp = 0;
    m_start_time = 0;
    setRawSpeed(50);
  }
  
  void setSpeed(uint8_t speed)
  {
    chMtxLock(&m_lock);
    m_mode=AFM_MANUAL;
    setRawSpeed(speed);
    chMtxUnlock();
  }

  void setMode(uint8_t mode)
  {
    chMtxLock(&m_lock);
    m_mode = mode;
    m_start_time = 0;
    m_speed = 0;
    updateTemp();
    chMtxUnlock();
  }
  
  void setTemp(uint16_t temp, uint8_t factor = 1, uint8_t ofactor = 1)
  {
    chMtxLock(&m_lock);
    if(m_temp)
    {
      m_temp = (m_temp*ofactor + temp*factor)/(factor+ofactor);
    }
    else
      m_temp = temp;
    updateTemp();
    chMtxUnlock();
  }  

  void getData(fan_data_t &data)
  {
    chMtxLock(&m_lock);
    data.mode = m_mode;
    data.speed = m_speed;
    data.value = m_value;
    chMtxUnlock();    
  }
};

#endif /* FAN_H_ */
