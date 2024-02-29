/*
 * TODO: Insert disclaimer
 */

// Libraries needed
#include <list> // For std::list
#include <numeric> // for std::accumulate
#include <Preferences.h> // For storing data into flash

// Define PINS
#define waterValveIn1 D12
#define waterValveIn2 D11
#define fertValveIn3 D10
#define fertValveIn4 D9
#define topValvesIn1 D8
#define topValvesIn2 D7
#define botValvesIn3 D6
#define botValvesIn4 D5
#define fertInput D3
#define startInput D2
#define messageOutput A7
#define floatSensorL A1
#define floatSensorR A0

// Mask for SolTag input interupts
#define soltagStartMask (1<<5)
#define soltagFertMask (1<<6)

// Bitmask with both SolTag inputs
#define PIN_BITMASK soltagStartMask|soltagFertMask

#define uS_TO_S_FACTOR 1000000ULL // Conversion factor for micro seconds to seconds

// [3.3 Vin * 190 / (190 + 151)] / (3.3 Vref) * 4095 bits = 2281 bits
#define MAXBITS 2281.0 // Bit maximum for float sensors
#define MAXPERCENT 0.85 // Percent threshold for full Tank
#define MINPERCENT 0.20 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
const unsigned long fertWaitTime = 1 * 1000; // sec * 1000ms/sec = ms
const unsigned long errWaitTime = 5 * 1000; // sec * 1000ms/sec = ms
const int errSize = 5; // Max size of error checking lists
const int pulse = 100; // Pulse time for solenoid (ms)

// Create timers
unsigned long timer;
unsigned long errTimer;

uint16_t sensorValueR = 0; // Float sensor right reading
uint16_t sensorValueL = 0; // Float sensor left reading
float fertPercent = 0.0; // Percent threshold for filling fertilizer
unsigned char storedFertValue; // Value of saved fertilizer percentage
volatile float voltageR = 0;
volatile float voltageL = 0;
volatile float percentFullL; // Percent threshold when Left Tank is full
volatile float percentFullR; // Percent threshold when Right Tank is full
std::list<int> floatLeft, floatRight; // Lists for left and right float sensors
int errorFlag = 0;

Preferences prefs;

enum ErrorType {NO_ERROR, FLOAT_HIGH_BROKEN, FLOAT_HIGH, FLOAT_LOW, FLOAT_NO_CHANGE, FLOAT_RISING} leftError, rightError;
enum Message {HEARTBEAT = 78, GENERAL = 103, TOP_VALVE_SET = 129, BOT_VALVE_SET = 155, SOURCE_PUMP = 180, LEFT_FLOAT_BROKEN = 206, RIGHT_FLOAT_BROKEN = 232};
enum SystemState {WAKE_UP, FERT_INPUT, WAIT_FOR_START, ACTIVE, SLEEP, ERROR} systemState;
enum TankState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} tankState;

int message = Message::HEARTBEAT; // Message to send to SolTag when ACTIVE (~1V Heartbeat)

/**
 * Runs once controller turns on or resets
 */
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  // Initialize States
  systemState = WAKE_UP;
  leftError = NO_ERROR;
  rightError = NO_ERROR;

  prefs.begin("Conteroller", false);  // Begin preferences (namespace, RW-mode)
  storedFertValue = prefs.getUChar("Percentage", 0); // Get current fertilizer percentage

  // Initializing pins
  pinMode(startInput, INPUT); // High = Active, Low = Sleep
  pinMode(fertInput, INPUT);
  pinMode(messageOutput, OUTPUT);
  pinMode(topValvesIn1, OUTPUT);
  pinMode(topValvesIn2, OUTPUT);
  pinMode(botValvesIn3, OUTPUT);
  pinMode(botValvesIn4, OUTPUT);
  pinMode(waterValveIn1, OUTPUT);
  pinMode(waterValveIn2, OUTPUT);
  pinMode(fertValveIn3, OUTPUT);
  pinMode(fertValveIn4, OUTPUT);

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

  // if((millis()-timer) > 5000){
  //   timer = millis();
  //   Serial.println("STORED FERT VALUE: " + String(storedFertValue));
  // }

  // Running state machine
  systemStateMachine();
  
}

/**
 * The state machine for the whole system
 */
