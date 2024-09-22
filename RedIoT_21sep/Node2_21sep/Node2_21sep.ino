/**
 * @file Node2_21sep.ino
 * @brief Mesh network node controlling LED color based on received temperature and humidity data.
 * 
 * This code creates a mesh network using ESP32 devices to broadcast and receive sensor readings 
 * (temperature and humidity) from the other node. Based on the temperature, it changes the color of an RGB LED.
 * The nodes also synchronize time via an NTP server and enter deep sleep mode to save power.
 *
 * @author Avocados
 * @date 2024
 */

#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <WiFi.h>
#include "time.h"

// WiFi and NTP configuration
const char* ssid     = "SSID";      /**< SSID of the WiFi network */
const char* password = "CONTRASENA";/**< Password of the WiFi network */

const char* ntpServer = "pool.ntp.org";        /**< NTP server to synchronize time */
const long  gmtOffset_sec = -3600*6;           /**< GMT offset in seconds */
const int   daylightOffset_sec = 3600;         /**< Daylight saving time offset in seconds */

// Mesh network configuration
#define   MESH_PREFIX     "avocados"    /**< Mesh network prefix */
#define   MESH_PASSWORD   "avocados123" /**< Mesh network password */
#define   MESH_PORT       5555          /**< Mesh network port */

// Sleep configuration
#define uS_TO_S_FACTOR 1000000  /**< Conversion factor for microseconds to seconds */
#define TIME_TO_SLEEP  5.1      /**< Time ESP32 will go to sleep (in seconds) */

// RGB LED pin configuration
int redPin = 13;   /**< Pin for red LED */
int greenPin = 12; /**< Pin for green LED */
int bluePin = 14;  /**< Pin for blue LED */

// Node and readings
int nodeNumber = 2; /**< Node number identifier */
int node = 0;       /**< Node number received from another node */
double temp = 0;    /**< Temperature received from another node */
double hum = 0;     /**< Humidity received from another node */
String Color;       /**< LED color based on the temperature */

// String to send sensor readings to other nodes
String readings;    /**< JSON string containing the node number and color */

// Scheduler and mesh objects
Scheduler userScheduler; /**< Scheduler to control tasks */
painlessMesh mesh;       /**< PainlessMesh object for mesh communication */

// Time variables for RTC
RTC_DATA_ATTR char hour[3];    /**< Hour string */
RTC_DATA_ATTR char minut[3];   /**< Minute string */
RTC_DATA_ATTR char sec[3];     /**< Second string */
RTC_DATA_ATTR bool yettimed= 0;/**< Flag to check if time has been synchronized */

/**
 * @brief Send the LED color and time as JSON string via mesh network and turn on the deespleep mode.
 */
void sendMessage();


/**
 * @brief Callback function when a message is received via the mesh network.
 * 
 * @param from Node ID from which the message was received.
 * @param msg The message received.
 */
void receivedCallback(uint32_t from, String &msg);

/**
 * @brief Callback function when a new connection is established in the mesh network.
 * 
 * @param nodeId The ID of the node that has connected.
 */
void newConnectionCallback(uint32_t nodeId);

/**
 * @brief Sets the color of the RGB LED based on the temperature.
 * 
 * @param temp Temperature value.
 * @param hum Humidity value.
 */
void condition(float temp, int hum);

/**
 * @brief Callback function when a connection is changed in the mesh network.
 */
void changedConnectionCallback();

/**
 * @brief Callback function when the mesh node time is adjusted.
 * 
 * @param offset Time offset in milliseconds.
 */
void nodeTimeAdjustedCallback(int32_t offset);

/**
 * @brief Sets the color of the RGB LED.
 * 
 * @param redValue Red color intensity (0-255).
 * @param greenValue Green color intensity (0-255).
 * @param blueValue Blue color intensity (0-255).
 */
void setColor(int redValue, int greenValue, int blueValue);

/**
 * @brief Obtains the local time from the NTP server.
 */
void printLocalTime();

/**
 * @brief Sets up the ESP32, initializes the mesh network, RGB LED, and connects to WiFi for time synchronization.
 */
void setup();

/**
 * @brief Main loop that updates the mesh network.
 */
void loop();

//Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 5 , TASK_FOREVER, &sendMessage);

/**
 * @brief Gets the LED color and the current time.
 * 
 * This function gets the LED color and combines
 * this data with the current time (retrieved via NTP) to create a JSON object.
 * 
 * @return String containing LED color and time in JSON format.
 */

String getReadings () {
  JSONVar jsonReadings;
  printLocalTime();
  
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minut);
  Serial.print(":");
  Serial.println(sec);

  jsonReadings["node"] = nodeNumber;
  jsonReadings["color"] = Color;
  jsonReadings["hour"] = hour;
  jsonReadings["min"] = minut;
  jsonReadings["sec"] = sec;

 
  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage () {
  String msg = getReadings();
  Serial.println(msg);
  mesh.sendBroadcast(msg);
  taskSendMessage.disable();
  yettimed= 1;
  esp_deep_sleep_start();
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());

  int hour = myObject["hour"];   
  int min = myObject["min"];
  int sec = myObject["sec"];

  node = myObject["node"];

  temp = myObject["temp"];
  
  hum = myObject["hum"];

  Serial.print(hour);
  Serial.print(":");
  Serial.print(minut);
  Serial.print(":");
  Serial.println(sec);
  
  Serial.print("Nodep: ");
  Serial.println(node);
  Serial.print("Temperaturep: ");
  Serial.print(temp);
  Serial.println(" C");
  Serial.print("Humidityp: ");
  Serial.print(hum);
  Serial.println(" %");

  condition(temp, hum);
  
  taskSendMessage.enable();
  

}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}
void condition( float temp, int hum){
  if  (temp > 29){
    setColor(255, 0, 0);
    Color = "Red";
    }
  else {
    setColor(0,  255, 0);
    Color = "Green";
  }
  Serial.print("Led state changed to = ");
  Serial.println(Color);
}
void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}
void setColor(int redValue, int greenValue,  int blueValue) {
  analogWrite(redPin, redValue);
  analogWrite(greenPin,  greenValue);
  analogWrite(bluePin, blueValue);
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
  
  pinMode ( redPin ,   OUTPUT ) ;              
  pinMode ( greenPin ,  OUTPUT ) ;
  pinMode ( bluePin ,  OUTPUT ) ;
  
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
  
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();

}