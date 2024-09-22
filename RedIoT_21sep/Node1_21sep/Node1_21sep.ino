/**
 * \file Node1_21sep.ino
 * \brief Mesh network node for read temperature and humidity by sensor using DHT11, ESP32, and painlessMesh.
 * 
 * This code creates a mesh network using ESP32 devices to broadcast sensor readings (temperature and humidity) 
 * from a DHT11 sensor. The devices synchronize time via an NTP server and enter deep sleep mode to save power.
 *
 * \author Avocados
 * \date 2024
 */

#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"
#include <WiFi.h>
#include "time.h"

// WiFi and NTP configuration
const char* ssid     = "SSID";      /**< SSID of the WiFi network */
const char* password = "Contrasena";/**< Password of the WiFi network */

const char* ntpServer = "pool.ntp.org";        /**< NTP server to synchronize time */
const long  gmtOffset_sec = -3600*6;           /**< GMT offset in seconds */
const int   daylightOffset_sec = 3600;         /**< Daylight saving time offset in seconds */

// Mesh network configuration
#define   MESH_PREFIX     "avocados"    /**< Mesh network prefix */
#define   MESH_PASSWORD   "avocados123" /**< Mesh network password */
#define   MESH_PORT       5555          /**< Mesh network port */

// DHT sensor configuration
#define DHTPIN 4     /**< Pin connected to the DHT sensor */
#define DHTTYPE DHT11 /**< DHT type, DHT11 in this case */
DHT dht(DHTPIN, DHTTYPE); /**< DHT sensor object */

// Sleep configuration
#define uS_TO_S_FACTOR 1000000  /**< Conversion factor for microseconds to seconds */
#define TIME_TO_SLEEP  5        /**< Time ESP32 will go to sleep (in seconds) */

// Node and readings
int nodeNumber = 1; /**< Node number identifier */
String readings;    /**< String to send to other nodes with sensor readings */

// Scheduler and mesh objects
Scheduler userScheduler; /**< Scheduler to control tasks */
painlessMesh mesh;       /**< PainlessMesh object for mesh communication */

// Time variables for RTC
RTC_DATA_ATTR char hour[3];    /**< Hour string */
RTC_DATA_ATTR char minut[3];   /**< Minute string */
RTC_DATA_ATTR char sec[3];     /**< Second string */
RTC_DATA_ATTR bool yettimed= 0;/**< Flag to check if time has been synchronized */

/**
 * \brief Sends the sensor readings as a JSON string via mesh network.
 */
void sendMessage();

/**
 * \brief Callback function when a message is received via the mesh network.
 * 
 * \param from Node ID from which the message was received.
 * \param msg The message received.
 */
void receivedCallback(uint32_t from, String &msg);

/**
 * \brief Callback function when a new connection is established in the mesh network.
 * 
 * \param nodeId The ID of the node that has connected.
 */
void newConnectionCallback(uint32_t nodeId);

/**
 * \brief Callback function when a connection is changed in the mesh network.
 */
void changedConnectionCallback();

/**
 * \brief Callback function when the mesh node time is adjusted.
 * 
 * \param offset Time offset in milliseconds.
 */
void nodeTimeAdjustedCallback(int32_t offset);

/**
 * \brief Obtains the local time from the NTP server.
 */
void printLocalTime();

/**
 * \brief Sets up the ESP32, initializes the mesh network and sensor, and connects to the WiFi network for time synchronization.
 */
void setup();

/**
 * \brief Main loop that updates the mesh network.
 */
void loop();

//Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 5 , TASK_FOREVER , &sendMessage);

/**
 * \brief Gets the sensor readings (temperature, humidity) and the current time.
 * 
 * This function reads temperature and humidity from the DHT11 sensor and combines
 * this data with the current time (retrieved via NTP) to create a JSON object.
 * 
 * \return String containing sensor readings and time in JSON format.
 */
String getReadings () {
  JSONVar jsonReadings;

  // Get the current time
  printLocalTime();
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minut);
  Serial.print(":");
  Serial.println(sec);

  // Read humidity and temperature from the sensor
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  // Print readings to serial monitor
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C "));

  // Create a JSON object with the readings
  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = temperature;
  jsonReadings["hum"] = humidity;
  jsonReadings["hour"] = hour;
  jsonReadings["min"] = minut;
  jsonReadings["sec"] = sec;

  // Convert the JSON object to a string
  readings = JSON.stringify(jsonReadings);
  
  return readings; // Return the JSON string
}


void sendMessage () {
  String msg = getReadings();
  Serial.println(msg);
  mesh.sendBroadcast(msg);
  
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());

  int hour = myObject["hour"];   
  int min = myObject["min"];
  int sec = myObject["sec"];

  int node = myObject["node"];

  String color = myObject["color"];

  Serial.print(hour);
  Serial.print(":");
  Serial.print(minut);
  Serial.print(":");
  Serial.println(sec);

  Serial.print("Node: ");
  Serial.println(node);

  Serial.print("Led color: ");
  Serial.println(color);
  taskSendMessage.disable();
  yettimed= 1;
  esp_deep_sleep_start();
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
 
  strftime(hour,3, "%H", &timeinfo);
  
  strftime(minut,3, "%M", &timeinfo);

  strftime(sec,3, "%S", &timeinfo);
}
void setup() {

  Serial.begin(115200);
  
  dht.begin();
  if (yettimed== 0){
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    
    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
    
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  else{
    configTime(gmtOffset_sec, daylightOffset_sec,ntpServer);
    printLocalTime();
  
  }
   Serial.print(hour);
    Serial.print(":");
    Serial.print(minut);
    Serial.print(":");
    Serial.println(sec);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();
  
}