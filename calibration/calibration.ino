#include "MPU9250.h"
#include<Wire.h>

#define NUM_ITERATIONS 100

MPU9250 mpu;

float sumAccXBias = 0, sumAccYBias = 0, sumAccZBias = 0;
float sumGyrXBias = 0, sumGyrYBias = 0, sumGyrZBias = 0;


void print_calibration() {
    Serial.println("< calibration parameters >");
    Serial.println("accel bias [g]: ");
    sumAccXBias += (mpu.getAccBiasX() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(mpu.getAccBiasX() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(", ");
    sumAccYBias += (mpu.getAccBiasY() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(mpu.getAccBiasY() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(", ");
    sumAccZBias += (mpu.getAccBiasZ() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.print(mpu.getAccBiasZ() * 1000.f / (float)MPU9250::CALIB_ACCEL_SENSITIVITY);
    Serial.println();
    Serial.println("gyro bias [deg/s]: ");
    sumGyrXBias += (mpu.getGyroBiasX() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(mpu.getGyroBiasX() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(", ");
    sumGyrYBias += (mpu.getGyroBiasY() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(mpu.getGyroBiasY() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.print(", ");
    sumGyrZBias += (mpu.getGyroBiasZ() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
    Serial.println(mpu.getGyroBiasZ() / (float)MPU9250::CALIB_GYRO_SENSITIVITY);
   Serial.print("======================================================================");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(12, 14);
  Wire.setClock(100000);
  delay(2000);
  mpu.setup(0x68); 

  // calibrate anytime you want to
  Serial.println("Accel Gyro calibration will start in 5sec.");
  Serial.println("Please leave the device still on the flat plane.");
  mpu.verbose(true);
  delay(5000);
  for (int i = 0 ; i < NUM_ITERATIONS ; i++) {
    mpu.calibrateAccelGyro();
    print_calibration();
  }
  mpu.verbose(false);
  Serial.println("Final averaged calibration values: ");
  float fAccXBias = sumAccXBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for X Acceleration: ");
  Serial.println(fAccXBias);
  float fAccYBias = sumAccYBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for Y Acceleration: ");
  Serial.println(fAccYBias);
  float fAccZBias = sumAccZBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for Z Acceleration: ");
  Serial.println(fAccZBias);
  float fGyrXBias = sumGyrXBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for X Rotation: ");
  Serial.println(fGyrXBias);
  float fGyrYBias = sumGyrYBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for Y Rotation: ");
  Serial.println(fGyrYBias);
  float fGyrZBias = sumGyrZBias / NUM_ITERATIONS;
  Serial.println("Final averaged calibration values for Z Rotation: ");
  Serial.println(fGyrZBias);
}

void loop() {

}
