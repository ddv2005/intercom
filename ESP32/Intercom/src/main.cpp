#include <Arduino.h>
#include "led_controller.h"
#include "dynamic_delay.h"
#include "config.h"
#include "comm_prot_parser.h"
#include "arduino_comm.h"
#include "unilib/uni_mutex.h"
#include "esp_task_wdt.h"
#include "esp_bt.h"

SemaphoreHandle_t debugLock = xSemaphoreCreateMutex();

#ifdef HAS_ULTRASONIC
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
    if(pullup)
    {
      pinMode(m_pin,INPUT_PULLUP);
    }
    else
      pinMode(m_pin,INPUT_PULLDOWN);
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

static uni_simple_mutex globalMutex;
#ifdef HAS_ULTRASONIC
#define ULTRASONIC_AVR 10
static dynamicDelay_c ultrasonicSensorDelay;
static TaskHandle_t waUltrasonicSensorThread;
static void UltrasonicSensorThread(void *arg) {
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
  		delay(10);
  		values[i] = analogReadMilliVolts(PIN_ULTRASONIC);
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
			{
        uni_locker lock(globalMutex);
				ultrasonicValue = us;
			}
  	}

    delay(10);
    ultrasonicSensorDelay.delay(200);
  }
}
#endif

#define checkInput(VALUES,VALUE,MASK) {if(VALUE)VALUES|=MASK;}
//========================== BTN LED SCRIPTS ============================
#define BTN_LED_MAX  255
#define BTN_LED_MIN  0
//static byte_t  btnLedScript_ID01[] = { LCSC_OFF,  LCSC_WAIT, LCS_TIME_MS(50), LCSC_ON, LCSC_WAIT, LCS_TIME_MS(50)};
static byte_t  btnLedScript_ID01[] = { LCSC_OFF,  LCSC_WAIT, LCS_TIME_MS(100), LCSC_FADE, BTN_LED_MAX, LCS_TIME_MS(500), LCSC_WAIT, LCS_TIME_MS(100), LCSC_FADE, BTN_LED_MIN, LCS_TIME_MS(500), LCSC_GOTO, 4 };
static byte_t  btnLedScript_ID02[] = { LCSC_FADE, BTN_LED_MAX/3, LCS_TIME_MS(1500), LCSC_FADE, BTN_LED_MAX, LCS_TIME_MS(1500)};

#define LED_SCRIPT(N,ID) { sizeof N, ID, N }
static ledScript_t  btnLedScriptsArray[] = {LED_SCRIPT(btnLedScript_ID01,1),LED_SCRIPT(btnLedScript_ID02,2)};
static ledScripts_t  btnLedScripts = { (sizeof btnLedScriptsArray)/sizeof(ledScript_t) , btnLedScriptsArray };
//========================== BTN LED SCRIPTS ============================
static ledController_c btnLed(PIN_BTN_LED,0,&btnLedScripts);
static ledController_c btn2Led(PIN_BTN2_LED,1,&btnLedScripts);
static TaskHandle_t waBtnLedThread;
static void BtnLedThread(void *arg) {
  {
    uni_locker lock(globalMutex);
    btnLed.begin();
    btn2Led.begin();
  }
  while(1)
  {
    btnLed.loop();
    btn2Led.loop();
    delay(10);
  }
}

#ifdef MAIN_BTN_PULLUP
static input_c  btnInput(PIN_BTN,BTN_GUARD_TIME,1,1);
#else
static input_c  btnInput(PIN_BTN,BTN_GUARD_TIME,0,0);
#endif
static input_c  input1(PIN_INP1,INP_GUARD_TIME,1,1);
static input_c  input2(PIN_INP2,INP_GUARD_TIME,0,1);
static dynamicDelayWithSignal_c controllerDelay;


static TaskHandle_t waControllerThread;
static void ControllerThread(void *arg) {
  byte_t lastMode = 100;

  pinMode(PIN_LED, OUTPUT);

  digitalWrite(PIN_RELAY, LOW);
  pinMode(PIN_RELAY,OUTPUT);

  btnLed.setON();
  btn2Led.setOFF();

  controllerDelay.start();
  while (true) {
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
      }
    }

    if(currentMode)
    {
    	uint32_t now = millis();
      byte_t buttonState = btnInput.getValue(now);
      if(buttonState)
      {
        digitalWrite(PIN_RELAY, HIGH);
        btnLed.setOFF();
      }
      else
      {
        digitalWrite(PIN_RELAY, LOW);
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
      {
        uni_locker lock(globalMutex);
        currentOutputsValue = outputsValue;
        currentLed1Value = led1Value;
        currentLed2Value = led2Value;
      }      
      if(currentOutputsValue != lastOutputsValue)
      {
        if(currentOutputsValue & AOM_RELAY)
          digitalWrite(PIN_RELAY, HIGH);
        else
          digitalWrite(PIN_RELAY, LOW);
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
        uni_locker lock(globalMutex);
        inputsValue = currentInputsValue;
      }
    }
    controllerDelay.delay(10);
  }
}

static TaskHandle_t waSerialThread;
static comm_protocol_parser_c protParser(cp_prefix_t(APP_1, APP_2),MAX_PACKET_SIZE);

