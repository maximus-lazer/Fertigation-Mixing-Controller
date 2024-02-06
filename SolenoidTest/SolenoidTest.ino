#include <AFMotor.h>                //you must download and install this library: https://drive.google.com/file/d/1zsMywqJjvzgMBoVZyrYly-2hXePFXFzw/view?usp=sharing



//These lines declare which ports your motors will be connected to on the motor shield.
AF_DCMotor solenoid(2);


// In setup, we tell bluetooth communication to start and set all of our motors to not move
void setup() {
  Serial.begin(9600);
  solenoid.setSpeed(255);
}

void loop() {
  Serial.println("FORWARD");
  solenoid.run(FORWARD);     // These comands tell the motors what speed and direction to move
  // solenoid.setSpeed(255);
  delay(2000);
  Serial.println("STOP");
  solenoid.run(RELEASE);
  // solenoid.setSpeed(0);
  delay(5000);
  Serial.println("BACKWARD");
  solenoid.run(BACKWARD);     // These comands tell the motors what speed and direction to move
  // solenoid.setSpeed(255);
  delay(2000);
  Serial.println("STOP");
  solenoid.run(RELEASE);
  // solenoid.setSpeed(0);
  delay(5000);
  

}

