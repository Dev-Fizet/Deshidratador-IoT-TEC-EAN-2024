#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "ThingSpeak.h"
#include "HX711.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

// CHANGE // // CHANGE // // CHANGE // // CHANGE //
#define TEAM_INITIALS "CRFZ"                 // Escribe las iniciales de tu equipo aquí
#define SECRET_NAME_TEAM "Carlos-Fizet"      // replace Carlos-Fizet with your name Team
boolean Calibration_Weight_Sensor = false;     // false Not Calibration // true Calibration
double Known_Weight = 14.7;                    // Replace 14.6 with the value of the known object's weight in grams
double Scale_Calibration_Result = 773;         // Replace 817 with the value result after of the process of calibration
#define SECRET_CH_ID 2258947                   // replace 0000000 with your channel number of ThingSpeak
#define SECRET_WRITE_APIKEY "WFQCMUA1Y5X6QIE5" // replace XYZ with your channel write API Key of ThingSpeak
// CHANGE // // CHANGE // // CHANGE // // CHANGE //

// HX711 Circuit wiring
const int LOADCELL_DOUT_PIN = 2; // GPIO2 D2
const int LOADCELL_SCK_PIN = 4;  // GPIO4 D4

HX711 scale;

WiFiClient client;
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;
String myStatus = "";

unsigned long previousMillis = 0;
const long interval = 1000;

Ticker ticker;
int LED_WiFi = 2;
float tempC_Sensor_1;
float tempC_Sensor_2;
float Weight_Sensor;

// Sensores adicionales
#define DHTPIN 5          // Pin GPIO donde se conecta el DHT11
#define DHTTYPE DHT11     // Define el tipo de sensor DHT
DHT dht(DHTPIN, DHTTYPE);

// Pines I2C para el sensor BH1750
#define SDA_PIN 21  // Pin SDA
#define SCL_PIN 22  // Pin SCL
BH1750 lightMeter;

void tick()
{
  // toggle state
  int state = digitalRead(LED_WiFi); // get the current state of GPIO1 pin
  digitalWrite(LED_WiFi, !state);    // set pin to the opposite state
}

void tick_update_channel()
{
  ThingSpeak.setField(1, tempC_Sensor_1);
  ThingSpeak.setField(2, tempC_Sensor_2);
  ThingSpeak.setField(3, Weight_Sensor);
  ThingSpeak.setField(4, dht.readTemperature());  // Temperatura ambiente
  ThingSpeak.setField(5, dht.readHumidity());     // Humedad relativa
  ThingSpeak.setField(6, lightMeter.readLightLevel()); // Luz en lux
  myStatus = String("Deshidratador IoT OK");
  ThingSpeak.setStatus(myStatus);

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

#define ONE_WIRE_BUS 15 // GPIO15 D15
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

DeviceAddress Sensor_1, Sensor_2;

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
}

