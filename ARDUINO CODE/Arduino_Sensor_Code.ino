#include <DHT.h>
#include <SoftwareSerial.h>

// Define pins
#define DHTPIN 4            // GPIO2
#define DHTTYPE DHT11
#define SOIL_PIN A0          // Analog input
#define RAIN_SENSOR_PIN 6   // GPIO12 (digital pin)

// Define TX and RX pins for SoftwareSerial
#define TX_PIN 13            // GPIO5
#define RX_PIN 12           // GPIO4 (not used for sending but required for SoftwareSerial)

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize software serial
SoftwareSerial sensorSerial(RX_PIN, TX_PIN); // RX, TX

void setup() {
  Serial.begin(115200);      // Start serial for debug output
  sensorSerial.begin(9600);  // Start serial for communication with NodeMCU
  dht.begin();
  pinMode(RAIN_SENSOR_PIN, INPUT);
  
  Serial.println("Sensor board initialized. Sending data to NodeMCU...");
}

void loop() {
  // Read sensor values
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);  // 0 (wet) to 1023 (dry)
  int rainStatus = digitalRead(RAIN_SENSOR_PIN); // LOW = rain
  
  // Create a data string in format "temp,humidity,soil,rain"
  String dataString = "";
  
  // Add temperature
  if (!isnan(temp)) {
    dataString += String(temp);
  } else {
    dataString += "ERR";
  }
  dataString += ",";
  
  // Add humidity
  if (!isnan(hum)) {
    dataString += String(hum);
  } else {
    dataString += "ERR";
  }
  dataString += ",";
  
  // Add soil moisture
  dataString += String(soilValue);
  dataString += ",";
  
  // Add rain status
  dataString += String(rainStatus);
  
  // Send the data string to NodeMCU
  sensorSerial.println(dataString);
  

  Serial.print("Sending to NodeMCU: ");
  Serial.println(dataString);
  
 
  if (!isnan(temp) && !isnan(hum)) {
    Serial.print("🌡 Temp: "); Serial.print(temp); Serial.println(" °C");
    Serial.print("💧 Humidity: "); Serial.print(hum); Serial.println(" %");
  } else {
    Serial.println("❌ DHT11 read error!");
  }
  
  Serial.print("🌱 Soil Value: ");
  Serial.println(soilValue);
  if (soilValue > 700) Serial.println("⚠️ Soil is dry!");
  else if (soilValue > 300) Serial.println("😊 Soil is moist.");
  else Serial.println("💧 Soil is wet!");
  
  if (rainStatus == LOW) Serial.println("🌧 Rain detected!");
  else Serial.println("☀️ No rain.");
  
  Serial.println("-----------------------------");
  
  delay(10000);  // Delay for 10 seconds
}
