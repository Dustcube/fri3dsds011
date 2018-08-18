/*
   Measure particulate matter, display it on a display along with the GPS time and GPS speed
   Keep the particulate matter measurements together with the GPS coordinates and the time in a file.
   Provide tools to read and edit the file.

   by Luc Janssens 24/5/2018
   version 0.05
*/

/*
   HARDWARE
   ESP32 Development board, Dual Core, WiFi+Bluetooth ultra low power consumption
   SDS011
   GPS GY-NEO6MV2
   LCD 2016 I2C module
   18650 Battery
   18650 Battery shield V3, Micro USB input - type-A output, DIY Kit
*/

/*
   Pinout ESP32
   TX 12  SDS011 RXD
   RX 14  SDS011 TXD
   TX 17  GPS RX
   RX 16  GPS TX
   SDA 21
   SCL 22
*/


/*
   LCD DISPLAY EXAMPLE

 * *****************
   PM2.5= 6   88,57
   PM10 = 12  13:54
 * *****************

   top line right is GPS speed
   bottom line right is time
*/

/*-----( Import needed libraries )-----*/

#include <HardwareSerial.h>
#include <SPIFFS.h>
#include <FS.h>

#include <Wire.h>                                                   // Comes with Arduino IDE

#include <LiquidCrystal_I2C.h>
// Get the LCD I2C Library here:
// https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads
// Move anywher LCD libraries to another folder or delete them
// See Library "Docs" folder for possible commands etc.
// !!! analogWrite() does not work on ESP32 mark lines in LiquidChristal.ccp with analogWrite() as comment !!!

#include <TinyGPS++.h>
#include <Fri3dMatrix.h>

/*-----( Declare Constants )-----*/
const char* filename = "/GPS.txt";

/*-----( Declare Variables )-----*/
float p10;
float p25;
int sdsValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};                      // declare a 9 element array  and make shure all values are 0
String location$ = "";                                              // define string for GPS location
String date$ = "";                                                  // define string for GPS date
String time$ = "";                                                  // define string for GPS time
String speed$ = "";                                                 // define string for GPS speed kmph

unsigned long lastLogTime = 0;

TinyGPSPlus gps;                                                    // The TinyGPS++ object
TaskHandle_t displayTask;
HardwareSerial SerialSDS(1);                                          // Define Serial port 1 and 2 with HardwareSerial#(Uart#)
HardwareSerial SerialGPS(2);

Fri3dMatrix matrix = Fri3dMatrix();

void displayThread( void * parameter ) {
  while(true) {
    data2Ser();
    data2Lcd2x16();
  }
  delay(500);
}

void logToFile() {
  // log every 30 seconds
  if (lastLogTime + 5000 > millis()) {
    return;
  }
  Serial.println("logging");
  
  File fileToAppend = SPIFFS.open("/log.csv", FILE_APPEND);
 
  if (!fileToAppend) {
    Serial.println("There was an error opening the file for appending");
    return;
  }

  String s = "";
  s += date$;
  s += ",";
  s += time$;
  s += ",";
  s += location$;
  s += ",";
  s += p25;
  s += ",";
  s += p10;
  
  if (!fileToAppend.println(s)){
    Serial.println("File append failed");
  }

  lastLogTime = millis();
  fileToAppend.close();
}

