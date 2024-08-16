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
#define TEAM_INITIALS "CRFZ"                   // Iniciales del equipo
#define SECRET_NAME_TEAM "Carlos-Fizet"        // Nombre del equipo
#define SECRET_CH_ID 2627355                   // ID del canal de ThingSpeak
#define SECRET_WRITE_APIKEY "IRFNZHLR3V1U3GNB" // Clave API de ThingSpeak

boolean Calibration_Weight_Sensor = false; // Modo de calibración de sensor de peso
double Known_Weight = 14.7;                // Peso conocido para la calibración
double Scale_Calibration_Result = 263;     // Resultado de calibración de la balanza
// CHANGE // // CHANGE // // CHANGE // // CHANGE //

// Definiciones para pines
#define SDA_PIN 13
#define SCL_PIN 12
#define VCC_VIRTUAL_PIN_1 27
#define GND_VIRTUAL_PIN_2 14
#define DHT_VIRTUAL_PIN_GND 33
#define DHT_VIRTUAL_PIN_VCC 26
#define DHT_DATA_PIN 25
#define ONE_WIRE_BUS 15
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 4;

// Inicialización de objetos
HX711 scale;
WiFiClient client;
Ticker ticker;
DHT dht(DHT_DATA_PIN, DHT11);
BH1750 lightMeter;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress Sensor_1, Sensor_2;

// Variables globales
int LED_WiFi = 2;
float tempC_Sensor_1;
float tempC_Sensor_2;
float Weight_Sensor;
unsigned long previousMillis = 0; // Declaración de la variable global
const long interval = 1000;       // Declaración de la variable global
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // Intervalo de reconexión en milisegundos

// Función de actualización de estado
void tick_update_channel()
{
    ThingSpeak.setField(1, tempC_Sensor_1);
    ThingSpeak.setField(2, tempC_Sensor_2);
    ThingSpeak.setField(3, Weight_Sensor);
    ThingSpeak.setField(4, dht.readTemperature());
    ThingSpeak.setField(5, dht.readHumidity());
    ThingSpeak.setField(6, lightMeter.readLightLevel());
    String myStatus = "Deshidratador IoT OK";
    ThingSpeak.setStatus(myStatus);

    int x = ThingSpeak.writeFields(SECRET_CH_ID, SECRET_WRITE_APIKEY);
    if (x == 200)
    {
        Serial.println("Actualización del canal exitosa.");
    }
    else
    {
        Serial.println("Problema actualizando el canal. Código de error HTTP: " + String(x));
    }
}

void tick()
{
    // toggle state
    int state = digitalRead(LED_WiFi); // get the current state of GPIO1 pin
    digitalWrite(LED_WiFi, !state);    // set pin to the opposite state
}

// Función para imprimir la dirección del dispositivo
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

// Función para imprimir la resolución del dispositivo
void printResolution(DeviceAddress deviceAddress)
{
    Serial.print("Resolución: ");
    Serial.print(sensors.getResolution(deviceAddress));
    Serial.println();
}

// Función para verificar y reconectar a WiFi si es necesario
void checkWiFiConnection()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - lastReconnectAttempt >= RECONNECT_INTERVAL)
        {
            lastReconnectAttempt = currentMillis;
            Serial.println("Intentando reconectar a WiFi...");
            WiFiManager wm;
            wm.setTimeout(60);                                               // 3 minutos para intentar la conexión
            String ssid = "Deshidratador IoT 2024 " + String(TEAM_INITIALS); // Nombre de la red
            digitalWrite(LED_WiFi, HIGH);
            if (!wm.autoConnect(ssid.c_str()))
            {
                Serial.println("No se pudo conectar. Intentando de nuevo...");
            }
            else
            {
                Serial.println("Reconexión exitosa.");
                digitalWrite(LED_WiFi, LOW);
                
            }
        }
    }
}

