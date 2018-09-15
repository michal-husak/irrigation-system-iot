#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char *ssid = "[YOUR_SSID]";
const char *password = "[YOUR_PASSWORD]";
const char* www_username = "[YOUR_USERNAME]";
const char* www_password = "[YOUR_PASSWORD]";
int tick = 5000;
unsigned long currentTime = 0;

#define VALVE_PIN 5 // D1
#define PUMP_PIN 4  // D2
#define WATER_LEVEL_SENSOR_PIN 14 // D5

ESP8266WebServer server(7999);

void handleNotFound() {
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ ) {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}

	server.send ( 404, "text/plain", message );
}

void setup(void) {
  
	pinMode(VALVE_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(WATER_LEVEL_SENSOR_PIN, INPUT_PULLUP);
  
  digitalWrite(VALVE_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  currentTime = millis();
  
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.println("");

	// Wait for connection
	//while ( WiFi.status() != WL_CONNECTED ) {
	//	delay ( 500 );
	//	Serial.print(".");
	//}

  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  ArduinoOTA.begin();

	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	if (MDNS.begin("esp8266")) {
		Serial.println("MDNS responder started");
	}
  
	server.on("/all", []() {

    addHeaders();
    
    if(handleOptionsRequest())
      return;

    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
      
    byte pump = digitalRead(PUMP_PIN);
    byte valve = digitalRead(VALVE_PIN);
    int waterLevel = digitalRead(WATER_LEVEL_SENSOR_PIN);
    String message = "{";
    message += "\"pump\": ";
    message += String(pump) + ", ";
    message += "\"valve\": ";
    message += String(valve) + ", ";
    message += "\"waterLevel\": ";
    message += waterLevel;
    message += "}";
    server.send(200, "application/json", message);
    
	});

  server.on("/pump", []() {
    addHeaders();
    if(handleOptionsRequest())
      return;
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    handleEndpoint("pump", PUMP_PIN);
  });

  server.on("/valve", []() {
    addHeaders();
    if(handleOptionsRequest())
      return;
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    handleEndpoint("valve", VALVE_PIN);
  });

	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("HTTP server started");
}

void loop ( void ) {
	server.handleClient();
  
  if(millis() > currentTime + tick) {
    currentTime = millis();
    int waterLevel = digitalRead(WATER_LEVEL_SENSOR_PIN);
    // if the water level hits the sensor close the valve
    if(waterLevel == HIGH) {
      digitalWrite(VALVE_PIN, LOW);
    }
  }
}

bool handleOptionsRequest() {
  if(server.method() == HTTP_OPTIONS) {  
    server.send(200, "application/json", "");
    return true;
  }
  return false;
}

void addHeaders() {
  server.sendHeader("Access-Control-Allow-Methods", "GET,PUT,POST,DELETE,PATCH,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
  server.sendHeader("Access-Control-Allow-Origin", "*");
}

void handleEndpoint(String pinName, byte pin) {
  int responseCode = 200;
  String message = "";
  
  if(server.method() == HTTP_GET) {
    message = getJSONFromPin(pinName, digitalRead(pin));
  } else if(server.method() == HTTP_PUT) {
    message = getJSONFromPin(pinName, togglePin(pin));
  } else {
    responseCode = 400;
    message = "{\"message\": \"Bad Request. Only GET and PUT allowed\"}";
  }
  server.send(responseCode, "application/json", message);
}

byte togglePin(byte pin) {
  byte value = digitalRead(pin);
  value = value ? LOW : HIGH;
  digitalWrite(pin, value);
  return value;
}

String getJSONFromPin(String pinName, byte pinValue){
  String message = "{";
  message += "\"" + pinName + "\": ";
  message += String(pinValue);
  message += "}";
  return message;
}
