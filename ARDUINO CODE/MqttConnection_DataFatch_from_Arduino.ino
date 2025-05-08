#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <coredecls.h>                  // settimeofday_cb()

// Define RX and TX pins for SoftwareSerial
#define RX_PIN D1            // GPIO5 - Connect to TX pin of sensor board
#define TX_PIN D2            // GPIO4 - Connect to RX pin of sensor board (not used but required)

// Initialize software serial
SoftwareSerial nodeSerial(RX_PIN, TX_PIN); // RX, TX

// WiFi credentials
const char* ssid = "BISHAL";
const char* password = "12345678";

// MQTT Broker settings for HiveMQ Cloud
const char* mqtt_server = "e9da612c10c54fed802eb3fcbd0ad60d.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;                     // TLS port for HiveMQ Cloud
const char* mqtt_user = "bishaldas";  
const char* mqtt_password = "Bishaldas290620@"; 
const char* clientID = "ESP8266-Bishal-Weather";  // More unique client ID

// MQTT Topics
const char* topicTemperature = "bishal/iot/temp";
const char* topicHumidity = "bishal/iot/humidity";
const char* topicSoil = "bishal/iot/soilsensor";
const char* topicRain = "bishal/iot/rain";
const char* topicStatus = "bishal/iot/iregation/status";

// NTP Server settings for certificate validation
// Using regional NTP servers for better response - change these to servers closer to you
const char* ntpServer1 = "asia.pool.ntp.org";     // Asia pool (closer to India)
const char* ntpServer2 = "in.pool.ntp.org";       // India pool
const char* ntpServer3 = "time.google.com";       // Google's NTP server as backup

// Time settings - Adjust for your timezone
// India is UTC+5:30, so 5.5 hours × 3600 seconds = 19800 seconds
const long gmtOffset_sec = 19800;                 // India timezone offset (UTC+5:30)
const int daylightOffset_sec = 0;                 // India doesn't use DST

// WiFi and MQTT client objects
WiFiClientSecure espClient;
PubSubClient client(espClient);

// DigiCert Global Root G2 - Used by HiveMQ Cloud
static const char digicert_global_root_g2[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// Variables to store sensor data
float temperature;
float humidity;
int soilMoisture;
int rainStatus;
unsigned long lastPublishTime = 0;
const long publishInterval = 30000;  // Publish data every 30 seconds
unsigned long lastReconnectAttempt = 0;

// Connection state
bool mqttConnected = false;
volatile bool timeInitialized = false;
bool forceInsecureMode = false;      // Use this to force insecure mode when all else fails

// Time sync callback
void timeSyncCallback(bool success) {
  if (success) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.println("Time synchronization SUCCESS!");
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
    timeInitialized = true;
  } else {
    Serial.println("Time synchronization FAILED!");
    timeInitialized = false;
  }
}

void setupWiFi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wifiCounter = 0;
  const int maxAttempts = 40;  // Increase timeout to 20 seconds
  
  while (WiFi.status() != WL_CONNECTED && wifiCounter < maxAttempts) {
    delay(500);
    Serial.print(".");
    wifiCounter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Print network diagnostic info
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("");
    Serial.println("WiFi connection FAILED");
    Serial.println("Will retry in the main loop");
  }
}

