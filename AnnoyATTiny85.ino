/*

When this program runs, it first waits some time for configuration to be
entered via the activation pin.  This configuration is entered by shorting
the activation pin to Vcc.  When assembled using an ATtiny45/85, pin 2, the
activation pin, is right beside the Vcc pin.  Nearly any metal object can
be used to short the pins together.  I like using my house keys, since I
always have them on me.

Configuration is done in 4 stages:
1. Activation
2. Number of days to wait
3. Ramp-up
4. Test mode


1. Activation

When started, the program waits 5 seconds for the pins to be shorted
to "activate" it.  If this initial activation isn't done, it pauses for
3 seconds, and then goes into "super annoying" mode, where it beeps every
2 seconds.  This should make it easy to find, if it's hidden, which
prevents someone getting payback on you by trying to return the favour.
(Unless, of course, they know about these instructions.)
When successfully activated, it plays a single note.  If it isn't activated,
it plays two descending notes.

2. Number of days to wait
3. Ramp-up
4. Test mode



After being activated, it then waits for the pins to be shorted any number
of times, to configure how many days to wait before starting beeping.  It
waits 1 second for each short, beeping each time it's shorted, to give
feedback.  After a full second without a short, it beeps the number of
times it was shorted, to ensure it was configured correctly.

This sketch should fit on an ATtiny45.


*/

#include <avr/sleep.h>
#include <avr/power.h>

// Uncomment this if you're using an ATmega168/328 or similar
//#define ATMEGA

#ifdef ATMEGA
#define WDTCR_REG WDTCSR
#define SERIAL_DEBUG // enable some debug messages on the serial port when using an ATmega
#else
#define WDTCR_REG WDTCR
#endif


int MIN_COUNTER_TRIGGER = 37; // 37 * 8 sec = 5 minutes (approximately)
int MAX_COUNTER_TRIGGER = 60; // 60 * 8 sec = 8 minutes

int const TEST_MIN_COUNTER_TRIGGER = 2;
int const TEST_MAX_COUNTER_TRIGGER = 4;

int WATCHDOG_PARAM = 9; // 9 = 8 sec
int const TEST_WATCHDOG_PARAM = 7; // 7 = 2 sec

long const DAY = 10800; // 10800 * 8 sec = 24 hours
long INITIAL_COUNTER_TRIGGER = 1 * DAY;
long TEST_INITIAL_COUNTER_TRIGGER = 3;


int device_count = 1;
long day_counter = 0;
boolean started = false;
long counter_trigger;
long const ON_MICROS=300;
//int DUTY = 5; // quiet
int const DUTY = 30; // loud

int const piezoPin = 3;
int const activationPin = 2;

boolean readActivationPinBoolean(int period) {
  return readActivationPinBoolean(period, 5000);
}

boolean readActivationPinBoolean(int period, long timeout) {
  return readActivationPinBoolean(period, ON_MICROS, timeout);
}

boolean readActivationPinBoolean(int period, long duration, long timeout) {
  unsigned long const end  = millis() + timeout;
  while (millis() < end) {
    int val = digitalRead(activationPin);
    if (val == HIGH) {
      play(period, duration);
      return true;
    }
  }
  return false;
}

byte readActivationPinByte(int period, long timeout) {
  int count = 0;
  while (readActivationPinBoolean(period, 100, timeout)) {
    delay(100);
    while (digitalRead(activationPin) == HIGH) { delay(10); }
    delay(100);
    count++;
  }
  return count;
}

