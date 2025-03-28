#include <Arduino.h>
#include <Adafruit_MAX1704X.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <WebSerial.h>
#include <ReactESP.h>
#include <Adafruit_GFX.h>
#include <elapsedMillis.h>
#include <esp32-hal-log.h>
#include "chick_wb.h"

#define SC_WIDTH 240
#define SC_HEIGHT 135
#define DISPLAYUPDATEMSECS 1000
int chickXinit = SC_WIDTH*6/10;
int chickYinit = SC_HEIGHT-64;
int chickX = chickXinit;
int chickY = chickYinit;
int chickSpeed = 2;
int chickWidth = 64;

#define OPEN true
#define CLOSE false

#ifdef FEATHER
Adafruit_MAX17048 lipo;
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(SC_WIDTH, SC_HEIGHT);
#define MOTOR1 GPIO_NUM_10
#define MOTOR2 GPIO_NUM_11
#define MOTORPWM GPIO_NUM_8
#define SWITCH GPIO_NUM_17 // door reed switch
#define SWITCH1 GPIO_NUM_18 // open?
#define SWITCH2 GPIO_NUM_16  // close?
#else // feather GPIOs are not available on generic devkit
//#define MOTOR1 GPIO_NUM_15
#define MOTOR1 GPIO_NUM_19
#define MOTOR2 GPIO_NUM_16
#define MOTORPWM GPIO_NUM_17
#define SWITCH GPIO_NUM_3
#define SWITCH1 GPIO_NUM_18 // open?
#define SWITCH2 GPIO_NUM_16  // close?
#endif
bool doorState, wifiConnect=false;
bool sw1state = false;
bool sw2state = false;
volatile unsigned long lastIntrDoor, lastIntrSW1, lastIntrSW2;
const unsigned long debounceTime = 1000; // Adjust as needed
float lat, lon;

File consLog;
using namespace reactesp;
ReactESP app;
Reaction *animatePolloReact;
void animatePollo(bool direction);
Preferences preferences;
#define HTTP_PORT 80
const char *ssid1 = "XXX";
const char *password1 = "XXX";
const char *ssid2 = "XXX";
const char *password2 = "XXX";
bool serverStarted = false;
int switchDelay = 500;
String host = "Chixomatic";
AsyncWebServer server(HTTP_PORT);
HTTPClient http;

// timing
#define DEFDELAY 50 // for compass driver update
unsigned long WebTimerDelay = 1000; // for display
unsigned long previousMillis, previousDisplay, previousReading;
unsigned int readingId = 0;
unsigned long currentMillis = millis();
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp, sunRiseTime, sunSetTime;
float msecsToSunRise, msecsToSunSet;
int lastHours = 0;
int lastMinutes = 0;
int lastSeconds = 0;

void displayUpdate(String time);
void logToAll(String s);

String outputState(){
    if (doorState == CLOSE) {
        Serial.println("outputState checked");
        return "checked";
    } else {
        Serial.println("outputstate not checked");
        return "";
    }
}

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons += "<span style=\"font-size: 1.2em;\">CLOSED </span>";
    buttons += "<label class=\"switch\">";
    buttons +=      "<input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"door\" ";
    buttons +=             String((doorState == OPEN) ? "checked" : "") + ">";
    buttons += "<span class=\"slider\"></span></label>";
    buttons += "<span style=\"font-size: 1.2em;\"> OPEN</span>";
    //buttons += "<h4>Output - GPIO 33</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
    logToAll(buttons);
    return buttons;
  }
  return String();
}

void ToggleLed() {
  static bool led_state = false;
  digitalWrite(LED_BUILTIN, HIGH);   
  delay(100);                       
  digitalWrite(LED_BUILTIN, LOW);      
  delay(100);                       
  digitalWrite(LED_BUILTIN, HIGH);   
  delay(100);                       
  digitalWrite(LED_BUILTIN, LOW);    
  //delay(1000);        
  led_state = !led_state;
}

void logToAll(String s) {
  Serial.println(s);
  if (consLog) consLog.println(s);
  if (serverStarted)
    WebSerial.println(s);
}

void WebSerialonMessage(uint8_t *data, size_t len);