void systemStateMachine() {
  switch (systemState) {
    case WAKE_UP:
      if(digitalRead(fertInput) == 1){
        systemState = FERT_INPUT;
        storedFertValue = 2;
      }
      else if(digitalRead(startInput) == 1) {
        systemState = WAIT_FOR_START;
        fertPercent = float(storedFertValue) / 100.0; // Convert stored char into percentage
      }
      else
        systemState = SLEEP;

      break;
    case FERT_INPUT:
      if((millis()-timer) > fertWaitTime){
        timer = millis();
        if(digitalRead(fertInput) == 1){
          storedFertValue += 2;
          storedFertValue = constrain(storedFertValue, 0, 100); // Keep value between 0 and 100
          Serial.println("FertPercent: " + String(fertPercent));
        }
        else if(digitalRead(startInput) == 1) {
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
        analogWrite(messageOutput, message);
      }else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT)){
        tankState = FILL_FERT_LEFT;
        systemState = ACTIVE;
        tankValveSwitch(0);
        fertValve(1);
        analogWrite(messageOutput, message);
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

      // if((millis()-timer) > 500){
      //   timer = millis();
      //   Serial.print("L: " + String(sensorValueL) + " R: " + String(sensorValueR));
      //   Serial.print(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
      //   Serial.print(" TankState: " + String(tankState));
      //   Serial.print(" SystemState: " + String(systemState));
      //   Serial.println(" FertPercent: " + String(fertPercent));
      // }

      // TODO: uncomment
      // if((digitalRead(startInput) == 0)&& tankFull())
      //     systemState = SLEEP;

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
      errorChecking();

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
        fertValve(0);
        waterValve(0);
      break;
  }
}

/**
 * Set fertilizer valve state
 * @param openClose: True Open, False Close
 */
void fertValve(bool openClose){
  if (openClose){
    digitalWrite(fertValveIn3, LOW); 
    digitalWrite(fertValveIn4, HIGH); 
    delay(pulse);
    digitalWrite(fertValveIn3, LOW); 
    digitalWrite(fertValveIn4, LOW);
  }
  else{
    digitalWrite(fertValveIn3, HIGH); 
    digitalWrite(fertValveIn4, LOW); 
    delay(pulse);
    digitalWrite(fertValveIn3, LOW); 
    digitalWrite(fertValveIn4, LOW);
  }
}

/**
 * Set water valve state
 * @param openClose: True Open, False Close
 */
void waterValve(bool openClose){
  if (openClose){
    digitalWrite(waterValveIn1, LOW); 
    digitalWrite(waterValveIn2, HIGH); 
    delay(pulse);
    digitalWrite(waterValveIn1, LOW); 
    digitalWrite(waterValveIn2, LOW);
  }
  else{
    digitalWrite(waterValveIn1, HIGH); 
    digitalWrite(waterValveIn2, LOW); 
    delay(pulse);
    digitalWrite(waterValveIn1, LOW); 
    digitalWrite(waterValveIn2, LOW);
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
    digitalWrite(botValvesIn3, HIGH); 
    digitalWrite(botValvesIn4, LOW); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    digitalWrite(botValvesIn3, LOW); 
    digitalWrite(botValvesIn4, LOW); 
  }
  else{
    digitalWrite(topValvesIn1, HIGH); 
    digitalWrite(topValvesIn2, LOW); 
    digitalWrite(botValvesIn3, LOW); 
    digitalWrite(botValvesIn4, HIGH); 
    delay(pulse);
    digitalWrite(topValvesIn1, LOW); 
    digitalWrite(topValvesIn2, LOW);
    digitalWrite(botValvesIn3, LOW); 
    digitalWrite(botValvesIn4, LOW); 
  }
}

/**
 * Returns if the tank is finished filling/emptying
 * @return True if one tank is full and other is empty, else False
 */
bool tankFull() {
  if((percentFullR > MAXPERCENT)&&(percentFullL < MINPERCENT))
    return true;
  else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT))
    return true;
  else
    return false;
}

/**
 * Checks for any errors with the float sensors
 */
