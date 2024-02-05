/*
*/
#include "<ArduinoLowPower.h>"
// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5
#define waterPIN D7
#define fertilizerPIN D8

#define MAXPERCENT 0.85 // Percent threshold for full Tank
#define MINPERCENT 0.20 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
float fertPercent = .40; // Percent threshold for filling fertilizer TODO: Change this to read from controller DIO

int startBit = 0;

int sensorValueR = 0;
int sensorValueL = 0;
volatile float percentFullL; // Percent threshold when Left Tank is full
volatile float percentFullR; // Percent threshold when Right Tank is full

enum SourceValveState {CLOSE, OPEN} waterSource, fertilizerSource;
enum SystemState {WAIT_FOR_START, FERT_INPUT, ACTIVE, SETUP, ERROR} systemState;
enum TankState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} tankState;

/**
 * Runs once controller turns on or resets
 */
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  // Initializing pins
  pinMode(tankLTOP, OUTPUT);
  pinMode(tankLBOT, OUTPUT);
  pinMode(tankRTOP, OUTPUT);
  pinMode(tankRBOT, OUTPUT);
  pinMode(waterPIN, OUTPUT);
  pinMode(fertilizerPIN, OUTPUT);
}

/**
 * Runs while controller is on
 */
void loop() {

  // read the input on analog pin 0 & 1:
  int sensorValueR = analogRead(A0);
  int sensorValueL = analogRead(A1);

  percentFullL = sensorValueL/4095.0;
  percentFullR = sensorValueR/4095.0;

  float voltageR = sensorValueR * conversion;
  float voltageL = sensorValueL * conversion;

  // print out the value you read:
  Serial.print("VL: " + String(voltageL) + " VR: " + String(voltageR));
  Serial.print(" %L: " + String(percentFullL) + " %R: " + String(percentFullR));
  Serial.print(" State: " + String(systemState));
  Serial.print(" FertSource: " + String(fertilizerSource));
  Serial.println(" WaterSource: " + String(waterSource));

  // Running state machine
  systemStateMachine();
}

void systemStateMachine() {
  switch (systemState) {
    case WAIT_FOR_START:
      sensorValueR = analogRead(A0);
      sensorValueL = analogRead(A1);

      if((sensorValueR > 3500)&& sensorValueL < 500){
        tankState = FILL_FERT_RIGHT;
        tankValveSwitch(1);
      }else if((sensorValueL > 3500)&& sensorValueR < 500){
        tankState = FILL_FERT_LEFT;
        tankValveSwitch(0);
      }else {
        Serial.print("Set the potentiometers to a starting state");
        Serial.println(" ValueL: " + String(sensorValueL) + " ValueR: " + String(sensorValueR));
      }
      waterSource = CLOSE;
      fertilizerSource = CLOSE;

      if(startBit == 1) // Will be interrupts - Wake up and set state
        systemState = FERT_INPUT;


      break;
    case FERT_INPUT:
      //Start timer - Every two minutes
      //Start interrupt - For when bit goes low
      break;
    case ACTIVE:
      tankStateMachine();

      if(startBit == 0)
        systemState = SETUP;

      break;
    case SETUP:


      // Put in Deep Sleep
      break;
    case ERROR:

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
    digitalWrite(tankLTOP, LOW);
    digitalWrite(tankLBOT, HIGH);
    digitalWrite(tankRTOP, HIGH);
    digitalWrite(tankRBOT, LOW);
  }
  else{
    digitalWrite(tankLTOP, HIGH);
    digitalWrite(tankLBOT, LOW);
    digitalWrite(tankRTOP, LOW);
    digitalWrite(tankRBOT, HIGH);
  }
}
