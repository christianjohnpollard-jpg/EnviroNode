
extern "C" {
  #include <nvs_flash.h>
  #include <nvs.h>
}

//=== LIBRARIES ===
#include <Wire.h>              // I2C sensors
#include <WiFi.h>              // WiFi connection
#include <WiFiClientSecure.h>  // TLS for AWS IoT
#include <PubSubClient.h>      // Call TLS keys for AWS from NVS
#include <Arduino_JSON.h>       // JSON encoding
#include <Adafruit_TestBed.h>  // Define I2C ports
extern Adafruit_TestBed TB;
#include <Adafruit_SHT4x.h>    // Temp & Humidity library
#include <Adafruit_ADS1X15.h>  // ADC library
#include <stdint.h> 
#include <Preferences.h>
#include <time.h>
#include <SparkFun_Qwiic_Buzzer_Arduino_Library.h>


// === MQ-9 CONSTANTS ===
const float R0 = 36618.43;     // your calibrated clean-air value
const float Vc = 5;       // MQ-9 powered from ADS1115/STEMMA
const float RL = 10000.0;   // typical MQ-9 module load resistor

// === GLOBAL SENSOR OBJECTS ===
#define DEFAULT_I2C_PORT &Wire
Adafruit_ADS1115 ads;
Adafruit_SHT4x sht4;
QwiicBuzzer buzzer;
Preferences prefs;
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// === VARIABLES ===
String wifiSSID;
String wifiPASS;
unsigned long lastPublish = 0;
const unsigned long publishInterval = 60000; // 60 seconds
const char* DEVICE_ID = "qtpy-s3-01";


// === BUFFERS FOR NVS VALUES ===
char *awsEndpoint = nullptr;
char *awsThingName = nullptr;
char *awsCaCert = nullptr;
char *awsClientCert = nullptr;
char *awsPrivateKey = nullptr;

// === WIFI CONNECTION FUNCTION ===
void loadWifiFromNVS() {
  prefs.begin("wifi", true);  // open namespace in read-only mode

  wifiSSID = prefs.getString("ssid", "");
  wifiPASS = prefs.getString("pass", "");

  prefs.end();

  Serial.println("Loaded Wi-Fi credentials from NVS:");
  Serial.println("SSID: " + wifiSSID);
  // Don’t print password in production, but fine for debugging
}

// === HELPER FUNCTION TO LOAD NVS BLOBS ====
bool loadBlob(nvs_handle_t handle, const char* key, char* buffer, size_t bufferSize) {
  size_t requiredSize = 0;

  // First call: get required size
  if (nvs_get_blob(handle, key, NULL, &requiredSize) != ESP_OK) {
    Serial.printf("Failed to get size for key: %s\n", key);
    return false;
  }

  if (requiredSize > bufferSize) {
    Serial.printf("Buffer too small for key: %s\n", key);
    return false;
  }

  // Second call: actually read the blob
  if (nvs_get_blob(handle, key, buffer, &requiredSize) != ESP_OK) {
    Serial.printf("Failed to read blob for key: %s\n", key);
    return false;
  }

  return true;
}

