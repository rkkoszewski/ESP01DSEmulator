#include "FastLED.h"
//#include <math.h>

// Arduino RPM controller based Light Driver

#define DATA_PIN 1                                             // Serial data pin
#define CLOCK_PIN 3                                             // Clock pin for WS2801 or APA102
#define COLOR_ORDER BGR                                       // It's GRB for WS2812B and GBR for APA102
#define LED_TYPE P9813                                       // What kind of strip are you using (APA102, WS2801 or WS2812B)?
#define NUM_LEDS 1                                           // Number of LED's


struct CRGB leds[NUM_LEDS];

// Legacy Methods

void sendColor(){
	#ifdef USE_FLOATING_POINT

		fbuffer[0] = currentColor[0];
		fbuffer[1] = currentColor[1];
		fbuffer[2] = currentColor[2];

		#ifdef CALCULATE_GAMMA_CORRECTION
		  fbuffer[0] = pow(fbuffer[0] / (float) 255.0, (float) 2.8) * 255 + 0.5;
		  fbuffer[1] = pow(fbuffer[1] / (float) 255.0, (float) 2.8) * 255 + 0.5;
		  fbuffer[2] = pow(fbuffer[2] / (float) 255.0, (float) 2.8) * 255 + 0.5;
		#endif

		#ifdef CALCULATE_SATURATION
		  fbuffer[0] = (fbuffer[0] * EmulatorPersistentData.saturationR) / 255.0;
		  fbuffer[1] = (fbuffer[1] * EmulatorPersistentData.saturationG) / 255.0 ;
		  fbuffer[2] = (fbuffer[2] * EmulatorPersistentData.saturationB) / 255.0 ;
		#endif

		#ifdef CALCULATE_BRIGHTNESS
		  if(brightness == 0){
			fbuffer[0] = fbuffer[1] = fbuffer[2] = 0.0;
		  }else if(brightness < 100){
			fbuffer[0] = (fbuffer[0] * brightness) / 100.0;
			fbuffer[1] = (fbuffer[1] * brightness) / 100.0;
			fbuffer[2] = (fbuffer[2] * brightness) / 100.0;
		  }
		#endif

		buffer[0] = (int)(fbuffer[0]);
		buffer[1] = (int)(fbuffer[1]);
		buffer[2] = (int)(fbuffer[2]);

		#else

		buffer[0] = currentColor[0];
		buffer[1] = currentColor[1];
		buffer[2] = currentColor[2];

		#ifdef CALCULATE_GAMMA_CORRECTION
		  buffer[0] = pgm_read_byte(&GAMMA8[buffer[0]]);
		  buffer[1] = pgm_read_byte(&GAMMA8[buffer[1]]);
		  buffer[2] = pgm_read_byte(&GAMMA8[buffer[2]]);
		#endif

		#ifdef CALCULATE_SATURATION
		  buffer[0] = round(buffer[0] / (float) 255.0 * EmulatorPersistentData.saturationR);
		  buffer[1] = round(buffer[1] / (float) 255.0 * EmulatorPersistentData.saturationG);
		  buffer[2] = round(buffer[2] / (float) 255.0 * EmulatorPersistentData.saturationB);
		#endif

		#ifdef CALCULATE_BRIGHTNESS
		  buffer[0] = round(buffer[0] / (float) 100.0 * brightness);
		  buffer[1] = round(buffer[1] / (float) 100.0 * brightness);
		  buffer[2] = round(buffer[2] / (float) 100.0 * brightness);
		#endif

	#endif
  
  	leds[0].r = buffer[0]; 
    leds[0].g = buffer[1]; 
    leds[0].b = buffer[2];
	
	// FastLED.setBrightness(255);
	
	FastLED.show();
}

void setColor(unsigned char &r, unsigned char &g, unsigned char &b){	
	if(currentColor[0] == r && currentColor[1] == g && currentColor[2] == b){
		// Brightness Changed
		if(currentBrightness != brightness){
		  currentBrightness = brightness;
		  sendColor();
		}

		// Saturation Changed
		if(saturationChanged){
		  sendColor();
		  saturationChanged = false;
		}
		
		return; // Avoid updating lights if there are no changes
	}

	currentColor[0] = r;
	currentColor[1] = g;
	currentColor[2] = b;

	if(currentBrightness != brightness){
		currentBrightness = brightness;
	}

	if(saturationChanged){
	saturationChanged = false;
	}

	sendColor();
}

// Driver Implementation

void driverSetup(){
	//GPIO 1 (TX) swap the pin to a GPIO.
	pinMode(1, FUNCTION_3); 
	//GPIO 3 (RX) swap the pin to a GPIO.
	pinMode(3, FUNCTION_3); 
	
	// Initialize FastLED
	FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(255);
}

void driverUpdate(){
	switch(mode){
    case MODE_SLEEP:
      setColor(zero, zero, zero);
    break;
    case MODE_AMBIENT:
      setColor(ambientColor[0], ambientColor[1], ambientColor[2]);
    break;
    case MODE_VIDEO:
      setColor(realtimeColor[0], realtimeColor[1], realtimeColor[2]);
    break;
    case MODE_MUSIC:
      setColor(realtimeColor[0], realtimeColor[1], realtimeColor[2]);
    break;
  }
}