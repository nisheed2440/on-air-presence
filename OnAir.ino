// Include Libraries
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// Pin Definitions
#define SDFILE_PIN_CS D8
#define LED D0
// Screen Definitions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
/**
    Constants
*/
const char FILE_NAME[] = "config.txt";
const char CLIENT_ID[] = "d25a7a02-b95a-45c7-80c4-8a2cac95c002";
const char TENANT_ID[] = "777fd943-bc3b-456c-afff-41f772edc972";
const char SCOPES[] = "&scope=user.read%20presence.read%20offline_access";
const int POLLING_INTERVAL = 15;
const int POLLING_TIMEOUT = 600;

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

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void initializeDisplay() {

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }
  // Wait for 2s
  delay(2000);

  // Clear the buffer.
  display.clearDisplay();

  // Set the display text color to white
  display.setTextColor(WHITE);

  // Set the default text size
  display.setTextSize(1);
}

// Utility function to center strings on LCD
void drawCentreString(const String buf, int x, int y)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
  display.setCursor(x - w / 2, y);
  display.print(buf);
}


// Utility function to display info messages
void displayInfoMessage(const String message) {
  display.clearDisplay();
  drawCentreString("INFO:", SCREEN_WIDTH / 2, 5);
  display.setCursor(0, 20);
  display.print(message);
  display.display();
}


// Utility function to display error messages
void displayErrorMessage(const String message) {
  display.clearDisplay();
  drawCentreString("ERROR:", SCREEN_WIDTH / 2, 5);
  display.setCursor(0, 20);
  display.print(message);
  display.display();
}

// Utility function to display user code
void displayUserCode(String code) {
  display.clearDisplay();
  drawCentreString("GO TO:", SCREEN_WIDTH / 2, 6);
  display.setCursor(10, 18);
  display.print("aka.ms/devicelogin");
  drawCentreString("AND ENTER:", SCREEN_WIDTH / 2, 30);
  display.setTextSize(2);
  display.setCursor(10, 42);
  display.print(code);
  display.setTextSize(1);
  display.display();
}

// Loads the configuration from a file
void loadConfiguration(const char *filename)
{

  // Open file for reading
  File file = SD.open(filename);
  int lineNum = 0;
  int lineNumsOfInterest = 2;

  Serial.println("Loading config file...");
  displayInfoMessage("Loading config file...");
  delay(2000);

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
  Serial.println(String("SSID: " + ssid) + "\n" + String("Password: " + password));
  displayInfoMessage(String("SSID: " + ssid) + "\nPassword: *******");
  delay(2000);
}

// starts a WIFI connection based on the ssid/password from the SD card
void startWifiConnection()
{
  // Explicitly set the ESP8266 to be a WiFi-client
  WiFi.mode(WIFI_STA);
  // Start connecting
  WiFi.begin(ssid, password);

  Serial.print("Starting WIFI connection!");
  displayInfoMessage("Starting WIFI connection!");
  delay(2000);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    displayInfoMessage("Connecting...");
  }

  Serial.println("");
  Serial.println(String("WiFi connected!\nIP address:\n" + WiFi.localIP().toString()));
  displayInfoMessage(String("WiFi connected!\nIP address:\n" + WiFi.localIP().toString()));
  delay(4000);
}

void getDeviceCode()
{

  // Create client instance
  WiFiClientSecure client;
  // ignore SSL certificate
  client.setInsecure();
  // HTTP Client instance
  HTTPClient http;
  
  DynamicJsonDocument doc(1024);

  // Create request body
  String body = "client_id=" + String(CLIENT_ID) + String(SCOPES);
  http.useHTTP10(true);
  http.begin(client, "https://login.microsoftonline.com/" + String(TENANT_ID) + "/oauth2/v2.0/devicecode");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  Serial.println("Device Code - POST");
  displayInfoMessage("Getting device code...");

  // start connection and send HTTP header
  int httpCode = http.POST(body);

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    if (httpCode == HTTP_CODE_OK)
    {
      Serial.println("Recieved data!");
      displayInfoMessage("Recieved data!");
      delay(2000);

      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (error)
      {
        Serial.println("Failed to parse payload!");
        displayErrorMessage("Failed to parse payload!");
        delay(2000);
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

        displayUserCode(userCode);
        waitForUserAuth(client, http);
      }
    }
    else
    {
      // In case some other error code
      Serial.printf("Device Code - POST - Code: %d\n", httpCode);
      displayErrorMessage("Getting device code - Failed");
    }
  }
  else
  {
    Serial.printf("Device Code - POST - Failed!, error: %s\n", http.errorToString(httpCode).c_str());
    displayErrorMessage("Getting device code - Failed");
  }
  http.end();

  // Check if authtoken and refresh token is set
  if (authToken == "" && refreshToken == "")
  {
    // Restart the auth flow
    Serial.printf("Restarting device authorization!");
    displayInfoMessage("Restarting device authorization!");
    delay(1000);
    getDeviceCode();
  }
}

