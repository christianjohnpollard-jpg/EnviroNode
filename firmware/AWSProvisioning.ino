// AWSProvisioning_Template.ino
//
// This sketch writes AWS IoT credentials into NVS on the ESP32.
//
// After provisioning, flash your actual firmware that reads from NVS.

extern "C" {
  #include <nvs_flash.h>
  #include <nvs.h>
}

#include <Arduino.h>

// ============================================================================
// 1) PLACEHOLDER CERTIFICATES — DO NOT COMMIT REAL PEM CONTENT
// ============================================================================
// Replace these placeholders ONLY in your local, untracked copy.
// Keep BEGIN/END lines intact when inserting real PEMs.

static const char *AWS_CA_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
REPLACE_WITH_YOUR_AWS_ROOT_CA
-----END CERTIFICATE-----
)EOF";

static const char *AWS_CLIENT_CERT_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
REPLACE_WITH_YOUR_DEVICE_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

static const char *AWS_CLIENT_KEY_PEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
REPLACE_WITH_YOUR_PRIVATE_KEY
-----END RSA PRIVATE KEY-----
)EOF";

// ============================================================================
// 2) ENDPOINT + THING NAME — PLACEHOLDERS ONLY
// ============================================================================
static const char *AWS_ENDPOINT   = "YOUR_ENDPOINT_HERE.amazonaws.com";
static const char *AWS_THING_NAME = "YOUR_THING_NAME_HERE";

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("=== AWS NVS Provisioning (Template) ===");

  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.println("Erasing NVS...");
    nvs_flash_erase();
    nvs_flash_init();
  }

  nvs_handle_t h;
  err = nvs_open("tls", NVS_READWRITE, &h);
  if (err != ESP_OK) {
    Serial.printf("Failed to open NVS namespace 'tls': %d\n", err);
    return;
  }

  // Write certs/keys as blobs
  err = nvs_set_blob(h, "ca_cert", AWS_CA_PEM, strlen(AWS_CA_PEM) + 1);
  if (err != ESP_OK) Serial.printf("nvs_set_blob(ca_cert) failed: %d\n", err);

  err = nvs_set_blob(h, "client_cert", AWS_CLIENT_CERT_PEM, strlen(AWS_CLIENT_CERT_PEM) + 1);
  if (err != ESP_OK) Serial.printf("nvs_set_blob(client_cert) failed: %d\n", err);

  err = nvs_set_blob(h, "client_key", AWS_CLIENT_KEY_PEM, strlen(AWS_CLIENT_KEY_PEM) + 1);
  if (err != ESP_OK) Serial.printf("nvs_set_blob(client_key) failed: %d\n", err);

  // Write endpoint + thing name as strings
  err = nvs_set_str(h, "endpoint", AWS_ENDPOINT);
  if (err != ESP_OK) Serial.printf("nvs_set_str(endpoint) failed: %d\n", err);

  err = nvs_set_str(h, "thing_name", AWS_THING_NAME);
  if (err != ESP_OK) Serial.printf("nvs_set_str(thing_name) failed: %d\n", err);

  // Commit changes
  err = nvs_commit(h);
  if (err != ESP_OK) {
    Serial.printf("nvs_commit failed: %d\n", err);
  } else {
    Serial.println("Template provisioning complete.");
    Serial.println("Insert real credentials in a private copy to use this.");
  }

  nvs_close(h);
}

void loop() {
  // Nothing to do here.
}
