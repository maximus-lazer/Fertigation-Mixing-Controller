#include <list>
#include <numeric> // for std::accumulate

// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5
#define waterPIN D7
#define fertilizerPIN D8
#define sleep D2
#define topValvesIn1 D10
#define topValvesIn2 D9
#define floatSensorL A1
#define floatSensorR A0

#define PIN_BITMASK (1<<5)


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

#define MAXBITS 1520.0
#define MAXPERCENT 0.90 // Percent threshold for full Tank
#define MINPERCENT 0.10 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
float fertPercent = .40; // Percent threshold for filling fertilizer TODO: Change this to read from controller DIO

int startBit = 0;
unsigned long timer;
unsigned long errTimer;
unsigned long currentTime;
const int errWaitTime = 30 * 1000; // sec * 1000ms/sec = ms
const int pulse = 100;

volatile int sensorValueR = 0;
volatile int sensorValueL = 0;
volatile float voltageR = 0;
volatile float voltageL = 0;
volatile float percentFullL; // Percent of left tank
volatile float percentFullR; // Percent of right tank
std::list<int> floatLeft, floatRight;

enum ErrorType {FLOAT_HIGH_BROKEN_LEFT, FLOAT_HIGH_LEFT, FLOAT_LOW_LEFT, FLOAT_NO_CHANGE_LEFT, FLOAT_HIGH_BROKEN_RIGHT, FLOAT_HIGH_RIGHT, FLOAT_LOW_RIGHT, FLOAT_NO_CHANGE_RIGHT, } err;
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

  // Running state machine
  systemStateMachine();
  errorChecking();
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
      //esp_deep_sleep_enable_gpio_wakeup(PIN_BITMASK, 1);
      //esp_sleep_enable_ext0_wakeup(GPIO_NUM_38, HIGH);
      esp_sleep_enable_ext1_wakeup(PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
      //esp_sleep_enable_timer_wakeup(5*uS_TO_S_FACTOR);
      esp_deep_sleep_start();
      break;
    case ERROR:

      // errorHandler();
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

bool tankFull() {
  if((percentFullR > MAXPERCENT)&&(percentFullL < MINPERCENT))
    return true;
  else if((percentFullL > MAXPERCENT)&&(percentFullR < MINPERCENT))
    return true;
  else
    return false;
}

void errorChecking() {

  currentTime = millis();
  if (currentTime - errTimer >= errWaitTime) {

    // 
    if (floatLeft.size() == 5) floatLeft.pop_front();
    if (floatRight.size() == 5) floatRight.pop_front();
    floatLeft.push_back(int(sensorValueL));
    floatRight.push_back(int(sensorValueR));

    if (floatLeft.size() == 5) {
      float avg = std::accumulate(floatLeft.begin(), floatRight.end(), 0.0) / floatLeft.size();
      
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

    if (floatRight.size() == 5) {
      float avg = std::accumulate(floatRight.begin(), floatRight.end(), 0.0) / floatRight.size();
      
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

    errTimer = currentTime;
  }
}

void errorHandler(){
  switch(err){

      case FLOAT_HIGH_BROKEN_LEFT:
        Serial.print("ERROR: LEFT FLOAT NOT FOUND");

        break;
      
      case FLOAT_HIGH_LEFT:
        Serial.print("ERROR: LEFT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW_LEFT:
        Serial.print("ERROR: LEFT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE_LEFT:
        Serial.print("ERROR: LEFT FLOAT NOT MOVING");

        break;
        
      case FLOAT_HIGH_BROKEN_RIGHT:
        Serial.print("ERROR: RIGHT FLOAT NOT FOUND");

        break;
        
      case FLOAT_HIGH_RIGHT:
        Serial.print("ERROR: RIGHT FLOAT STUCK HIGH");

        break;
        
      case FLOAT_LOW_RIGHT:
        Serial.print("ERROR: RIGHT FLOAT STUCK LOW");

        break;
        
      case FLOAT_NO_CHANGE_RIGHT:
        Serial.print("ERROR: RIGHT FLOAT NOT MOVING");

        break;
    default:
      break;
  }
}
