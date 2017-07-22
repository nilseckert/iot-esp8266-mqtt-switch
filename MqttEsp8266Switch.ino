#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "Config.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// How many leds are in the strip?

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

const bool TRACE = false;
const bool DEBUG = true;
const bool TEST_RELAIS_ON_INIT = false;
const bool STATE_ON = LOW;
const bool STATE_OFF = HIGH;

const int numberOfSwitches = 2;
int switchPins[] = { D1, D2 };
unsigned long turnOffTimes[numberOfSwitches];

void connectWifi() {
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(F(""));
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  handleBody(payload);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      logMessage("info", "Started");
      
      // ... and resubscribe
      client.subscribe(consumeTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }  
}

void handleBody(byte* payload) {
  StaticJsonBuffer<150> newBuffer;
  JsonObject& root = newBuffer.parseObject(payload);

  JsonVariant id = root["id"];
  JsonVariant stateVariant = root["state"];
  JsonVariant durationVariant = root["duration"];
  
  if (id.success() == false) {
    sendError(F("no switch id specified"));
    return;
  }

  int switchNumber = id.as<int>();

  if (switchNumber >= numberOfSwitches) {
    sendError(F("id is invalid"));
    return;
  }

  if (stateVariant.success() == false) {
    sendError(F("no state specified"));
    return;
  }

  bool state = stateVariant.as<bool>();
  
  
  
  
  if (durationVariant.success()) {
    unsigned long duration = durationVariant.as<unsigned long>();  
    Serial.printf("contains duration: %d\n", duration);
      
    if (duration > 0) {
      updateDuration(switchNumber, duration);  
    } else {
      state = false;
    }
  } else {
    Serial.println(F("no duration detected"));
  }
  
  if (state == false) {
    switchOff(switchNumber);
    return;
  }

  switchOn(switchNumber);
}

void sendError(String msg) {
  Serial.println(msg);
}

void switchOff(int id) {
  changeSwitchState(id, STATE_OFF); 
}

void switchOn(int id) {
  changeSwitchState(id, STATE_ON);
}

void updateDuration(int id, unsigned long duration) {
  unsigned long endTime = millis() + duration;

  Serial.printf("updateDuration with value %d\n", duration);
  
  turnOffTimes[id] = endTime;
}

void changeSwitchState(int id, bool state) {
  int pin = mapIdToPin(id);
  
  bool isOn = (state == STATE_ON);
  
  digitalWrite(pin, state);

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = id;
  root["state"] = isOn;

  publishMessage(stateTopic, root);
  
  Serial.printf("info: id: %d, state: %d\n", id, isOn);
}

int mapIdToPin(int id) {
  return switchPins[id];
}

void setup() {
  Serial.begin(115200);
  connectWifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  for (int i = 0; i < numberOfSwitches; ++i) {
    initRelay(i);
  }

}

void initRelay(int id) {
  int pin = mapIdToPin(id);

  char message[128];
  sprintf(message, "Initializing switch id: %d pin: %d", id, pin);
  
  logDebug(F("logging init relay message"));  
  logMessage("info", message);
  
  pinMode(pin, OUTPUT);
  digitalWrite(pin, STATE_OFF);
  
  if (TEST_RELAIS_ON_INIT) { 
    digitalWrite(pin, STATE_ON);
  
    delay(500);
  
    digitalWrite(pin, STATE_OFF);  
  
    delay(500);
  }

  logDebug(F("Init Relay done."));
}

int iteration = 0;
unsigned long lastRun = 0;

void turnOffIfRequired() {
  iteration++;

  if (iteration % 100 != 0) {
    return; 
  }

  unsigned long now = millis();
  if (now - lastRun > 500) {
    logTrace(F("processing turnOfIfRequired"));
    
    for (int i = 0; i < numberOfSwitches; ++i) {
      unsigned long value = turnOffTimes[i];

      if (TRACE) {
        Serial.printf("now: %d - i: %d - value: %d\n", now, i, value);
      }
      
      if (value == 0) {
        continue;
      }

      if (value < now) {
        logDebug(F("Switch needs to be powered off. time exeeded."));
        char message[50];
        sprintf(message, "Duration for id %d exeeded. Turning off.", i);
                
        logMessage("info", message);
        
        logDebug(F("Switchign off"));
        switchOff(i);  

        logDebug(F("resetting turnOfTimes to zero"));
        turnOffTimes[i] = 0;
      }
    }
    iteration = 0;
    lastRun = now;
  }
}

void logMessage(char* category, char* message) {
  logTrace(F("starting logMessage"));
  
  Serial.printf("%s: %s\n", category, message);

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["category"] = category;
  root["message"] = message;
  
  publishMessage(logTopic, root);

  logTrace(F("exiting logMessage"));
}

void publishMessage(const char* topic, JsonObject& object) {
  char message[256];
  object.printTo(message, 256);

  client.publish(topic, message);
}

void logDebug(String message) {
  if (DEBUG) {
    Serial.println(message);
  }
}

void logTrace(String message) {
  if (TRACE) {
    Serial.println(message);
  }
}

void loop() {
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
  turnOffIfRequired();
}