bool loadAwsCredentials() {
    Serial.println("=== Loading AWS credentials from NVS ===");

    nvs_handle_t handle;
    esp_err_t err = nvs_open("tls", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        Serial.printf("Failed to open NVS namespace 'tls': %d\n", err);
        return false;
    }

    //
    // 1. Load AWS endpoint
    //
    size_t endpointSize = 0;
    err = nvs_get_str(handle, "endpoint", NULL, &endpointSize);
    if (err != ESP_OK) {
        Serial.println("Failed to get endpoint size");
        nvs_close(handle);
        return false;
    }

    char *endpointBuf = (char *)malloc(endpointSize);
    if (!endpointBuf) {
        Serial.println("Failed to allocate memory for endpoint");
        nvs_close(handle);
        return false;
    }

    err = nvs_get_str(handle, "endpoint", endpointBuf, &endpointSize);
    if (err != ESP_OK) {
        Serial.println("Failed to read endpoint");
        free(endpointBuf);
        nvs_close(handle);
        return false;
    }

    Serial.print("Endpoint loaded: ");
    Serial.println(endpointBuf);

    //
    // 2. Load Thing Name
    //
    size_t thingSize = 0;
    err = nvs_get_str(handle, "thing_name", NULL, &thingSize);
    if (err != ESP_OK) {
        Serial.println("Failed to get thing_name size");
        free(endpointBuf);
        nvs_close(handle);
        return false;
    }

    char *thingBuf = (char *)malloc(thingSize);
    if (!thingBuf) {
        Serial.println("Failed to allocate memory for thing_name");
        free(endpointBuf);
        nvs_close(handle);
        return false;
    }

    err = nvs_get_str(handle, "thing_name", thingBuf, &thingSize);
    if (err != ESP_OK) {
        Serial.println("Failed to read thing_name");
        free(endpointBuf);
        free(thingBuf);
        nvs_close(handle);
        return false;
    }

    Serial.print("Thing Name loaded: ");
    Serial.println(thingBuf);

    //
    // 3. Load CA certificate
    //
    size_t caSize = 0;
    err = nvs_get_blob(handle, "ca_cert", NULL, &caSize);
    if (err != ESP_OK) {
        Serial.println("Failed to get ca_cert size");
        free(endpointBuf);
        free(thingBuf);
        nvs_close(handle);
        return false;
    }

    char *caBuf = (char *)malloc(caSize + 1);
    if (!caBuf) {
        Serial.println("Failed to allocate memory for ca_cert");
        free(endpointBuf);
        free(thingBuf);
        nvs_close(handle);
        return false;
    }

    err = nvs_get_blob(handle, "ca_cert", caBuf, &caSize);
    if (err != ESP_OK) {
        Serial.println("Failed to read ca_cert");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        nvs_close(handle);
        return false;
    }

    caBuf[caSize] = '\0';
    Serial.println("CA cert loaded OK");

    //
    // 4. Load client certificate
    //
    size_t certSize = 0;
    err = nvs_get_blob(handle, "client_cert", NULL, &certSize);
    if (err != ESP_OK) {
        Serial.println("Failed to get client_cert size");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        nvs_close(handle);
        return false;
    }

    char *certBuf = (char *)malloc(certSize + 1);
    if (!certBuf) {
        Serial.println("Failed to allocate memory for client_cert");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        nvs_close(handle);
        return false;
    }

    err = nvs_get_blob(handle, "client_cert", certBuf, &certSize);
    if (err != ESP_OK) {
        Serial.println("Failed to read client_cert");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        free(certBuf);
        nvs_close(handle);
        return false;
    }

    certBuf[certSize] = '\0';
    Serial.println("Client cert loaded OK");

    //
    // 5. Load private key
    //
    size_t keySize = 0;
    err = nvs_get_blob(handle, "client_key", NULL, &keySize);
    if (err != ESP_OK) {
        Serial.println("Failed to get client_key size");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        free(certBuf);
        nvs_close(handle);
        return false;
    }

    char *keyBuf = (char *)malloc(keySize + 1);
    if (!keyBuf) {
        Serial.println("Failed to allocate memory for client_key");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        free(certBuf);
        nvs_close(handle);
        return false;
    }

    err = nvs_get_blob(handle, "client_key", keyBuf, &keySize);
    if (err != ESP_OK) {
        Serial.println("Failed to read client_key");
        free(endpointBuf);
        free(thingBuf);
        free(caBuf);
        free(certBuf);
        free(keyBuf);
        nvs_close(handle);
        return false;
    }

    keyBuf[keySize] = '\0';
    Serial.println("Private key loaded OK");

    //
    // 6. Store globally
    //
    awsEndpoint   = endpointBuf;
    awsThingName  = thingBuf;
    awsCaCert     = caBuf;
    awsClientCert = certBuf;
    awsPrivateKey = keyBuf;

    nvs_close(handle);

    Serial.println("=== AWS credentials loaded successfully ===");
    return true;
}

// === MQ-9 CO CURVE FUNCTION ===
float mq9_co_ppm(float ratio) {
    const float m = -0.77;
    const float b = 0.35;

    float log_ratio = log10(ratio);
    float log_ppm = (log_ratio - b) / m;

    return pow(10, log_ppm);
}


// === CREATE JSON STRING FOR AWS ===
String buildTelemetryJson(float temp_c, float humidity, int mq9_raw, float mq9_ppm, int rssi) {

  // Round floats to safe precision
  float temperature_rounded = roundf(temp_c * 10) / 10.0f;       // 1 decimal place
  float humidity_rounded    = roundf(humidity * 10) / 10.0f;     // 1 decimal place
  float mq9_ppm_rounded     = roundf(mq9_ppm * 100) / 100.0f;    // 2 decimal places

  JSONVar root;
  root["device_id"] = DEVICE_ID;
  root["timestamp"] = (long) time(nullptr);
  root["room"] = "livingroom";

  // --- SENSORS ---
  JSONVar sensors;
  sensors["temperature_c"] = String(temperature_rounded, 1);  // force 1 decimal
  sensors["humidity_pct"]  = String(humidity_rounded, 1);     // force 1 decimal
  sensors["mq9_raw"]       = mq9_raw;
  sensors["mq9_ppm"]       = String(mq9_ppm_rounded, 2);      // force 2 decimals
  root["sensors"] = sensors;

  // --- META ---
  JSONVar meta;
  meta["firmware"] = "1.0.0";
  meta["rssi"] = rssi;
  root["meta"] = meta;

  return JSON.stringify(root);
}

