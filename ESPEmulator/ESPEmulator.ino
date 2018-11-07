#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define DSDEVICEID 0x03 // Device ID (0x03 for SideKick)
#define DSSERVERPORT 8888
//#define DEBUG true

const char* ssid = "";
const char* password = "";

// Device Variables
unsigned char name[16] = "ESPSKEmu";
unsigned char groupName[16] = "Nogroup";
unsigned char groupNumber = 0x00;
unsigned char brightness = 100;
unsigned char mode = 0x00;
unsigned char ambientColor[3] = {0x00, 0x00, 0x00};
unsigned char ambientScene = 0x00;
unsigned char ambientMode = 0x00;
unsigned char sectorSettings[15] = {0x00};

// Buffers
unsigned char buffer[256] = {0x00};

// Light Emulation
unsigned char currentBrightness = 0;
unsigned char currentColor[3] = {0x00, 0x00, 0x00};
unsigned char realtimeColor[3] = {0x00, 0x00, 0x00};
unsigned char sectorData[36] = {0x00};

// Message Data
unsigned char group;
unsigned char flags;
unsigned char upper;
unsigned char lower;

// Loop State
namespace Emulator{
  enum State{
      LISTEN,
      CLEANUP,
  
      VAL_HEADER,
      VAL_LENGTH,
      //VAL_CRC,
  
      DEC_GROUP,
      DEC_FLAGS,
      DEC_UPPER,
      DEC_LOWER,
  
      COM_CURRENT_STATE_REQUEST,
      COM_DEVICE_NAME,
      COM_GROUP_NAME,
      COM_GROUP_NUMBER,
      COM_SUBSCRIPTION_REQUEST,

      COM_MODE,
      COM_BRIGHTNESS,
      COM_SECTOR_DATA,
      COM_SECTOR_SETTINGS,
      COM_AMBIENT_COLOR,
      COM_AMBIENT_MODE,
      COM_AMBIENT_SCENE
  };
}

Emulator::State state = Emulator::LISTEN;

// Other Variables
WiFiUDP Udp;

// Constants
unsigned char payload_header = 0xFC;
unsigned char payload_length_default = 0x05;
unsigned char zero = 0x00;

// CRC8 TABLE
const unsigned char CRC8_TABLE[] PROGMEM = {
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23,
  0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41,
  0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF,
  0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
  0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC,
  0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
  0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 0x27, 0x20,
  0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74,
  0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8,
  0xAD, 0xAA, 0xA3, 0xA4, 0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6,
  0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
  0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10, 0x05, 0x02,
  0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, 0x4E, 0x49, 0x40, 0x47,
  0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39,
  0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D,
  0x84, 0x83, 0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
  0xFA, 0xFD, 0xF4, 0xF3
};

// Modes
#define MODE_SLEEP 0x00
#define MODE_VIDEO 0x01
#define MODE_MUSIC 0x02
#define MODE_AMBIENT 0x03

// Network Read and Write Functions
unsigned char b;
unsigned char crc = 0x00;
unsigned int read_length = 0;
unsigned int read_pos = 0;

void read(){
    b = Udp.read();
    crc = pgm_read_word_near(CRC8_TABLE + ((crc ^ b) & 0xFF)); // Calculate CRC
    read_pos++;
    #ifdef DEBUG
      Serial.printf("READ BYTE: 0x%02X\n", b);
    #endif
}

void readTill(unsigned char buffer[], unsigned int buffer_size, unsigned int to_pos){
    for(unsigned int i = 0; read_pos < to_pos ; i++){
      read();
      if(i <= buffer_size){
        buffer[i] = b;
      }
      #ifdef DEBUG
        else{
          Serial.println("Buffer ignored"); 
        }
      #endif
    }
}

bool checkCRC(){
    b = Udp.read(); // Read Byte
    #ifdef DEBUG
      Serial.print("CRC: Remote: ");
      Serial.printf("%02X", b);
      Serial.print(" | Expected: ");
      Serial.printf("%02X\n", crc);
    #endif
    return crc == b; // Check CRC
}

void write(unsigned char &c){
    crc = pgm_read_word_near(CRC8_TABLE + ((crc ^ c) & 0xFF)); // Calculate CRC
    Udp.write(c); // Send Byte
    #ifdef DEBUG
      Serial.printf("%02X", c);
    #endif
}

void sendCRC(){
    Udp.write(crc); // Write CRC
    #ifdef DEBUG
      Serial.printf("%02X", crc);
    #endif
}

void sendResponse(unsigned char groupNumber, unsigned char flags,
                  unsigned char command_upper, unsigned char command_lower,
                  unsigned char payload[], unsigned char payload_length){
    #ifdef DEBUG
      Serial.printf("SENDING MESSAGE: 0x");
    #endif
    Udp.beginPacket(Udp.remoteIP(), DSSERVERPORT); // Start Packet
    crc = 0x00; // Reset CRC;
    write(payload_header); // Header
    unsigned char p_length = 0x05 + payload_length;
    write(p_length); // Length
    write(groupNumber); // Group Number
    write(flags); // Flags
    write(command_upper); // Upper Command
    write(command_lower); // Lower Command
    // Payload
    for(unsigned int i = 0; i < payload_length; i++){
        write(payload[i]);
    }
    sendCRC(); // CRC
    Udp.endPacket(); // Close Packet
    #ifdef DEBUG
      Serial.printf("\n");
    #endif
}

void fillInArray(unsigned char dst[], const unsigned int dst_offset, const unsigned char src[], unsigned int src_len){
    for(unsigned int i = 0; i < src_len; i++){
        dst[dst_offset + i] = src[i];
    }
}

