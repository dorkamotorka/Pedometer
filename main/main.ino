#include<Wire.h>
#include<Ticker.h>
#include "MPU9250.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <math.h>

// Calibration iterations
#define NUM_ITERATIONS 10

#define CONSTANT_OFFSET_Z 0.27

float sumAccXBias = 0, sumAccYBias = 0, sumAccZBias = 0;
float sumGyrXBias = 0, sumGyrYBias = 0, sumGyrZBias = 0;

// Average human walking frequency aprox. 2-3Hz)
#define STEP_DELAY 250

// Average step acceleration of a person walking is 0.2-0.3g
#define AVG_STEP_ACCEL 0.2
#define MAX_STEP_ACCEL 0.3

// Main loop rate
#define LOOP_RATE 10

#define USERNAME "admin"
#define PASSWORD "admin"
#define MODE 1

#define MPU_ADD 104 // Naslov MPU9250 na I2C vodilu
#define ACC_MEAS_REG 59 // Naslov registra za pospesek
#define GYRO_MEAS_REG 67 // Naslov registra za ziroskop
#define CAL_NO 1000 // Stevilo uzorcev za kalibracijo
#define READ_NO 10 // Stevilo uzorcev za branje

// Calories
float burnedCalories = 0;
float weight = 100.0;
float height = 1.95;
int stepsPer2Sec = 0;
float stride = height / 3; // Aprox. Third of the height is a human step size 
float fspeed = stride / 2; 
float start;

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP request on port 80
MPU9250 mpu;
Ticker readSensor;
uint8_t iter = 0; // Globalni stevec zanke 
float accel_x, accel_y, accel_z;
float gyro_x, gyro_y, gyro_z;
float mag_x, mag_y, mag_z, temp;
float paccel_x = 0, paccel_y = 0, paccel_z = 0;
float pgyro_x = 0, pgyro_y = 0, pgyro_z = 0;
float pmag_x = 0, pmag_y = 0, pmag_z = 0, ptemp = 0;
unsigned int StepsWeb = 0;
float prevMagnitude = 0;
float maxMagnitude = 0;
bool alreadyStep = false;

const char *ssid = "Pirc"; // The name of the Wi-Fi network that will be created
const char *password = "mojaluna";   // The password required to connect to it, leave blank for an open network

void I2CWriteRegister(uint8_t I2CDevice, uint8_t RegAdress, uint8_t Value){
  // I2CDevice - Naslov I2C naprave
  // RegAddress - Naslov registra 
  // Value - Vrednost za vpisati v register
  
  Wire.beginTransmission(I2CDevice);
  Wire.write(RegAdress); // Napravi sporočimo naslov registra, s katerega želimo brati
  Wire.write(Value); // Posljemo vrednost
  Wire.endTransmission();
}

void I2CReadRegister(uint8_t I2CDevice, uint8_t RegAdress, uint8_t NBytes, uint8_t *Value){
  Wire.beginTransmission(I2CDevice);
  Wire.write(RegAdress); // Napravi sporočimo naslov registra, s katerega želimo brati
  Wire.endTransmission(); // Končamo prenos
 
  Wire.requestFrom(I2CDevice, NBytes); // Napravi sporočimo, da želimo prebrati določeno število 8-bitnih registrov
  for (int q = 0; q < NBytes; q++) {
    *Value = (uint8_t)Wire.read(); // Preberemo naslednji 8-bitni register oz. naslednji bajt
    Value++;
    //uint32_t vrednost = Wire.read();
  }
}

