/*

*/

// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5
#define MAXPERCENT 0.90
#define MINPERCENT 0.20


float VCC = 3.3;
int bits = 4095;
float conversion = VCC / bits;
volatile float percentFullL;
volatile float percentFullR;
const float fertPercent = .50;

enum TankState {FILL, EMPTY} tankLeft, tankRight;
enum SourceValveState {CLOSE, OPEN} waterSource, fertilizerSource;
enum SystemState {FILL_FERT_RIGHT, FILL_WATER_RIGHT, RIGHT_FULL_WAIT, FILL_FERT_LEFT, FILL_WATER_LEFT,LEFT_FULL_WAIT} systemState;

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  percentFullL = 100.0;
  percentFullR = 0.0;

  tankLeft = FILL;
  tankRight = EMPTY;
  waterSource = CLOSE;
  fertilizerSource = CLOSE;
  systemState = FILL_FERT_RIGHT;
  tankValveSwitch(1);

  pinMode(tankLTOP, OUTPUT);
  pinMode(tankLBOT, OUTPUT);
  pinMode(tankRTOP, OUTPUT);
  pinMode(tankRBOT, OUTPUT);
}

// the loop routine runs over and over again forever:
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

  systemStateMachine();

}

void systemStateMachine() {
  switch (systemState) {

    case FILL_FERT_RIGHT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      if (percentFullR >= fertPercent)
        systemState = FILL_WATER_RIGHT;
      break;
    
    case FILL_WATER_RIGHT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      if (percentFullR >= MAXPERCENT)
        systemState = RIGHT_FULL_WAIT;
      break;
    
    case RIGHT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      if (percentFullL < MINPERCENT){
        systemState = FILL_FERT_LEFT;
        tankValveSwitch(0);
      }
      break;

    case FILL_FERT_LEFT:
      fertilizerSource = OPEN;
      waterSource = CLOSE;
      if (percentFullL >= fertPercent)
        systemState = FILL_WATER_LEFT;
      break;

    case FILL_WATER_LEFT:
      fertilizerSource = CLOSE;
      waterSource = OPEN;
      if (percentFullL >= MAXPERCENT)
        systemState = LEFT_FULL_WAIT;
      break;

    case LEFT_FULL_WAIT:
      fertilizerSource = CLOSE;
      waterSource = CLOSE;
      if (percentFullR < MINPERCENT){
        systemState = FILL_FERT_RIGHT;
        tankValveSwitch(1);
      }
      break;
    break;

    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
      break;
  }
}

void tankValveSwitch(bool LeftOrRight){
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
