/*

*/

// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5
#define MAXPERCENT 95.0
#define MINPERCENT 1.0


float VCC = 3.3;
int bits = 4095;
float conversion = VCC / bits;
volatile float percentFullL;
volatile float percentFullR;
const float fertPercent = 10.0;

enum TankState {FILL, EMPTY} tankLeft, tankRight;
enum SourceValveState {OPEN, CLOSE} waterSource, fertilizerSource;
enum SystemState {FILL_RIGHT, FILL_LEFT} system;

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
  system = FILL_RIGHT;

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
  Serial.println("Voltage Left: " + String(voltageL) + " Voltage Right: " + String(voltageR));

  systemStateMachine();

}

void systemStateMachine() {
  switch (system) {
    case FILL_RIGHT:
      tankValveSwitch(1);
      fillTank(1);
      if (percentFillL < MINPERCENT)
        system = FILL_LEFT;
      break;

    case FILL_LEFT:
      tankValveSwitch(0);
      fillTank(0);
      if (percentFillL < MINPERCENT)
        system = FILL_RIGHT;
      break;
    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
      break;
  }
}

void fillTank(bool LeftOrRight) {
  if(LeftOrRight?percentFillR:percentFillL <= fertPercent){
    fertilizerSource = OPEN;
    waterSource = CLOSE;
  }
  if(LeftOrRight?percentFillR:percentFillL <= MAXPERCENT){
    fertilizerSource = CLOSE;
    waterSource = OPEN;
  }
  fertilizerSource = CLOSE;
  waterSource = CLOSE;
}

void tankValveSwitch(bool LeftOrRight){
  if (bool LeftOrRight){
    digitalWrite(tankLTOP, HIGH);
    digitalWrite(tankLBOT, LOW);
    digitalWrite(tankRTOP, LOW);
    digitalWrite(tankRBOT, HIGH);
  }
  else{
    digitalWrite(tankLTOP, LOW);
    digitalWrite(tankLBOT, HIGH);
    digitalWrite(tankRTOP, HIGH);
    digitalWrite(tankRBOT, LOW);
  }
}
