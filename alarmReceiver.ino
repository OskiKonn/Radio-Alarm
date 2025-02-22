#include <SPI.h>
#include <Ds1302.h>
#include <RF24_config.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <printf.h>

#define RTC_CLK D3
#define RTC_DAT D2
#define RTC_RST D4
#define BUZZER_IO D1
#define RF_CE D0 
#define RF_CS D8

typedef Ds1302::DateTime DateTime;

struct Hour
{
  uint8_t h1 = 0;
  uint8_t h2 = 0;
  uint8_t m1 = 0;
  uint8_t m2 = 0;
  bool set = false;
};

const uint8_t ackData[2] = {1, 1};
uint8_t hour = 0, minute = 0;

volatile bool buzzActivated = false;
bool ring = false;
bool signalReceived = false;
const byte address[5] = {'0', '0', '0', '0', '1'};

Ds1302 rtc(RTC_RST, RTC_CLK, RTC_DAT);
RF24 radio(RF_CE, RF_CS);
Hour alarmHour;

void IRAM_ATTR setBuzzActivated()
{
  buzzActivated = !buzzActivated;
}

void setup() {

  Serial.begin(115200);
  while(!Serial) {}
  pinMode(BUZZER_IO, OUTPUT);
  Serial.println("ESP READY");

  rtc.init();

  if (rtc.isHalted())
  {
    Serial.println("RTC HALTED. SETTING DATE");

    DateTime dateTime = {

      .year = 25,
      .month = Ds1302::MONTH_FEB,
      .day = 21,
      .hour = 16,
      .minute = 48,
      .second = 0,
      .dow = Ds1302::DOW_FRI

    };

    rtc.setDateTime(&dateTime);

  }


  if (!radio.begin())
  {
    Serial.println("RADIO FAILED");
  }
  else
  {
    radio.setPALevel(RF24_PA_HIGH);
    radio.openReadingPipe(0, address);
    radio.enableAckPayload();
    radio.startListening();
    radio.writeAckPayload(0, &ackData, sizeof(ackData));
    Serial.println("RADIO CONNECTED");
  }


}

void loop() {
  
  static unsigned long prevMillis = 0;
  static bool hourMatch = false;
  unsigned long currMillis = millis();

  listen();

  if (currMillis - prevMillis >= 45000 && alarmHour.set)
  {
    hourMatch = checkTime();
    if (hourMatch) ring = true;

    prevMillis = currMillis;
  }

  if (ring && alarmHour.set)
    wakeUp();
  else
  {
    noTone(BUZZER_IO);
    ring = false;
  }

}

void listen()
{
  if (radio.available())
  {
    radio.read(&alarmHour, sizeof(alarmHour));
    signalReceived = true;
    radio.writeAckPayload(0, &ackData, sizeof(ackData));
    parseData();
  }
}

void parseData()
{
  hour = alarmHour.h1 * 10 + alarmHour.h2;
  minute = alarmHour.m1 * 10 + alarmHour.m2;
  Serial.println("DATA RECEIVED");
  signalReceived = false;
}

bool checkTime()
{
  DateTime nowDate;
  rtc.getDateTime(&nowDate);

  return nowDate.hour == hour && nowDate.minute == minute;

}

void wakeUp()
{
  static bool changeTone = false;
  static unsigned long prevMillis = 0;
  unsigned long currMillis = millis();

  if (currMillis - prevMillis >= 500)
    {
      prevMillis = currMillis;
      changeTone = !changeTone;
    }

  if (!changeTone)
    tone(BUZZER_IO, 2000);
  else
    tone(BUZZER_IO, 4000);
  
}




