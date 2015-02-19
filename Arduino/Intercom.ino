#include <Arduino.h>
#include <ChibiOS.h>
#include <util/atomic.h>
#include "comm_prot_parser.h"
#include "arduino_comm.h"
#include "led_controller.h"
#include "dynamic_delay.h"
#ifdef	HAS_FAN
#include "fan.h"
#ifdef	HAS_TEMP_SENSOR
#include "DallasTemperature.h"
#include "OneWire_ChibiOS.h"
#endif
#endif

#undef  DEBUG_TEMP

#define HOST_TIMEOUT  15000
#define BTN_GUARD_TIME    100
#define INP_GUARD_TIME    100
#define MAX_PACKET_SIZE   32
#define PIN_LED      13
#define PIN_BTN_LED  6
#define PIN_RELAY    4
#define PIN_BTN      9
#define PIN_INP1     7
#define PIN_INP2     8
#define PIN_BTN2_LED   10
#ifdef HAS_FAN
#ifdef HAS_TEMP_SENSOR
#define PIN_TEMP     5
#endif
#define PIN_FAN      3
#define PIN_FAN_SPEED  2
#define INT_FAN_SPEED  0
#endif

#ifdef HAS_ULTRASONIC
#define PIN_ULTRASONIC A0
volatile uint16_t ultrasonicValue = 0;
uint16_t lastUltrasonicValue = 0;
#endif
volatile byte_t isStandaloneMode = 0;
volatile byte_t inputsValue = 0;
volatile byte_t outputsValue = 0;
volatile byte_t led1Value = 0;
volatile byte_t led2Value = 0;

byte_t lastInputsValue = 0;
byte_t lastOutputsValue = 0;
byte_t lastLed1Value = 0;
byte_t lastLed2Value = 0;

static void resetIOValues()
{
  inputsValue = outputsValue = led2Value = led1Value = 0;
  lastInputsValue = lastOutputsValue = lastLed1Value = lastLed2Value = 0xFF;
}

class input_c
{
protected:
  byte_t  m_lastValue;
  byte_t  m_pin;
  byte_t  m_inverse;
  uint16_t m_guardTime;
  uint32_t m_lastChangeTime;
public:
  input_c(byte_t pin,uint16_t guardTime,byte_t  inverse,byte_t  pullup):
  m_pin(pin),m_inverse(inverse),m_guardTime(guardTime)
  {
    pinMode(m_pin,INPUT);
    if(pullup)
      digitalWrite(m_pin, HIGH);
    reset();
  }

  void reset()
  {
    m_lastChangeTime = 0;
    m_lastValue = 0;
  }

  byte_t getRawValue()
  {
    return m_inverse?(digitalRead(m_pin)==HIGH)?0:1:((digitalRead(m_pin)==HIGH)?1:0);
  }

  byte_t getValue(uint32_t now)
  {
    if((m_lastChangeTime>0)&&(now>=m_lastChangeTime))
    {
      if((now-m_lastChangeTime)<m_guardTime)
        return m_lastValue;
      m_lastChangeTime = 0;
    }
    byte_t value = getRawValue();
    if(value!=m_lastValue)
    {
      m_lastChangeTime = now;
      m_lastValue = value;
    }
    return m_lastValue;
  }
};
#ifdef HAS_ULTRASONIC
#define ULTRASONIC_AVR 10
static dynamicDelay_c ultrasonicSensorDelay;
static WORKING_AREA(waUltrasonicSensorThread, 128);
static msg_t UltrasonicSensorThread(void *arg) {
	ultrasonicSensorDelay.start();
  while(true)
  {
  	uint32_t cnt = 0;
  	uint32_t sum = 0;
  	uint32_t max = 110;
  	uint32_t min = 0;
  	static uint32_t values[ULTRASONIC_AVR];

  	for(int i = 0; i < ULTRASONIC_AVR ; i++)
 	  {
  		chThdSleepMilliseconds(10);
  		values[i] = analogRead(PIN_ULTRASONIC);
  		if(values[i]>max)
  		{
  			max = values[i];
  		}
  	}

  	min = max-max/5;

  	for(int i = 0; i < ULTRASONIC_AVR ; i++)
 	  {
  		if(values[i]>min)
  		{
  			sum += values[i];
  			cnt++;
  		}
  	}

  	if(cnt>=4)
  	{
			uint16_t us = sum/cnt;
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
			{
				ultrasonicValue = us;
			}
  	}

    chThdSleepMilliseconds(10);
    ultrasonicSensorDelay.delay(200);
  }
  return 0;
}