void MPU9250_init() {
  // Resetiraj MPU9250 senzora => Register PWR_MGMT_1 (107)
  I2CWriteRegister(MPU_ADD, 107, 128); // 128 = 1000 0000
  delay(500);
  
  uint8_t ID;
  I2CReadRegister(MPU_ADD, 117, 1, &ID); // Preveri ID od senzora => Register WHO_AM_I (117) 
  Serial.println("ID:");
  Serial.println(ID, HEX);
  
  // Gyroscope Conf => Register GYRO_CONFIG (27) 
  // 4 in 3 bit dolocata obseg 
  I2CWriteRegister(MPU_ADD, 27, 0); 
  delay(100); 
  Serial.println("Gyro configured");
  
  // Accelerator Conf => Register ACCEL_CONFIG (28)
  // 4 in 3 bit dolocata obseg 
  // Opciono => Register ACCEL_CONFIG_2 (29)
  I2CWriteRegister(MPU_ADD, 28, 0);
  delay(100);
  Serial.println("Accel configured");
}

bool is_authentified(){
  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie")){
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;
}

void handleLogin(){
  String msg;
  if (server.hasHeader("Cookie")){
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")){
    Serial.println("Disconnection");
    server.sendHeader("Location","/login");
    server.sendHeader("Cache-Control","no-cache");
    server.sendHeader("Set-Cookie","ESPSESSIONID=0");
    server.send(301);
    return;
  }
  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")){
    if (server.arg("USERNAME") == USERNAME &&  server.arg("PASSWORD") == PASSWORD ){
      server.sendHeader("Location","/");
      server.sendHeader("Cache-Control","no-cache");
      server.sendHeader("Set-Cookie","ESPSESSIONID=1");
      server.send(301);
      Serial.println("Log in Successful");
      return;
    }
  msg = "Wrong username/password! try again.";
  Serial.println("Log in Failed");
  }
  String content = "<html><body><form action='/login' method='POST'>To log in enter user id and password:<br>";
  content += "User:<input type='text' name='USERNAME' placeholder='user name'><br>";
  content += "Password:<input type='password' name='PASSWORD' placeholder='password'><br>";
  content += "<input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "<br>";
  content += "You also can go <a href='/inline'>here</a></body></html>";
  server.send(200, "text/html", content);
}

void handleRoot(){
  Serial.println("Enter handleRoot");
  String header;
  if (!is_authentified()){
    server.sendHeader("Location","/login");
    server.sendHeader("Cache-Control","no-cache");
    server.send(301);
    return;
  }

  server.send(200, "text/html", SendHTML(StepsWeb));
}

void handleNotFound(){
  /*
   * Dodajte ustrezno HTML kodo, ki bo uporabnika obvestila, da strani ni bilo mogoče najti.
   * Omogočite tudi, da se uporabnik lahko vrne na glavno stran.
   */
  String vsebina = ""; // dopišite ustrezno HTML kodo
  server.send(404, "text/html", vsebina);
}

void setupWiFiAP() {
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started");

  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());         // Send the IP address of the ESP8266 to the computer
}

void setupWiFiSTA() {
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}