bool setupTime() {
  Serial.println("Setting up time with NTP servers:");
  Serial.println(ntpServer1);
  Serial.println(ntpServer2);
  Serial.println(ntpServer3);
  
  // Set callback for time synchronization
  settimeofday_cb(timeSyncCallback);
  
  // Make sure WiFi is connected before attempting NTP
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot sync time.");
    return false;
  }
  
  // Configure time with multiple NTP servers - try each server separately
  Serial.println("Trying primary NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1);

  // Wait for initial time sync attempt
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int timeCounter = 0;
  const int initialAttempts = 60;  // 30 seconds for first attempt

  // First quick attempt
  while (now < 8 * 3600 * 2 && timeCounter < initialAttempts) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    timeCounter++;
  }
  
  // If first server fails, try second server
  if (now < 8 * 3600 * 2) {
    Serial.println("\nFirst NTP server failed, trying secondary server...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer2);
    
    timeCounter = 0;
    while (now < 8 * 3600 * 2 && timeCounter < initialAttempts) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      timeCounter++;
    }
  }
  
  // If second server fails, try third server
  if (now < 8 * 3600 * 2) {
    Serial.println("\nSecond NTP server failed, trying third server...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer3);
    
    timeCounter = 0;
    while (now < 8 * 3600 * 2 && timeCounter < initialAttempts) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      timeCounter++;
    }
  }
  
  // Try all three servers if still failing
  if (now < 8 * 3600 * 2) {
    Serial.println("\nTrying all three NTP servers together with longer timeout...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    
    timeCounter = 0;
    const int maxAttempts = 120;  // 1 minute more for final attempt
    
    while (now < 8 * 3600 * 2 && timeCounter < maxAttempts) {
      delay(500);
      
      // Print progress differently to see activity
      if (timeCounter % 20 == 0) {
        Serial.println();
        Serial.print(timeCounter / 2);
        Serial.print(" seconds: ");
      }
      Serial.print(".");
      
      now = time(nullptr);
      timeCounter++;
    }
  }

  if (now > 8 * 3600 * 2) {
    Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
    Serial.printf("Unix timestamp: %ld\n", (long)now);
    return true;
  } else {
    Serial.println("");
    Serial.println("NTP time sync FAILED after multiple attempts");
    Serial.println("This will cause SSL certificate validation to fail!");
    
    // Manual time setting as last resort
    Serial.println("Setting time manually as last resort");
    struct timeval tv;
    // Setting to May 4, 2025
    tv.tv_sec = 1746576000; // Unix timestamp for May 4, 2025
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    
    now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Time manually set to: ");
    Serial.println(asctime(&timeinfo));
    
    return (now > 8 * 3600 * 2);
  }
}

void verifyTimeSync() {
  // Check current time
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) {
    Serial.println("WARNING: Time not properly set!");
    Serial.println("Current time appears to be: 1970-01-01 or close to it");
    Serial.println("Attempting to re-sync time...");
    
    // Try to sync time again
    timeInitialized = setupTime();
    
    // If time sync still fails, enable insecure mode
    if (!timeInitialized) {
      Serial.println("Time sync failed again!");
      forceInsecureMode = true;
      Serial.println("IMPORTANT: Setting client to insecure mode.");
      Serial.println("This is not recommended for production use!");
      
      // Set client to insecure mode
      espClient.setInsecure();
    }
  } else {
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
    timeInitialized = true;
  }
}