int bytesAvailable;
byte_t command;
byte_t packetSize;
byte_t incomingByte = 0; 
byte_t *packetBuffer;
byte_t newPacketBuffer[MAX_PACKET_SIZE+4];
uint32_t lastHostActivityTime;

#define sendPacket(P,V) Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),P,&V,sizeof(V)))
#define sendOutput(V) sendPacket(ACC_OSTAT,V)

static void SerialThread(void *arg) {
  lastHostActivityTime = millis();
  Serial2.begin(9600,SERIAL_8N1,PIN_UART_RX,PIN_UART_TX);
  packetBuffer = protParser.get_packet_buffer();
  while (true) {
    if(!isStandaloneMode)
    {
      byte_t currentInputsValue;
      {
        uni_locker lock(globalMutex);
        currentInputsValue = inputsValue;
      }
  
      if(lastInputsValue!=currentInputsValue)
      {
        lastInputsValue = currentInputsValue;
        Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ISTAT,&lastInputsValue,sizeof(lastInputsValue)));
      }
#ifdef HAS_ULTRASONIC
      int16_t currentUValue;

      {
        uni_locker lock(globalMutex);
      	currentUValue =  ultrasonicValue;
      }

      if(abs((int32_t)lastUltrasonicValue-(int32_t)currentUValue)>=(lastUltrasonicValue/10))
      {
      	lastUltrasonicValue = currentUValue;
        Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ULTRASONIC_DATA,&lastUltrasonicValue,sizeof(lastUltrasonicValue)));
      }
#endif
    }

    bytesAvailable = Serial2.available();
    if(bytesAvailable>0)
    {
      while(bytesAvailable>0)
      {
        incomingByte = Serial2.read();
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
                Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),command,AC_PROTOCOL_VERSION,sizeof(AC_PROTOCOL_VERSION)-1));
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
                Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_PING,NULL,0));
                break;
              }              
              
            case ACC_SET_RELAY:
              {
                if(packetSize>=2)
                {
                  if(packetBuffer[1])
                  {
                    uni_locker lock(globalMutex);
                    outputsValue |= AOM_RELAY;
                  }
                  else
                  {
                    uni_locker lock(globalMutex);
                    outputsValue &= ~AOM_RELAY;
                  }
                }
                byte_t tmp = outputsValue;
                sendOutput(tmp);
                controllerDelay.signal();
                break;
              }
            case ACC_SET_BTN_LED:
              {
                if(packetSize>=2)
                {
                  uni_locker lock(globalMutex);
                  led1Value =  packetBuffer[1];
                }
                sendPacket(ACC_SET_BTN_LED,packetBuffer[1]);
                controllerDelay.signal();
                break;
              }

            case ACC_SET_BTN2_LED:
              {
                if(packetSize>=2)
                {
                  uni_locker lock(globalMutex);
                  led2Value =  packetBuffer[1];
                }
                sendPacket(ACC_SET_BTN2_LED,packetBuffer[1]);
                controllerDelay.signal();
                break;
              }

            default:
              {
                Serial2.write(newPacketBuffer,protParser.create_packet(newPacketBuffer,sizeof(newPacketBuffer),ACC_ERROR,NULL,0));
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
    delay(1);
  }
}

void setup() 
{
  setCpuFrequencyMhz(80); 
  Serial.begin(115200);

  uint32_t seed = esp_random();
  DEBUG_MSG("Setting random seed %u\n", seed);
  randomSeed(seed); // ESP docs say this is fairly random

  DEBUG_MSG("CPU Clock: %d\n", getCpuFrequencyMhz());
  DEBUG_MSG("Total heap: %d\n", ESP.getHeapSize());
  DEBUG_MSG("Free heap: %d\n", ESP.getFreeHeap());
  DEBUG_MSG("Total PSRAM: %d\n", ESP.getPsramSize());
  DEBUG_MSG("Free PSRAM: %d\n", ESP.getFreePsram());

  auto res = esp_task_wdt_init(APP_WATCHDOG_SECS_INIT, true);
  assert(res == ESP_OK);

  res = esp_task_wdt_add(NULL);
  assert(res == ESP_OK);

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  isStandaloneMode = 1;

  resetIOValues();
  vTaskPrioritySet( NULL, tskIDLE_PRIORITY + 5 );
#ifdef HAS_ULTRASONIC
  adcAttachPin(PIN_ULTRASONIC);
  xTaskCreatePinnedToCore(UltrasonicSensorThread, "Ultrasonic Thread", 1024, NULL, tskIDLE_PRIORITY+4, &waUltrasonicSensorThread,1);
#endif
  xTaskCreatePinnedToCore(BtnLedThread, "BtnLed Thread", 4096, NULL, tskIDLE_PRIORITY+3, &waBtnLedThread,0);

  xTaskCreatePinnedToCore(ControllerThread, "Controller Thread", 4096, NULL, tskIDLE_PRIORITY+2, &waControllerThread,1);

  xTaskCreatePinnedToCore(SerialThread, "Serial Thread", 8192, NULL, tskIDLE_PRIORITY+1, &waSerialThread,1);

  DEBUG_MSG("Started\n");
  delay(100);
  vTaskPrioritySet( NULL, tskIDLE_PRIORITY);
  
  esp_task_wdt_delete(NULL);
  esp_task_wdt_init(APP_WATCHDOG_SECS_WORK, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  esp_task_wdt_reset();  
  delay(50);
}
