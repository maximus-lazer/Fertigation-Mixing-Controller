/*

*/

// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5

#define MAXPERCENT 0.90 // Percent threshold for full Tank
#define MINPERCENT 0.20 // Percent threshold for empty Tank

// Defining constants
const float VCC = 3.3; // VCC output voltage from pins
const int bits = 4095; // 2^(16)-1 bits for ADC
const float conversion = VCC / bits; // Conversion multiplier for converting analog input to voltage
const float fertPercent = .50; // Percent threshold for filling fertilizer TODO: Change this to read from controller DIO

volatile float percentFullL; // Percent threshold when Left Tank is full
volatile float percentFullR; // Percent threshold when Right Tank is full

enum TankState {FILL, EMPTY} tankLeft, tankRight;
enum SourceValveState {CLOSE, OPEN} waterSource, fertilizerSource;
enum SystemState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} systemState;

/**
 * Runs once controller turns on or resets
 */
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  // Setting some initial value
  percentFullL = 1.0;
  percentFullR = 0.0;

  // TODO: initialize by reading sensors
  tankLeft = FILL;
  tankRight = EMPTY;
  waterSource = CLOSE;
  fertilizerSource = CLOSE;
  systemState = FILL_FERT_RIGHT;
  tankValveSwitch(1);

  // Initializing pins
  pinMode(tankLTOP, OUTPUT);
  pinMode(tankLBOT, OUTPUT);
  pinMode(tankRTOP, OUTPUT);
  pinMode(tankRBOT, OUTPUT);
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

/**
* The state machine for the entire system
*/
void systemStateMachine() {
  switch (systemState) {

    // Filling Right Tank to Fertilizer percentage
    case FILL_FERT_RIGHT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      if (percentFullR >= fertPercent)
        systemState = FILL_WATER_RIGHT;
      break;
    
    // Filling rest of Right tank with water
    case FILL_WATER_RIGHT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      if (percentFullR >= MAXPERCENT)
        systemState = RIGHT_FULL_WAIT;
      break;
    
    // Waiting for Left Tank to empty
    case RIGHT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      if (percentFullL < MINPERCENT){
        systemState = FILL_FERT_LEFT;
        tankValveSwitch(0);
      }
      break;

    // Filling Left Tank to Fertilizer percentage
    case FILL_FERT_LEFT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      if (percentFullL >= fertPercent)
        systemState = FILL_WATER_LEFT;
      break;

    // Filling rest of Left tank with water
    case FILL_WATER_LEFT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      if (percentFullL >= MAXPERCENT)
        systemState = LEFT_FULL_WAIT;
      break;

    // Waiting for Right Tank to empty
    case LEFT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      if (percentFullR < MINPERCENT) {
        systemState = FILL_FERT_RIGHT;
        tankValveSwitch(1);
      }
      break;
    break;

    // A default state if something goes wrong (should never get here)
    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
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
