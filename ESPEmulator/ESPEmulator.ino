// ESP DS Sidekick Emulator written by Robert Koszewski
// The power saving features seem to require nonos-sdk 2.2.2 or it delays for ever (Bug)

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
 
// Settings
//#define DEBUG true
#define CALCULATE_BRIGHTNESS true
#define CALCULATE_SATURATION true
//#define CALCULATE_GAMMA_CORRECTION true
#define USE_FLOATING_POINT true

// Load Utils
#include "Utils.c"
#include "Definitions.h"

// Light Driver
//#include "Drivers/Arduino_LightDriver.c"
#include "Drivers/Fastled_Analog_LightDriver.c"

// Wifi Settings
const char* ssid = "";
const char* password = "";

// Initialization
void setup()
{
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  // Enable EEPROM
  EEPROM.begin(128);

  // Setup Light Device
  driverSetup(); 

  // Enable Wifi
  WiFi.mode(WIFI_STA); //  Force the ESP into client-only mode
  WiFi.persistent(false); // Do not write Wifi settings to Flash
  #ifdef DEBUG
    Serial.printf("Connecting to %s ", ssid);
  #endif
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    #ifdef DEBUG
      Serial.print(F("."));
    #endif
  }
  #ifdef DEBUG
    Serial.println(F(" connected"));
  #endif

  Udp.begin(DSSERVERPORT);
  #ifdef DEBUG
    Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), DSSERVERPORT);
  #endif

  //  Enable light sleep
  //wifi_set_sleep_type(LIGHT_SLEEP_T); // https://community.blynk.cc/t/esp8266-light-sleep/13584
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  // USE: delay(1000); to put esp to sleep

  // Load Settings
  EEPROM.get(0, EmulatorPersistentData);

  // Set Initial Settings
  driverUpdate();

  // Set Action Timestamp
  action_last_timestamp = millis(); 
}

