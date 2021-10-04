
// Include Libraries
#include <SPI.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// Pin Definitions
#define SDFILE_PIN_CS D3
#define LED D4
/**
    Constants
*/
const char FILE_NAME[] = "config.txt";
const char CLIENT_ID[] = "d25a7a02-b95a-45c7-80c4-8a2cac95c002";
const char TENANT_ID[] = "777fd943-bc3b-456c-afff-41f772edc972";
const char SCOPES[] = "&scope=user.read%20presence.read%20offline_access";
const int POLLING_INTERVAL = 15;

/**
   Variables
*/
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
void loadConfiguration(const char *filename)
{

  // Open file for reading
  File file = SD.open(filename);
  int lineNum = 0;
  int lineNumsOfInterest = 2;

  Serial.println("Loading config file...");

  // Loop through the file data
  while (file.available())
  {
    String buffer = file.readStringUntil('\n');
    // Trim the string
    buffer.trim();
    // Count the record
    lineNum++;

    // Exit from the loop in-case number of line exceeds interest
    if (lineNum > lineNumsOfInterest)
    {
      break;
    }

    // On line one should be the username
    if (lineNum == 1)
    {
      ssid = buffer;
    }

    // On line two should be the password
    if (lineNum == 2)
    {
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
void startWifiConnection()
{
  // Explicitly set the ESP8266 to be a WiFi-client
  WiFi.mode(WIFI_STA);
  // Start connecting
  WiFi.begin(ssid, password);

  Serial.print("Starting WIFI connection...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void getDeviceCode(WiFiClientSecure &client, HTTPClient &http)
{

  DynamicJsonDocument doc(1024);

  // Create request body
  String body = "client_id=" + String(CLIENT_ID) + String(SCOPES);

  http.begin(client, "https://login.microsoftonline.com/" + String(TENANT_ID) + "/oauth2/v2.0/devicecode");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  Serial.println("Device Code - POST");

  // start connection and send HTTP header
  int httpCode = http.POST(body);

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);
      if (error)
      {
        Serial.printf("Failed to parse payload");
      }
      else
      {

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

        waitForUserAuth(client, http);
      }
    }
    else
    {
      // In case some other error code
      Serial.printf("Device Code - POST - Code: %d\n", httpCode);
    }
  }
  else
  {
    Serial.printf("Device Code - POST - Failed!, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void waitForUserAuth(WiFiClientSecure &client, HTTPClient &http)
{
  static bool isPending = true;
  String body = "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=" + String(CLIENT_ID) + "&device_code=" + deviceCode;
  DynamicJsonDocument doc(4096);

  while (expiresIn > 0 && isPending)
  {
    // Delay the calls
    delay(authInterval * 1000);

    // Reduce the expires time
    expiresIn -= authInterval;

    // Wait for users authtoken
    http.begin(client, "https://login.microsoftonline.com/" + String(TENANT_ID) + "/oauth2/v2.0/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.println("Device Auth - POST");
    // start connection and send HTTP header
    int httpCode = http.POST(body);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_BAD_REQUEST)
    {
      String payload = http.getString();
      Serial.println(payload);
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);
      if (error)
      {
        Serial.printf("Failed to parse payload");
        isPending = false;
      }
      else
      {
        if (doc.containsKey("error"))
        {
          String errorCode = doc["error"].as<String>();
          if (errorCode != "authorization_pending")
          {
            Serial.print("Device Auth - POST - Authorization failed:");
            Serial.println(errorCode);
            isPending = false;
          }
          else
          {
            Serial.print("Device Auth - POST - Authorization pending:");
            Serial.println(errorCode);
          }
        }
        else
        {
          authToken = doc["access_token"].as<String>();
          refreshToken = doc["refresh_token"].as<String>();
          expiresIn = doc["expires_in"].as<int>();
          isPending = false;
        }
      }
    }
    else
    {
      // In case some other error code
      Serial.printf("Device Auth - POST - Code: %d\n", httpCode);
      isPending = false;
    }
    http.end();
  }

  // Check if authtoken and refresh token is set
  if (authToken == "" && refreshToken == "")
  {
    // Restart the auth flow
    getDeviceCode(client, http);
  }
}

JsonObject getPresence()
{
  DynamicJsonDocument doc(512);
  String errorStr = "";

  if (authToken != "")
  {
    // Create client instance
    WiFiClientSecure client;
    // ignore SSL certificate
    client.setInsecure();
    // HTTP Client instance
    HTTPClient http;
    http.begin(client, "https://graph.microsoft.com/v1.0/me/presence");
    http.addHeader("Authorization", "Bearer " + authToken);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      Serial.print(payload);
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);
      if (error)
      {
        errorStr = "Presence GET - Parse Error";
        Serial.println(errorStr);
      }
      else
      {
        Serial.println("Presence GET - Success");
      }
    }
    http.end();
  }
  else
  {
    errorStr = "Presence GET - Auth Token Missing";
    Serial.print(errorStr);
  }

  JsonObject result = doc.as<JsonObject>();

  if (errorStr != "") {
    result["error"] = errorStr;
  }

  return result;
}

// Setup the essentials for your circuit to work. It runs first every time your circuit is powered with electricity.
void setup()
{
  // initialize GPIO 16 as an output
  pinMode(LED, OUTPUT);
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
    ;

  // Initialize SD library
  while (!SD.begin(SDFILE_PIN_CS))
  {
    Serial.println("Failed to initialize SD library");
    delay(1000);
  }

  // Load config file
  loadConfiguration(FILE_NAME);

  if (ssid != "" && password != "")
  {
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
}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop()
{
  if (authToken != "" && refreshToken != "")
  {
    while (expiresIn > 0)
    {
      expiresIn -= POLLING_INTERVAL;
      JsonObject presence = getPresence();
      if (!presence.containsKey("error"))
      {
        String activity = presence["activity"].as<String>();
        if (activity == "DoNotDisturb" ||  activity == "InACall" || activity == "InAConferenceCall" || activity == "InAMeeting" || activity == "Presenting") {
          Serial.println("Turn On Air signal!");
          digitalWrite(LED, HIGH);
        } else {
          Serial.println("Turn Off Air signal!");
          digitalWrite(LED, LOW);
        }
      }
      delay(POLLING_INTERVAL * 1000);
    }

    // Get new auth token / refresh token here
  }
}
