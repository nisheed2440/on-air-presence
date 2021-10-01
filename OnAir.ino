
// Include Libraries
#include <SPI.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// Pin Definitions
#define SDFILE_PIN_CS  4

// Default file
#define SDFILE_NAME "config.txt"


// Client ID
const String clientID = "1385b487-9035-4609-bc4c-8855b7368b4c";
// Tenant ID
const String tenantID = "72f988bf-86f1-41af-91ab-2d7cd011db47";
// Scopes Required
const String scopes = "&scope=user.read%20presence.read";

// WIFI username/password
String ssid = "";
String password = "";

// Authentication variables
String deviceCode = "";
String userCode = "";
String verificationURL = "";
int authInterval = 0;
int expiresIn = 0;
String authToken = "";
String refreshToken = "";

// Loads the configuration from a file
void loadConfiguration(const char *filename) {

  // Open file for reading
  File file = SD.open(filename);
  int lineNum = 0;
  int lineNumsOfInterest = 2;

  Serial.println("Loading config file...");

  // Loop through the file data
  while (file.available()) {
    String buffer = file.readStringUntil('\n');
    // Trim the string
    buffer.trim();
    // Count the record
    lineNum++;

    // Exit from the loop in-case number of line exceeds interest
    if (lineNum > lineNumsOfInterest) {
      break;
    }

    // On line one should be the username
    if (lineNum == 1) {
      ssid = buffer;
    }

    // On line two should be the password
    if (lineNum == 2) {
      password = buffer;
    }
  }

  // Close the file (File's destructor doesn't close the file)
  file.close();

  // Print the new values of username and password
  Serial.println(String("SSID: " + ssid));
  Serial.println(String("Password: " + password));
}

// starts a WIFI connection based on the ssid/password from the SD card
void startWifiConnection() {
  // Explicitly set the ESP8266 to be a WiFi-client
  WiFi.mode(WIFI_STA);
  // Start connecting
  WiFi.begin(ssid, password);

  Serial.print("Starting WIFI connection...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void getDeviceCode(WiFiClientSecure& client, HTTPClient& http) {

  // Create request body
  String body = String("client_id=" + clientID + scopes);

  StaticJsonDocument<768> doc;

  http.begin(client, String("https://login.microsoftonline.com/" + tenantID + "/oauth2/v2.0/devicecode"));
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  Serial.println("Device Code - POST");

  // start connection and send HTTP header
  int httpCode = http.POST(body);

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.printf("Failed to parse payload");
      } else {

        // Use this to make the polling request for auth
        deviceCode = doc["device_code"].as<String>();
        // Use this to set the polling interval for auth
        authInterval = doc["interval"].as<int>();
        // Use this to check if the device token has expired
        expiresIn = doc["expires_in"].as<int>();
        // Use this to authenticate the user / show on screen
        userCode = doc["user_code"].as<String>();
        // Use this URL to very the userCode
        verificationURL = doc["verification_uri"].as<String>();

        Serial.println("Device Code - POST - Response");
        Serial.print("Device Code: ");
        Serial.println(deviceCode);
        Serial.print("Expires In: ");
        Serial.println(expiresIn);
        Serial.print("User Code: ");
        Serial.println(userCode);
        Serial.print("Polling Interval: ");
        Serial.println(authInterval);
        Serial.print("Verification URL: ");
        Serial.println(verificationURL);

        // TBD: Show it on the LCD
      }
    } else {
      // In case some other error code
      Serial.printf("Device Code - POST - Code: %d\n", httpCode);
    }
  } else {
    Serial.printf("Device Code - POST - Failed!, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// Setup the essentials for your circuit to work. It runs first every time your circuit is powered with electricity.
void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial);

  // Initialize SD library
  while (!SD.begin(SDFILE_PIN_CS)) {
    Serial.println("Failed to initialize SD library");
    delay(1000);
  }

  // Load config file
  loadConfiguration(SDFILE_NAME);

  // Start WIFI connection
  startWifiConnection();

  // Create client instance
  WiFiClientSecure client;
  // ignore SSL certificate
  client.setInsecure();
  // HTTP Client instance
  HTTPClient http;

  // Get Device Code
  getDeviceCode(client, http);

}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop()
{


}