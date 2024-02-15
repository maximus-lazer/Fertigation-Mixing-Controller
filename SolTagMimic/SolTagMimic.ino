
#include <Arduino.h>

#define start_Stop PD2
#define fertInput PD3

void setup() {
  Serial.begin(9600);
  // put your setup code here, to run once:
  pinMode(start_Stop, OUTPUT);
  pinMode(fertInput, OUTPUT);

  digitalWrite(start_Stop, LOW);
  digitalWrite(fertInput, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(start_Stop, LOW);
  Serial.println("\nLOW");
  for(int i = 10; i > 0; i--){
    Serial.print(String(i)+" ");
    delay(1000);
  }
  digitalWrite(start_Stop, HIGH);
  Serial.println("\nHIGH");
   for(int i = 20; i > 0; i--){
    Serial.print(String(i)+" ");
    delay(1000);
  }
}
