#include <SPI.h>
#include "printf.h"
#include "RF24.h"
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include "ArduinoJson.h"


/* ---------- */

#define LOG_INTERVAL 10     // time interval in seconds between log file to be written to SD card
#define TIMEOUT 1           // time in seconds after which lost connection will show
#define SCREEN_INTERVAL 20  // time interval in seconds for screen to change shown data

/* ---------- */

#define ROLE 0      // 0 - RX, 1 - TX

#define CE_PIN 49
#define CS_RF_PIN 53

#define CS_SD_PIN 48

#define BTN_PIN 36

/* ---------- */


LiquidCrystal_I2C lcd(0x27, 20, 4);
RF24 radio(CE_PIN, CS_RF_PIN);

uint8_t address[][6] = {"1Node", "2Node"}; 

bool newDataAvailable = false;
int page; 

struct DataPackage {
  char sensor;    // 1B
  int valInt;     // 4B
  long valLong;   // 8B
  float valFloat; // 4B
};

DataPackage data;

struct Sensors {
  float temp;
  float pressure;
  int height;
  int humidity;
  long latitude;
  long longtitude;
  long time;
  long date;
  int x;
  int y;
  int z;
  int roll;
  int pitch;
  int yaw;
  int rpm;
  float thrust;
  float power;
  float air;
};

Sensors s;

File logFile; 
DynamicJsonDocument doc(1024);
String logString; 
bool readyToLog = false; 

bool online; 
unsigned long lastRecvTime = 0, btnTime = 0, lastLog = 0;

void setup() {

  Serial.begin(9600);
  while (!Serial) { }
  
  pinMode(BTN_PIN, INPUT);

  pinMode(CS_RF_PIN, OUTPUT);
  pinMode(CS_SD_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  switchToRF();

  // Check if radio is working
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!"));
    lcd.setCursor(0,0);
    lcd.print("Radio not responding");
    while (1) {} // hold in infinite loop
  }

  switchtoSD();

  // Check if sd card module is working
  if (!SD.begin(CS_SD_PIN)) {
    Serial.println(F("SD card reader is not responding!"));
    lcd.setCursor(0,0);
    lcd.print("SD not responding");
    while (1) {} // hold in infinite loop
  }
  
  radio.setPALevel(RF24_PA_MAX);
  radio.setPayloadSize(sizeof(DataPackage));

  radio.openWritingPipe(address[ROLE]);
  radio.openReadingPipe(1, address[!ROLE]);
  radio.startListening();

  updateDisplay();

  data.valInt = 0;
  data.valFloat = 0;
  data.valLong = 0;
  data.sensor = 't';

}

void loop() {

  if (millis() - btnTime >= SCREEN_INTERVAL * 1000)
  {
    page++;
    if (page == 4) page = 0;
    btnTime = millis();
    updateDisplay();
  }
  bool led_state = false;
 
  uint8_t pipe; 
    if (radio.available(&pipe)) {  
      uint8_t bytes = radio.getPayloadSize(); 
      radio.read(&data, bytes); 
      Serial.print(F("Received "));
      Serial.print(bytes);        
      Serial.print(F(" bytes on pipe "));
      Serial.print(pipe);
      Serial.print(F(": "));
      Serial.print(data.sensor);
      Serial.print("\t");
      Serial.print(data.valInt);
      Serial.print("\t");
      Serial.print(data.valFloat);
      Serial.print("\t");
      Serial.println(data.valLong); 
      led_state = !led_state; 
      digitalWrite(LED_BUILTIN, led_state);
      newDataAvailable = true;
      online = true;
      lastRecvTime = millis();
      updateDisplay();
  }

  if (millis() - lastRecvTime > TIMEOUT * 1000 ) {
    online = false;
  }

  // once in 10 sec save log to sd card
  if (millis() - lastLog > (LOG_INTERVAL * 1000) && readyToLog) { // 
    saveLogFile();
    readyToLog = false;
  }
  
  updateHardware();

}