void setup()
{
  pinMode(LED_WiFi, OUTPUT);
  digitalWrite(LED_WiFi, LOW);
  ticker.attach(0.6, tick);

  Serial.begin(115200);
  Serial.println("Deshidratador IoT TEC EAN 2024");

  // Obtener la dirección MAC del ESP32 y formatearla
  String mac = WiFi.macAddress();
  mac.replace(":", "");  // Quitar los dos puntos de la MAC
  String name_iot = "Deshidratador IoT 2024" + mac + " " + TEAM_INITIALS;  // Concatenar MAC e iniciales al nombre de la red

  if (Calibration_Weight_Sensor == false)
  {
    Serial.println("Mode Normal");
    WiFiManager wm;

    bool res;
    res = wm.autoConnect(name_iot.c_str()); // Usar la cadena con el nombre modificado
    ticker.attach(0.1, tick);

    if (!res)
    {
      Serial.println("Failed to connect");
    }
    else
    {
      Serial.println("connected...yeey :)");
      ticker.detach();
      digitalWrite(LED_WiFi, LOW);
    }
  }
  else
  {
    Serial.println("Mode Calibration Weight Sensor");
  }

  // Inicializar HX711
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  if (Calibration_Weight_Sensor == false)
  {
    sensors.begin();
    Serial.println("Buscando Sensores de Temperatura DS18B20...");

    if (!sensors.getAddress(Sensor_1, 0))
    {
      Serial.println("Error en Sensor 1");
      digitalWrite(LED_WiFi, HIGH);
      while (true);
    }
    if (!sensors.getAddress(Sensor_2, 1))
    {
      Serial.println("Error en Sensor 2");
      digitalWrite(LED_WiFi, HIGH);
      while (true);
    }
    Serial.println("Sensores OK!");
    sensors.setResolution(Sensor_1, 12);
    sensors.setResolution(Sensor_2, 12);

    ThingSpeak.begin(client);
    ticker.attach(20, tick_update_channel);

    Serial.println("Inicializando la balanza");

    scale.set_scale(Scale_Calibration_Result);
    scale.tare(); // reset the scale to 0

    Serial.println("Después de configurar la balanza:");
    Serial.print("lectura: \t\t");
    Serial.println(scale.read()); 

    Serial.print("lectura promedio: \t\t");
    Serial.println(scale.read_average(20)); 

    Serial.print("obtener valor: \t\t");
    Serial.println(scale.get_value(5)); 

    Serial.print("obtener unidades: \t\t");
    Serial.println(scale.get_units(5), 1);

    // Inicializar el sensor de luz BH1750
    Wire.begin(SDA_PIN, SCL_PIN);  // Establecer los pines I2C
    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
    {
      Serial.println("Sensor de luz BH1750 inicializado correctamente.");
    }
    else
    {
      Serial.println("Error al inicializar el BH1750.");
    }

    // Inicializar el sensor DHT11
    dht.begin();
  }
}

void loop()
{
  if (Calibration_Weight_Sensor == false)
  {
    Serial.println("-----------------------------");
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      sensors.requestTemperatures();

      // Lectura de temperatura y humedad con DHT11
      float humidity = dht.readHumidity();
      float tempC_Ambient = dht.readTemperature();
      
      if (isnan(humidity) || isnan(tempC_Ambient))
      {
        Serial.println("Error al leer el DHT11.");
      }
      else
      {
        Serial.print("Humedad: ");
        Serial.print(humidity);
        Serial.print("%  Temperatura Ambiente: ");
        Serial.print(tempC_Ambient);
        Serial.println("°C");
      }

      // Lectura del sensor de luz BH1750
      float lux = lightMeter.readLightLevel();
      if (lux == -1)
      {
        Serial.println("Error al leer el BH1750.");
      }
      else
      {
        Serial.print("Luz: ");
        Serial.print(lux);
        Serial.println(" lux");
      }

      // Lectura de los sensores DS18B20
      tempC_Sensor_1 = sensors.getTempC(Sensor_1);
      if (tempC_Sensor_1 == DEVICE_DISCONNECTED_C)
      {
        Serial.println("Error: Could not read temperature data");
        return;
      }
      Serial.print("Sensor 1 => ");
      Serial.print("Temp C: ");
      Serial.print(tempC_Sensor_1);
      Serial.print(" Temp F: ");
      Serial.println(DallasTemperature::toFahrenheit(tempC_Sensor_1));

      tempC_Sensor_2 = sensors.getTempC(Sensor_2);
      if (tempC_Sensor_2 == DEVICE_DISCONNECTED_C)
      {
        Serial.println("Error: Could not read temperature data");
        return;
      }
      Serial.print("Sensor 2 => ");
      Serial.print("Temp C: ");
      Serial.print(tempC_Sensor_2);
      Serial.print(" Temp F: ");
      Serial.println(DallasTemperature::toFahrenheit(tempC_Sensor_2));

      Weight_Sensor = scale.get_units(10);
      Serial.print("Peso: ");
      Serial.print(Weight_Sensor);
      Serial.println(" gramos");

      previousMillis = currentMillis;
    }
  }
}