void moveMotor(boolean direction, uint16_t t) {
    digitalWrite(MOTORPWM, HIGH); // enable motor
    Serial.printf("moveMotor MOTORPWM pin %d is %d direction is %d\n", MOTORPWM, digitalRead(MOTORPWM), direction);
    if (direction) {
        digitalWrite(MOTOR1, HIGH);
        digitalWrite(MOTOR2, LOW);
    } else {
        digitalWrite(MOTOR2, HIGH);
        digitalWrite(MOTOR1, LOW);
    }
    delay(t);
    digitalWrite(MOTORPWM, LOW); // disable motor
    digitalWrite(MOTOR1, LOW);
    digitalWrite(MOTOR2, LOW);
    Serial.printf("moveMotor MOTORPWM pin %d is %d\n", MOTORPWM, digitalRead(MOTORPWM));
}

void openCloseDoor(boolean direction) {
    Serial.printf("openCloseDoor MOTORPWM pin %d is %d direction is %s\n", MOTORPWM, digitalRead(MOTORPWM), ((direction == OPEN) ? "OPEN" : "CLOSE"));
    digitalWrite(MOTORPWM, HIGH);
    if (!direction) {
        animatePolloReact = app.onRepeat(DISPLAYUPDATEMSECS/20, []() {
            animatePollo(CLOSE);
        });   
        Serial.print("reaction: ");    
        Serial.println((unsigned long)animatePolloReact, HEX);        
        digitalWrite(MOTOR2, HIGH);
        digitalWrite(MOTOR1, LOW);
        delay(debounceTime);
    } else {
        animatePolloReact = app.onRepeat(DISPLAYUPDATEMSECS/20, []() {
            animatePollo(OPEN);
        });
        Serial.print("reaction: ");    
        Serial.println((unsigned long)animatePolloReact, HEX);
        digitalWrite(MOTOR1, HIGH);
        digitalWrite(MOTOR2, LOW);
        delay(debounceTime);
    }
    // motor will stop when switchISR called from interrupt on SWITCH
}

volatile bool doorTrig = false;
volatile bool s1Trig = false;
volatile bool s2Trig = false;

void IRAM_ATTR doorISR() {
    unsigned long interruptTime = esp_timer_get_time() / 1000; // Get time in milliseconds
    if (interruptTime - lastIntrDoor > debounceTime) {
        doorTrig = true;
        lastIntrDoor = interruptTime;
    }
}

void IRAM_ATTR s1ISR() {
    unsigned long interruptTime = esp_timer_get_time() / 1000; // Get time in milliseconds
    //Serial.printf("s1ISR @ %lu lastIntrSW1 %lu\n", interruptTime, lastIntrSW1);
    if (interruptTime - lastIntrSW1 > debounceTime) {
        s1Trig = true;
        lastIntrSW1 = interruptTime;
    }
}

void IRAM_ATTR s2ISR() {
    unsigned long interruptTime = esp_timer_get_time() / 1000; // Get time in milliseconds
    //Serial.printf("s2ISR @ %lu lastIntrSW2 %lu\n", interruptTime, lastIntrSW1);
    if (interruptTime - lastIntrSW2 > debounceTime) {
        s2Trig = true;
        lastIntrSW2 = interruptTime;
    }
}

void handleSwitch() {
    if (doorTrig || s1Trig || s2Trig) {
        Serial.printf("\nswitchISR @ %lu door %d s1 %d s2 %d\n", millis(), doorTrig, s1Trig, s2Trig);    
    }
    if (doorTrig) {
        doorTrig = false;
        int motorPwmState = digitalRead(MOTORPWM);
        Serial.printf("MOTORPWM pin %d is %d\n", MOTORPWM, motorPwmState);
        if (motorPwmState) { // motor is on
            Serial.println("stopping motor");
            digitalWrite(MOTORPWM, LOW); // disable motor
            digitalWrite(MOTOR1, LOW);
            digitalWrite(MOTOR2, LOW);
            Serial.print("deleting reaction: ");
            Serial.println((unsigned long)animatePolloReact, HEX);
            if (animatePolloReact) {
                animatePolloReact->remove();
                animatePolloReact = NULL;
            }
            chickX = chickXinit;
            chickY = chickYinit;
            doorState = !doorState;
            preferences.putInt("doorState", doorState);
        }
    } else if (s1Trig) {
        s1Trig = false;
        if (!doorState) { // door is closed
            Serial.println("opening door");
            openCloseDoor(OPEN);
        }
    } else if (s2Trig) {    
        s2Trig = false;        
        if (doorState) { // door is open
            Serial.println("closing door");
            openCloseDoor(CLOSE);
        }
    }
}