void updateDisplay()
{
  if ( page == 0) 
  {
    lcd.setCursor(0, 0);
    lcd.print(String(s.x) + " " + String(s.y) + " " + String(s.z) + "       ");
    lcd.setCursor(0, 1);
    lcd.print(String(s.roll) + " " + String(s.pitch) + " " + String(s.yaw)  + "       ");
    lcd.setCursor(0, 2);
    lcd.print("THR " + String(s.thrust)  + "       ");
  }
  if ( page == 1) 
  {
    lcd.setCursor(0, 0);
    lcd.print("LAT " + String(s.latitude) + "       ");
    lcd.setCursor(0, 1);
    lcd.print("LON " + String(s.longtitude) + "       ");
    lcd.setCursor(0, 2);
    lcd.print("HGT " + String(s.height) + " m" + "       ");
  }
  if ( page == 2) 
  {
    lcd.setCursor(0, 0);
    lcd.print("TMP " + String(s.temp) + " C" + "       ");
    lcd.setCursor(0, 1);
    lcd.print("PRS " + String(s.pressure) + " Pa" + "       ");
    lcd.setCursor(0, 2);
    lcd.print("HUM " + String(s.humidity) + "       ");
  }
  if ( page == 3) 
  {
    lcd.setCursor(0, 0);
    lcd.print("RPM  " + String(s.rpm) + "       ");
    lcd.setCursor(0, 1);
    lcd.print("PWR  " + String(s.power) + "       ");
    lcd.setCursor(0, 2);
    lcd.print("WND " + String(s.air) + "       ");
  }
  if (!online)
  {
    lcd.setCursor(0, 3);
    lcd.print("NO SIGNAL");
  }
  else
  {
    lcd.setCursor(0, 3);
    lcd.print("                   ");
  }
}

void updateHardware() 
{

  if (millis() - btnTime >= 250)
  {
    if (digitalRead(BTN_PIN) == HIGH)
    {
      page++;
      if (page == 4) page = 0;
      btnTime = millis();
    }
  }

  if (newDataAvailable)
  {
    lastRecvTime = millis();
    online = true;
    switch (data.sensor) {
        case 't':
          s.temp = data.valFloat;
          doc["temperature"] = s.temp;
          break;
        case 'P':
          s.pressure = data.valFloat;
          doc["pressure"] = s.pressure;
          break;
        case 'H':
          s.height = data.valInt;
          doc["height"] = s.height;
          break;
        case 'h':
          s.humidity = data.valInt;
          doc["humidity"] = s.humidity;
          break;
        case 'L':
          s.latitude = data.valLong;
          doc["latitude"] = s.latitude;
          break;
        case 'l':
          s.longtitude = data.valLong;
          doc["longtitude"] = s.longtitude;
          break;
        case 'X':
          s.x = data.valInt;
          doc["x"] = s.x;
          break;
        case 'Y':
          s.y = data.valInt;
          doc["y"] = s.y;
          break;
        case 'Z':
          s.z = data.valInt;
          doc["z"] = s.z;
          break;
        case 'p':
          s.pitch = data.valInt;
          doc["pitch"] = s.pitch;
          break;
        case 'r':
          s.roll = data.valInt;
          doc["roll"] = s.roll;
          break;
        case 'y':
          s.yaw = data.valInt;
          doc["yaw"] = s.yaw;
          break;
        case 'R':
          s.rpm = data.valInt;
          doc["rpm"] = s.rpm;
          break;
        case 'T':
          s.thrust = data.valInt;
          doc["thrust"] = s.thrust;
          break;
        case 'a':
          s.power = data.valFloat;
          doc["power"] = s.power;
          break;
        case 'A':
          s.air = data.valFloat;
          doc["wind"] = s.air;
          break;
        case 'e':
          s.time = data.valInt;
          doc["time"] = s.time;
          break;
        case 'd':
          s.date = data.valInt;
          // doc["date"] = s.date;
          readyToLog = true;
          break;
        default:
          break;
    }
    newDataAvailable = false;
  }
}

void saveLogFile()
{
  switchtoSD();
  logString = "";

  serializeJson(doc, logString);

  String fname = String(s.date) + ".txt-";
  char charname[fname.length()];
  fname.toCharArray(charname, fname.length());
  logFile = SD.open(charname, FILE_WRITE);
  if (!logFile)
  {
      Serial.println("Could not save file");
  }
  if (logFile)
  {
    logFile.println(logString);
    logFile.close();
    Serial.println("Saved log file");
    lastLog = millis();
  }
  switchToRF();
}

void switchToRF()
{
  digitalWrite(CS_RF_PIN, LOW);
  digitalWrite(CE_PIN, HIGH);
  digitalWrite(CS_SD_PIN, HIGH);
}

void switchtoSD()
{
  digitalWrite(CE_PIN, LOW);
  digitalWrite(CS_RF_PIN, HIGH);
  digitalWrite(CS_SD_PIN, LOW);
}
