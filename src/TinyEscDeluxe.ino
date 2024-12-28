/*
Brushed ESC with ATTiny85 and RZ7886 driver
                 +====+
  RESET/A0/PB5 1 |*   | 8 Vcc
        A3/PB3 2 |    | 7 PB2/A1/SCK
       A2/~PB4 3 |    | 6 PB1~/MISO PWM (OC0B)
           Gnd 4 |    | 5 PB0~/MOSI PWM (OC0A)
                 +====+

Reqiured settings:
Clock: 8MHz internal
BOD: enabled at 2.7V

based on: https://github.com/TheHLab/Brushed-ESC/blob/master/BrushedESC.ino

Software Version 1.0
*/

//
// =======================================================================================================
// PIN ASSIGNMENTS & GLOBAL VARIABLES (Do not play around here)
// =======================================================================================================
//

#define OUT1 0       // Pin 5
#define OUT2 1       // Pin 6
#define SERVO_IN 0   // Pin 7 = Interrupt 0 Servo signal input
#define CONFIG_IN1 3 // Pin 2 brake off, if connected to GND
#define CONFIG_IN2 4 // Pin 3

volatile int pwm_value = 0;
volatile int prev_time = 0;
volatile int middlePoint = 1500;
volatile int maxSpeed = 255;
volatile bool pulseInit = false;
int neutralSpan = 30;
bool brakeMode = false;
bool limitSpeed = false;

//
// =======================================================================================================
// MAIN ARDUINO SETUP (1x during startup)
// =======================================================================================================
//

void setup()
{

  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  pinMode(CONFIG_IN1, INPUT_PULLUP);
  pinMode(CONFIG_IN2, INPUT_PULLUP);

  // Stop without brake (make sure motor stays off until initialization is done!)
  digitalWrite(OUT1, LOW);
  digitalWrite(OUT2, LOW);

  // Read configuration jumpers
  brakeMode = digitalRead(CONFIG_IN1);
  limitSpeed = !digitalRead(CONFIG_IN2);

  // Limit power according to jumper setting
  if (limitSpeed)
  {
    maxSpeed = 194; // 194 = about 75%
  }
  else
  {
    maxSpeed = 255; // 255 = 100%
  }

  // wait 2s for RC receiver to initialize
  while (millis() <= 2000)
    ;

  // when pin IN goes high, call the rising function
  attachInterrupt(SERVO_IN, rising, RISING);
}

//
// =======================================================================================================
// READ RC SERVO SIGNAL USING INTERRUPTS
// =======================================================================================================
//

void rising()
{
  attachInterrupt(SERVO_IN, falling, FALLING);
  prev_time = micros();
}

void falling()
{
  attachInterrupt(SERVO_IN, rising, RISING);
  pwm_value = micros() - prev_time;
  if (!pulseInit)
  {
    // auto zero calibration
    if (pwm_value < 1700 && pwm_value > 1300)
    { // Signal needs to be near 1500 (neutral)
      middlePoint = pwm_value;
      pulseInit = true;
    }
  }
}

//
// =======================================================================================================
// GENERATE PWM SIGNALS FOR RZ7886 MOTOR DRIVER
// =======================================================================================================
//

void driveMotor(int speedIn)
{
  if (pulseInit) // After middlepoint is adjusted
  {
    static int speed = middlePoint;
    static uint32_t lastFrameTime = micros();
    if (micros() - lastFrameTime > 500)
    { // every 0.5ms
      lastFrameTime = micros();

      // stop, if servo signal is out of range
      if (speedIn < 500 || speedIn > 2500)
        speedIn = middlePoint;

      // generate ramp
      if (speedIn < speed)
        speed--;
      if (speedIn > speed)
        speed++;

      // speed
      int PwmOut = map(abs(speed - middlePoint), 0, 500, 0, maxSpeed);

      if (speed > middlePoint + neutralSpan)
      {
        if (brakeMode)
        {
          // Forward with brake
          analogWrite(OUT1, 255 - PwmOut);
          digitalWrite(OUT2, HIGH);
        }
        else
        {
          // Forward without brake
          analogWrite(OUT1, PwmOut);
          digitalWrite(OUT2, LOW);
        }
      }
      else if (speed < middlePoint - neutralSpan)
      {
        if (brakeMode)
        {
          // Reverse with brake
          digitalWrite(OUT1, HIGH);
          analogWrite(OUT2, 255 - PwmOut);
        }
        else
        {
          // Reverse without brake
          digitalWrite(OUT1, LOW);
          analogWrite(OUT2, PwmOut);
        }
      }
      else
      {
        if (brakeMode)
        {
          // Stop with brake
          digitalWrite(OUT1, HIGH);
          digitalWrite(OUT2, HIGH);
        }
        else
        {
          // Stop without brake
          digitalWrite(OUT1, LOW);
          digitalWrite(OUT2, LOW);
        }
      }
    }
  }
  else
  {
    // Stop without brake (make sure motor stays off until initialization is done!)
    digitalWrite(OUT1, LOW);
    digitalWrite(OUT2, LOW);
  }
}

//
// =======================================================================================================
// MAIN LOOP
// =======================================================================================================
//

void loop()
{
  // drive motor
  driveMotor(pwm_value);
}