void setupSecureClient() {
  // Set TLS certificate and increase buffer sizes
  espClient.setBufferSizes(2048, 2048);  // Increased buffer size for better stability
  
  // Add certificate with proper error handling
  X509List *cert = new X509List(digicert_global_root_g2);
  if (cert) {
    Serial.println("Certificate loaded successfully");
    espClient.setTrustAnchors(cert);
  } else {
    Serial.println("ERROR: Failed to allocate memory for certificate!");
    forceInsecureMode = true;
  }
  
  // Set session timeout to just 15 seconds for faster reconnection if issues occur
 
  
  // Disable certificate checking if time sync completely failed
  if (!timeInitialized) {
    Serial.println("WARNING: Time sync failed, SSL validation would fail!");
    Serial.println("Temporarily setting client to insecure mode");
    espClient.setInsecure();
  }
  // Set insecure only if forced
  else if (forceInsecureMode) {
    Serial.println("WARNING: Using insecure mode due to persistent time issues");
    espClient.setInsecure();
  } else {
    Serial.println("Secure client setup complete with certificate validation");
  }
}

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot connect to MQTT.");
    return false;
  }

  // Verify time is properly set (unless forced insecure)
  if (!forceInsecureMode) {
    verifyTimeSync();
  }
  
  Serial.println("Attempting MQTT connection...");
  Serial.print("Server: ");
  Serial.print(mqtt_server);
  Serial.print(", Port: ");
  Serial.println(mqtt_port);
  
  // Create a unique client ID with random component
  String clientId = "ESP8266-Bishal-Weather-";
  clientId += String(random(0xffff), HEX);
  
  Serial.print("ClientID: ");
  Serial.print(clientId);
  Serial.print(" | Username: ");
  Serial.print(mqtt_user);
  Serial.print(" | Password: ");
  Serial.println("********");

  // Set longer timeout for connection
  client.setSocketTimeout(30);
  
  // Attempt to connect with credentials
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
    Serial.println("Connected to MQTT server!");
    
    // Show connection mode
    if (forceInsecureMode) {
      client.publish(topicStatus, "Weather station online (insecure mode)");
      Serial.println("Connected in INSECURE mode - certificate validation bypassed!");
    } else {
      client.publish(topicStatus, "Weather station online (secure mode)");
      Serial.println("Connected in SECURE mode with certificate validation!");
    }
    
    return true;
  } else {
    int state = client.state();
    Serial.print("MQTT connection failed, rc=");
    Serial.print(state);
    Serial.print(" (");
    
    // Decode error code
    switch(state) {
      case -4: Serial.print("MQTT_CONNECTION_TIMEOUT"); break;
      case -3: Serial.print("MQTT_CONNECTION_LOST"); break;
      case -2: Serial.print("MQTT_CONNECT_FAILED"); break;
      case -1: Serial.print("MQTT_DISCONNECTED"); break;
      case 1: Serial.print("MQTT_CONNECT_BAD_PROTOCOL"); break;
      case 2: Serial.print("MQTT_CONNECT_BAD_CLIENT_ID"); break;
      case 3: Serial.print("MQTT_CONNECT_UNAVAILABLE"); break;
      case 4: Serial.print("MQTT_CONNECT_BAD_CREDENTIALS"); break;
      case 5: Serial.print("MQTT_CONNECT_UNAUTHORIZED"); break;
      default: Serial.print("UNKNOWN"); break;
    }
    Serial.println(")");
    
    // Add detailed SSL error debugging
    int sslError = espClient.getLastSSLError();
    Serial.print("Last SSL error: ");
    Serial.print(sslError);
    
    // Add more details about common SSL errors
    switch(sslError) {
      case 60: Serial.println(" (SSL certificate expired)"); break;
      case 62: Serial.println(" (SSL certificate not yet valid - check your device's time)"); break;
      case 83: Serial.println(" (SSL client certificate required)"); break;
      case 51: Serial.println(" (SSL handshake failed)"); break;
      default: Serial.println();
    }
    
    // If we're still having SSL issues after time sync, try insecure mode as last resort
    if (sslError > 0 && state == -2 && !forceInsecureMode) {
      Serial.println("\nSSL issues persist despite time sync attempts.");
      Serial.println("TEMPORARILY enabling insecure mode for troubleshooting.");
      Serial.println("WARNING: This bypasses certificate validation!");
      
      espClient.setInsecure();
      Serial.println("Retrying connection with insecure mode...");
      
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
        Serial.println("Connected to MQTT server in insecure mode!");
        Serial.println("Please fix the time sync issue for proper security.");
        client.publish(topicStatus, "Weather station online (insecure mode)");
        return true;
      } else {
        Serial.println("Connection still failed even in insecure mode.");
        Serial.println("Please check your credentials and broker settings.");
        // Reset to secure mode for next attempt
        if (!forceInsecureMode) {
          setupSecureClient();
        }
        return false;
      }
    }
    
    return false;
  }
}