#endif

#ifdef HAS_FAN
//========================== FAN SPEED DATA   ==========================
#define FAN_MIN_SPEED  20
// Speeds MUST be sorted from 0 -> 100
fan_speed_item_t  fan_speeds[] = { {0,0}, {19,0}, {20,70}, {100,0xFF} };
//fan_speed_item_t  fan_speeds[] = { {0,0}, {100,0xFF} };
fan_speed_t fan_speeds_data = { (sizeof fan_speeds)/sizeof(fan_speed_item_t), FAN_MIN_SPEED, fan_speeds};
fan_temp_data_t fan_temp_data = {530, 100, 600};

fan_c  fan(PIN_FAN,fan_speeds_data,fan_temp_data);

uint32_t fan_pulse = 0;
systime_t fan_pulse_start = 0;
uint16_t  fan_rpm = 0;
uint16_t  fan_counter = 0;
uint8_t fan_speed_bit;
uint8_t fan_speed_port;
uint8_t fan_stopped = 0;


//=======================================================================
void fanSpeedInt()
{
  fan_stopped = 0;
  int8_t v = (*portInputRegister(fan_speed_port) & fan_speed_bit);
  if(fan_pulse_start==0)
  {
    if(v)
    {
      fan_pulse_start = chTimeNow();
      fan_counter = 0;
    }
  }
  else
  {
    if(!v)
    {
      if(fan_pulse_start<chTimeNow())
      {
        fan_counter++;
        if(fan_counter==8)
        {
          fan_pulse = (uint32_t)(chTimeNow() - fan_pulse_start)*(uint32_t)1000/(fan_counter*2-1);
          if(fan_pulse)
            fan_rpm = (uint32_t)60/4*(uint32_t)S2ST(1)*1000/(uint32_t)fan_pulse;
          else
            fan_rpm = 0;
          fan_pulse_start = 0; 
        }
      }
      else
      {
        fan_pulse_start = 0; 
      }
    }
  }
}

void checkFanSpeedInt()
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    if(fan_stopped)
    {
        fan_pulse_start = 0;
        fan_rpm = 0;
        fan_pulse = 0;
        fan_stopped = 0;
    }
    else
      fan_stopped = 1;
  }      
}

uint16_t getFanRPM()
{
  uint16_t result;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    result = fan_rpm;
  }      
  return result;
}

#ifdef	HAS_TEMP_SENSOR
//========================== TEMPERATURE SENSOR   =======================
static OneWire  tempSensorData(PIN_TEMP);
static DallasTemperature tempSensor(&tempSensorData);
static dynamicDelay_c tempSensorDelay;

static WORKING_AREA(waTempSensorThread, 64);
static msg_t TempSensorThread(void *arg) {
  tempSensor.setWaitForConversion(false);
  tempSensorDelay.start();
  while(true)
  {
    if(tempSensor.getDeviceCount()==0)
      tempSensor.begin();
    if(tempSensor.getDeviceCount()>0)
    {
      tempSensor.requestTemperatures();
      chThdSleepMilliseconds(850);
      float fTemp = tempSensor.getTempCByIndex(0);
#ifdef  DEBUG_TEMP      
      Serial.print(" ");
      Serial.print(fTemp);
      Serial.print(" ");  
      Serial.print(getFanRPM());
      Serial.print(" ");  
#endif      
      if((fTemp!=DEVICE_DISCONNECTED)&&(fTemp<=80)&&(fTemp>=15))
      {
        int16_t tempC = ceil(fTemp*10-0.5);
        if(tempC<0)
          tempC=0;
        if(tempC>250)
        {
        	tempC += (tempC-250)/2;
        }
        tempC += 100;
        fan.setTemp(tempC,1);
      }
    }
    chThdSleepMilliseconds(10);
    tempSensorDelay.delay(1000);
    checkFanSpeedInt();
  }
  return 0;
}
//=======================================================================
#endif
#endif

