// Replace with your network credentials
const char* ssid = "ssid";
const char* password = "wifi-password";
const char* mqtt_server = "mqtt-server";

const char* logTopic = "switch1/log";
const char* consumeTopic = "switch1/command";
const char* stateTopic = "switch1/state";

const int numberOfSwitches = 4;
const int switchPins[] = { D1, D2, D3, D4 };

