

// Default DS Device Variables
//unsigned char name[16] = "ESPSKEmu";
//unsigned char groupName[16] = "Nogroup";
//byte groupNumber = 0x00;
byte brightness = 100;
byte mode = 0x00;
byte ambientColor[3] = {0x00, 0x00, 0x00};
byte ambientScene = 0x00;
byte ambientMode = 0x00;
byte sectorSettings[15] = {0xFF};
//byte saturationR = 255;
//byte saturationG = 255;
//byte saturationB = 255;

struct { 
	unsigned char name[16] = "ESPSKEmu";
	unsigned char groupName[16] = "Nogroup";
	byte groupNumber = 0x00;
	//byte sectorSetingsBin = 0x00;
	byte sectorSettings[15] = {0xFF};
	byte saturationR = 255;
	byte saturationG = 255;
	byte saturationB = 255;
} EmulatorPersistentData;

// Light Emulation
byte currentBrightness = 0;
byte currentColor[3] = {0x00, 0x00, 0x00};
byte realtimeColor[3] = {0x00, 0x00, 0x00};
byte sectorData[36] = {0x00};
bool saturationChanged = false;

// Buffers
byte buffer[256] = {0x00};
float fbuffer[4] = {0x00};

// Network Read and Write Functions
unsigned char b;
unsigned char crc = 0x00;
unsigned int read_length = 0;
unsigned int read_pos = 0;

// Constants
#define DSDEVICEID 0x03 // Device ID (0x03 for SideKick)
#define DSSERVERPORT 8888

// Modes
#define MODE_SLEEP 0x00
#define MODE_VIDEO 0x01
#define MODE_MUSIC 0x02
#define MODE_AMBIENT 0x03

// Emulator State
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
      COM_AMBIENT_SCENE,
      COM_SATURATION_SETTING
  };
}

// Message Data
byte group;
byte flags;
byte upper;
byte lower;

// EEPROM
bool eeprom_dirty = false;
unsigned long eeprom_last_timestamp = 0;

// Action Timestamp
unsigned long action_last_timestamp = 0;
bool receiving_data = false;

// Emulator State
Emulator::State state = Emulator::LISTEN;

// Constants
unsigned char payload_header = 0xFC;
unsigned char payload_length_default = 0x05;
unsigned char zero = 0x00;
unsigned char one = 0x01;

WiFiUDP Udp;

// Timers
unsigned long currentMillis;
unsigned long lastTimeDiff;

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

// Read Sector Data and Find Color
void processSectorData(){
  unsigned char sectors = 0;
  unsigned int r = 0;
  unsigned int g = 0;
  unsigned int b = 0;

  for(int i = 0; i < 15; i++){
    if(EmulatorPersistentData.sectorSettings[i] == 0x00){
      break; // Stop when first 0x00 appears
    }
    if(EmulatorPersistentData.sectorSettings[i] > (unsigned char) 12){
      continue; // Values over 12 are not valid
    }

    r = r + sectorData[(EmulatorPersistentData.sectorSettings[i]*3) - 3];
    g = g + sectorData[(EmulatorPersistentData.sectorSettings[i]*3) - 2];
    b = b + sectorData[(EmulatorPersistentData.sectorSettings[i]*3) - 1];
    
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
