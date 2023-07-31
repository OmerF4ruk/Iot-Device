#include <Arduino.h>
#include <DHT.h>
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <EEPROM.h>

#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define sendDelay 900000
#define checkDelay 30000
const int probePin = 34;
const int ntcPin = 35;
const int rLed = 27;
const int gLed = 25;
const int bLed = 26;


bool firstStart = true;
int i = 0;
int statusCode;
char jsonOutPut[128];
const char* ssid = "Default SSID";
const char* passphrase = "Default passord";
String st;
String content;
String esid;
String epass = "";
String ip="http://192.168.1.2:3500/data";
long previousMillisSend = 0;
long previousMillis = 0;

void reset(void);
double ntc_calc(void);
double ntc_calc(void);
bool testWifi(void);
void getData(void);
void launchWeb(void);
void sendData(void);
void setupAP(void);
WebServer server(80);
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(rLed, OUTPUT);
  pinMode(gLed, OUTPUT);
  pinMode(bLed, OUTPUT);

  Serial.println();
  Serial.println("Disconnecting current wifi connection");
  WiFi.disconnect();
  EEPROM.begin(512);  //Initialasing EEPROM
  delay(10);
  pinMode(15, INPUT);
  
  pinMode(23, INPUT);
  
  //---------------------------------------- Read eeprom for ssid and pass
  Serial.println("Reading EEPROM ssid");


  for (int i = 0; i < 32; ++i) {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");

  for (int i = 32; i < 64; ++i) {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);


  WiFi.begin(esid.c_str(), epass.c_str());
  digitalWrite(rLed, HIGH);
  digitalWrite(bLed, HIGH);
  digitalWrite(gLed, HIGH);
  delay(5000);
  digitalWrite(rLed, LOW);
  digitalWrite(bLed, LOW);
  delay(5000);
}

void loop() {
  
  
  long currentMillis = millis();
  if (((long)(currentMillis - previousMillis) >= checkDelay) || firstStart) {
    
    getData();
    Serial.println(firstStart);
    Serial.println(jsonOutPut);


    if ((WiFi.status() == WL_CONNECTED)) {
       Serial.println(firstStart);
      long currentMillisSend = millis();
      Serial.println(currentMillisSend);
      if (((long)(currentMillisSend - previousMillisSend) >= sendDelay) || firstStart) {
        sendData();
        Serial.println(firstStart+"aaa");
        previousMillisSend = currentMillisSend;
      }


    } else {
      Serial.println("else");
    }
 
    if (testWifi() && (digitalRead(15) != 1)) {
      previousMillis = currentMillis;
      Serial.println(" connection status positive");
      firstStart = false;
      return;
    } else {
      Serial.println("Connection Status Negative / D15 HIGH");
      Serial.println("Turning the HotSpot On");
      launchWeb();
      setupAP();  // Setup HotSpot
    }

    Serial.println();
    Serial.println("Waiting.");

    while ((WiFi.status() != WL_CONNECTED)) {
      Serial.print(".");
      delay(100);
      server.handleClient();
    }
    delay(1000);
    firstStart = false;
    previousMillis = currentMillis;
  }
}

void sendData() {
  digitalWrite(bLed, HIGH);
  HTTPClient client;
  client.begin(ip);
  client.addHeader("Content-Type", "application/json");

  int httpCode = client.POST(String(jsonOutPut));
  if (httpCode > 0) {
    String payload = client.getString();
    Serial.print("başarılı ");
    Serial.println("\nStatusCode: " + String(httpCode));
    Serial.println(payload);
  } else {
  	Serial.println("başarısız"); 
    digitalWrite(rLed, HIGH);
  }
  delay(5000);

  digitalWrite(bLed, LOW);
  digitalWrite(rLed, LOW);
  client.end();
}

void getData(void) {
  char output[128];
  DynamicJsonDocument doc(1024);
  doc["device_id"] = "13212223";
  doc["air_temperature"] = dht.readTemperature();
  doc["air_humidity"] = dht.readHumidity();
  doc["soil_temperature"] = ntc_calc();
  doc["soil_humidity"] = probe_calc();
  serializeJson(doc, jsonOutPut);
}

int probe_calc(void) {
  int value = analogRead(probePin);
  int humidity = map(value, 530, 4096, 100, 0);
  return humidity;
}
double ntc_calc(void) {


  const double VCC = 5.5;              // NodeMCU on board 3.3v vcc
  const double R2 = 10000;             // 10k ohm series resistor
  const double adc_resolution = 4096;  // 10-bit adc

  const double A = 0.001129148;  // thermistor equation parameters
  const double B = 0.000234125;
  const double C = 0.0000000876741;

  double Vout, Rth, temperature, adc_value;
  adc_value = analogRead(ntcPin);
  Vout = (adc_value * VCC) / adc_resolution;
  Rth = (VCC * R2 / Vout) - R2;


  // Temperature in Kelvin = 1 / (A + B[ln(R)] + C[ln(R)]^3)
  // where A = 0.001129148, B = 0.000234125 and C = 8.76741*10^-8
  temperature = (1 / (A + (B * log(Rth)) + (C * pow((log(Rth)), 3))));
  temperature = temperature - 273;  
  return temperature;
}
bool testWifi(void) {
  int c = 0;
  //Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchWeb() {
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void setupAP(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i) {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("HerbDevice", "");
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  launchWeb();
  Serial.println("over");
}
void reset(){
  Serial.println("clearing eeprom");
  for (int i = 0; i < 96; ++i) {
    EEPROM.write(i, 0);
    }
  EEPROM.commit();
  Serial.println("Done");
  delay(1000);
  ESP.restart();
}

void createWebServer() {
  {
    digitalWrite(bLed, HIGH);
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Wifi Ayarlari";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += "Cihaz No:";
      content += "13212223";
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><label> Sifre: <input name='pass' length=64><input type='submit'></form>";
      
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      String ip = server.arg("ip");
      Serial.println(ip);
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");

        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i) {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom ssid: ");
        for (int i = 0; i < qpass.length(); ++i) {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
       
        EEPROM.commit();

        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
    });
    digitalWrite(bLed, LOW);
  }
  
}


