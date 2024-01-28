
#include <WiFi.h>
#include <PubSubClient.h>

// Replace the next variables with your SSID/Password combination
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// Add your MQTT Broker IP address, example:
//const char* mqtt_server = "192.168.1.144";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP_ADDRESS";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Used Pins
const int DHT11_PIN = 39;      
const int RELAY_PIN = 38;
const int RAIN_PIN = 48;
const int MOIST_PIN = A1;
const int RAIN_POWER = 47;

float temperature = 0;
float humidity = 0;

//input sensor
#define DHTTYPE DHT11
DHT dht(DHT11_PIN, DHTTYPE);

//define pump state
bool pump_state = false;

void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(RAIN_PIN, INPUT);
  pinMode(MOIST_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RAIN_POWER, OUTPUT);
  digitalWrite(RAIN_POWER, LOW);
  pump_state = false;
  dht.begin();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/pump") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(RELAY_PIN, HIGH);
      pump_state = true;
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(RELAY_PIN, LOW);
      pump_state = false;
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/pump");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    //read telemetry data
    float h = dht.readHumidity();
    int t = dht.readTemperature();

    //read moist telemetry data
    int moist_analog = analogRead(MOIST_PIN);
    int _moisture = ( 100 - ( (moist_analog/4095.00) * 100 ) );

    JSONVar payloadObject;    
    payloadObject["Humidity"] = h;
    payloadObject["Temperature"] = t;
    payloadObject["Soil moisture"] = _moisture;
    payload = JSON.stringify(payloadObject);  
    client.publish("esp32/sensor", payload);

    if(_moisture > 60 & pump_state){
      client.publish("esp32/pump", "off");
    }

    bool rain_read = is_raining();
    if(rain_read & pump_state)
      client.publish("esp32/pump", "off");
    delay(5000);
  }
}

bool is_raining(){
  digitalWrite(RAIN_POWER, HIGH);
  delay(10);
  bool val = digitalRead(RAIN_PIN);
  digitalWrite(RAIN_POWER, LOW);
  return !val;
}
