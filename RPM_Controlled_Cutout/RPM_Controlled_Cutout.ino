#define ENCODER_OPTIMIZE_INTERRUPTS

#include <Encoder.h>
#include <Timer.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

const byte encoderButtonPin = 4;    // initialize encoder button pin
const byte engineSpeedPin = 5;      // initialize engine speed pin
const byte controllerInput1Pin = 6; // initialize motor controller input 1 pin
const byte controllerInput2Pin = 7; // initialize motot controller input 2 pin

bool oldButtonState = HIGH;                           // set initial old encoder button state
byte oldPosition = 127;                               // set initial old encoder position
byte cutoutStatus = 0;                                // set initial cutout status
int openEvent = 0, closeEvent = 0;                    // set initial timer events
unsigned int cutoutRPM = 2500;                        // set initial cutout RPM
unsigned int totalTimeOpen = 0, totalTimeClosed = 0;  // set initial total time variables
unsigned long openTime = 0, closeTime = 0;            // set initial timestamp varaibles

Encoder encoder(2, 3);        // initialize encoder variable
Timer timer;                  // initialize timer variable
Adafruit_SSD1306 display(8);  // initialize display variable

void setup() {
  Serial.begin(9600); // start monitor link

  pinMode(encoderButtonPin, INPUT_PULLUP);  // declare encoder button pin as an input with an internal pull-up resistor
  pinMode(engineSpeedPin, INPUT);           // declare engine speed signal as an input
  pinMode(controllerInput1Pin, OUTPUT);     // declare motor controller input 1 pin as an output
  pinMode(controllerInput2Pin, OUTPUT);     // declare motor controller input 2 pin as an output

  encoder.write(127 * 4); // set initial encoder position

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize OLED display with the I2C address 0x3C
  display.clearDisplay();                     // clear the buffer
  display.setTextColor(WHITE);                // set text color to white

  updateDisplay();  // initially update display
}

void loop() {
  readEncoder();  // check if there is a new encoder position

  unsigned int engineRPM = 2000;                                     // DEBUGGING: set the engine RPM to 2000
  //unsigned int engineRPM = 15000000 / pulseIn(engineSpeedPin, HIGH); // calculate current engine RPM from ECU signal

  if (engineRPM > 0 && engineRPM < 6000) {
    if (engineRPM >= cutoutRPM) {
      if (cutoutStatus == 0 || cutoutStatus == 1) {
        timer.stop(closeEvent); // stop the close event because the cutout is going to open
        startCutout(true);      // open the cutout

        if (cutoutStatus == 0) {
          totalTimeOpen = 0;    // set the total time the cutout is open to 0 because it is fully closed
          totalTimeClosed = 0;  // set the total time the cutout is closed to 0 because it is fully closed

          openEvent = timer.after(3250, stopCutout); // start an open event for the length of time the cutout takes to fully open because it is fully closed
        } else {
          unsigned int timeClosed = millis() - closeTime; // calculate the amount of time the cutout was closed for
          totalTimeClosed += timeClosed;                  // update the total amount of the time the cutout has been closed for since the last time it has been fully open or fully closed

          if (totalTimeOpen >= totalTimeClosed) {
            unsigned int difference = totalTimeOpen - totalTimeClosed;  // calculate the absolute time the cutout has opened
            openEvent = timer.after(3250 - difference, stopCutout);    // start an open event for the remaining time it takes the cutout to fully open
          } else {
            unsigned int difference = totalTimeClosed - totalTimeOpen;  // calculate the absolute time the cutout has closed
            openEvent = timer.after(difference, stopCutout);           // start an open event for the time it takes the cutout to fully close
          }
        }
        cutoutStatus = 2; // change the cutout status to "Opening"
        updateDisplay();  // update the display
      }
    } else {
      if (cutoutStatus == 2 || cutoutStatus == 3) {
        timer.stop(openEvent);  // stop the open event because the cutout is going to close
        startCutout(false);     // close the cutout

        if (cutoutStatus == 3) {
          totalTimeOpen = 0;    // set the total time the cutout is open to 0 because it is fully open
          totalTimeClosed = 0;  // set the total time the cutout is closed to 0 because is is fully open

          closeEvent = timer.after(3250, stopCutout);  // start a close event for the length of time the cutout takes to fully close because it is fully open
        } else {
          unsigned int timeOpen = millis() - openTime;  // calculate the amount of time the cutout was open for
          totalTimeOpen += timeOpen;                    // update the total amount of time the cuout has been open for since the last time it has been fully open or fully closed

          if (totalTimeClosed >= totalTimeOpen) {
            unsigned int difference = totalTimeClosed - totalTimeOpen;  // calculate the abosolute time the cutout has closed
            closeEvent = timer.after(3250 - difference, stopCutout);    // start a close event for the remaining time it takes the cutout to fully close
          } else {
            unsigned int difference = totalTimeOpen - totalTimeClosed;  // caculate the absolute time the cutout has opened
            closeEvent = timer.after(difference, stopCutout);           // start a close event for the reamining time it takes the cutout to fully open
          }
        }
        cutoutStatus = 1; // change the cutout status to "Closing"
        updateDisplay();  // update the display
      }
    }
  }
  timer.update(); // update timer events
}

