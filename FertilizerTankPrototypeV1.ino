/*

*/

// Define Tank PINS
#define tankLTOP D4
#define tankLBOT D3
#define tankRTOP D6
#define tankRBOT D5


float VCC = 3.3;
int bits = 4095;
float conversion = VCC / bits;

enum TankState {FILL, EMPTY} tankLeft, tankRight;
enum SourceValveState {OPEN, CLOSE} waterSource, fertilizerSource;
enum SystemState {FILL_RIGHT, FILL_LEFT} system;

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

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

  float voltageR = sensorValueR * conversion;
  float voltageL = sensorValueL * conversion;
  // print out the value you read:
  Serial.println("Voltage Left: " + String(voltageL) + " Voltage Right: " + String(voltageR));

  tankLeftFill(true);
  tankRightFill(false);
  delay(250);
  tankLeftFill(false);
  tankRightFill(true);
  delay(250);
  
}

void systemStateMachine(int floatSensorL, int floatSensorR) {
  
  switch (system) {
    case FILL_RIGHT:

      if ()

      break;

    case FILL_LEFT:

      break;

    default:
        waterSource = CLOSE;
        fertilizerSource = CLOSE;
      break;

  }
}

void fillTank(int floatSensorL, int floatSensorR) {

  if (waterSource == CLOSE && floatSensorR <= )

}

void tankLeftFill(bool isFilling) {
  if (isFilling) {
    digitalWrite(tankLTOP, LOW);
    digitalWrite(tankLBOT, HIGH);
  } else {
    digitalWrite(tankLTOP, HIGH);
    digitalWrite(tankLBOT, LOW);
  }

}

void tankRightFill(bool isFilling) {
  if (isFilling) {
    digitalWrite(tankRTOP, LOW);
    digitalWrite(tankRBOT, HIGH);
  } else {
    digitalWrite(tankRTOP, HIGH);
    digitalWrite(tankRBOT, LOW);
  }
  
}