#define checkInput(VALUES,VALUE,MASK) {if(VALUE)VALUES|=MASK;}
//========================== BTN LED SCRIPTS ============================
#define BTN_LED_MAX  255
#define BTN_LED_MIN  0
//static byte_t  btnLedScript_ID01[] = { LCSC_OFF,  LCSC_WAIT, LCS_TIME_MS(50), LCSC_ON, LCSC_WAIT, LCS_TIME_MS(50)};
static byte_t  btnLedScript_ID01[] = { LCSC_OFF,  LCSC_WAIT, LCS_TIME_MS(100), LCSC_FADE, BTN_LED_MAX, LCS_TIME_MS(500), LCSC_WAIT, LCS_TIME_MS(100), LCSC_FADE, BTN_LED_MIN, LCS_TIME_MS(500), LCSC_GOTO, 3 };
static byte_t  btnLedScript_ID02[] = { LCSC_FADE, BTN_LED_MAX/2, LCS_TIME_MS(1500), LCSC_FADE, BTN_LED_MAX, LCS_TIME_MS(1500)};

#define LED_SCRIPT(N,ID) { sizeof N, ID, N }
static ledScript_t  btnLedScriptsArray[] = {LED_SCRIPT(btnLedScript_ID01,1),LED_SCRIPT(btnLedScript_ID02,2)};
static ledScripts_t  btnLedScripts = { (sizeof btnLedScriptsArray)/sizeof(ledScript_t) , btnLedScriptsArray };
//========================== BTN LED SCRIPTS ============================
static ledController_c btnLed(PIN_BTN_LED,&btnLedScripts);
static WORKING_AREA(waBtnLedThread, 64);
static msg_t BtnLedThread(void *arg) {
  while(1)
    btnLed.run();
  return 0;
}

static ledController_c btn2Led(PIN_BTN2_LED,&btnLedScripts);
static WORKING_AREA(waBtn2LedThread, 64);
static msg_t Btn2LedThread(void *arg) {
  while(1)
    btn2Led.run();
  return 0;
}

#ifdef MAIN_BTN_PULLUP
static input_c  btnInput(PIN_BTN,BTN_GUARD_TIME,1,1);
#else
static input_c  btnInput(PIN_BTN,BTN_GUARD_TIME,0,0);
#endif
static input_c  input1(PIN_INP1,INP_GUARD_TIME,1,1);
static input_c  input2(PIN_INP2,INP_GUARD_TIME,0,1);
static dynamicDelayWithSignal_c controllerDelay;


static WORKING_AREA(waControllerThread, 64);
static msg_t ControllerThread(void *arg) {
  byte_t lastMode = 100;

  pinMode(PIN_LED, OUTPUT);

  digitalWrite(PIN_RELAY, HIGH);
  pinMode(PIN_RELAY,OUTPUT);

  btnLed.setON();
  btn2Led.setOFF();

  controllerDelay.start();
  while (TRUE) {
    byte_t currentMode = isStandaloneMode;
    if(currentMode!=lastMode)
    {
      btnInput.reset();
      lastMode = currentMode;
      if(!currentMode)
        digitalWrite(PIN_LED, HIGH);
      else
      {
        digitalWrite(PIN_LED, LOW);
#ifdef HAS_FAN
        fan.setMode(AFM_AUTO);
#endif
      }
    }

    if(currentMode)
    {
    	uint32_t now = millis();
      byte_t buttonState = btnInput.getValue(now);
      if(buttonState)
      {
        digitalWrite(PIN_RELAY, LOW);
        btnLed.setOFF();
      }
      else
      {
        digitalWrite(PIN_RELAY, HIGH);
        btnLed.setON();
      }
      btn2Led.setOFF();
    }
    else
    {
      byte_t currentInputsValue = 0;
      byte_t currentOutputsValue;
      byte_t currentLed1Value;
      byte_t currentLed2Value;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        currentOutputsValue = outputsValue;
        currentLed1Value = led1Value;
        currentLed2Value = led2Value;
      }      
      if(currentOutputsValue != lastOutputsValue)
      {
        if(currentOutputsValue & AOM_RELAY)
          digitalWrite(PIN_RELAY, LOW);
        else
          digitalWrite(PIN_RELAY, HIGH);
        lastOutputsValue = currentOutputsValue;
      }
      
      if(currentLed1Value != lastLed1Value )
      {
        if(currentLed1Value>1)
        {
          btnLed.runScript(currentLed1Value >> 1);
        }
        else
        {
          if(currentLed1Value)
            btnLed.setON();
          else
            btnLed.setOFF();
        }
        lastLed1Value = currentLed1Value;
      }
      
      if(currentLed2Value != lastLed2Value )
      {
        if(currentLed2Value>1)
        {
          btn2Led.runScript(currentLed2Value >> 1);
        }
        else
        {
          if(currentLed2Value)
            btn2Led.setON();
          else
            btn2Led.setOFF();
        }
        lastLed2Value = currentLed2Value;
      }

      uint32_t now = millis();      
      checkInput(currentInputsValue,btnInput.getValue(now),AIM_BTN);
      checkInput(currentInputsValue,input1.getValue(now),AIM_INP1);
      checkInput(currentInputsValue,input2.getValue(now),AIM_INP2);
      if(inputsValue != currentInputsValue)
      {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
          inputsValue = currentInputsValue;
        }
      }
    }
    controllerDelay.delay(10);
  }
  return 0;
}

