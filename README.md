# Weather Station Dashboard with Water Prediction Model

## Overview

This project is a web-based dashboard that connects to an IoT weather station via MQTT protocol. It displays real-time sensor data and includes an intelligent water prediction model based on multi-linear regression.

![Weather Station Dashboard]()

## Features

- **Real-time Weather Monitoring**:
  - Temperature (°C)
  - Humidity (%)
  - Soil Moisture
  - Rain Detection

- **Smart Water Prediction**:
  - Machine learning model using multi-linear regression
  - Predicts watering needs based on sensor data
  - Self-training capabilities from historical data
  - Visual indication of water needs percentage

- **MQTT Connectivity**:
  - Secure connection to MQTT broker
  - Real-time data updates
  - Connection status monitoring
  - Configurable connection settings

- **Responsive Design**:
  - Mobile-friendly interface
  - Modern card-based layout
  - Intuitive visual indicators

## Technical Details

### MQTT Configuration

The dashboard connects to an MQTT broker to receive sensor data from the ESP8266-based weather station. Default settings are provided, but can be modified in the settings panel.

Default MQTT topics:
- Temperature: `bishal/iot/temp`
- Humidity: `bishal/iot/humidity`
- Soil Moisture: `bishal/iot/soilsensor`
- Rain Detection: `bishal/iot/rain`
- Status Messages: `bishal/iot/iregation/status`

### Water Prediction Algorithm

The dashboard implements a multi-linear regression model to predict watering needs:

```
water_need = (coefficient_temp × temperature) + 
             (coefficient_humidity × humidity) + 
             (coefficient_soil × soil_moisture) + 
             (coefficient_rain × rain_status) + 
             intercept
```

The model adapts over time as more data is collected. Training data is stored in the browser's local storage.

## Installation

1. Clone this repository:
   ```
   git clone https://github.com/yourusername/weather-station-dashboard.git
   ```

2. Open the `index.html` file in a web browser.

3. Configure your MQTT settings in the dashboard interface.

## Hardware Requirements

This dashboard is designed to work with an ESP8266-based weather station with the following sensors:
- DHT22/DHT11 for temperature and humidity
- Soil moisture sensor
- Rain detection sensor

## ESP8266 Code Setup

To set up the ESP8266 to work with this dashboard, you need to program it to publish sensor data to the configured MQTT topics. Sample Arduino code for the ESP8266:

```cpp
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Broker settings
const char* mqtt_server = "e9da612c10c54fed802eb3fcbd0ad60d.s1.eu.hivemq.cloud";
const int mqtt_port = 8884;
const char* mqtt_user = "bishaldas";
const char* mqtt_password = "YOUR_MQTT_PASSWORD";

// Pin definitions
#define DHTPIN 2     // DHT sensor pin
#define DHTTYPE DHT22   // DHT 22 (AM2302)
#define SOIL_PIN A0     // Soil moisture sensor analog pin
#define RAIN_PIN 4      // Rain sensor digital pin

// MQTT topics
const char* topic_temp = "bishal/iot/temp";
const char* topic_humidity = "bishal/iot/humidity";
const char* topic_soil = "bishal/iot/soilsensor";
const char* topic_rain = "bishal/iot/rain";
const char* topic_status = "bishal/iot/iregation/status";

// Objects
WiFiClientSecure espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  
  // Initialize sensors
  pinMode(RAIN_PIN, INPUT);
  dht.begin();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  
  // Connect to MQTT
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  
  // Publish initial status
  client.publish(topic_status, "Weather station started");
}

void loop() {
  // Ensure MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read sensors
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soil = analogRead(SOIL_PIN);
  bool isRaining = !digitalRead(RAIN_PIN); // Assuming LOW when rain is detected

  // Publish data if valid
  if (!isnan(temp)) {
    client.publish(topic_temp, String(temp).c_str());
  }
  if (!isnan(humidity)) {
    client.publish(topic_humidity, String(humidity).c_str());
  }
  
  client.publish(topic_soil, String(soil).c_str());
  client.publish(topic_rain, isRaining ? "detected" : "none");
  
  delay(5000); // Update every 5 seconds
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266-Weather-Station", mqtt_user, mqtt_password)) {
      client.publish(topic_status, "Weather station connected to MQTT");
    } else {
      delay(5000);
    }
  }
}
```

## Usage

1. **Connect to MQTT Broker**:
   - Click the Settings button to access MQTT configuration
   - Enter your MQTT broker details
   - Click Connect

2. **Monitor Sensor Data**:
   - View real-time temperature, humidity, soil moisture, and rain data
   - Check system status messages

3. **Water Prediction Model**:
   - Monitor the water need prediction
   - The model automatically learns from sensor data
   - Use the "Retrain Model" button to manually retrain the model
   - Use the "Reset Training Data" button to clear historical data

## Browser Compatibility

The dashboard is compatible with:
- Chrome 70+
- Firefox 63+
- Safari 12+
- Edge 79+

## Data Storage

The application stores the following data in your browser's local storage:
- MQTT password (session storage)
- Training data for the prediction model (local storage)
- Model coefficients (local storage)

No data is sent to external servers other than the configured MQTT broker.

## Customization

You can customize the dashboard by:
- Modifying the CSS styles in the `<style>` section
- Adjusting the initial model coefficients in the JavaScript code
- Changing the MQTT topics to match your ESP8266 configuration

## Troubleshooting

Common issues:
1. **Connection Failures**:
   - Verify MQTT broker URL and port
   - Check username and password
   - Ensure your network allows websocket connections

2. **No Data Updates**:
   - Verify ESP8266 is publishing to the correct topics
   - Check MQTT subscription status
   - Verify sensor connections on your ESP8266

3. **Prediction Model Issues**:
   - Click "Reset Training Data" and allow the system to collect new data
   - Ensure all sensors are providing valid readings

## License

This project is released under the MIT License. See the LICENSE file for details.

## Credits

- HiveMQ Cloud for MQTT broker services
- Paho MQTT JavaScript client library
- Font Awesome for icons

---

Created by Bishal Das. For questions or support, please contact bishaldas@example.com