void errorChecking() {

  if (millis() - errTimer >= errWaitTime) {

    // Set running average
    if (floatLeft.size() == errSize) floatLeft.pop_front();
    if (floatRight.size() == errSize) floatRight.pop_front();
    floatLeft.push_back(int(sensorValueL));
    floatRight.push_back(int(sensorValueR));

    // Check Left Side
    if (floatLeft.size() == errSize) {
      int avg = std::accumulate(floatLeft.begin(), floatLeft.end(), 0.0) / floatLeft.size();
      String a = "Float left: ";
      for (int n : floatLeft) a += String(n) + " ";
      Serial.println(a + "LEFT AVG: " + String(avg));
      
      // Sensor too high / broken
      if (avg > MAXBITS) {
        leftError = FLOAT_HIGH_BROKEN;
      }
      // LOW when Filling-Full
      else if (avg <= 10 && systemState >= FILL_FERT_LEFT ) {
        leftError = FLOAT_LOW;
      }

      // High when emtying
      else if (abs(avg - MAXBITS) <= 10 && systemState < FILL_FERT_LEFT ) {
        leftError = FLOAT_HIGH;
      }

      // Not changing when filling
      else if (abs(floatLeft.front() - floatLeft.back()) <= 10 && systemState >= FILL_FERT_LEFT && systemState != LEFT_FULL_WAIT) {
        leftError = FLOAT_NO_CHANGE;
      }

      // Increasing when emptying
      else if (floatLeft.back() - floatLeft.front() >= 10 && systemState < FILL_FERT_LEFT) {
        leftError = FLOAT_RISING;
      }

    }

    // Check Right Side
    if (floatRight.size() == errSize) {
      int avg = std::accumulate(floatRight.begin(), floatRight.end(), 0.0) / floatRight.size();
      String a = "Float Right: ";
      for (int n : floatRight) a += String(n) + " ";
      Serial.println(a + "RIGHT AVG: " + String(avg));

      // Sensor too high / broken
      if (avg > MAXBITS) {
        rightError = FLOAT_HIGH_BROKEN;
      }
      // LOW when Filling-Full
      else if (avg <= 10 && systemState < FILL_FERT_LEFT ) {
        rightError = FLOAT_LOW;
      }

      // High when emtying
      else if (abs(avg - MAXBITS) <= 10 && systemState >= FILL_FERT_LEFT ) {
        rightError = FLOAT_HIGH;
      }

      // Not changing when filling
      else if (abs(floatRight.front() - floatRight.back()) <= 10 && systemState < FILL_FERT_LEFT && systemState != RIGHT_FULL_WAIT) {
        rightError = FLOAT_NO_CHANGE;
      }

      // Increasing when emptying
      else if (floatRight.back() - floatRight.front() >= 10 && systemState >= FILL_FERT_LEFT) {
        rightError = FLOAT_RISING;
      }

    }

    if (leftError || rightError) {
      errorHandler();
    }

    errTimer = millis();
  }
}

/**
 * Changes the message based on float errors
 */
void errorHandler() {

  if (rightError == FLOAT_HIGH_BROKEN) {
    message = Message::RIGHT_FLOAT_BROKEN;
  }

  else if (rightError == FLOAT_HIGH_BROKEN) {
    message = Message::LEFT_FLOAT_BROKEN;
  }

  else if (leftError == FLOAT_NO_CHANGE || rightError == FLOAT_NO_CHANGE) {
    message = Message::SOURCE_PUMP;
  } 
  else {

    // Check when high when emptying and no error on other
    if ((leftError == FLOAT_HIGH && !rightError) || (rightError == FLOAT_HIGH && !leftError)) {
      message = Message::BOT_VALVE_SET;

    // Check when high when emptying and error on other
    } else if ((leftError == FLOAT_HIGH && rightError) || (rightError == FLOAT_HIGH && leftError)) {
      message = Message::TOP_VALVE_SET;

    // Check when low when filling and no error on other
    } else if ((leftError == FLOAT_LOW && !rightError) || (rightError == FLOAT_LOW && !leftError)) {
      message = Message::SOURCE_PUMP;

    // Check when low when filling and error on other
    } else if ((leftError == FLOAT_LOW && rightError) || (rightError == FLOAT_LOW && leftError)) {
      message = Message::TOP_VALVE_SET;
    
    // Check when rising when emptying and no error on other
    } else if ((leftError == FLOAT_RISING && !rightError) || (rightError == FLOAT_RISING && !leftError)) {
      message = Message::TOP_VALVE_SET;

    // Check when rising when emptying and error on other
    } else if ((leftError == FLOAT_RISING && rightError) || (rightError == FLOAT_RISING && leftError)) {
      message = Message::SOURCE_PUMP;

    } else {
      message = Message::GENERAL;
    }

  }

  analogWrite(messageOutput, message);
  
  switch(leftError){

      case FLOAT_HIGH_BROKEN:
        Serial.println("ERROR: LEFT FLOAT NOT FOUND");

        break;
      
      case FLOAT_HIGH:
        Serial.println("ERROR: LEFT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW:
        Serial.println("ERROR: LEFT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE:
        Serial.println("ERROR: LEFT FLOAT NOT MOVING");

        break;

  }
      switch(rightError){
        
      case FLOAT_HIGH_BROKEN:
        Serial.println("ERROR: RIGHT FLOAT NOT FOUND");

        break;
        
      case FLOAT_HIGH:
        Serial.println("ERROR: RIGHT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW:
        Serial.println("ERROR: RIGHT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE:
        Serial.println("ERROR: RIGHT FLOAT NOT MOVING");

        break;
    default:
      break;
  }
}
