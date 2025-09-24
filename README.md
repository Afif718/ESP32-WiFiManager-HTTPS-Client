# ESP32-WiFiManager-HTTPS-Client

ESP32 project that automatically manages WiFi connections and sends sensor data to a server via HTTPS.

## Features

- **Auto WiFi Management**: Connects using saved credentials or opens a captive portal for setup
- **HTTPS Data Transmission**: Securely sends JSON data to your server with API key authentication
- **Dual Data Streams**: Sends both environmental data (temperature, humidity) and water quality data (pH, dissolved oxygen, etc.)
- **Retry Logic**: Automatically retries failed connections with exponential backoff
- **Reset Function**: Press EN button to clear saved WiFi credentials

## How it Works

1. **WiFi Setup**: On first boot, creates "ESP32_AP" hotspot for WiFi configuration
2. **Auto Connect**: Remembers WiFi settings and connects automatically on subsequent boots
3. **Data Collection**: Reads sensor values (currently hardcoded for testing)
4. **HTTPS POST**: Sends data to two endpoints: `/env_data` and `/pond_data`
5. **Error Handling**: Retries failed connections up to 4 times with increasing delays

## Configuration

Update these values in the code:
```cpp
const char* server = "your-server.example.com";
const char* apiKey = "YOUR_API_KEY";
```

## Use Cases

- **Environmental Monitoring**: Temperature, humidity, pressure sensing
- **Water Quality Monitoring**: pH, dissolved oxygen, TDS measurement for ponds/aquariums
- **IoT Data Logger**: Any application requiring secure data transmission to a server
- **Remote Monitoring**: Monitor sensors from anywhere with internet connection

## JSON Data Format

**Environmental Data:**
```json
{
  "api_key": "YOUR_API_KEY",
  "sensor": "SHT20",
  "Temperature": 22.3,
  "Humidity": 55.2,
  "Pressure": 0
}
```

**Water Quality Data:**
```json
{
  "api_key": "YOUR_API_KEY",
  "nitrogen": 0.0,
  "ammonia": 0.0,
  "do": 7.2,
  "waterPH": 7.1,
  "waterTemp": 26.4
}
```

## Installation

1. Install required libraries: `WiFiManager` and `ArduinoJson`
2. Update server URL and API key in the code
3. Upload to ESP32
4. Connect to "ESP32_AP" hotspot and configure WiFi

## Screenshots
<img width="226" height="245" alt="image" src="https://github.com/user-attachments/assets/8f3a0e5b-9ee4-4bb8-a0c4-f5ea0e7922c4" />

<img width="886" height="450" alt="image" src="https://github.com/user-attachments/assets/a2d6a55d-2e02-4c39-a175-e20fcc8db4cb" />



## Libraries Used

- WiFiManager - For easy WiFi configuration
- ArduinoJson - For JSON data formatting
- WiFiClientSecure - For HTTPS connections