void readConfiguration() {
  #ifdef SERIAL_DEBUG
  Serial.println("Reading configuration");
  #endif
  // beep once to let the user know the power is on
  play(450, ON_MICROS);
  
  #ifdef SERIAL_DEBUG
  Serial.println("Tap to activate");
  #endif
  // must short the pin once to activate the chip.
  boolean active = readActivationPinBoolean(400, 5000);
  
  if (active) {
    #ifdef SERIAL_DEBUG
    Serial.println("Activated");
    Serial.println("Tap number of days to wait");
    #endif
    int const days = readActivationPinByte(400, 1000);
    #ifdef SERIAL_DEBUG
    Serial.print("Waiting ");
    Serial.print(days);
    Serial.println(" days");
    #endif
    for (int x = 0; x < days; ++x) {
      play(400, 200, 200);
    }
    if (days == 0) {
        play(500, 200, 200);
    }
    #ifdef SERIAL_DEBUG
    Serial.println("Tap number of other devices");
    #endif
    device_count = readActivationPinByte(400, 1000) + 1;
    #ifdef SERIAL_DEBUG
    Serial.print(device_count);
    Serial.println(" other devices");
    #endif
    for (int x = 0; x < device_count; ++x) {
      play(400, 200, 200);
    }
    #ifdef SERIAL_DEBUG
    Serial.println("Tap to enable test mode");
    #endif
    boolean const testMode = readActivationPinBoolean(350, 1000);
    if (!testMode) {
      #ifdef SERIAL_DEBUG
      Serial.println("Test mode NOT enabled");
      #endif
      play(450, ON_MICROS);
    } else {
      #ifdef SERIAL_DEBUG
      Serial.println("Test mode enabled");
      #endif
      MIN_COUNTER_TRIGGER = TEST_MIN_COUNTER_TRIGGER;
      MAX_COUNTER_TRIGGER = TEST_MAX_COUNTER_TRIGGER;
      INITIAL_COUNTER_TRIGGER = TEST_INITIAL_COUNTER_TRIGGER;
      WATCHDOG_PARAM = TEST_WATCHDOG_PARAM;
    }
    delay(100);
    MIN_COUNTER_TRIGGER *= device_count;
    MAX_COUNTER_TRIGGER *= device_count;
    INITIAL_COUNTER_TRIGGER += DAY * days;
  } else {
    #ifdef SERIAL_DEBUG
    Serial.println("NOT activated, enabling super annoying mode");
    #endif
    play(500, ON_MICROS);
    play(550, ON_MICROS*3);
    
    delay(3000);
    
    INITIAL_COUNTER_TRIGGER = 0;
    MIN_COUNTER_TRIGGER = 0;
    MAX_COUNTER_TRIGGER = 0;
    WATCHDOG_PARAM = TEST_WATCHDOG_PARAM;
  }
  
  for(int x = 0 ; x <= 4 ; x++){
    if (x != piezoPin) {
      pinMode(x, INPUT);
      digitalWrite(x, LOW);
    }
  }

}

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(9600);
  Serial.println("Starting");
  #endif
  pinMode(piezoPin, OUTPUT);
  pinMode(activationPin, INPUT);

  readConfiguration();

  // After reading configuration, if the user tapped the activation pin at all,
  // millis() should be a fairly unique value.  If they didn't, then we're in
  // "super annoying" more anyway, so it doesn't matter much.
  randomSeed(millis());

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
  sleep_enable();

  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR = (1<<ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins

  sei();
    
  newTriggerCounter();
  
  counter_trigger += INITIAL_COUNTER_TRIGGER;
  setup_watchdog(WATCHDOG_PARAM);
}

volatile long watchdog_counter = 0;

void newTriggerCounter() {
  counter_trigger = random(MIN_COUNTER_TRIGGER, MAX_COUNTER_TRIGGER);
}

