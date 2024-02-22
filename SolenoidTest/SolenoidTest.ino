#define in1 A4
#define in2 A5
const int pulse = 100;
const int wait = 30000;

void setup() {
  Serial.begin(9600);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
}

void loop() {
  Serial.println("FORWARD");
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  delay(pulse);
  Serial.println("STOP");
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  delay(wait);
  Serial.println("BACKWARD");
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  delay(pulse);
  Serial.println("STOP");
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  delay(wait);
}

