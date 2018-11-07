#include <LEDFader.h>
//#include <SoftwareSerial.h>

//######################
//      SETTINGS
//######################

//#define REDPIN   13
#define REDPIN   3
#define GREENPIN 6
#define BLUEPIN  5

#define ESP8266_RX 11
#define ESP8266_TX 10

// WiFi References
//SoftwareSerial ESPSerial = SoftwareSerial(ESP8266_RX, ESP8266_TX);
//ESPSerial = Serial;

// LED References
LEDFader led_r;
LEDFader led_g;
LEDFader led_b;

unsigned long fade_time = 0; // Right now just 0, but maybe later it might be adjusted

// Loop State
enum State{
    HEADER,
    MODE,
    LENGTH,
    BRIGHTNESS,
    COLOR_R,
    COLOR_G,
    COLOR_B,
    CRC_COMMIT
};

State state = HEADER;
unsigned char buffer[4] = {0x00};

//######################
//      MAIN LOOP
//######################

void setup(void){

/*
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  
  analogWrite(REDPIN, 0);     // R 
  analogWrite(GREENPIN, 0);   // G 
  analogWrite(BLUEPIN, 0);    // B 

*/

  Serial.begin(115200);
 
  // Initialize LED
  led_r = LEDFader(REDPIN);
  led_g = LEDFader(GREENPIN);
  led_b = LEDFader(BLUEPIN);

  led_r.fade(0, fade_time);
  led_g.fade(0, fade_time);
  led_b.fade(0, fade_time);

  //unsigned long last_update_time = millis();

  // Main Loop
  for(;;){

    // Process Received Message
    if (Serial.available() > 0) {

        switch(state){
          case HEADER:
            if(Serial.read() == 0xFC){
              state = MODE;
            }
          break;
          case MODE:
            Serial.read(); // Read Mode
            state = LENGTH;
          break;
          case LENGTH:
            Serial.read(); // Read Length
            state = BRIGHTNESS;
          break;
          case BRIGHTNESS:
            buffer[0] = Serial.read();
            state = COLOR_R;
          break;
          case COLOR_R:
            buffer[1] = Serial.read();
            state = COLOR_G;
          break;
          case COLOR_G:
            buffer[2] = Serial.read();
            state = COLOR_B;
          break;
          case COLOR_B:
            buffer[3] = Serial.read();
            state = CRC_COMMIT;
          break;
          case CRC_COMMIT:
            // Update LED Colors
            //Serial.println(buffer[1], DEC);
            led_r.fade(buffer[1], fade_time); // Red LED
            led_g.fade(buffer[2], fade_time); // Green LED
            led_b.fade(buffer[3], fade_time); // Blue LED
            state = HEADER;
          break;
        }

        // Update Fading Time
        //fade_time  = millis() - last_update_time; // Dynamic Fade Time
        //last_update_time = millis();
      }

    // Update LEDs
    led_r.update();
    led_g.update();
    led_b.update();
  } 
}

void loop(void){
  // Empty - Using for loop in setup() because it's slightly quicker
}