void loop() {
  #ifdef SERIAL_DEBUG
  Serial.print("Sleeping...");
  Serial.flush();
  delay(2
  0);
  
  #endif
  sleep_mode();
  #ifdef SERIAL_DEBUG
  Serial.println("awake");
  #endif
  // Zzzz...
  if (started) {
    day_counter++;
  }

  #ifdef SERIAL_DEBUG
  Serial.print("day_counter = ");
  Serial.println(
  
  day_counter);
  #endif
  
  if (day_counter >= DAY*3) {
    #ifdef SERIAL_DEBUG
    Serial.println("It's been two days");
    Serial.print("device_count = ");
    Serial.println(device_count);
    #endif
    day_counter = 0;
    if (device_count > 1) {
      #ifdef SERIAL_DEBUG
      Serial.print("MIN_COUNTER_TRIGGER = ");
      Serial.println(MIN_COUNTER_TRIGGER);
      Serial.print("MAX_COUNTER_TRIGGER = ");
      Serial.println(MAX_COUNTER_TRIGGER);
      #endif
      // every 2 days, decrease the beep interval
      MIN_COUNTER_TRIGGER = (MIN_COUNTER_TRIGGER / device_count) * (device_count - 1);
      MAX_COUNTER_TRIGGER = (MAX_COUNTER_TRIGGER / device_count) * (device_count - 1);
      device_count--;
      #ifdef SERIAL_DEBUG
      Serial.print("MIN_COUNTER_TRIGGER = ");
      Serial.println(MIN_COUNTER_TRIGGER);
      Serial.print("MAX_COUNTER_TRIGGER = ");
      Serial.println(MAX_COUNTER_TRIGGER);
      #endif
      if (WATCHDOG_PARAM == TEST_WATCHDOG_PARAM) {
        play(500, 100);
        play(568, 100);
        play(610, 100);
      }
    }
  }

    #ifdef SERIAL_DEBUG
    Serial.print("watchdog_counter = ");
    Serial.println(watchdog_counter);
    Serial.print("counter_trigger = ");
    Serial.println(counter_trigger);
    #endif

  if (watchdog_counter >= counter_trigger) {
    watchdog_counter = 0;
    newTriggerCounter();
    started = true;
    annoy();
  } else if (WATCHDOG_PARAM == TEST_WATCHDOG_PARAM) {
    play(568, 100);
  }
}

// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {

  byte bb;
  int ww;
  if (ii > 9 ) ii=9;
  bb=ii & 7;
  if (ii > 7) bb|= (1<<5);
  bb|= (1<<WDCE);
  ww=bb;

  MCUSR &= ~(1<<WDRF);
  // start timed sequence
  WDTCR_REG |= (1<<WDCE) | (1<<WDE);
  // set new watchdog timeout value
  WDTCR_REG = bb;
  WDTCR_REG |= _BV(WDIE);
}


// executed when watchdog timer expires
ISR(WDT_vect) {
  watchdog_counter++;  // increment volatile
}

void play(long period, long durationMillis) {
  play(period, durationMillis, 0, DUTY);
}

void play(long period, long durationMillis, long pauseMillis) {
  play(period, durationMillis, pauseMillis, DUTY);
}

void play(long period, long durationMillis, long pauseMillis, int duty) {
  period = period;
  durationMillis = durationMillis;
  pauseMillis = pauseMillis;
  unsigned long const end = millis() + durationMillis;
  unsigned long pa = period * (long)(duty) / (long)100;
  if (pa > 150) {
    pa = 150;
  }
  unsigned long const pb = period - pa;
  while (end > millis()) {
    digitalWrite(piezoPin, HIGH);
    delayMicroseconds(pa);
    digitalWrite(piezoPin, LOW);
    delayMicroseconds(pb);
  }
  delay(pauseMillis);
}

void annoy() {
  int const annoyance = random(0, 6);
  switch(annoyance) {
  case 0:
    annoy0();
    break;
  case 1:
    annoy1();
    break; 
  case 2:
    annoy2();
    break; 
  case 3:
    annoy3(400, 500);
    break; 
  case 4:
    annoy3(300, 400);
    break; 
  case 5:
    annoy3(200, 300);
    break; 
  }
}

void annoy0() {
  play(400, 50);
  play(390, 50);
  play(380, 50);
  play(370, 50);
  play(360, 50);
  play(350, 500);
}

void annoy1() {
  play(300, 50);
  play(290, 50);
  play(280, 50);
  play(270, 50);
  play(260, 50);
  play(250, 50);
  play(240, 50);
  play(230, 50);
  play(220, 50);
  play(210, 500);
}

void annoy2() {
  play(350, 500);
}

void annoy3(int min, int max) {
  int periods[100];
  for (int x = 0; x < 100; ++x) {
    periods[x] = random(min, max);
  }
  
  for (int x = 0; x < 100; ++x) {
    play(periods[x], 1);
  }
  for (int x = 100; x >= 0; --x) {
    play(periods[x], 1);
  }
}

