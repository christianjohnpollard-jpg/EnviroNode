#include <Arduino.h>
#include <Preferences.h>

Preferences prefs;

// Replace these with your actual Wi-Fi credentials
const char* WIFI_SSID = "  ";
const char* WIFI_PASS = "  ";

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("=== Wi-Fi Provisioning ===");

  // Open the "wifi" namespace in NVS
  prefs.begin("wifi", false);

  // Write SSID and password
  prefs.putString("ssid", WIFI_SSID);
  prefs.putString("pass", WIFI_PASS);

  // Close the namespace
  prefs.end();

  Serial.println("Wi-Fi credentials successfully written to NVS.");
  Serial.println("You can now flash your main firmware.");
  

}

void loop() {
  // Nothing to do here
}