void publishData() {
  if (!mqttConnected) {
    Serial.println("MQTT not connected. Cannot publish data.");
    return;
  }
  
  // Prepare string buffers for publishing
  char tempStr[10];
  char humStr[10];
  char soilStr[10];
  char rainStr[10];
  char statusMsg[100];
  
  // Convert values to strings
  dtostrf(temperature, 5, 2, tempStr);  // Convert float to string with 2 decimal places
  dtostrf(humidity, 5, 2, humStr);
  itoa(soilMoisture, soilStr, 10);      // Convert int to string
  
  // Determine rain status string
  if (rainStatus == LOW) {
    strcpy(rainStr, "detected");
  } else {
    strcpy(rainStr, "none");
  }
  
  // Create status message
  sprintf(statusMsg, "Temp: %.2f°C, Humidity: %.2f%%, Soil: %d, Rain: %s", 
          temperature, humidity, soilMoisture, rainStr);
  
  // Publish to MQTT topics
  Serial.println("Publishing to MQTT:");
  
  if (client.publish(topicTemperature, tempStr)) {
    Serial.println(" - Temperature published");
  } else {
    Serial.println(" - Temperature publish FAILED");
  }
  
  if (client.publish(topicHumidity, humStr)) {
    Serial.println(" - Humidity published");
  } else {
    Serial.println(" - Humidity publish FAILED");
  }
  
  if (client.publish(topicSoil, soilStr)) {
    Serial.println(" - Soil published");
  } else {
    Serial.println(" - Soil publish FAILED");
  }
  
  if (client.publish(topicRain, rainStr)) {
    Serial.println(" - Rain published");
  } else {
    Serial.println(" - Rain publish FAILED");
  }
  
  if (client.publish(topicStatus, statusMsg)) {
    Serial.println(" - Status published");
  } else {
    Serial.println(" - Status publish FAILED");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n===== NodeMCU Weather Station =====");
  Serial.println("Version: 2.1 - Improved Time Synchronization");
  
  // Initialize sensor serial connection
  nodeSerial.begin(9600);
  
  // Initialize random seed for client ID generation
  randomSeed(micros());
  
  // Connect to WiFi with retry
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 3) {
    setupWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi setup failed, retrying...");
      delay(1000);
      wifiAttempts++;
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WARNING: WiFi connection failed after multiple attempts");
    Serial.println("Please check your WiFi credentials and signal strength");
  }
  
  // Ensure time sync starts correctly by setting ESP timezone
  // Needed before time sync
  configTzTime("IST-5:30", ntpServer1, ntpServer2, ntpServer3);  // More robust time setup
  
  // Set up time (needed for SSL/TLS)
  timeInitialized = setupTime();
  
  // If time setup fails completely, force insecure mode
  if (!timeInitialized) {
    Serial.println("WARNING: Time initialization failed completely");
    Serial.println("Certificate validation will be disabled");
    forceInsecureMode = true;
  }
  
  // Setup secure client
  setupSecureClient();
  
  // Setup MQTT client with larger buffer
  client.setBufferSize(512);  // Increased buffer for MQTT
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Setup complete. Waiting for sensor data...");
  
  // Try to connect to MQTT right away
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Attempting initial MQTT connection...");
    connectMQTT();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost! Reconnecting...");
    setupWiFi();
    delay(1000); // Give WiFi some time to stabilize
  }

  // Periodically check time sync (every 3 hours if not forced insecure)
  static unsigned long lastTimeCheck = 0;
  if (!forceInsecureMode && currentMillis - lastTimeCheck > 3 * 60 * 60 * 1000UL) {
    lastTimeCheck = currentMillis;
    Serial.println("Performing periodic time check...");
    verifyTimeSync();
  }

  // Manage MQTT connection
  if (!client.connected()) {
    mqttConnected = false;
    // Try to reconnect every 30 seconds
    if (currentMillis - lastReconnectAttempt > 30000) {
      lastReconnectAttempt = currentMillis;
      Serial.println("MQTT disconnected, attempting reconnect...");
      mqttConnected = connectMQTT();
      
      // If reconnection failed, don't try again immediately
      if (!mqttConnected) {
        Serial.println("Will try again in 30 seconds.");
      }
    }
  } else {
    mqttConnected = true;
    client.loop(); // Process MQTT messages
  }

  // Check if data is available from sensor board
  if (nodeSerial.available() > 0) {
    // Read the incoming data as string
    String receivedData = nodeSerial.readStringUntil('\n');
    receivedData.trim();  // Remove any whitespace or newline characters
    
    Serial.print("Received raw data: ");
    Serial.println(receivedData);
    
    // Parse the received data string (format: "temp,humidity,soil,rain")
    int firstComma = receivedData.indexOf(',');
    int secondComma = receivedData.indexOf(',', firstComma + 1);
    int thirdComma = receivedData.indexOf(',', secondComma + 1);
    
    if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
      String tempStr = receivedData.substring(0, firstComma);
      String humStr = receivedData.substring(firstComma + 1, secondComma);
      String soilStr = receivedData.substring(secondComma + 1, thirdComma);
      String rainStr = receivedData.substring(thirdComma + 1);
      
      // Convert strings to appropriate values
      if (tempStr != "ERR") {
        temperature = tempStr.toFloat();
      }
      
      if (humStr != "ERR") {
        humidity = humStr.toFloat();
      }
      
      soilMoisture = soilStr.toInt();
      rainStatus = rainStr.toInt();
      
      // Print parsed values with nice formatting
      Serial.println("=== Weather Station Data ===");
      if (tempStr != "ERR") {
        Serial.print("🌡 Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
      } else {
        Serial.println("❌ Temperature: Error reading sensor");
      }
      
      if (humStr != "ERR") {
        Serial.print("💧 Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
      } else {
        Serial.println("❌ Humidity: Error reading sensor");
      }
      
      Serial.print("🌱 Soil Value: ");
      Serial.println(soilMoisture);
      if (soilMoisture > 700) Serial.println("⚠️ Soil is dry!");
      else if (soilMoisture > 300) Serial.println("😊 Soil is moist.");
      else Serial.println("💧 Soil is wet!");
      
      if (rainStatus == LOW) Serial.println("🌧 Rain detected!");
      else Serial.println("☀️ No rain.");
      
      Serial.println("=============================");
      
      // Publish data to MQTT server (if enough time passed since last publish)
      if (currentMillis - lastPublishTime >= publishInterval) {
        publishData();
        lastPublishTime = currentMillis;
      }
    }
  }
  
  // Small delay to prevent CPU hogging
  delay(1000);
}