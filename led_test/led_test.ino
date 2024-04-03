// Define the pin number
const int ledPin = 5;

void setup() {
  // Initialize the digital pin as an output.
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // Turn on the LED
  digitalWrite(ledPin, HIGH);
  // Wait for 500 milliseconds
  delay(500);
  // Turn off the LED
  digitalWrite(ledPin, LOW);
  // Wait for another 500 milliseconds
  delay(500);
}
