#include <DigitalIO.h>
#include <nRF24L01.h>
#include <printf.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <splash.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <RF24_config.h>
#include <RF24.h>

#define DISP_ADDR 0x3C
#define SET_BTN D2
#define MOV_BTN D1
#define CS_PIN D8
#define CE_PIN D0
#define DIS_HEIGHT 64
#define DIS_WIDTH 128

struct Hour
{
  uint8_t h1 = 0;
  uint8_t h2 = 0;
  uint8_t m1 = 0;
  uint8_t m2 = 0;
  bool set = false;
};

bool dispActive = false, isAlarmSet = false;
volatile uint8_t selectedOption = 0;
unsigned long timeSinceLastInput = 0;
unsigned long setBtnPressTime = 0, movBtnPressTime = 0;
volatile bool awaitSendData = false;
const byte address[5] = {'0', '0', '0', '0', '1'};
 uint8_t ackData[2] = {0, 0};

Hour hour;
Adafruit_SSD1306 display(DIS_WIDTH, DIS_HEIGHT, &Wire, -1);
RF24 radio(CE_PIN, CS_PIN);

void sendSignal(bool isSet = true);

void IRAM_ATTR changeDigit()
{

  dispActive = true;

  switch (selectedOption)
  {
    case 0:
      if (hour.h1 < 2 && hour.h2 <= 3)
      {
        hour.h1++;
      }
      else if (hour.h1 < 2 && hour.h2 > 3)
      {
        hour.h1++;
        hour.h2 = 0;
      }
      else if (hour.h1 > 1)
      {
        hour.h1 = 0;
      }

      break;

    case 1:
      if (hour.h1 < 2)
      {
        (hour.h2 > 8) ? hour.h2 = 0 : hour.h2++;
      }
      else
      {
        (hour.h2 > 2) ? hour.h2 = 0 : hour.h2++;
      }

      break;

    case 2:
      (hour.m1 > 4) ? hour.m1 = 0 : hour.m1++;
      break;

    case 3:
      (hour.m2 > 8) ? hour.m2 = 0 : hour.m2++;
      break;

    case 4:
      awaitSendData = true;
      break;
  }

  timeSinceLastInput = millis();
  setBtnPressTime = timeSinceLastInput;
  delay(200);

}

void IRAM_ATTR moveCursor()
{

  dispActive = true;

  if (selectedOption > 3)
    selectedOption = 0;
  else
    selectedOption++;

  timeSinceLastInput = millis();
  movBtnPressTime = timeSinceLastInput;
  delay(200);
}


void setup(){

  pinMode(SET_BTN, INPUT_PULLUP);
  pinMode(MOV_BTN, INPUT_PULLUP);

  Serial.begin(115200); 
  while(!Serial){}
  Wire.begin(14, 12);

  if (!display.begin(SSD1306_SWITCHCAPVCC, DISP_ADDR))
  {
    Serial.println("FAILED INITIALIZIN DISPLAY");
    for(;;);
  }
  else
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
  }

  if (!radio.begin())
  {
    Serial.println("RADIO NOT CONNECTED");
  }
  else
  {
    radio.setPALevel(RF24_PA_HIGH);
    radio.enableAckPayload();
    radio.setRetries(5, 5);
    radio.openWritingPipe(address);
    radio.stopListening();
    Serial.println("RADIO CONNECTED");
  }


  attachInterrupt(digitalPinToInterrupt(MOV_BTN), moveCursor, FALLING);
  attachInterrupt(digitalPinToInterrupt(SET_BTN), changeDigit, FALLING);
  Serial.println("ESP READY");

}

void loop()
{

  if (awaitSendData)
  {
    sendSignal();
  }

  if (millis() - timeSinceLastInput >= 15000 && dispActive)
  {
    sleepDisplay();
  }

  if (dispActive)
    drawDisplay();

  if (buttonsHolded())
    sendSignal(false);
}


void drawDisplay()
{
  static bool blink = false;
  static unsigned long prevMillis = 0;
  unsigned long currMillis = millis();

  if (currMillis - prevMillis >= 600)
  {
    blink = !blink;
    prevMillis = currMillis;
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  if (isAlarmSet)
  {
    display.setTextSize(1);
    display.setCursor(5, 1);
    display.print('*');
  }

  displayHour(blink);

  display.display();
}

void displayHour(bool blink)
{
  display.setTextSize(3);
  display.setCursor(20, 20);

  if (selectedOption == 0 && blink) display.print(" ");
    else display.print(hour.h1);

  if (selectedOption == 1 && blink) display.print(" ");
    else display.print(hour.h2);

  display.print(":");

  if (selectedOption == 2 && blink) display.print(" ");
    else display.print(hour.m1);

  if (selectedOption == 3 && blink) display.print(" ");
    else display.print(hour.m2);

  display.setTextSize(1);
  display.setCursor(48, 51);

  if (selectedOption == 4 && blink) display.print(" ");
    else display.print("SET");
}

bool buttonsHolded()
{
  bool buttonsPressed = digitalRead(SET_BTN) == LOW && digitalRead(MOV_BTN) == LOW;
  bool holded = millis() - setBtnPressTime >= 2000 && millis() - movBtnPressTime >= 2000;
  return buttonsPressed && holded;
}

void sendSignal(bool isSet)
{
  hour.set = isSet;
  Hour packet = hour;
  bool sent = radio.write(&packet, sizeof(packet));

  if (sent)
  {
    if (radio.available())
    {
      radio.read(&ackData, sizeof(ackData));
      parseResponse(isSet);
      Serial.println("DELIVERED. ACK DATA RECEIVED");
    }
    else
    {
      Serial.println("DELIVERED. NO ACK DATA");
    }
  }
  else
  {
    Serial.println("FAILED SENDING SIGNAL");
  }

  awaitSendData = false;

}

void sleepDisplay()
{
  display.clearDisplay();
  display.display();
  dispActive = false;
}

void parseResponse(bool &isSet)
{
  bool correctAck = ackData[0] == 1 && ackData[1] == 1;

  if (correctAck)
  {
    Serial.println("RESPONSE CORRECT");
    isAlarmSet = isSet;
  }

  ackData[0] = 0;    // Reset ack buffer to default
  ackData[1] = 0;

}