void readEncoder() {
  byte newPosition = encoder.read() / 4;  // read current encoder position

  if (oldPosition != newPosition) {
    if (oldPosition < newPosition) {
      if (cutoutRPM < 6000) {
        cutoutRPM += 100; // increase the cutout RPM by 100 if it is less than 6000 and the encoder was turned clockwise
      }
    } else {
      if (cutoutRPM > 0) {
        cutoutRPM -= 100; // decrease the cutout RPM by 100 if it is greater than 0 and the encoder was turned counterclockwise
      }
    }
    updateDisplay();                // update the display
    oldPosition = newPosition;      // set the old encoder position to the new encoder position
    //Serial.println(newPosition);  // DEBUGGING: print the current position of the encoder
  }
  bool buttonState = digitalRead(encoderButtonPin); // read current encoder button state

  if (buttonState != oldButtonState) {
    if (buttonState == LOW) {
      //Serial.println("Button Closed");  // DEBUGGING: print that button has been closed
    } else {
      //Serial.println("Button Opened");  // DEBUGGING: print that button has been opened
    }
    oldButtonState = buttonState;  // set the old button state to the new button state
  }
}

void startCutout(bool forward) {
  if (forward) {
    openTime = millis();  // get timestamp when cutout starts opening
  } else {
    closeTime = millis(); // get timestamp when cutout starts closing
  }
  digitalWrite(controllerInput1Pin, !forward); // set the input pins to opposite logic values to open or close the cutout
  digitalWrite(controllerInput2Pin, forward);  // set the input pins to opposite logic values to open or close the cutout
}

void stopCutout() {
  if (cutoutStatus == 1) {
    cutoutStatus = 0; // change the cutout status to "Closed" if the previous cutout status was "Closing"
  } else if (cutoutStatus == 2) {
    cutoutStatus = 3; // change the cutout status to "Open" if the previous cutout status was "Closing"
  }
  digitalWrite(controllerInput1Pin, LOW); // set the input pins to the same logic value to stop the cutout
  digitalWrite(controllerInput2Pin, LOW); // set the input pins to the same logic value to stop the cutout

  updateDisplay();  // update the display
}

void updateDisplay() {
  display.drawLine(0, 15, display.width(), 15, WHITE);  // draw a line at the bottom of yellow section of the OLED display
  display.drawLine(0, 16, display.width(), 16, WHITE);  // draw a line at the top of the blue section of the OLED display

  display.setFont(&FreeMonoBold18pt7b); // set the font of the text
  display.setCursor(0, 56);             // set the position of the cutout RPM text
  display.print(cutoutRPM);             // print the set cutout RPM

  display.setFont(&FreeMono9pt7b);  // set the font of the text
  display.setCursor(83, 56);        // set the position of the "RPM" text
  display.print(" RPM");            // print the "RPM" text

  switch (cutoutStatus) {
    case 0:
      display.setCursor(32, 10);  // set the position of "Closed" text
      display.print("Closed");    // print the "Closed" text
      break;
    case 1:
      display.setCursor(26, 10);  // set the position of the "Closing" text
      display.print("Closing");   // print the "Closing" text
      break;
    case 2:
      display.setCursor(26, 10);  // set the position of the "Opening" text
      display.print("Opening");   // print the "Opening" text
      break;
    case 3:
      display.setCursor(42, 10);  // set the position of the "Open" text
      display.print("Open");      // print the "Open" text
      break;
  }
  display.display();      // display the buffer
  display.clearDisplay(); // clear the buffer
}
