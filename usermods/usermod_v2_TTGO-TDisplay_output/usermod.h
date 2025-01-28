/*
  -----------------------------------------------------------------------------------------------
  Lilygo/TTGO T-Display screen output
  -----------------------------------------------------------------------------------------------

  Ideas/todos:
    - add rotation support
    - choose between square and nonsquare pixels (nonsquare fills the screen, square goes to center)
    - add reserved pins (tft) to xml.cpp
    - usermod configuration instead of led and 2D settings ?
    - add "display" as led type 
*/

#pragma once

#include "wled.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <Wire.h>

#define ADC_EN 14  // Used for enabling battery voltage measurements - not used in this program
// #define measureRenderTime

class TTGOTDisplayOutputUsermod : public Usermod {
  private:
    TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT); // Setup display

    uint8_t backlightChannel = 255;
    uint16_t blocksW = 0;
    uint16_t blocksH = 0;
    bool forceSquareBlocks = true;
    uint16_t top = 0;
    uint16_t left = 0;
    uint16_t oneWidth = 0;
    uint16_t oneHeight = 0;
    uint16_t margin = 1;
#ifdef measureRenderTime
    uint32_t rtSum     = 0;
    uint32_t rtSamples = 0;
#endif
    bool isSettingsValid(){
      if(!strip.isMatrix) return false;
      if(strip.panels!=1) return false;
      return true;
    }

    bool isSettingsChanged(){
      if((strip.panel[0].serpentine?0:1) != margin) return true;
      if(blocksW!=strip.panel[0].width) return true;
      if(blocksH!=strip.panel[0].height) return true;
      return false;
    }

    bool checkSettings(){
      if(!isSettingsValid()) return false;
      if(!isSettingsChanged()) return true;

      tft.fillScreen(TFT_BLACK); // clear screen on change

      blocksW = strip.panel[0].width;
      blocksH = strip.panel[0].height;
      margin  = strip.panel[0].serpentine?0:1;

      oneWidth  = floor((tft.width()  - (blocksW-1)*margin) / blocksW);
      oneHeight = floor((tft.height() - (blocksH-1)*margin) / blocksH);
      if(forceSquareBlocks){
        oneHeight = oneWidth = min(oneWidth, oneHeight);
      }
      top  = (tft.height() - (blocksH-1)*margin - oneHeight * blocksH) / 2;
      left = (tft.width()  - (blocksW-1)*margin - oneWidth  * blocksW) / 2;

      // Serial.printf("blocksW: %d, blocksH: %d, oneWidth: %d, oneHeight: %d, top: %d, left: %d\n", blocksW, blocksH, oneWidth, oneHeight, top, left);

      return true;
    }

    void setupBrightnessControl(){
      backlightChannel = pinManager.allocateLedc(1);
      if(backlightChannel==255){
        pinMode(TFT_BL, OUTPUT);
      }else{
        ledcSetup(backlightChannel, 10000, 8);    // t-display-s3 led driver: AW9364DNR, T-display direct led control with pwm
        ledcAttachPin(TFT_BL, backlightChannel);
      }
    }

    void setBrightness(){
      if(backlightChannel==255){
        digitalWrite(TFT_BL, bri==0?LOW:HIGH );
      }else{
        ledcWrite(backlightChannel, bri);
      }
    }

  public:

    void setup() {
      // Serial.println("T-Display usermod - Setup");

#ifdef UM_DisplayMatrix_Pins
      uint8_t pinsToAllocate[] = {UM_DisplayMatrix_Pins};
      uint8_t pinCount = sizeof(pinsToAllocate) / sizeof(uint8_t);
      String notAllocated = "";
      String allPins = "";
      for(uint8_t i=0;i<pinCount;i++){
        allPins += ", ";
        allPins += pinsToAllocate[i];
        if(!pinManager.allocatePin(pinsToAllocate[i], true, PinOwner::UM_Unspecified)){ // UM_DisplayMatrix
          notAllocated += ", " + pinsToAllocate[i];
        }  
      }
      if(notAllocated.length()>0){
        Serial.printf( "Usermod DisplayMatrix - Cannot allocate all the pins (%s) from (%s)\n", notAllocated.substring(2), allPins.substring(2)) ;
      }else{
        Serial.printf( "Usermod DisplayMatrix - All %d pins alocated (%s)\n", pinCount, allPins.substring(2)) ;
      }
      //pinManager.allocatePin(TFT_BL, true, PinOwner::UM_Unspecified);  // UM_DisplayMatrix
#endif

      tft.init();
      tft.setRotation(3);  //Rotation here is set up for the text to be readable with the port on the left.
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM);

      setupBrightnessControl();
      setBrightness();

      uint8_t textSize = 4;
      while (true)
      {
        tft.setTextSize(textSize);
        if(textSize==1) break;
        if(tft.textWidth(F("WLED - DisplayMatrix"))<=tft.width()) break;
        textSize--;        
      }
      tft.drawString(F("WLED - DisplayMatrix"), tft.width()/2, tft.height()/2);
    }

    void onStateChange(uint8_t mode) {
      setBrightness();
    }

    void loop() {
#ifdef measureRenderTime
      unsigned long rtStart = micros();
#endif

      if(bri==0) return;
      if(!checkSettings()) return;

      for(uint16_t h = 0; h<blocksH; h++){
        for(uint16_t w = 0; w<blocksW; w++){
          tft.fillRect(left + w * (oneWidth + margin), top + h * (oneHeight + margin), oneWidth, oneHeight, tft.color24to16(strip.getPixelColorXY(w,h)));
        }
      }

#ifdef measureRenderTime
      if(micros() > rtStart){
        rtSum += micros() - rtStart;
        rtSamples++;
        if(rtSamples==100){
          Serial.printf("2D Matrix: %dx%d with %d gap, display: %d microsecond => %d fps\n", blocksW, blocksH, margin, rtSum / rtSamples, sampleCount * 1000000 / rtSum);
          rtSamples = 0;
          sampleCount = 0;
        }
      }
#endif
    }

};