// Serial Communication Functions

void serialWrite(unsigned char &c){
  crc = pgm_read_word_near(CRC8_TABLE + ((crc ^ c) & 0xFF)); // Calculate CRC
  Serial.write(c);
  //Serial.printf("%02X", c);
}

void serialWriteCRC(){
  Serial.write(crc);
  //Serial.printf("%02X", crc);
}

unsigned char one = 0x01;

void sendColor(){
  //Serial.print("++ SET COLOR TO: 0x");
  crc = 0x00;
  serialWrite(payload_header);
  serialWrite(one); // Mode
  serialWrite(payload_length_default);
  serialWrite(brightness);
  serialWrite(currentColor[0]);
  serialWrite(currentColor[1]);
  serialWrite(currentColor[2]);
  serialWriteCRC();
  //Serial.println("");
}

void setColor(unsigned char &r, unsigned char &g, unsigned char &b){
  if(currentColor[0] == r && currentColor[1] == g && currentColor[2] == b){
    if(currentBrightness != brightness){
      currentBrightness = brightness;
      sendColor();
    }
    return;  
  }

  currentColor[0] = r;
  currentColor[1] = g;
  currentColor[2] = b;

  if(currentBrightness != brightness){
    currentBrightness = brightness;
  }

  sendColor();
}

void processSectorData(){
  // Update Realtime Colors

  unsigned char sectors = 0;
  unsigned int r = 0;
  unsigned int g = 0;
  unsigned int b = 0;

  for(int i = 0; i < 15; i++){
    if(sectorSettings[i] == 0x00){
      break; // Stop when first 0x00 appears
    }
    if(sectorSettings[i] > (unsigned char) 12){
      continue; // Values over 12 are not valid
    }

    r = r + sectorData[(sectorSettings[i]*3) - 3];
    g = g + sectorData[(sectorSettings[i]*3) - 2];
    b = b + sectorData[(sectorSettings[i]*3) - 1];
    
    sectors = sectors + 1;
  }

  if(sectors == 0){
    realtimeColor[0] = 0x00;
    realtimeColor[1] = 0x00;
    realtimeColor[2] = 0x00;
  }else{
    realtimeColor[0] = r / sectors;
    realtimeColor[1] = g / sectors;
    realtimeColor[2] = b / sectors;
  }
}

// Initialization
void setup()
{
  Serial.begin(115200);
  Serial.println();

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

  // Load Settings
  currentBrightness = brightness;
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
          }else{
            #ifdef DEBUG
              Serial.println(F("### FLUSHING MESSAGE WITH SIZE UNDER 7 bytes ###"));
            #endif
            Udp.flush(); // Discard any bytes that have been written to the client but not yet read.
          }
        }
        break;
  
    case Emulator::CLEANUP:
        #ifdef DEBUG
          Serial.println(F("STATE: CLEANUP"));
        #endif
        Udp.flush(); // Discard any bytes that have been written to the client but not yet read.
        read_pos = 0;
        state = Emulator::LISTEN;
        break;
  
    case Emulator::VAL_HEADER: // Validate Header (0xFC)
        #ifdef DEBUG
          //Serial.println(F("STATE: VAL_HEADER"));
        #endif
        crc = 0x00;
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
        // CHECK LENGTH with Packet Length
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
        if(b == groupNumber || // Is Current Group
           b == 0xFF){ // Is Broadcast Group
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
          //Serial.println(F("STATE: DEC_FLAGS"));
        #endif
        read();
        flags = b;
        state = Emulator::DEC_UPPER;
        break;
  
    case Emulator::DEC_UPPER: // Decode Upper Command
        #ifdef DEBUG
          //Serial.println(F("STATE: DEC_UPPER"));
        #endif
        read();
        upper = b;
        state = Emulator::DEC_LOWER;
        break;
  
    case Emulator::DEC_LOWER: // Decode Lower Command
        #ifdef DEBUG
          //Serial.println(F("STATE: DEC_LOWER"));
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
                fillInArray(buffer, 0, name, sizeof(name)); // Device Name
                fillInArray(buffer, 16, groupName, sizeof(groupName)); // Write Group Name
                buffer[32] = groupNumber; // Group Number
                buffer[33] = mode; // Current Mode
                buffer[34] = brightness; // Current Brigthness
                buffer[35] = ambientColor[0]; // Ambient Color: R
                buffer[36] = ambientColor[1]; // Ambient Color: G
                buffer[37] = ambientColor[2]; // Ambient Color: B
                fillInArray(buffer, 42, sectorSettings, 15); // Device Name
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
           memcpy(&name, &buffer, 16);
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
           memcpy(&groupName, &buffer, 16);
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
            groupNumber = buffer[0];
        }
        state = Emulator::CLEANUP;
        break;
    
    case Emulator::COM_SUBSCRIPTION_REQUEST: // Subscription Request
        #ifdef DEBUG
          Serial.println(F("STATE: COM_SUBSCRIPTION_REQUEST"));
        #endif
        if(mode == MODE_VIDEO || mode == MODE_MUSIC){
           #ifdef DEBUG
            Serial.println(F("RESPONDING TO SUBSCRIPTION REQUEST ++"));
           #endif
           buffer[0] = 0x01;
           sendResponse(groupNumber, 0x30, 0x01, 0x0C, buffer, (unsigned char) 1);
        }
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
           memcpy(&sectorSettings, &buffer, 15);
           processSectorData();
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
      
  /*
        case Emulator::COM_NEW_COMMAND: //
        state = Emulator::CLEANUP;
        break;
  */
  
  }

  // Light Logic
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
