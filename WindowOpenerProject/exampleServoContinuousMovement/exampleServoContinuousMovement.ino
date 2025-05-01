// Motor A connections
int in1 = 17;
int in2 = 16;


void setup() {
  Serial.begin(115200);
	// Set all the motor control pins to outputs

	pinMode(in1, OUTPUT);
	pinMode(in2, OUTPUT);
	
	// Turn off motors - Initial state
	digitalWrite(in1, LOW);
	digitalWrite(in2, LOW);
}

void loop() {
	digitalWrite(in1, HIGH);
	digitalWrite(in2, LOW);
	delay(5000);
  digitalWrite(in1, LOW);
	digitalWrite(in2, LOW);
	delay(5000);
	digitalWrite(in1, LOW);
	digitalWrite(in2, HIGH);
	delay(5000);
	digitalWrite(in1, LOW);
	digitalWrite(in2, LOW);
	delay(5000);

}