void setup() {
  // show we're live
  matrix.clear(1);
  
  /* Init Serial ports

     Change Hardware Serial
        if(_uart_nr == 1 && rxPin < 0 && txPin < 0) {
        rxPin = 14;
        txPin = 12;
  */
  Serial.begin(115200);                                             // Serial port Uart0 is connected to USB (UART0)
  SerialSDS.begin(9600, SERIAL_8N1, 16, 17);                                  // Used to read SDS011 (UART1) rxPin = 14 / txPin = 12 !! change in !! HardwareSerial.cpp
  SerialGPS.begin(9600, SERIAL_8N1, 12, 14);                                              // Used to read GPS (UART2) rxPin = 16 / txPin = 17

  delay(1000);
  Serial.println("Serial is active");

  // start display thread
  xTaskCreatePinnedToCore(
    displayThread,             /* Task function. */
    "displayThread",           /* name of task. */
    1000,                       /* Stack size of task */
    0,                       /* parameter of the task */
    1,                          /* priority of the task */
    &(displayTask),            /* Task handle to keep track of created task */
    0);                         /* Core */

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
   }

  /*                                             // set LCD cursor
  if (SPIFFS.begin())
  {
    Serial.println("SPIFFS Initialize....ok");                      // print to Serial
    lcd.print("SPIFFS Init OK");                                    // print to LCD
  }
  else
  {
    Serial.println("SPIFFS Initialization...failed");               // print to Serial
    lcd.print("SPIFFS Init FAIL");                                  // print to LCD
  }



  if (SPIFFS.exists(filename)) {
    Serial.println("GPS.txt exists.");
    lcd.setCursor(0, 1);
    lcd.print(filename);
    lcd.print(" Exist");
  } else {
    Serial.println("GPS.txt doesn't exist.");

    if (SPIFFS.format())                                            //Format File System

    {
      Serial.println("File System Formated");                       // print to Serial
    }
    else
    {
      Serial.println("File System Formatting Error");               // print to Serial
    }
  }
  delay(2000);                                                      // some time to be able to read LCD messages
  */

  /* Create New File And Write Data to It     w=Write Open file for writing a= open to append data  */
  /*File f = SPIFFS.open(filename, "a");                              // open to append data

  if (!f) {
    Serial.println("file open failed");                             // print to Serial
    lcd.print("File Open Failed");                                  // print to LCD
  }
  else
  {
    Serial.println("Writing Data to File");                         //Write data to file
    Serial.print("File size is now ");
    Serial.println(f.size());
    f.println("This is some data which is written in file");
    f.println("This is other data which is written in this file");
    f.println("This is nog other data");
    //    f.println("Heel weinig data");
    Serial.print("File size after printing is now ");
    Serial.println(f.size());
    delay(500);
    f.close();                                                      //Close file
  }*/
}

void data2Ser() {
  Serial.print("PM 2.5 = ");
  Serial.println(p25);         // PM2.5
  Serial.print("PM 10 =  ");
  Serial.println(p10);         // PM 10
  Serial.print("SDS ID = ");
  Serial.println(sdsValues[6] * 256 + sdsValues[5]);                // SDS ID
  Serial.print("Locatie= ");
  Serial.println(location$);                                        // GPS location
  Serial.print("Cursor locatie= ");
  Serial.println(15 - speed$.length());
  Serial.print("Speed= ");
  Serial.println(speed$);                                           // GPS speed
  Serial.print("Datum= ");
  Serial.println(date$);                                            // GPS datum
  Serial.print("Tijd= ");
  Serial.println(time$);                                            // GPS time
  Serial.println();
}

void data2Lcd2x16() {
  if (p10 > 0 && p25 > 0) {
    matrix.clear();

    String s = "PM2.5 = ";
    s += round(p25);
    
    if (p25 < 6) {
      s += " Zeer Goed";
    } else if (p25 > 6 && p25 < 12) {    
      s += " Goed";
    } else if (p25 > 12 && p25 < 18) {
      s += " Matig";
    } else if (p25 > 18 && p25 < 25) {
      s += " Slecht";
    } else {
      s += " Zeer Slecht";
    }

    s += "    PM10 = ";
    s += round(p10);

    if (p10 < 10) {
      s += " Zeer Goed";
    } else if (p10 > 10 && p10 < 20) {    
      s += " Goed";
    } else if (p10 > 20 && p10 < 30) {
      s += " Matig";
    } else if (p10 > 30 && p10 < 40) {
      s += " Slecht";
    } else {
      s += " Zeer Slecht";
    }

    for( int i = -15; i < (int)( s.length() ) * 4; i++ ) {
      matrix.clear();
      matrix.drawString( -i, s );
      delay(100);
    };
  }
}

void noData2Lcd2x16() {
  matrix.clear();
}