static WORKING_AREA(waSerialThread, 128);
static comm_protocol_parser_c protParser(cp_prefix_t(APP_1, APP_2),MAX_PACKET_SIZE);

int bytesAvailable;
byte_t command;
byte_t packetSize;
byte_t incomingByte = 0; 
byte_t *packetBuffer;
byte_t newPacketBuffer[MAX_PACKET_SIZE+4];
uint32_t lastHostActivityTime;
#ifdef HAS_FAN
fan_data_t fan_data;
#endif

#define sendPacket(P,V) Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),P,&V,sizeof(V)))
#define sendOutput(V) sendPacket(ACC_OSTAT,V)

static msg_t SerialThread(void *arg) {
  lastHostActivityTime = millis();
  Serial.begin(9600);
  packetBuffer = protParser.get_packet_buffer();
  while (TRUE) {
    if(!isStandaloneMode)
    {
      byte_t currentInputsValue;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        currentInputsValue = inputsValue;
      }
  
      if(lastInputsValue!=currentInputsValue)
      {
        lastInputsValue = currentInputsValue;
        Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ISTAT,&lastInputsValue,sizeof(lastInputsValue)));
      }
#ifdef HAS_ULTRASONIC
      int16_t currentUValue;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
      	currentUValue =  ultrasonicValue;
      }

      if(abs((int32_t)lastUltrasonicValue-(int32_t)currentUValue)>=(lastUltrasonicValue/10))
      {
      	lastUltrasonicValue = currentUValue;
        Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ULTRASONIC_DATA,&lastUltrasonicValue,sizeof(lastUltrasonicValue)));
      }
#endif
    }

    bytesAvailable = Serial.available();
    if(bytesAvailable>0)
    {
      while(bytesAvailable>0)
      {
        incomingByte = Serial.read();
        if(protParser.post_data(&incomingByte,sizeof(byte_t))== CP_RESULT_PACKET_READY)
        {
          if(isStandaloneMode)
          {
            isStandaloneMode = 0;
            controllerDelay.signal();
          }
          lastHostActivityTime = millis();          
          packetSize=protParser.get_packet_size();
          if(packetSize>=1)
          {
            command = packetBuffer[0];
            switch(command)
            {
            case ACC_VER:
              {
                Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),command,AC_PROTOCOL_VERSION,sizeof(AC_PROTOCOL_VERSION)-1));
                break;
              }
            case ACC_GET_OSTAT:
              {
                byte_t tmp = outputsValue;
                sendOutput(tmp);
                break;
              }              
              
            case ACC_GET_ISTAT:
              {
                byte_t tmp = inputsValue;
                sendPacket(ACC_ISTAT,tmp);
                break;
              }              

            case ACC_PING:
              {
                Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_PING,NULL,0));
                break;
              }              
              
            case ACC_SET_RELAY:
              {
                if(packetSize>=2)
                {
                  if(packetBuffer[1])
                  {
                    ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
                      outputsValue |= AOM_RELAY;
                    }
                  }
                  else
                  {
                    ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
                      outputsValue &= ~AOM_RELAY;
                    }
                  }
                }
                byte_t tmp = outputsValue;
                sendOutput(tmp);
                controllerDelay.signal();
                break;
              }
