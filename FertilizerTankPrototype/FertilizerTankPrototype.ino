/*
 * TODO: Insert disclaimer
 */

// Libraries needed
#include "esp_sleep.h" // For sleep modes
#include <list> // For std::list
#include <numeric> // for std::accumulate
#include <Preferences.h> // For storing data into flash

// Define PINS
#define transistor D12
#define tankLTOP D4
#define tankLBOT D11
#define tankRTOP D6
#define tankRBOT D5
#define waterPIN D7
#define fertilizerPIN D8
#define sleep D2
#define fertInput D3
#define topValvesIn1 D10
#define topValvesIn2 D9
// #define waterValveIn1 D7
// #define waterValveIn2 D8
// #define fertValveIn1 D12
// #define fertValveIn2 D13
#define floatSensorL A1
#define floatSensorR A0

// Mask for SolTag input interupts
#define soltagStartMask (1<<5)
#define soltagFertMask (1<<6)

// Bitmask with both SolTag inputs
#define PIN_BITMASK soltagStartMask|soltagFertMask


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define MAXBITS 1520.0 // Bit maximum for float sensors
#define MAXPERCENT 0.85 // Percent threshold for full Tank
#define MINPERCENT 0.20 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
const unsigned long errWaitTime = 5 * 1000; // sec * 1000ms/sec = ms
const int errSize = 5; // Max size of error checking lists
const int pulse = 100; // Pulse time for solenoid (ms)

int startBit = 0;
unsigned long timer;
unsigned long errTimer;
unsigned long currentTime;

uint16_t sensorValueR = 0;
uint16_t sensorValueL = 0;
float fertPercent = 0.0; // Percent threshold for filling fertilizer
unsigned char storedFertValue; // Value of saved fertilizer percentage
volatile float voltageR = 0;
volatile float voltageL = 0;
volatile float percentFullL; // Percent threshold when Left Tank is full
volatile float percentFullR; // Percent threshold when Right Tank is full
std::list<int> floatLeft, floatRight; // Lists for left and right float sensors
Preferences prefs;

enum ErrorType {FLOAT_HIGH_BROKEN_LEFT, FLOAT_HIGH_LEFT, FLOAT_LOW_LEFT, FLOAT_NO_CHANGE_LEFT, FLOAT_HIGH_BROKEN_RIGHT, FLOAT_HIGH_RIGHT, FLOAT_LOW_RIGHT, FLOAT_NO_CHANGE_RIGHT, } err;
enum SourceValveState {CLOSE, OPEN} waterSource, fertilizerSource;
enum SystemState {WAKE_UP, FERT_INPUT, WAIT_FOR_START, ACTIVE, SLEEP, ERROR} systemState, preErrorState;
enum TankState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} tankState;

/**
 * Runs once controller turns on or resets
 */
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  systemState = WAKE_UP;

  prefs.begin("Conteroller", false);  // Begin preferences (namespace, RW-mode)
  storedFertValue = prefs.getUChar("Percentage", 0); // Get current fertilizer percentage

  // Initializing pins
  pinMode(transistor, OUTPUT);
  pinMode(tankLTOP, OUTPUT);
  pinMode(tankLBOT, OUTPUT);
  pinMode(tankRTOP, OUTPUT);
  pinMode(tankRBOT, OUTPUT);
  pinMode(waterPIN, OUTPUT);
  pinMode(fertilizerPIN, OUTPUT);
  pinMode(sleep, INPUT); // High = Active, Low = Sleep
  pinMode(fertInput, INPUT);
  pinMode(topValvesIn1, OUTPUT);
  pinMode(topValvesIn2, OUTPUT);
  // pinMode(waterValveIn1, OUTPUT);
  // pinMode(waterValveIn2, OUTPUT);
  // pinMode(fertValveIn1, OUTPUT);
  // pinMode(fertValveIn2, OUTPUT);

  timer = millis();
  errTimer = millis();
}

/**
 * Runs while controller is on
 */
void loop() {

  // read the input on analog pin 0 & 1:
  sensorValueR = analogRead(floatSensorR);
  sensorValueL = analogRead(floatSensorL);

  percentFullL = sensorValueL/MAXBITS;
  percentFullR = sensorValueR/MAXBITS;

  if(percentFullL > 1.0){percentFullL = 1.0;}
  if(percentFullR > 1.0){percentFullR = 1.0;}

  if((millis()-timer) > 5000){
    timer = millis();
    Serial.println("STORED FERT VALUE: " + String(storedFertValue));
  }

  // Running state machine
  systemStateMachine();
  
}