void gpsInfo() {
  location$ = "0,0";
  if (gps.location.isValid()) {
     location$ = "";
     location$ += gps.location.lat();
     location$ += ",";
     location$ += gps.location.lng();
  }

  if (gps.date.isValid())
  {
    date$ = (gps.date.month());
    date$ = date$ + ("/");
    date$ = date$ + (gps.date.day());
    date$ = date$ + ("/");
    date$ = date$ + (gps.date.year());
  }
  else
  {
    date$ = ("INVALID");
  }

  if (gps.time.isValid())
  {
    time$ = "";                                                     // reset time$
    if (gps.time.hour() < 10) time$ = time$ +("0");
    time$ = time$ + (gps.time.hour());
    time$ = time$ + (":");
    if (gps.time.minute() < 10) time$ = time$ +(F("0"));
    time$ = time$ +(gps.time.minute());
    time$ = time$ +(":");
    if (gps.time.second() < 10) time$ = time$ +(F("0"));
    time$ = time$ +(gps.time.second());
    time$ = time$ +(".");
    if (gps.time.centisecond() < 10) time$ = time$ +("0");
    time$ = time$ +(gps.time.centisecond());
  }
  else
  {
    time$ = ("INVALID");
  }

  if (gps.speed.isValid())
  {
    speed$ = "";
    //Serial.print("Speed= ");
    //Serial.println(gps.speed.kmph());
    speed$ = (gps.speed.kmph());
  }
  else
  {
    speed$ = ("INVALID");
  }

}

void readSDS() {
  /* Read SDS011 at SerialSDS and Write to Serial */
  if (SerialSDS.available() > 0) {
    delay(100);                                                       // time needed to read the valuesand
    if ((170 == SerialSDS.read()) && (lowByte(sdsValues[1] + sdsValues[2] + sdsValues[3] + sdsValues[4] + sdsValues[5] + sdsValues[6]) == sdsValues[7])) {

      // if ((170 == SerialSDS.read()) && ((lowByte(sdsValues[1] + sdsValues[2] + sdsValues[3] + sdsValues[4] + sdsValues[5] + sdsValues[6]) == sdsValues[7]))) {                                      // read the incoming byte and check if it is the message header
      Serial.println("Check-sum data correct");                       // Check-sum + Message tail correct
      /*
        DATA FORMAT
        0 - Message header       AA    170
        1 - Commander No         C0    192i
        2 - PM2.5 Low Byte
        3 - PM2.5 High Byte
        4 - PM10 Low Byte
        5 - PM10 High Byte
        6 - ID byte 1            Chip ID
        7 - ID Byte 2            Chip ID
        8 - Check-sum
        9 - Message Tail         AB    171
      */
      for (int i = 0 ; i < 9; i++) {                                  // clear SDSvalue array
        sdsValues[i] = 0;
      }
      for (int i = 0 ; i < 9; i++) {                                  // store data in SDSvalue array
        sdsValues[i] = (SerialSDS.read());
      }
      if (sdsValues[8] == 171) {                                      // Check op Message Tail
        p25 = (sdsValues[2] * 256 + sdsValues[1]) / 10;
        p10 = (sdsValues[4] * 256 + sdsValues[3]) / 10; 
      }
    }
    else {                                                            // Check-sum incorrect OR message header fault
      Serial.println("Data not correct");
      noData2Lcd2x16();
      data2Ser();
      SerialSDS.end();                                                  // if SerialSDS crashes try with restarting to solve the problem
      SerialSDS.begin(9600);
      delay(100);
    }
  }
  else {
    //Serial.println(F("No SDS detected: check wiring."));
    //while (true);
  }
}

void loop() {
  readSDS();
  delay(100);

  /* Read SerialGPS  for GPS */
  while (SerialGPS.available() > 0)
    if (gps.encode(SerialGPS.read()))
      //      displayInfo();
      gpsInfo();
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    //Serial.println(F("No GPS detected: check wiring."));
    //while (true);
  }

  logToFile();
}



