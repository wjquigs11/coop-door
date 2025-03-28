#include <Arduino.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <math.h>
#include <WebSerial.h>
#include <Preferences.h>
#include <SPIFFS.h>

extern String host;
//extern JSONVar readings;
extern Preferences preferences;
extern unsigned long WebTimerDelay;

void logToAll(String s);

void i2cScan() {
  byte error, address;
  int nDevices = 0;

  logToAll("Scanning...");

  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmission to see if
    // a device acknowledged the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    char buf[16];
    sprintf(buf, "%2X", address); // Formats value as uppercase hex

    if (error == 0) {
      logToAll("I2C device found at address 0x" + String(buf) );
      nDevices++;
    }
    else if (error == 4) {
      logToAll("error at address 0x" + String(buf) );
    }
  }

  if (nDevices == 0) {
    logToAll("No I2C devices found");
  } else {
    logToAll("done");
  }
}

void WebSerialonMessage(uint8_t *data, size_t len) {
  Serial.printf("Received %lu bytes from WebSerial: ", len);
  Serial.write(data, len);
  Serial.println();
  WebSerial.println("Received Data...");
  String dataS = String((char*)data);
  // Split the String into an array of Strings using spaces as delimiters
  String words[10]; // Assuming a maximum of 10 words
  int wordCount = 0;
  int startIndex = 0;
  int endIndex = 0;
  while (endIndex != -1) {
    endIndex = dataS.indexOf(' ', startIndex);
    if (endIndex == -1) {
      words[wordCount++] = dataS.substring(startIndex);
    } else {
      words[wordCount++] = dataS.substring(startIndex, endIndex);
      startIndex = endIndex + 1;
    }
  }
  for (int i = 0; i < wordCount; i++) {
    WebSerial.println(words[i]);
    if (words[i].equals("?")) {
      WebSerial.println("restart");
      WebSerial.println("format");
      WebSerial.println("ls");
      WebSerial.println("scan (i2c)");
      WebSerial.println("hostname");
      WebSerial.println("status");
    }
    if (words[i].equals("format")) {
      SPIFFS.format();
      WebSerial.println("SPIFFS formatted");
    }
    if (words[i].equals("restart")) {
      ESP.restart();
    }
    if (words[i].equals("ls")) {
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while (file) {
        WebSerial.println(file.name());
        file.close(); // Close the file after reading its name
        file = root.openNextFile();
      }
      root.close();
      WebSerial.println("done");
    }
    if (words[i].equals("scan")) {
      i2cScan();
    }
    if (words[i].equals("hostname (change)")) {
      if (words[++i]) {
        host = words[i];
        preferences.putString("hostname",host);
        logToAll("hostname set to " + host );
        logToAll("restart to change hostname");
      }
    }
    if (words[i].equals("status")) {
      String buf = "hostname: " + host;
      logToAll(buf );
      //logToAll(readings );
      buf = String();
    }
  }
  for (int i=0; i<wordCount; i++) words[i] = String();
  dataS = String();
}