void publishTelemetry(float temp_c, float humidity_pct, int mq9_raw, float mq9_ppm, int rssi) {
  String payload = buildTelemetryJson(
    temp_c,
    humidity_pct,
    mq9_raw,
    mq9_ppm,
    rssi
  );

  String topic = String("sensors/") + DEVICE_ID + "/telemetry";

  mqttClient.publish(topic.c_str(), payload.c_str());

  Serial.print("Published to topic: ");
  Serial.println(topic);
  Serial.println(payload);
}


// === MQTT RECONNECT HELPER ===
void ensureMqttConnection() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected — attempting reconnect...");

    while (!mqttClient.connected()) {
      if (mqttClient.connect(awsThingName)) {
        Serial.println("MQTT reconnected!");
      } else {
        Serial.print("Reconnect failed, state=");
        Serial.println(mqttClient.state());
        delay(2000);
      }
    }
  }
}

void syncTime() {
  Serial.println("Starting NTP sync...");

  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

  time_t now = time(nullptr);
  int retries = 0;

  while (now < 1700000000 && retries < 20) {  // 1700000000 = year 2023+
    Serial.print(".");
    delay(500);
    now = time(nullptr);
    retries++;
  }

  Serial.println();
  if (now < 1700000000) {
    Serial.println("Failed to sync time via NTP");
  } else {
    Serial.print("Time synced: ");
    Serial.println(ctime(&now));
    }
  }



//=== SETUP ===
void setup() { 
  Serial.begin(115200);

  //buzzer.off();
  //// Wait for Serial port to open
  //while (!Serial) {
  //  delay(2000);
  //}

  // 1. Load Wi-Fi credentials from NVS
  loadWifiFromNVS();

  // 2. Start Wi-Fi connection
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  // 3. WAIT HERE until Wi-Fi is connected
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(" connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // WiFi connected successfully
  syncTime();

  // 4. Load AWS credentials
  loadAwsCredentials();

  // 5. Apply TLS materials
  secureClient.setCACert(awsCaCert);
  secureClient.setCertificate(awsClientCert);
  secureClient.setPrivateKey(awsPrivateKey);

  mqttClient.setServer(awsEndpoint, 8883);

  Serial.print("Connecting to AWS IoT...");
  if (mqttClient.connect(awsThingName)) {
      Serial.println("connected!");
  } else {
    Serial.println("failed!");
    Serial.print("MQTT state: ");
    Serial.println(mqttClient.state());
  }


  #if defined(ARDUINO_ADAFRUIT_QTPY_ESP32S2) || \
    defined(ARDUINO_ADAFRUIT_QTPY_ESP32S3_NOPSRAM) || \
    defined(ARDUINO_ADAFRUIT_QTPY_ESP32S3_N4R2) || \
    defined(ARDUINO_ADAFRUIT_QTPY_ESP32_PICO)
  // ESP32 is kinda odd in that secondary ports must be manually
  // assigned their pins with setPins()!
  Wire1.setPins(SDA1, SCL1);
#endif

  Wire1.begin();

  //Setup ADS 1115 
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  

  if(!ads.begin(0x48, &Wire1)) {
    Serial.print("ADS not detected....");
    while(1);
  }

  //Setup sht4
  sht4.setPrecision(SHT4X_HIGH_PRECISION); //three different precisions High, Mid, Low

  if(!sht4.begin(&Wire1)) {
    Serial.print("SHT4 not detected....");
    while(1);
  }

  //Setup Qwiic Buzzer
  if (buzzer.begin(0x34, Wire1) == false) {
    Serial.println("Buzzer not detected....");
    while (1);
  }

}


// === LOOP ===
void loop() {

  // 1. Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi dropped — reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;  // skip MQTT until WiFi is back
  }

  // 2. Maintain MQTT connection
  ensureMqttConnection();

  // 3. Let the MQTT client process PINGREQ/PINGRESP
  mqttClient.loop();

  // 4. Publish every minute
  unsigned long now = millis();
  if (now - lastPublish >= publishInterval) {
    lastPublish = now;

    Serial.println("Preparing telemetry...");

    // === MQ9 ADC ===
    int16_t raw = ads.readADC_SingleEnded(0);
    float voltage = raw * 0.0001875;  // ADS1115 gain = 2/3
    float Rs = RL * ((Vc / voltage) - 1.0);
    float ratio = Rs / R0;
    float co_ppm = mq9_co_ppm(ratio);

    if (co_ppm > 50.00 ) {
      Serial.println("Dangerous levels of CO detected");
      buzzer.configureBuzzer(2730, 60000, SFE_QWIIC_BUZZER_VOLUME_MAX); // frequency: 2.73KHz, duration: 100ms, volume: MAX
      buzzer.on();
    }

    // === SHT4x ===
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);

    // === WiFi RSSI ===
    int rssi = WiFi.RSSI();

    // === Publish ===
    Serial.println("Publishing telemetry...");
    publishTelemetry(
      temp.temperature,
      humidity.relative_humidity,
      raw,
      co_ppm,
      rssi
    );
  }
}
