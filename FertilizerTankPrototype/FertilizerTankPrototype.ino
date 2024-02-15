/*
*/
// #include "ArduinoLowPower.h"
// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5
#define waterPIN D7
#define fertilizerPIN D8
#define sleep D11
#define topValvesIn1 D10
#define topValvesIn2 D9


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define MAXBITS 1520.0
#define MAXPERCENT 0.90 // Percent threshold for full Tank
#define MINPERCENT 0.20 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
float fertPercent = .40; // Percent threshold for filling fertilizer TODO: Change this to read from controller DIO

int startBit = 0;
unsigned long timer;
const int pulse = 100;

int sensorValueR = 0;
int sensorValueL = 0;
volatile float voltageR = 0;
volatile float voltageL = 0;
volatile float percentFullL; // Percent threshold when Left Tank is full
volatile float percentFullR; // Percent threshold when Right Tank is full

enum ErrorType {FLOAT_READ} err;
enum SourceValveState {CLOSE, OPEN} waterSource, fertilizerSource;
enum SystemState {WAIT_FOR_START, FERT_INPUT, ACTIVE, SLEEP, ERROR} systemState, preErrorState;
enum TankState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} tankState;

/**
 * Runs once controller turns on or resets
 */
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  systemState = WAIT_FOR_START;

  // Initializing pins
  pinMode(tankLTOP, OUTPUT);
  pinMode(tankLBOT, OUTPUT);
  pinMode(tankRTOP, OUTPUT);
  pinMode(tankRBOT, OUTPUT);
  pinMode(waterPIN, OUTPUT);
  pinMode(fertilizerPIN, OUTPUT);
  pinMode(sleep, INPUT); // High = Active, Low = Sleep
  pinMode(topValvesIn1, OUTPUT);
  pinMode(topValvesIn2, OUTPUT);

  timer = millis();
}

/**
 * Runs while controller is on
 */
void loop() {

  // read the input on analog pin 0 & 1:
  int sensorValueR = analogRead(A0);
  int sensorValueL = analogRead(A1);

  percentFullL = sensorValueL/MAXBITS;
  percentFullR = sensorValueR/MAXBITS;

  if(percentFullL > 1.0){percentFullL = 1.0;}
  if(percentFullR > 1.0){percentFullR = 1.0;}

  // Running state machine
  systemStateMachine();
}

void systemStateMachine() {
  switch (systemState) {
    case WAIT_FOR_START:
      if((percentFullR > MAXPERCENT)&&(percentFullL < MINPERCENT)){
        tankState = FILL_FERT_RIGHT;
        systemState = FERT_INPUT;
        tankValveSwitch(1);
      }else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT)){
        tankState = FILL_FERT_LEFT;
        systemState = FERT_INPUT;
        tankValveSwitch(0);
      }else {
        if((millis()-timer) > 1000){
          timer = millis();
          Serial.print("Set the float sensors to a starting state");
          Serial.println(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
        }
      }
      waterSource = CLOSE;
      fertilizerSource = CLOSE;

      // if(startBit == 1) // Will be interrupts - Wake up and set state
      //   systemState = FERT_INPUT;

      break;
    case FERT_INPUT:
      //Start timer - Every two minutes
      //Start interrupt - For when bit goes low
      systemState = ACTIVE;
      break;
    case ACTIVE:
      tankStateMachine();

      voltageR = sensorValueR * conversion;
      voltageL = sensorValueL * conversion;
      
      // print out the value you read:


      if((millis()-timer) > 500){
        timer = millis();
        Serial.print("VL: " + String(voltageL) + " VR: " + String(voltageR));
        Serial.print(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
        Serial.print(" TankState: " + String(tankState));
        Serial.print(" SystemState: " + String(systemState));
        Serial.print(" FertSource: " + String(fertilizerSource));
        Serial.println(" WaterSource: " + String(waterSource));
      }
      if((digitalRead(sleep) == 0)&& tankFull())
          systemState = SLEEP;

      break;
    case SLEEP:
      // Put in Deep Sleep
      esp_sleep_enable_timer_wakeup(5 * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
      break;
    case ERROR:
      errorHandler();
      break;
    default:

      break;
  }
}

/**
* The state machine for the entire system
*/
void tankStateMachine() {
  switch (tankState) {
    // Filling Right Tank to Fertilizer percentage
    case FILL_FERT_RIGHT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      digitalWrite(fertilizerPIN, HIGH);
      digitalWrite(waterPIN, LOW);
      if (percentFullR >= fertPercent)
        tankState = FILL_WATER_RIGHT;
      break;
    
    // Filling rest of Right tank with water
    case FILL_WATER_RIGHT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      digitalWrite(fertilizerPIN, LOW);
      digitalWrite(waterPIN, HIGH);
      if (percentFullR >= MAXPERCENT)
        tankState = RIGHT_FULL_WAIT;
      break;
    
    // Waiting for Left Tank to empty
    case RIGHT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      digitalWrite(fertilizerPIN, LOW);
      digitalWrite(waterPIN, LOW);
      if (percentFullL < MINPERCENT){
        tankState = FILL_FERT_LEFT;
      
        tankValveSwitch(0);
      }
      break;

    // Filling Left Tank to Fertilizer percentage
    case FILL_FERT_LEFT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      digitalWrite(fertilizerPIN, HIGH);
      digitalWrite(waterPIN, LOW);
      if (percentFullL >= fertPercent)
        tankState = FILL_WATER_LEFT;
      break;

    // Filling rest of Left tank with water
    case FILL_WATER_LEFT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      digitalWrite(fertilizerPIN, LOW);
      digitalWrite(waterPIN, HIGH);
      if (percentFullL >= MAXPERCENT)
        tankState = LEFT_FULL_WAIT;
      break;

    // Waiting for Right Tank to empty
    case LEFT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      digitalWrite(fertilizerPIN, LOW);
      digitalWrite(waterPIN, LOW);
      if (percentFullR < MINPERCENT) {
        tankState = FILL_FERT_RIGHT;

        tankValveSwitch(1);
      }
      break;
    break;

    // A default state if something goes wrong (should never get here)
    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
        digitalWrite(fertilizerPIN, LOW);
      digitalWrite(waterPIN, LOW);
      break;
  }
}

/**
 * Switch filling and emptying the system
 * @param LeftOrRight: True for Right Fill, False for Left Fill
 */
void tankValveSwitch(bool LeftOrRight) {
  if (LeftOrRight){
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, HIGH); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    

    digitalWrite(tankLTOP, LOW);
    digitalWrite(tankLBOT, HIGH);
    digitalWrite(tankRTOP, HIGH);
    digitalWrite(tankRBOT, LOW);
  }
  else{
    digitalWrite(topValvesIn1, HIGH); 
    digitalWrite(topValvesIn2, LOW); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    

    digitalWrite(tankLTOP, HIGH);
    digitalWrite(tankLBOT, LOW);
    digitalWrite(tankRTOP, LOW);
    digitalWrite(tankRBOT, HIGH);
  }
}

bool tankFull(){
  if((percentFullR > MAXPERCENT)&&(percentFullL < MINPERCENT))
    return true;
  else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT))
    return true;
  else
    return false;
}

void errorHandler(){
  switch(err){
    case FLOAT_READ:
      break;
    default:
      break;
  }
}