bool initWiFi() {
    logToAll("initWiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid1, password1);
    Serial.print("Connecting to WiFi ..");
    int s, numtries;
    while ((s = WiFi.status()) != WL_CONNECTED && numtries++ < 10) {
        Serial.print(s);
        delay(500);
        Serial.print(".");
    }
    if (s != WL_CONNECTED) {
        numtries = 0;
        WiFi.begin(ssid2, password2);
        while ((s = WiFi.status()) != WL_CONNECTED && numtries++ < 10) {
            Serial.print(s);
            delay(500);
            Serial.print(".");
        }
    }
    if (s == WL_CONNECTED) {
        Serial.print("\nConnected: ");
        Serial.println(WiFi.localIP());
        Serial.println(WiFi.macAddress());
        return true;
    } else {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
}

String getTimeStringFromElapsed(int elapsedSeconds) {
    //flogToAll("getTimeStringFromElapsed " + String(elapsedSeconds));
    // Update the time
    lastSeconds += elapsedSeconds;
    // Handle overflow
    lastMinutes += lastSeconds / 60;
    lastSeconds %= 60;
    lastHours += lastMinutes / 60;
    lastMinutes %= 60;
    lastHours %= 24;  // Wrap around at 24 hours
    // Format the time string
    char timeString[9];  // HH:MM:SS\0
    //Serial.println("timeString");
    snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", lastHours, lastMinutes, lastSeconds);
    return String(timeString);
}

void checkNTP() {
    logToAll("checkNTP");
    // Update the time
    while(!timeClient.update()) {
        timeClient.forceUpdate();
    }
    // The formattedDate comes with the following format:
    // 2018-05-28T16:00:13Z
    // We need to extract date and time
    formattedDate = timeClient.getFormattedDate();
    logToAll(formattedDate);
    // Extract date
    int splitT = formattedDate.indexOf("T");
    dayStamp = formattedDate.substring(0, splitT);
    Serial.print("DATE: ");
    Serial.println(dayStamp);
    // Extract time
    timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
    Serial.print("HOUR: ");
    Serial.println(timeStamp);
    int firstColon = timeStamp.indexOf(':');
    int secondColon = timeStamp.lastIndexOf(':');
    lastHours = timeStamp.substring(0, firstColon).toInt();
    lastMinutes = timeStamp.substring(firstColon + 1, secondColon).toInt();
    lastSeconds = timeStamp.substring(secondColon + 1).toInt();
}

void displayUpdate(String time) {
#ifdef FEATHER
    canvas.fillScreen(ST77XX_BLACK);
    //canvas.fillScreen(0x0719);
    canvas.setCursor(0, 17);
    canvas.setTextColor(ST77XX_YELLOW);
    canvas.println("Chixomatic v1 ready!");
#if 0
    canvas.setTextColor(ST77XX_GREEN);
    canvas.print("Battery: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.print(lipo.cellVoltage(), 1);
    canvas.print(" V  /  ");
    canvas.print(lipo.cellPercent(), 0);
    canvas.println("%");
    canvas.setTextColor(ST77XX_BLUE);
    canvas.print("I2C: ");
    canvas.setTextColor(ST77XX_WHITE);
    canvas.println("");
#endif
    //canvas.println(dayStamp);
    canvas.println(timeStamp);
    canvas.print("Door: ");
    canvas.println((doorState == 1) ? "open" : "closed");
    // draw chicken part of the way across screen and at bottom
    canvas.drawRGBBitmap(chickX, chickY, gif_data, 64, 64);
    display.drawRGBBitmap(0, 0, canvas.getBuffer(), SC_WIDTH, SC_HEIGHT);
#else
    logToAll("displayUpdate " + time);
#endif
}

void animatePollo(bool direction) {
    if (direction) {
        Serial.print("+");
#ifdef FEATHER
        chickX += chickSpeed;
        if (chickX > SC_WIDTH)
            chickX = chickXinit;
#endif
    } else {
        Serial.print("-");
#ifdef FEATHER
        chickX -= chickSpeed;
        if (chickX < chickXinit) {
            canvas.fillRect(chickX, chickY, 64, 64, ST77XX_BLACK);
            chickX = SC_WIDTH;
        }
#endif
    }
#ifdef FEATHER
    canvas.drawRGBBitmap(chickX, chickY, gif_data, 64, 64);
    display.drawRGBBitmap(0, 0, canvas.getBuffer(), SC_WIDTH, SC_HEIGHT);
#endif
}

void listFiles() {
    Serial.println("listing files");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    
    while(file){
        Serial.print("FILE: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
}

void getLatitudeLongitude() {
  String url = "http://ip-api.com/json";
  http.begin(url);
  logToAll(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    JSONVar json = JSON.parse(payload);

    if (JSON.typeof(json) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }

    lat = (double)json["lat"];
    lon = (double)json["lon"];

    Serial.print("Latitude: ");
    Serial.println(lat);
    Serial.print("Longitude: ");
    Serial.println(lon);
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
  }
  http.end();
}

void getSunriseSunsetData(float lat, float lon) {
    //String url = "https://api.sunrisesunset.io/json?lat="  + String(lat, 6) + "&lng=" + String(lon, 6) + "&formatted=0";
    String url = "https://api.sunrise-sunset.org/json?lat="  + String(lat, 6) + "&lng=" + String(lon, 6) + "&formatted=0";
    logToAll(url);
    WiFiClientSecure client;
    client.setInsecure(); // Ignore SSL certificate validation
    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
        JSONVar json = JSON.parse(payload);
        if (JSON.typeof(json) == "undefined") {
            Serial.println("Parsing input failed!");
            return;
        }
        JSONVar results = json["results"];
        String sunrise = (const char*)results["sunrise"];
        String sunset = (const char*)results["sunset"];
        Serial.print("Sunrise: ");
        Serial.println(sunrise);
        Serial.print("Sunset: ");
        Serial.println(sunset);
    } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
}

void checkSSL() {
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client->setInsecure(); // Ignore SSL certificate validation
    HTTPClient https;
    Serial.println("[HTTPS] begin...");
    if (https.begin(*client, "https://www.howsmyssl.com/a/check")) {
      Serial.println("HTTPS is supported!");
      // Proceed with your HTTPS request here
    } else {
      Serial.println("HTTPS may not be supported");
    }
    https.end();
  }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("chixomatic!");
#ifdef FEATHER
    Serial.println("Adafruit Feather version");
    display.init(SC_HEIGHT, SC_WIDTH); // Init ST7789 240x135
    display.setRotation(3);
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);
    canvas.setFont(&FreeSans12pt7b);
    canvas.setTextColor(ST77XX_YELLOW);
    
    if (!lipo.begin()) {
        Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    } else Serial.println("Found lipo");

    Serial.print(F("Found MAX17048 with Chip ID: 0x"));
    Serial.println(lipo.getChipID(), HEX);
#endif
    Serial.println("set up motor pins");
    // set up motor pins, but turn them off
    pinMode(MOTOR1, OUTPUT);
    digitalWrite(MOTOR1, LOW);
    Serial.println("set up motor pins 2");
    pinMode(MOTOR2, OUTPUT);
    digitalWrite(MOTOR2, LOW);
    Serial.println("set up motor pins 3");
    pinMode(MOTORPWM, OUTPUT);
    digitalWrite(MOTORPWM, LOW);
    Serial.println("set up motor pins 4");
    pinMode(SWITCH, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(SWITCH), doorISR, RISING);
    pinMode(SWITCH1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SWITCH1), s1ISR, FALLING);
    pinMode(SWITCH2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SWITCH2), s2ISR, FALLING);

    Serial.println("mounting SPIFFS");
    if (SPIFFS.begin())
        Serial.println("opened SPIFFS");
    else
        Serial.println("failed to open SPIFFS");
    listFiles();

    //start a console.log file in case we crash before Webserial starts
    consLog = SPIFFS.open("/console.log", "w", true);
    if (!consLog)
        Serial.println("failed to open console log");
    if (consLog.println("Console log opened.")) {
        Serial.println("console log written");
    } else {
        Serial.println("console log write failed");
    }
    logToAll("chicken coop door controller\n");

    preferences.begin(host.c_str(), false);
    host = preferences.getString("host", host);
    logToAll("hostname: " + host + "\n");
    switchDelay = preferences.getInt("switchDelay", switchDelay);
    doorState = preferences.getInt("doorState", doorState);
    logToAll("doorState: " + String((doorState == 1) ? "open" : "closed"));

    if ((wifiConnect = initWiFi()) == true) {
        timeClient.begin();     // Initialize a NTPClient to get time
        timeClient.setTimeOffset(-25200);   // PST seconds from GMT
        checkNTP();
        getLatitudeLongitude();
        getSunriseSunsetData(lat, lon);
    }

    if (!MDNS.begin(host.c_str()))
        logToAll(F("Error starting MDNS responder"));
    else {
        logToAll("MDNS started " + host);
    }

    // Add service to MDNS-SD
    if (!MDNS.addService("http", "tcp", HTTP_PORT))
        logToAll("MDNS add service failed");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
        logToAll("index.html");
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });

    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
        logToAll("heap.html");
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });
    
    server.on("/riseset", HTTP_GET, [](AsyncWebServerRequest *request) {
        logToAll("riseset.html");
        request->send(SPIFFS, "/riseset.html", "text/html");
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        for (int i=0; i<request->params(); i++) {
            logToAll("param: " + request->getParam(i)->name() + "=" + request->getParam(i)->value());
        }
        // GET input1 value on <ESP_IP>/config?door="open/closed"
        if (request->hasParam("door")) {
            if (request->getParam("door")->value() == "open" && doorState == CLOSE) {
                logToAll("open door");
                openCloseDoor(OPEN);
            } else if (doorState == OPEN) {
                logToAll("close door");
                openCloseDoor(CLOSE);
            }
        } else if (request->hasParam("switchdelay")) {
            switchDelay = request->getParam("switchdelay")->value().toInt();
            logToAll("switch delay: " + String(switchDelay));
            preferences.putInt("switchDelay", switchDelay);
        } else if (request->hasParam("up")) {
            int bumpUp = request->getParam("up")->value().toInt();
            logToAll("bump up: " + String(bumpUp));
            moveMotor(OPEN, bumpUp);
        } else if (request->hasParam("down")) {
            int bumpDown = request->getParam("down")->value().toInt();
            logToAll("bump down: " + String(bumpDown));
            moveMotor(CLOSE, bumpDown);
        } else if (request->hasParam("hourO")) {
            if (request->hasParam("minuteO")) {
                int hourO = request->getParam("hourO")->value().toInt();
                int minuteO = request->getParam("minuteO")->value().toInt();
                logToAll("open at: " + String(hourO) + ":" + String(minuteO));
                //preferences.putInt("hourO", hourO);
                //preferences.putInt("minuteO", minuteO);
                //TBD figure out fixed times
            }        
        //} else if (request->hasParam("beforeSunrise")) {    // open X minutes before sunrise
        } else {
            logToAll("/config: invalid request");
            int params = request->params();
            for (int i = 0; i < params; i++) {
                const AsyncWebParameter* p = request->getParam(i);
                String message = p->name() + ": " + p->value();
                logToAll(message);
            }
        }
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.println("404 - Not found");
        request->send(404, "text/plain", "Not found");
    });

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    server.serveStatic("/", SPIFFS, "/");   // for e.g. script.js
    server.begin();     // Start server
    //delay(5000);

    ElegantOTA.begin(&server);
    serverStarted = true;
    logToAll("HTTP server started @" + WiFi.localIP().toString());

    WebSerial.begin(&server);    // Initialize WebSerial

    logToAll("defining reactions");
    app.onRepeat(DISPLAYUPDATEMSECS, []() {
        timeStamp = getTimeStringFromElapsed(DISPLAYUPDATEMSECS/1000);
        displayUpdate(timeStamp);
    });

    app.onRepeat(100, []() {
        handleSwitch();
    });

    if (wifiConnect) {
        app.onRepeat(43200000, []() {
            checkNTP();
        });

        app.onRepeat(100, []() {
            WebSerial.loop();
          ElegantOTA.loop();
        });
    } // wifiConnect

    logToAll("finished setup");
}

void loop() { app.tick(); }
