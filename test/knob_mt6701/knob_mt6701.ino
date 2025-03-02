#include <SPI.h>
#include "MT6701.h"

MT6701 encoder;

#define MT_DATA 37
#define MT_CLOCK 13
#define CSN_PIN 14  // CSN pin of NT6701
int pushButton = 4;

void setup() {
  
  Serial.begin(115200);

  SPI.begin(MT_CLOCK, MT_DATA, 38, 15);
  // put your setup code here, to run once:
  encoder.initializeSSI(CSN_PIN);

  // Hom many poles should be emulated
  encoder.uvwModeSet(4);


  // Chip could be setting in ABZ mode wtih MODE pin
  pinMode(pushButton, INPUT_PULLUP);
}


void loop() {
  float angle = encoder.angleRead();
  Serial.print("Angle:");
  Serial.println(angle);
  
    // Serial.print("MOSI: ");
    // Serial.println(MOSI);
    // Serial.print("MISO: ");
    // Serial.println(MISO);
    // Serial.print("SCK: ");
    // Serial.println(SCK);
    // Serial.print("SS: ");
    // Serial.println(SS); 

  // read the input pin:
  // int buttonState = digitalRead(pushButton);
  // // // print out the state of the button:
  // Serial.println(buttonState);

  delay(100);  // delay in between reads for stability
}