// Emulator Loop
void loop()
{
  // State Machine
  switch(state){
    
    case Emulator::LISTEN:
        read_length = Udp.parsePacket();
        if (read_length){
          if(read_length >= 7){
            #ifdef DEBUG
              Serial.println(F("### GOT MESSAGE: ##"));
            #endif
            state = Emulator::VAL_HEADER;
            receiving_data = true;
          }else{
            #ifdef DEBUG
              Serial.println(F("### FLUSHING MESSAGE WITH SIZE UNDER 7 bytes ###"));
            #endif
            Udp.flush(); // Discard any bytes that have been written to the client but not yet read.
            receiving_data = false;
          }
        }else{
          receiving_data = false;
        }
        break;
  
    case Emulator::CLEANUP:
        #ifdef DEBUG
          Serial.println(F("STATE: CLEANUP"));
        #endif
        //Udp.flush(); // Discard any bytes that have been written to the client but not yet read. // TODO: With this enabled random color glitches appear. Fix.
        read_pos = 0;
        state = Emulator::LISTEN;
        break;
  
    case Emulator::VAL_HEADER: // Validate Header (0xFC)
        #ifdef DEBUG
          //Serial.println(F("STATE: VAL_HEADER"));
        #endif
        crc = 0x00; // Reset CRC
        read();
        if(b == 0xFC){
            state = Emulator::VAL_LENGTH;
        }else{
            state = Emulator::CLEANUP;
        }
        break;
  
    case Emulator::VAL_LENGTH: // Validate Packet Length
        #ifdef DEBUG
          //Serial.println(F("STATE: VAL_LENGTH"));
        #endif
        read();
        // TODO: CHECK LENGTH with Packet Length
        state = Emulator::DEC_GROUP;
        break;
  
    // Decode Message
  
    case Emulator::DEC_GROUP: // Decode Group
        #ifdef DEBUG
          //Serial.println(F("STATE: DEC_GROUP"));
        #endif
        read(); 
        group = b;
        // Check Group
        if(b == EmulatorPersistentData.groupNumber || // Is Current Group
           b == 0xFF){ // Is Broadcast Group
            action_last_timestamp = millis(); // Reset Action Timesamp
            state = Emulator::DEC_FLAGS;
        }else{ // Abort if not my nor broadcast group
            #ifdef DEBUG
              Serial.println(F("NOT MY GROUP: Ignoring."));
            #endif
            state = Emulator::CLEANUP;
        }
        break;
  
    case Emulator::DEC_FLAGS: // Decode Flags
        #ifdef DEBUG
          Serial.println(F("STATE: DEC_FLAGS"));
        #endif
        read();
        flags = b;
        state = Emulator::DEC_UPPER;
        break;
  
    case Emulator::DEC_UPPER: // Decode Upper Command
        #ifdef DEBUG
          Serial.println(F("STATE: DEC_UPPER"));
        #endif
        read();
        upper = b;
        state = Emulator::DEC_LOWER;
        break;
  
    case Emulator::DEC_LOWER: // Decode Lower Command
        #ifdef DEBUG
          Serial.println(F("STATE: DEC_LOWER"));
        #endif
        read();
        lower = b;
        // Decode Command
        switch(upper){
        case 0x01:
            switch(lower){
              
            case 0x0A: // Current State Request
                state = Emulator::COM_CURRENT_STATE_REQUEST;
                break;
                
            case 0x07: // Set Device Name
              state = Emulator::COM_DEVICE_NAME;
              break;
              
            case 0x08: // Set Group Name
              state = Emulator::COM_GROUP_NAME;
              break;

            case 0x09: // Set Group Number
              state = Emulator::COM_GROUP_NUMBER;
              break;

            case 0x0C: // Subscription Request
              state = Emulator::COM_SUBSCRIPTION_REQUEST;
              break;
              
            default: // Unknown Lower Command
                state = Emulator::CLEANUP;
            }
            break;
  
        case 0x03:
            switch(lower){
              
            case 0x01: // Set Mode
                state = Emulator::COM_MODE;
                break;
                
            case 0x02: // Set Brightness
              state = Emulator::COM_BRIGHTNESS;
              break;
              
            case 0x05: // Set Ambient Color
              state = Emulator::COM_AMBIENT_COLOR;
              break;

            case 0x06: // Set Saturation Settings
              state = Emulator::COM_SATURATION_SETTING;
              break;

            case 0x08: // Set Ambient Mode
              state = Emulator::COM_AMBIENT_MODE;
              break;

            case 0x16: // Set Sector Data
              state = Emulator::COM_SECTOR_DATA;
              break;

            case 0x17: // Set Sector Settings
              state = Emulator::COM_SECTOR_SETTINGS;
              break;

            case 0x0D: // Set Ambient Scene
              state = Emulator::COM_AMBIENT_SCENE;
              break;
              
            default: // Unknown Lower Command
                state = Emulator::CLEANUP;
            }
        
            break;
  
        default: // Unknown Upper Command
           state = Emulator::CLEANUP;
        }
  
        break;
  
    // Commands
  
    case Emulator::COM_CURRENT_STATE_REQUEST:
        #ifdef DEBUG
          Serial.println(F("STATE: COM_CURRENT_STATE_REQUEST"));
        #endif
        
        // Check CRC
        if(checkCRC()){
                // Respond to Request
                memset(buffer, 0x00, 63);
                fillInArray(buffer, 0, EmulatorPersistentData.name, sizeof(EmulatorPersistentData.name)); // Device Name
                fillInArray(buffer, 16, EmulatorPersistentData.groupName, sizeof(EmulatorPersistentData.groupName)); // Write Group Name
                buffer[32] = EmulatorPersistentData.groupNumber; // Group Number
                buffer[33] = mode; // Current Mode
                buffer[34] = brightness; // Current Brigthness
                buffer[35] = ambientColor[0]; // Ambient Color: R
                buffer[36] = ambientColor[1]; // Ambient Color: G
                buffer[37] = ambientColor[2]; // Ambient Color: B
                buffer[38] = EmulatorPersistentData.saturationR; // Saturation Color: R
                buffer[39] = EmulatorPersistentData.saturationG; // Saturation Color: G
                buffer[40] = EmulatorPersistentData.saturationB; // Saturation Color: B
                fillInArray(buffer, 42, EmulatorPersistentData.sectorSettings, 15); // Device Name
                buffer[60] = ambientScene; // Ambient Scene
                buffer[62] = DSDEVICEID; // Set Device ID
                sendResponse(0xFF, 0x60, 0x01, 0x0A, buffer, (unsigned char) 63);
        }
  
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_DEVICE_NAME: // Set Device Name
        #ifdef DEBUG
          Serial.println(F("STATE: COM_DEVICE_NAME"));
        #endif
        memset(buffer, 0x00, 16);
        readTill(buffer, 16, read_length - 1);
        if(checkCRC()){
           memcpy(&EmulatorPersistentData.name, &buffer, 16);
           eeprom_dirty = true; eeprom_last_timestamp = millis(); // Changes in EEPROM
        }
        state = Emulator::CLEANUP;
        break;
        
    case Emulator::COM_GROUP_NAME: // Set Group Name
        #ifdef DEBUG
          Serial.println(F("STATE: COM_GROUP_NAME"));
        #endif
        memset(buffer, 0x00, 16);
        readTill(buffer, 16, read_length - 1);
        if(checkCRC()){
           memcpy(&EmulatorPersistentData.groupName, &buffer, 16);
           eeprom_dirty = true; eeprom_last_timestamp = millis(); // Changes in EEPROM
        }
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_GROUP_NUMBER: // Set Group Number
        #ifdef DEBUG
          Serial.println(F("STATE: COM_GROUP_NUMBER"));
        #endif
        read();
        buffer[0] = b;
        if(checkCRC()){
            EmulatorPersistentData.groupNumber = buffer[0];
            eeprom_dirty = true; eeprom_last_timestamp = millis(); // Changes in EEPROM
        }
        state = Emulator::CLEANUP;
        break;
    
    case Emulator::COM_SUBSCRIPTION_REQUEST: // Subscription Request
        #ifdef DEBUG
          Serial.println(F("STATE: COM_SUBSCRIPTION_REQUEST"));
        #endif

        #ifdef DEBUG
          Serial.println(F("RESPONDING TO SUBSCRIPTION REQUEST ++"));
        #endif
        buffer[0] = 0x01;
        sendResponse(EmulatorPersistentData.groupNumber, 0x30, 0x01, 0x0C, buffer, (unsigned char) 1);

        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_MODE: // Set Operation Mode
        #ifdef DEBUG
          Serial.println(F("STATE: COM_MODE"));
        #endif
        read();
        buffer[0] = b;
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("MODE: CHANGED ++"));
            #endif
            mode = buffer[0];
        }
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_BRIGHTNESS: // Set Brightness
        #ifdef DEBUG
          Serial.println(F("STATE: COM_BRIGHTNESS"));
        #endif
        read();
        buffer[0] = b;
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("BRIGHTNESS: CHANGED ++"));
            #endif
            brightness = buffer[0];
        }
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_SECTOR_DATA: // Realtime Sector Data
        #ifdef DEBUG
          Serial.println(F("STATE: COM_SECTOR_DATA"));
        #endif
        memset(buffer, 0x00, 36);
        readTill(buffer, 36, read_length - 1);
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("REALTIME COLOR: CHANGED ++"));
           #endif
           memcpy(&sectorData, &buffer, 36);
           processSectorData();
        }
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_SECTOR_SETTINGS: // Set Sector Settings
        #ifdef DEBUG
          Serial.println(F("STATE: COM_SECTOR_SETTINGS"));
        #endif
        memset(buffer, 0x00, 15);
        readTill(buffer, 15, read_length - 1);
        if(checkCRC()){
           #ifdef DEBUG
            Serial.println(F("SECTOR SETTINGS ARE FINE. COMMIT CHANGES ++"));
           #endif
           memcpy(&EmulatorPersistentData.sectorSettings, &buffer, 15);
           processSectorData();
           eeprom_dirty = true; eeprom_last_timestamp = millis(); // Changes in EEPROM
        }
        state = Emulator::CLEANUP;
        break;
        
    case Emulator::COM_AMBIENT_COLOR: // Set Ambient Color
        #ifdef DEBUG
          Serial.println(F("STATE: COM_AMBIENT_COLOR"));
        #endif
        read(); buffer[0] = b; // Color R
        read(); buffer[1] = b; // Color G
        read(); buffer[2] = b; // Color B

        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("AMBIENT COLOR: CHANGED ++"));
            #endif
            ambientColor[0] = buffer[0];
            ambientColor[1] = buffer[1];
            ambientColor[2] = buffer[2];
        }
        
        state = Emulator::CLEANUP;
        break;
        
    case Emulator::COM_AMBIENT_MODE: // Set Ambient Mode
        #ifdef DEBUG
          Serial.println(F("STATE: COM_AMBIENT_MODE"));
        #endif
        read();
        buffer[0] = b;
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("AMBIENT MODE: CHANGED ++"));
            #endif
            ambientMode = buffer[0];
        }
        state = Emulator::CLEANUP;
        break;

    case Emulator::COM_AMBIENT_SCENE: // Set Ambient Scene
        #ifdef DEBUG
          Serial.println(F("STATE: COM_AMBIENT_SCENE"));
        #endif
        read();
        buffer[0] = b;
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("AMBIENT SCENE: CHANGED ++"));
            #endif
            ambientScene = buffer[0];
        }
        state = Emulator::CLEANUP;
        break;  

     case Emulator::COM_SATURATION_SETTING: // Set Saturation Settings
        #ifdef DEBUG
          Serial.println(F("STATE: COM_SATURATION_SETTING"));
        #endif
        read(); buffer[0] = b; // R
        read(); buffer[1] = b; // G
        read(); buffer[2] = b; // B
        if(checkCRC()){
            #ifdef DEBUG
              Serial.println(F("SATURATION COLOR: CHANGED ++"));
            #endif
            EmulatorPersistentData.saturationR = buffer[0];
            EmulatorPersistentData.saturationG = buffer[1];
            EmulatorPersistentData.saturationB = buffer[2];
            saturationChanged = true;
            eeprom_dirty = true; eeprom_last_timestamp = millis(); // Changes in EEPROM
        }
        state = Emulator::CLEANUP;
        break;
      
  /*
        case Emulator::COM_NEW_COMMAND: //
        state = Emulator::CLEANUP;
        break;
  */
  }

  // Current Timestamp
  currentMillis = millis();

  // Write Changes to EEPROM
  if(eeprom_dirty == true && currentMillis - eeprom_last_timestamp >= 10000){ // Write after 10 seconds of no changes
    eeprom_dirty = false;
    EEPROM.put(0, EmulatorPersistentData);
    EEPROM.commit();
  }

  // Update Light Driver
  driverUpdate();

  // Low Power Mode
  if(!receiving_data){
    if(currentMillis < action_last_timestamp){ // Reset on timestamp overflow
      #ifdef DEBUG
        Serial.println(F("- POWER SAVER: OVERFLOW - RESETTING TIMER"));
      #endif
      action_last_timestamp = currentMillis;
      
    }else{

      lastTimeDiff = currentMillis - action_last_timestamp;
      if(lastTimeDiff > 1000){ // Enter Lower Power Mode after 1 seconds

        if(lastTimeDiff > 5000){ // Enter Low Power Mode after 5 seconds

          if(lastTimeDiff > 30000){ // Enter Ultra Low Power Mode after 30 seconds

            if(lastTimeDiff > 60000){ // Enter Mega Low Power Mode after 5 seconds
              #ifdef DEBUG
                Serial.println(F("- POWER SAVER: SLEEP 600 - START"));
              #endif
              delay(600);
              #ifdef DEBUG
                Serial.println(F("- POWER SAVER: SLEEP 600 - END"));
              #endif
              
            }else{
              #ifdef DEBUG
                Serial.println(F("- POWER SAVER: SLEEP 200 - START"));
              #endif
              delay(200);
              #ifdef DEBUG
                Serial.println(F("- POWER SAVER: SLEEP 200 - END"));
              #endif
              
            }
          }else{
            #ifdef DEBUG
              Serial.println(F("- POWER SAVER: SLEEP 32 - START"));
            #endif
            delay(32);
            #ifdef DEBUG
              Serial.println(F("- POWER SAVER: SLEEP 32 - END"));
            #endif
            
          }
        }else{
          #ifdef DEBUG
          Serial.println(F("- POWER SAVER: SLEEP 16 - START"));
          #endif
          delay(16);
          #ifdef DEBUG
            Serial.println(F("- POWER SAVER: SLEEP 16 - END"));
          #endif
          
        }
      }else{
        delay(6); // CPU Breathing Room while no transmission
      
      }
    }
  }

  #ifdef DEBUG
    Serial.println(F("- +++"));
  #endif
}
