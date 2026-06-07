#include <SPI.h>
#include <WiFi101.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <PubSubClient.h>
#include <DHT.h>

// Sensor config
#define DHTPIN 7
#define DHTTYPE DHT11

// WiFi config
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// MQTT broker config
const char *MQTT_BROKER = "";
const int MQTT_PORT = 8883;
const char *MQTT_TOPIC = "devices/mkr1000/climate";
const char *MQTT_USER = "";
const char *MQTT_PASS = "";

// EMQX Serverless DigiCert Global Root G2
static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

const unsigned long PUBLISH_INTERVAL_MS = 60UL * 1000UL; // 1 minute

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
BearSSLClient sslClient(wifiClient);
PubSubClient mqttClient(sslClient);

unsigned long getTime()
{
  return WiFi.getTime();
}

void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED)
  {
    Serial.print(".");
    delay(5000);
  }
  Serial.println(". WiFi connected.");
}

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    connectWiFi();
    String clientId = "mkr1000-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT as ");
    Serial.println(clientId);
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
    {
      Serial.println("MQTT connected.");
    }
    else
    {
      Serial.print("Failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), retrying in 5s.");
      delay(5000);
    }
  }
}

bool readClimate(float &humidity, float &tempC, float &heatIndexC)
{
  humidity = dht.readHumidity();
  tempC = dht.readTemperature();
  if (isnan(humidity) || isnan(tempC))
  {
    return false;
  }
  heatIndexC = dht.computeHeatIndex(tempC, humidity, false);
  return true;
}

bool publishClimate(float humidity, float tempC, float heatIndexC)
{
  unsigned long uptimeSec = millis() / 1000UL;
  int rssi = WiFi.RSSI();
  unsigned long timestamp = getTime();

  if (timestamp == 0)
  {
    Serial.println("No time sync yet. Skipping publish.");
    return false;
  }

  // Payload shape:
  // {
  //   "timestamp": 1690000000,
  //   "temperature": 23.50,
  //   "humidity": 55.00,
  //   "heat_index": 24.10,
  //   "uptime_s": 3600,
  //   "rssi_dbm": -67,
  // }
  char payload[192];
  snprintf(payload, sizeof(payload),
           "{"
           "\"timestamp\":%lu,"
           "\"temperature\":%.2f,"
           "\"humidity\":%.2f,"
           "\"heat_index\":%.2f,"
           "\"uptime_s\":%lu,"
           "\"rssi_dbm\":%d"
           "}",
           timestamp, tempC, humidity, heatIndexC,
           uptimeSec, rssi);

  Serial.print("Publishing to topic ");
  Serial.print(MQTT_TOPIC);
  Serial.print(": ");
  Serial.println(payload);
  return mqttClient.publish(MQTT_TOPIC, payload);
}

void setup()
{
  Serial.begin(9600);
  while (!Serial)
  {
    ;
  }

  if (!ECCX08.begin())
  {
    Serial.println("ECCX08 not found.");
    while (1)
    {
      ;
    }
  }

  ArduinoBearSSL.onGetTime(getTime);
  sslClient.setEccSlot(0, (const byte *)CA_CERT, sizeof(CA_CERT));

  dht.begin();
  connectWiFi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connectMQTT();
}

void loop()
{
  if (!mqttClient.connected())
  {
    connectMQTT();
  }
  mqttClient.loop();

  static unsigned long lastPublish = 0;
  if (millis() - lastPublish < PUBLISH_INTERVAL_MS)
  {
    return;
  }
  lastPublish = millis();

  float humidity, tempC, heatIndexC;
  if (!readClimate(humidity, tempC, heatIndexC))
  {
    Serial.println("Sensor read failed. Skipping publish.");
    return;
  }

  if (!publishClimate(humidity, tempC, heatIndexC))
  {
    Serial.println("Publish failed.");
  }
}