#ifdef	HAS_FAN
            case ACC_SET_FAN_SPEED:
              {
                if(packetSize>=2)
                {
                  fan.setSpeed(packetBuffer[1]);
                  fan.getData(fan_data);
                  sendPacket(ACC_FAN_DATA,fan_data);
                }
                break;
              }
              
            case ACC_SET_FAN_MODE:
              {
                if(packetSize>=2)
                {
                  fan.setMode(packetBuffer[1]);
                  fan.getData(fan_data);
                  sendPacket(ACC_FAN_DATA,fan_data);
                }
                break;
              }
              
            case ACC_SET_FAN_TEMP:
              {
                if(packetSize>=2)
                {
                  fan.setTemp(packetBuffer[1]*10,2);
                  fan.getData(fan_data);
                  sendPacket(ACC_FAN_DATA,fan_data);
                }
                break;
              }     
     
            case ACC_FAN_DATA:
              {
                fan.getData(fan_data);
                sendPacket(ACC_FAN_DATA,fan_data);
                break;
              }                
#endif
            case ACC_SET_BTN_LED:
              {
                if(packetSize>=2)
                {
                  ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
                    led1Value =  packetBuffer[1];
                  }
                }
                sendPacket(ACC_SET_BTN_LED,packetBuffer[1]);
                controllerDelay.signal();
                break;
              }

            case ACC_SET_BTN2_LED:
              {
                if(packetSize>=2)
                {
                  ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
                    led2Value =  packetBuffer[1];
                  }
                }
                sendPacket(ACC_SET_BTN2_LED,packetBuffer[1]);
                controllerDelay.signal();
                break;
              }

            default:
              {
                Serial.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ERROR,NULL,0));
                break;
              }
            };

          }
          protParser.reset();
        }
        bytesAvailable--;
      }
    }
    else
    {
      if( (millis()-lastHostActivityTime)>HOST_TIMEOUT)
      {
        resetIOValues();
        isStandaloneMode = 1;
        lastHostActivityTime = millis();
      }
    }
    chThdSleepMilliseconds(1);
  }
  return 0;
}


//------------------------------------------------------------------------------
void setup() {
  // initialize ChibiOS with interrupts disabled
  // ChibiOS will enable interrupts
  Serial.begin(9600);

#ifdef HAS_FAN
  fan_speed_bit = digitalPinToBitMask(PIN_FAN_SPEED);
  fan_speed_port = digitalPinToPort(PIN_FAN_SPEED);

  attachInterrupt(INT_FAN_SPEED,fanSpeedInt,CHANGE);
#endif
  
  TCCR2B = 0x1;
  isStandaloneMode = 1;
  cli();
  halInit();
  chSysInit();

  tprio_t currentPriority = chThdGetPriority();
  chThdSetPriority(NORMALPRIO+5);

  resetIOValues();

#ifdef	HAS_FAN
#ifdef	HAS_TEMP_SENSOR
  chThdCreateStatic(waTempSensorThread, sizeof(waTempSensorThread),
  NORMALPRIO + 4, TempSensorThread, NULL);
#endif
#endif

#ifdef HAS_ULTRASONIC
  chThdCreateStatic(waUltrasonicSensorThread, sizeof(waUltrasonicSensorThread),
  NORMALPRIO + 4, UltrasonicSensorThread, NULL);
#endif
  chThdCreateStatic(waBtnLedThread, sizeof(waBtnLedThread),
  NORMALPRIO + 3, BtnLedThread, NULL);

  chThdCreateStatic(waBtn2LedThread, sizeof(waBtn2LedThread),
  NORMALPRIO + 3, Btn2LedThread, NULL);
  
  chThdCreateStatic(waControllerThread, sizeof(waControllerThread),
  NORMALPRIO + 2, ControllerThread, NULL);

  chThdCreateStatic(waSerialThread, sizeof(waSerialThread),
  NORMALPRIO + 1, SerialThread, NULL);



  chThdSleepMilliseconds(100);
  
  chThdSetPriority(currentPriority);
}


//------------------------------------------------------------------------------
// idle loop runs at NORMALPRIO
void loop() {
  chThdSleepMilliseconds(1);
}