String SendHTML(float StepsWeb){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP8266 Global Server</title>\n";

  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>ESP8266 Global Server</h1>\n";

  ptr +="<p>Number of steps: ";
  ptr +=(int)StepsWeb;
  ptr +="</p>";
  ptr +="<br>\n";
  ptr += "You can access this page until you <a href=\"/login?DISCONNECT=YES\">disconnect</a><br><hr />";
  
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

// Smoothen mpu output
float average(float x, float y) {
  return (x +y)/2;
}

void print_calibration() {
    Serial.println("< calibration parameters >");
    Serial.println("accel bias [g]: ");
    Serial.print(mpu.getAccBiasX() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(", ");
    Serial.print(mpu.getAccBiasY() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(", ");
    Serial.print(mpu.getAccBiasZ() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.println();
    Serial.println("gyro bias [deg/s]: ");
    Serial.print(mpu.getGyroBiasX() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(", ");
    Serial.print(mpu.getGyroBiasY() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(", ");
    Serial.print(mpu.getGyroBiasZ() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.println();
    Serial.println("mag bias [mG]: ");
    Serial.print(mpu.getMagBiasX());
    Serial.print(", ");
    Serial.print(mpu.getMagBiasY());
    Serial.print(", ");
    Serial.print(mpu.getMagBiasZ());
    Serial.println();
    Serial.println("mag scale []: ");
    Serial.print(mpu.getMagScaleX());
    Serial.print(", ");
    Serial.print(mpu.getMagScaleY());
    Serial.print(", ");
    Serial.print(mpu.getMagScaleZ());
    Serial.println();
}

// SETUP FUNCTION
void setup() {
  Serial.begin(115200);
  Wire.begin(12,14); // SDA - 12 pin // SCL - 14 pin
  Wire.setClock(100000);

  if (!mpu.setup(0x68)) {  // change to your own address
    while (1) {
      Serial.println("MPU connection failed. Please check your connection with `connection_check` example.");
      delay(5000);
    }
  }

  // Calibration
  Serial.println("Accel Gyro calibration will start in 5sec.");
  Serial.println("Please leave the device still on the flat plane.");
  mpu.verbose(true);
  delay(5000);
  mpu.calibrateAccelGyro();
  print_calibration();

  if (MODE == 0) { setupWiFiAP(); }
  else if (MODE == 1) { setupWiFiSTA(); }

  //**** nastavitve strežnika:
  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works without need of authentification");
  });
  server.onNotFound(handleNotFound);
  
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
  
  // zaženemo strežnik:
  server.begin();

  Serial.println("HTTP server started");
  start = millis(); 
}

void loop() {
  // Linear Acceleration has gravity acceleration substracted
  if (mpu.update()) {
    accel_x = mpu.getAccX();
    //accel_x = mpu.getLinearAccX();
    accel_x = average(accel_x, paccel_x);
    paccel_x = accel_x;
    //Serial.println(accel_x);
    accel_y = mpu.getAccY();
    //accel_y = mpu.getLinearAccY();
    accel_y = average(accel_y, paccel_y);
    paccel_y = accel_y;
    //Serial.println(accel_y);
    accel_z = mpu.getAccZ() - CONSTANT_OFFSET_Z;
    //accel_z = mpu.getLinearAccZ();
    //Serial.println(accel_z);
    accel_z = average(accel_z, paccel_z);
    paccel_z = accel_z;
    gyro_x = mpu.getGyroX();
    gyro_y = mpu.getGyroY();
    gyro_z = mpu.getGyroZ();
  }
  
  server.handleClient();
  float magnitude = sqrt(pow(accel_x, 2) + pow(accel_y, 2) + pow(accel_z, 2)) - 1; //Subtract gravitation
  prevMagnitude = magnitude;
  //Serial.println(magnitude);
  /*
  if (magnitude > maxMagnitude) {
    maxMagnitude = magnitude;
    Serial.println(magnitude);
  }
  */

  if ((millis() - start) > 2000) {
    start = millis();
    //Serial.println("2sec");
    float speed = stepsPer2Sec * fspeed;
    burnedCalories += (speed * weight/400);
    //Serial.println("Burned calories: ");
    //Serial.println(burnedCalories);
    stepsPer2Sec = 0;
  }

  // toggle boolean when crossing the line second time
  if (magnitude < AVG_STEP_ACCEL) {
    alreadyStep = false;  
  }

  // NOTE: Neccesary in order not to count the step when acceleration going down!
  if (magnitude > MAX_STEP_ACCEL) {
    alreadyStep = true; 
  }

  // Prevent non-step acceleration to count
  if (magnitude > AVG_STEP_ACCEL && 
      magnitude < MAX_STEP_ACCEL && 
      prevMagnitude > AVG_STEP_ACCEL && 
      prevMagnitude < MAX_STEP_ACCEL && 
      !alreadyStep) { // only update step when crossing the threshold for the first time
    StepsWeb += 1;
    stepsPer2Sec += 1;
    Serial.println("STEP");
    Serial.println(StepsWeb);
    delay(STEP_DELAY); // UGLY low pass filter
    alreadyStep = true; //
  }
  delay(1 / LOOP_RATE * 1000); // "reduce noice" 
}