void setup()
{
    pinMode(LED_WiFi, OUTPUT);
    digitalWrite(LED_WiFi, LOW);
    ticker.attach(0.6, tick);
    Serial.begin(115200);
    Serial.println("Deshidratador IoT TEC EAN 2024");

    // Configuración de pines virtuales
    pinMode(VCC_VIRTUAL_PIN_1, OUTPUT);
    pinMode(GND_VIRTUAL_PIN_2, OUTPUT);
    pinMode(DHT_VIRTUAL_PIN_GND, OUTPUT);
    pinMode(DHT_VIRTUAL_PIN_VCC, OUTPUT);
    digitalWrite(VCC_VIRTUAL_PIN_1, HIGH);
    digitalWrite(GND_VIRTUAL_PIN_2, LOW);
    digitalWrite(DHT_VIRTUAL_PIN_GND, LOW);
    digitalWrite(DHT_VIRTUAL_PIN_VCC, HIGH);

    // Obtener la dirección MAC del ESP32 y formatearla
    String name_iot = "Deshidratador IoT 2024 " + String(TEAM_INITIALS); // Nombre de la red sin MAC

    if (Calibration_Weight_Sensor == false)
    {
        Serial.println("Modo Normal");
        WiFiManager wm;
        bool res;
        res = wm.autoConnect(name_iot.c_str()); // Usar la cadena con el nombre modificado
        ticker.attach(0.1, tick);

        if (!res)
        {
            Serial.println("No se pudo conectar");
        }
        else
        {
            Serial.println("Conectado... ¡yey :)");
            ticker.detach();
            digitalWrite(LED_WiFi, LOW);
        }
    }
    else
    {
        Serial.println("Mode Calibration Weight Sensor");
    }
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    if (!Calibration_Weight_Sensor)
    {
        sensors.begin();
        Serial.println("Buscando sensores de temperatura DS18B20...");
        if (!sensors.getAddress(Sensor_1, 0))
        {
            Serial.println("Error en Sensor 1");
            digitalWrite(LED_WiFi, HIGH);
            while (true)
                ;
        }
        if (!sensors.getAddress(Sensor_2, 1))
        {
            Serial.println("Error en Sensor 2");
            digitalWrite(LED_WiFi, HIGH);
            while (true)
                ;
        }
        sensors.setResolution(Sensor_1, 12);
        sensors.setResolution(Sensor_2, 12);
        ThingSpeak.begin(client);

        ticker.attach(20, tick_update_channel);
        Serial.println("Inicializando la balanza");
        scale.set_scale(Scale_Calibration_Result);
        scale.tare();
        Wire.begin(SDA_PIN, SCL_PIN);
        if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
        {
            Serial.println("Sensor de luz BH1750 inicializado correctamente.");
        }
        else
        {
            Serial.println("Error al inicializar el BH1750.");
        }
        dht.begin();
    }
}

void loop()
{
    checkWiFiConnection(); // Verifica y reconecta si es necesario

    if (!Calibration_Weight_Sensor)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            sensors.requestTemperatures();
            float humidity = dht.readHumidity();
            float tempC_Ambient = dht.readTemperature();
            if (isnan(humidity) || isnan(tempC_Ambient))
            {
                Serial.println("Error al leer el DHT11.");
            }
            else
            {
                Serial.println("-----------------------------");
                digitalWrite(LED_WiFi, HIGH);
                Serial.print("Humedad: ");
                Serial.print(humidity);
                Serial.print("%  Temperatura Ambiente: ");
                Serial.print(tempC_Ambient);
                Serial.println("°C");
            }

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

            tempC_Sensor_1 = sensors.getTempC(Sensor_1);
            if (tempC_Sensor_1 == DEVICE_DISCONNECTED_C)
            {
                Serial.println("Error: No se pudo leer el dato de temperatura");
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
                Serial.println("Error: No se pudo leer el dato de temperatura");
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
            digitalWrite(LED_WiFi, LOW);
            previousMillis = currentMillis;
        }
    }
    else
    {
        if (scale.is_ready())
        {
            scale.set_scale();
            Serial.println("Calibrando... Retire los pesos de la báscula.");
            delay(5000);
            scale.tare();
            Serial.println("Calibracion hecha...");
            Serial.print("Coloque un peso conocido en la báscula...");
            delay(5000);
            long reading = scale.get_units(10);
            Serial.print("Resultado de la calibración: ");
            long calibration_final = reading / Known_Weight;
            Serial.println(calibration_final);
        }
        else
        {
            Serial.println("Balanza no encontrada.");
        }
        delay(1000);
    }
}