void waitForUserAuth(WiFiClientSecure &client, HTTPClient &http)
{
  static bool isPending = true;
  String body =  "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=" + String(CLIENT_ID) + "&device_code=" + deviceCode;

  DynamicJsonDocument doc(4096);

  while (expiresIn > 0 && isPending)
  {
    // Reduce the expires time
    expiresIn -= authInterval;

    // Wait for users authtoken
    http.useHTTP10(true);
    http.begin(client, "https://login.microsoftonline.com/" + String(TENANT_ID) + "/oauth2/v2.0/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.println("Device Auth - POST");
    // start connection and send HTTP header
    int httpCode = http.POST(body);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_BAD_REQUEST)
    {
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (error)
      {
        Serial.println("Failed to parse payload");
        isPending = false;
      }
      else
      {
        if (doc.containsKey("error"))
        {
          String errorCode = doc["error"].as<String>();
          if (errorCode != "authorization_pending")
          {
            Serial.print("Device authorization failed:");
            Serial.println(errorCode);
            displayErrorMessage("Device authorization failed!");
            delay(2000);
            isPending = false;
          }
          else
          {
            Serial.print("Device authorization pending:");
            Serial.println(errorCode);
          }
        }
        else
        {
          Serial.print("Device authorization successful!");
          displayInfoMessage("Device authorization successful!");
          delay(2000);
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
      displayErrorMessage("Device authorization failed!");
      delay(2000);
      isPending = false;
    }
    // Delay the calls
    delay(authInterval * 1000);
  }
}

// Get the new auth token based on refresh token
void refreshAuthToken()
{

  // Create client instance
  WiFiClientSecure client;
  // ignore SSL certificate
  client.setInsecure();
  // HTTP Client instance
  HTTPClient http;

  String body = "grant_type=refresh_token&client_id=" + String(CLIENT_ID) + "&refresh_token=" + refreshToken;

  DynamicJsonDocument doc(4096);

  // Wait for users authtoken
  http.useHTTP10(true);
  http.begin(client, "https://login.microsoftonline.com/" + String(TENANT_ID) + "/oauth2/v2.0/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  Serial.println("Refresh Auth Token - POST");
  displayInfoMessage("Refreshing auth token...");
  // start connection and send HTTP header
  int httpCode = http.POST(body);
  if (httpCode == HTTP_CODE_OK)
  {
    Serial.println("Recieved data!");
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error)
    {
      Serial.println("Failed to parse payload");
      displayErrorMessage("Failed to parse payload!");
    }
    else
    {
      if (doc.containsKey("error"))
      {
        String errorCode = doc["error"].as<String>();
        Serial.print("Device Auth - POST - Authorization failed:");
        Serial.println(errorCode);
        displayErrorMessage("Refreshing auth token failed!");
      }
      else
      {
        displayInfoMessage("Refreshing auth token successful!");
        authToken = doc["access_token"].as<String>();
        refreshToken = doc["refresh_token"].as<String>();
        expiresIn = doc["expires_in"].as<int>();
        Serial.print("Auth Token: ");
        Serial.println(authToken);
        Serial.print("Refresh Token: ");
        Serial.println(refreshToken);
        Serial.print("Expires In: ");
        Serial.println(expiresIn);
      }
    }
  }
  else
  {
    // In case some other error code
    Serial.printf("Device Auth - POST - Code: %d\n", httpCode);
    displayErrorMessage("Refreshing auth token failed!");
  }
  http.end();
}

// Get the users presence
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
    http.useHTTP10(true);
    http.begin(client, "https://graph.microsoft.com/v1.0/me/presence");
    http.addHeader("Authorization", "Bearer " + authToken);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, http.getStream());
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



  if (errorStr != "")
  {
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

  // Initialize Display
  initializeDisplay();

  // Initialize SD library
  while (!SD.begin(SDFILE_PIN_CS))
  {
    Serial.println("Failed to initialize SD library");
    displayError("Failed to initialize SD library", 1000);
  }


  // Load config file
  loadConfiguration(FILE_NAME);

  if (ssid != "" && password != "")
  {
    // Start WIFI connection
    startWifiConnection();

    // Get Device Code
    getDeviceCode();
  } else {
    displayErrorMessage("Unable to find Wifi ssid or password");
  }
}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop()
{
  if (authToken != "" && refreshToken != "")
  {
    while (expiresIn > POLLING_TIMEOUT)
    {
      JsonObject presence = getPresence();

      if (!presence.containsKey("error"))
      {
        Serial.print(presence["activity"].as<String>());
        String activity = presence["activity"].as<String>();
        if (activity == "DoNotDisturb" || activity == "InACall" || activity == "InAConferenceCall" || activity == "InAMeeting" || activity == "Presenting")
        {
          Serial.println("Turn On On-Air signal!");
          digitalWrite(LED, HIGH);
        }
        else
        {
          Serial.println("Turn Off On-Air signal!");
          digitalWrite(LED, LOW);
        }
        displayInfoMessage("Activity - " + activity);
        display.display();
      }
      delay(POLLING_INTERVAL * 1000);
      expiresIn -= POLLING_INTERVAL;
    }
    // Turn off the LED before getting new token
    Serial.println("Turn Off On-Air signal!");
    digitalWrite(LED, LOW);
    
    // Get new auth token / refresh token here
    refreshAuthToken();
  }
}