void systemStateMachine() {
  switch (systemState) {
    case WAKE_UP:
      if(digitalRead(fertInput) == 1){
        systemState = FERT_INPUT;
        storedFertValue = 2;
      }
      else if(digitalRead(sleep) == 1) {
        systemState = WAIT_FOR_START;
        fertPercent = float(storedFertValue) / 100.0; // Convert stored char into percentage
      }
      // else
      //   systemState = SLEEP;

      break;
    case FERT_INPUT:
      if((millis()-timer) > 1000){
        timer = millis();
        if(digitalRead(fertInput) == 1){
          storedFertValue += 2;
          storedFertValue = constrain(storedFertValue, 0, 100); // Keep value between 0 and 100
          Serial.println("FertPercent: " + String(fertPercent));
        }
        else if(digitalRead(sleep) == 1) {
           systemState = WAIT_FOR_START;
           fertPercent = float(storedFertValue) / 100.0; // Convert stored char into percentage
        }
         
        else
          systemState = SLEEP;
      }
      break;

    case WAIT_FOR_START:
      if((percentFullR > MAXPERCENT)&&(percentFullL < MINPERCENT)){
        tankState = FILL_FERT_RIGHT;
        systemState = ACTIVE;
        tankValveSwitch(1);
        fertValve(1);
      }else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT)){
        tankState = FILL_FERT_LEFT;
        systemState = ACTIVE;
        tankValveSwitch(0);
        fertValve(1);
      }else {
        if((millis()-timer) > 1000){
          timer = millis();
          Serial.print("Set the float sensors to a starting state");
          Serial.println(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
        }
      }
      
      break;
    case ACTIVE:
      tankStateMachine();
      errorChecking();

      voltageR = sensorValueR * conversion;
      voltageL = sensorValueL * conversion;

      if((millis()-timer) > 500){
        timer = millis();
        Serial.print("L: " + String(sensorValueL) + " R: " + String(sensorValueR));
        Serial.print(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
        Serial.print(" TankState: " + String(tankState));
        Serial.print(" SystemState: " + String(systemState));
        Serial.print(" FertSource: " + String(fertilizerSource));
        Serial.print(" WaterSource: " + String(waterSource));
        Serial.println(" FertPercent: " + String(fertPercent));
      }
      if((digitalRead(sleep) == 0)&& tankFull())
          systemState = SLEEP;

      break;
    case SLEEP:
      // Store value only if it is different (Same as update in EEPROM)
      if (prefs.getUChar("Percentage", 0) != storedFertValue)
        prefs.putUChar("Percentage", storedFertValue); // save current fert value
      prefs.end(); // End preferences
      
      // Put in Deep Sleep
      esp_sleep_enable_ext1_wakeup(PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
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
* The state machine for the dual tank system
*/
void tankStateMachine() {
  switch (tankState) {
    // Filling Right Tank to Fertilizer percentage
    case FILL_FERT_RIGHT:

      if (percentFullR >= fertPercent){
        tankState = FILL_WATER_RIGHT;
        fertValve(0);
        waterValve(1);
      }
      break;
    
    // Filling rest of Right tank with water
    case FILL_WATER_RIGHT:

      if (percentFullR >= MAXPERCENT){
        tankState = RIGHT_FULL_WAIT;
        waterValve(0);
      }
      break;
    
    // Waiting for Left Tank to empty
    case RIGHT_FULL_WAIT:
      if (percentFullL < MINPERCENT){
        tankState = FILL_FERT_LEFT;
        tankValveSwitch(0);
        delay(100);
        fertValve(1);
      }
      break;

    // Filling Left Tank to Fertilizer percentage
    case FILL_FERT_LEFT:
      if (percentFullL >= fertPercent){
        tankState = FILL_WATER_LEFT;
        fertValve(0);
        delay(100);
        waterValve(1);
      }
      break;

    // Filling rest of Left tank with water
    case FILL_WATER_LEFT:
      if (percentFullL >= MAXPERCENT){
        tankState = LEFT_FULL_WAIT;
        waterValve(0);
      }
      break;

    // Waiting for Right Tank to empty
    case LEFT_FULL_WAIT:
      if (percentFullR < MINPERCENT) {
        tankState = FILL_FERT_RIGHT;
        tankValveSwitch(1);
        fertValve(1);
      }
      break;
    break;

    // A default state if something goes wrong (should never get here)
    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
        fertValve(0);
        waterValve(0);
      break;
  }
}

void fertValve(bool openClose){
  if (openClose){
    digitalWrite(fertilizerPIN, HIGH); 

    // digitalWrite(fertValveIn1, LOW); 
    // digitalWrite(fertValveIn2, HIGH); 
    // delay(pulse);
    // digitalWrite(fertValveIn1, LOW); 
    // digitalWrite(fertValveIn2, LOW);
  }
  else{
    digitalWrite(fertilizerPIN, LOW); 

    // digitalWrite(fertValveIn1, HIGH); 
    // digitalWrite(fertValveIn2, LOW); 
    // delay(pulse);
    // digitalWrite(fertValveIn1, LOW); 
    // digitalWrite(fertValveIn2, LOW);
  }
}

void waterValve(bool openClose){
  if (openClose){
    digitalWrite(waterPIN, HIGH); 

    // digitalWrite(waterValveIn1, LOW); 
    // digitalWrite(waterValveIn2, HIGH); 
    // delay(pulse);
    // digitalWrite(waterValveIn1, LOW); 
    // digitalWrite(waterValveIn2, LOW);
  }
  else{
    digitalWrite(waterPIN, LOW); 
    // digitalWrite(waterValveIn1, HIGH); 
    // digitalWrite(waterValveIn2, LOW); 
    // delay(pulse);
    // digitalWrite(waterValveIn1, LOW); 
    // digitalWrite(waterValveIn2, LOW);
  }
}


/**
 * Switch filling and emptying the system
 * @param LeftOrRight: True for Right Fill, False for Left Fill
 */
void tankValveSwitch(bool LeftOrRight) {
  if (LeftOrRight){
    // digitalWrite(transistor, HIGH);
    // delay(5000);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, HIGH); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    // delay(50);
    // digitalWrite(transistor, LOW);

    digitalWrite(tankLTOP, LOW);
    digitalWrite(tankLBOT, HIGH);
    digitalWrite(tankRTOP, HIGH);
    digitalWrite(tankRBOT, LOW);
  }
  else{
    //digitalWrite(transistor, HIGH);
    //delay(5000);
    digitalWrite(topValvesIn1, HIGH); 
    digitalWrite(topValvesIn2, LOW); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    //delay(50);
    //digitalWrite(transistor, LOW);

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

void errorChecking() {

  if (millis() - errTimer >= errWaitTime) {

    // 
    if (floatLeft.size() == errSize) floatLeft.pop_front();
    if (floatRight.size() == errSize) floatRight.pop_front();
    floatLeft.push_back(int(sensorValueL));
    floatRight.push_back(int(sensorValueR));

    if (floatLeft.size() == errSize) {
      int avg = std::accumulate(floatLeft.begin(), floatLeft.end(), 0.0) / floatLeft.size();
      String a = "Float left: ";
      for (int n : floatLeft) a += String(n) + " ";
      Serial.println(a + "LEFT AVG: " + String(avg));
      
      // LOW when Filling-Full
      if (avg == 0 && systemState >= FILL_FERT_LEFT ) {
        err = FLOAT_LOW_LEFT;
        errorHandler();
      }

      // Sensor too high / broken
      else if (avg >= bits && systemState < FILL_FERT_LEFT ) {
        err = FLOAT_HIGH_BROKEN_LEFT;
        errorHandler();
      }

      // High when emtying
      else if (avg == MAXBITS && systemState < FILL_FERT_LEFT ) {
        err = FLOAT_HIGH_LEFT;
        errorHandler();
      }

      // Not changing
      else if (abs(floatLeft.front() - floatLeft.back()) <= 10 && systemState >= FILL_FERT_LEFT ) {
        err = FLOAT_NO_CHANGE_LEFT;
        errorHandler();
      }

    }

    if (floatRight.size() == errSize) {
      int avg = std::accumulate(floatRight.begin(), floatRight.end(), 0.0) / floatRight.size();
      String a = "Float Right: ";
      for (int n : floatRight) a += String(n) + " ";
      Serial.println(a + "RIGHT AVG: " + String(avg));
      
      // LOW when Filling-Full
      if (avg == 0 && systemState < FILL_FERT_LEFT ) {
        err = FLOAT_LOW_RIGHT;
        errorHandler();
      }

      // Sensor too high / broken
      else if (avg >= bits && systemState >= FILL_FERT_LEFT ) {
        err = FLOAT_HIGH_BROKEN_RIGHT;
        errorHandler();
      }

      // High when emtying
      else if (avg == MAXBITS && systemState >= FILL_FERT_LEFT ) {
        err = FLOAT_HIGH_RIGHT;
        errorHandler();
      }

      // Not changing
      else if (abs(floatRight.front() - floatRight.back()) <= 10 && systemState < FILL_FERT_LEFT ) {
        err = FLOAT_NO_CHANGE_RIGHT;
        errorHandler();
      }

    }

    errTimer = millis();
  }
}

void errorHandler(){
  switch(err){

      case FLOAT_HIGH_BROKEN_LEFT:
        Serial.println("ERROR: LEFT FLOAT NOT FOUND");

        break;
      
      case FLOAT_HIGH_LEFT:
        Serial.println("ERROR: LEFT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW_LEFT:
        Serial.println("ERROR: LEFT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE_LEFT:
        Serial.println("ERROR: LEFT FLOAT NOT MOVING");

        break;
        
      case FLOAT_HIGH_BROKEN_RIGHT:
        Serial.println("ERROR: RIGHT FLOAT NOT FOUND");

        break;
        
      case FLOAT_HIGH_RIGHT:
        Serial.println("ERROR: RIGHT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW_RIGHT:
        Serial.println("ERROR: RIGHT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE_RIGHT:
        Serial.println("ERROR: RIGHT FLOAT NOT MOVING");

        break;
    default:
      break;
  }
}
