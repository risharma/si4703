/*
 
 To use this code, connect the following 5 wires:
 Arduino : Si470x board
 3.3V : VCC
 GND : GND
 A5 : SCLK
 A4 : SDIO
 D2 : RST
 A0 : Trimpot (optional)
 
 Look for serial output at 9600bps.
 
 The Si4703 breakout does work with line out into a stereo or other amplifier. Be sure to test with different length 3.5mm
 cables. Too short of a cable may degrade reception.
 
 Set Station - "3xxxx^" e.g. "3949^"
 Get Station - "4^"
 Get RDS_RT  - "8xxxx^" e.g. "85000^"
 */

#include <Wire.h>

int STATUS_LED = 13;
int resetPin = 2;
int SDIO = A4; //SDA/A4 on Arduino
int SCLK = A5; //SCL/A5 on Arduino
char printBuffer[50];
uint16_t si4703_registers[16]; //There are 16 registers, each 16 bits large

#define FAIL     0
#define SUCCESS  1

#define SI4703        0x10 //0b._001.0000 = I2C address of Si4703 - note that the Wire function assumes non-left-shifted I2C address, not 0b.0010.000W
#define I2C_FAIL_MAX  10 //This is the number of attempts we will try to contact the device before erroring out

#define SEEK_DOWN  0 //Direction used for seeking. Default is down
#define SEEK_UP    1

//Define the register names
#define DEVICEID     0x00
#define CHIPID       0x01
#define POWERCFG     0x02
#define CHANNEL      0x03
#define SYSCONFIG1   0x04
#define SYSCONFIG2   0x05
#define STATUSRSSI   0x0A
#define READCHAN     0x0B
#define RDSA         0x0C
#define RDSB         0x0D
#define RDSC         0x0E
#define RDSD         0x0F

#define _BM(bit) (1 << ((uint16_t)bit)) // convert bit number to bit mask
#define PWR_DISABLE _BM(6)
#define PWR_ENABLE  _BM(0)

//Register 0x02 - POWERCFG
#define SMUTE   15
#define DMUTE   14
#define SKMODE  10
#define SEEKUP  9
#define SEEK    8

//Register 0x03 - CHANNEL
#define TUNE  15

//Register 0x04 - SYSCONFIG1
#define RDS  12
#define DE   11

//Register 0x05 - SYSCONFIG2
#define SPACE1  5
#define SPACE0  4

//Register 0x0A - STATUSRSSI
#define RDSR    15
#define STC     14
#define SFBL    13
#define AFCRL   12
#define RDSS    11
#define STEREO  8

#define  SEEK_UP      '1'
#define  SEEK_DOWN    '2'

#define  SET_STATION  '3'
#define  GET_STATION  '4'
#define  GET_RDS_RT   '8'

boolean  stringComplete = false;
String   inputString    = "";
  
void setup() {                
  pinMode(13, OUTPUT);
  pinMode(A0, INPUT); //Optional trimpot for analog station control

  Serial.begin(9600);
  inputString.reserve(200);
  
  si4703_init(); //Init the Si4703 - we need to toggle SDIO before Wire.begin takes over.
  for (int itr = 0; itr < 15; itr++)
  {
    volumeUp(); // Increment volume to level 15
  }
}

void loop() 
{
  int currentChannel; //Default the unit to a known good local radio station
  serialEvent();
  
  if (stringComplete) 
  {
    if (inputString[0] == SET_STATION)
    {
      String chan = "";
      char getChan[4] = {0};
      int i = 0;
      
      if (4 == inputString.length())
      {
        getChan[i++] = '0';
      }

      for(int itr = 1; itr < inputString.length(); itr++)
      {
        if (inputString[itr] != '^')
        {
          getChan[i] = inputString[itr];
          i++;
        }
      }
      chan += getChan;
      gotoChannel(chan.toInt());
    }
    
    else if (inputString[0] == GET_STATION)
    {
      currentChannel = readChannel();
      //sprintf(printBuffer, "%02d.%01dMHz", currentChannel / 10, currentChannel % 10);
      Serial.println(currentChannel);
    }
    
    else if (inputString[0] == GET_RDS_RT)
    {
      String tOut = "";
      char getTimeout[5] = {0};
      int i = 0;
      
      for(int itr = 1; itr < inputString.length(); itr++)
      {
        if (inputString[itr] != '^')
        {
          getTimeout[i] = inputString[itr];
          i++;
        }
      }
      tOut += getTimeout;
      siGetRT(tOut.toInt());
    }
    
    else if (inputString[0] == '9')
    {
      siGetPS(2000);
    }
    
    else if (inputString[0] == SEEK_UP)
    {
      seekUP();
    }
    
    else if (inputString[0] == SEEK_DOWN)
    {
      seekDOWN();
    }
    
    else if (inputString[0] == '+')
    {
      volumeUp();
    }
    
    else if (inputString[0] == '-') 
    {
       volumeDown(); 
    }
    
    else if (inputString[0] == '5')
    {
      //Set Volume
    }
    
    else if (inputString[0] == '6')
    {
      //current volume
    }
    
    else if (inputString[0] == '<')
    {
      //Tune up
    }
    
    else if (inputString[0] == '>')
    {
      //Tune down
    }
   
    else if (inputString[0] == '7')
    {
      mute();
    }
    
    else if (inputString[0] == '0')
    {
      siPowerOff();
    }
    
    inputString = "";
    stringComplete = false;
  }
}

void serialEvent()
{
  while (Serial.available())
  //while (!Serial.available()){if (true == getRDSRT) siGetRT(3000); };
  {
    char inChar = (char)Serial.read();
    inputString += inChar;

    if (inChar == '^')
    {
      //inputString += '\n';
      stringComplete = true;
    }
  }
}

//Given a channel, tune to it
//Channel is in MHz, so 973 will tune to 97.3MHz
//Note: gotoChannel will go to illegal channels (ie, greater than 110MHz)
//It's left to the user to limit these if necessary
//Actually, during testing the Si4703 seems to be internally limiting it at 87.5. Neat.
void gotoChannel(int newChannel){
  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //97.3 = 0.2 * Chan + 87.5
  //9.8 / 0.2 = 49
  newChannel *= 10; //973 * 10 = 9730
  newChannel -= 8750; //9730 - 8750 = 980
  newChannel /= 20; //980 / 20 = 49
  
  //These steps come from AN230 page 20 rev 0.5
  si4703_readRegisters();
  si4703_registers[CHANNEL] &= 0xFE00; //Clear out the channel bits
  si4703_registers[CHANNEL] |= newChannel; //Mask in the new channel
  si4703_registers[CHANNEL] |= (1<<TUNE); //Set the TUNE bit to start
  si4703_updateRegisters();

  //delay(60); //Wait 60ms - you can use or skip this delay

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
    //Serial.println("Tuning");
  }

  si4703_readRegisters();
  si4703_registers[CHANNEL] &= ~(1<<TUNE); //Clear the tune after a tune has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
    //Serial.println("Waiting...");
  }
}

//Reads the current channel from READCHAN
//Returns a number like 973 for 97.3MHz
int readChannel(void) {
  si4703_readRegisters();
  int channel = si4703_registers[READCHAN] & 0x03FF; //Mask out everything but the lower 10 bits

  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //X = 0.2 * Chan + 87.5
  channel *= 2; //49 * 2 = 98
  channel += 875; //98 + 875 = 973
  return(channel);
}

//Seeks out the next available station
//Returns the freq if it made it
//Returns zero if failed
byte seek(byte seekDirection){
  si4703_readRegisters();

  //Set seek mode wrap bit
  //si4703_registers[POWERCFG] |= (1<<SKMODE); //Allow wrap
  si4703_registers[POWERCFG] &= ~(1<<SKMODE); //Disallow wrap - if you disallow wrap, you may want to tune to 87.5 first

  if(seekDirection == SEEK_DOWN) si4703_registers[POWERCFG] &= ~(1<<SEEKUP); //Seek down is the default upon reset
  else si4703_registers[POWERCFG] |= 1<<SEEKUP; //Set the bit to seek up

  si4703_registers[POWERCFG] |= (1<<SEEK); //Start seek

  si4703_updateRegisters(); //Seeking will now start

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if((si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!

    //Serial.print("Trying station:");
    //Serial.println(readChannel());
  }

  si4703_readRegisters();
  int valueSFBL = si4703_registers[STATUSRSSI] & (1<<SFBL); //Store the value of SFBL
  si4703_registers[POWERCFG] &= ~(1<<SEEK); //Clear the seek bit after seek has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
    //Serial.println("Waiting...");
  }

  if(valueSFBL) { //The bit was set indicating we hit a band limit or failed to find a station
    Serial.println("Seek limit hit"); //Hit limit of band during seek
    return(FAIL);
  }

  //Serial.println("Seek complete"); //Tuning complete!
  return(SUCCESS);
}

//To get the Si4703 inito 2-wire mode, SEN needs to be high and SDIO needs to be low after a reset
//The breakout board has SEN pulled high, but also has SDIO pulled high. Therefore, after a normal power up
//The Si4703 will be in an unknown state. RST must be controlled
void si4703_init(void) {
  //Serial.println("Initializing I2C and Si4703");
  
  pinMode(resetPin, OUTPUT);
  pinMode(SDIO, OUTPUT); //SDIO is connected to A4 for I2C
  digitalWrite(SDIO, LOW); //A low SDIO indicates a 2-wire interface
  digitalWrite(resetPin, LOW); //Put Si4703 into reset
  delay(1); //Some delays while we allow pins to settle
  digitalWrite(resetPin, HIGH); //Bring Si4703 out of reset with SDIO set to low and SEN pulled high with on-board resistor
  delay(1); //Allow Si4703 to come out of reset

  Wire.begin(); //Now that the unit is reset and I2C inteface mode, we need to begin I2C

  si4703_readRegisters(); //Read the current register set
  //si4703_registers[0x07] = 0xBC04; //Enable the oscillator, from AN230 page 9, rev 0.5 (DOES NOT WORK, wtf Silicon Labs datasheet?)
  si4703_registers[0x07] = 0x8100; //Enable the oscillator, from AN230 page 9, rev 0.61 (works)
  si4703_updateRegisters(); //Update

  delay(500); //Wait for clock to settle - from AN230 page 9

  si4703_readRegisters(); //Read the current register set
  si4703_registers[POWERCFG] = 0x4001; //Enable the IC
  //si4703_registers[POWERCFG] = PWR_ENABLE;
  
  //  si4703_registers[POWERCFG] |= (1<<SMUTE) | (1<<DMUTE); //Disable Mute, disable softmute
  si4703_registers[SYSCONFIG1] |= (1<<RDS); //Enable RDS

  si4703_registers[SYSCONFIG2] &= ~(1<<SPACE1 | 1<<SPACE0) ; //Force 200kHz channel spacing for USA

  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= 0x0001; //Set volume to lowest
  si4703_updateRegisters(); //Update

  delay(110); //Max powerup time, from datasheet page 13
}

void siPowerOff(void)
{
  si4703_readRegisters(); //Read the current register set
  si4703_registers[POWERCFG] = PWR_DISABLE | PWR_ENABLE; //power down condition

  si4703_updateRegisters(); //Update
}

//Write the current 9 control registers (0x02 to 0x07) to the Si4703
//It's a little weird, you don't write an I2C addres
//The Si4703 assumes you are writing to 0x02 first, then increments
byte si4703_updateRegisters(void) {

  Wire.beginTransmission(SI4703);
  //A write command automatically begins with register 0x02 so no need to send a write-to address
  //First we send the 0x02 to 0x07 control registers
  //In general, we should not write to registers 0x08 and 0x09
  for(int regSpot = 0x02 ; regSpot < 0x08 ; regSpot++) {
    byte high_byte = si4703_registers[regSpot] >> 8;
    byte low_byte = si4703_registers[regSpot] & 0x00FF;

    Wire.write(high_byte); //Upper 8 bits
    Wire.write(low_byte); //Lower 8 bits
  }

  //End this transmission
  byte ack = Wire.endTransmission();
  if(ack != 0) { //We have a problem! 
    Serial.print("Write Fail:"); //No ACK!
    Serial.println(ack, DEC); //I2C error: 0 = success, 1 = data too long, 2 = rx NACK on address, 3 = rx NACK on data, 4 = other error
    return(FAIL);
  }

  return(SUCCESS);
}

//Read the entire register control set from 0x00 to 0x0F
void si4703_readRegisters(void){

  //Si4703 begins reading from register upper register of 0x0A and reads to 0x0F, then loops to 0x00.
  Wire.requestFrom(SI4703, 32); //We want to read the entire register set from 0x0A to 0x09 = 32 bytes.

  while(Wire.available() < 32) ; //Wait for 16 words/32 bytes to come back from slave I2C device
  //We may want some time-out error here

  //Remember, register 0x0A comes in first so we have to shuffle the array around a bit
  for(int x = 0x0A ; ; x++) { //Read in these 32 bytes
    if(x == 0x10) x = 0; //Loop back to zero
    si4703_registers[x] = Wire.read() << 8;
    si4703_registers[x] |= Wire.read();
    if(x == 0x09) break; //We're done!
  }
}

void si4703_printRegisters(void) {
  //Read back the registers
  si4703_readRegisters();

  //Print the response array for debugging
  for(int x = 0 ; x < 16 ; x++) {
    sprintf(printBuffer, "Reg 0x%02X = 0x%04X", x, si4703_registers[x]);
    Serial.println(printBuffer);
  }
}

void seekUP (void)
{
  seek(SEEK_UP);
}

void seekDOWN (void)
{
  seek(SEEK_DOWN);
}

void mute(void)
{
  si4703_readRegisters();
  si4703_registers[POWERCFG] ^= (1<<DMUTE); //Toggle Mute bit
  si4703_updateRegisters();
}
void volumeUp(void)
{
  byte  currentVolume;
  si4703_readRegisters();
  
  currentVolume = si4703_registers[SYSCONFIG2] & 0x000F; //Read the current volume level
  if (currentVolume < 16) currentVolume++; //Limit max volume to 0x000F
  
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= currentVolume; //Set new volume
  si4703_updateRegisters();
}

void volumeDown(void)
{
  byte  currentVolume;
  si4703_readRegisters();
  
  currentVolume = si4703_registers[SYSCONFIG2] & 0x000F; //Read the current volume level
  if (currentVolume > 0) currentVolume--; //Limit max volume to 0x000F
  
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= currentVolume; //Set new volume
  si4703_updateRegisters();
}

//#define _BM(bit) (1 << ((uint16_t)bit))

void siGetPS(int timeout)
{
  char psName[16] = {0};
  int dt = 0;
  uint16_t ps_mask = 0;
  
  while(dt < timeout)
  {
    if (ps_mask == 0x0F)
    {
      Serial.print("PS REG: ");
      Serial.println(si4703_registers[RDSA]);
    }
    si4703_readRegisters();
    
    if(si4703_registers[STATUSRSSI] & RDSR)
    {
      uint16_t gt = (si4703_registers[RDSB] >> 12) & 0x0F; // group type
      if (gt == 0)
      {
        uint16_t idx = (si4703_registers[RDSB] & 0x03); // PS name
        ps_mask != _BM(idx);
        idx <<= 1;
        psName[idx] = si4703_registers[RDSD] >> 8;
        if (psName[idx] < ' ') psName[idx] = '?';
        psName[idx+1] = si4703_registers[RDSD] & 0xFF;
        if (psName[idx+1] < ' ') psName[idx+1] = '?';
      }
      delay(40);
      dt += 40;
    }
    else
    {
      delay(30);
      dt += 30;
    }
  }
  
  Serial.print("PS NAME: ");
  Serial.println(psName);
}

void siGetRT(int timeout)
{
  char rtText[64] = {0};
  int dt = 0;
  uint16_t rt_mask = 0; // mask of radiotext regments processed
  uint16_t end_mask = 0xFFFF; // mask of radioootext segments processed
 
  while(dt < timeout)
  {
    if (rt_mask == end_mask)
    {
      break;
    }
    si4703_readRegisters();
    
    if(si4703_registers[STATUSRSSI] & RDSR)
    {
      uint16_t gt = (si4703_registers[RDSB] >> 12) & 0x0F; // group type
      if (gt == 2)
      {
        uint16_t seg = si4703_registers[RDSB] & 0x0F;
        rt_mask |= _BM(seg);
        uint16_t idx = seg << 2;
        char ch = si4703_registers[RDSC] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rtText[idx] = ch;
        ch = si4703_registers[RDSC] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rtText[idx+1] = ch;
        ch = si4703_registers[RDSD] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rtText[idx+2] = ch;
        ch = si4703_registers[RDSD] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rtText[idx+3] = ch;
      }
      delay(40);
      dt += 40;
    }
    else
    {
      delay(30);
      dt += 30;
    }
  }
  
  //Serial.print("RADIO TEXT: ");
  Serial.println(rtText);
}

void siGetRDS (int timeout)
{
  int endtime = 0;
  char ps_name[16];
  char rt_text[80];
  
  uint16_t ps_mask = 0; // mask of PS segments processed
  uint16_t rt_mask = 0; // mask of radiotext segments processed
  uint16_t di_mask = 0; // DI mask
  uint16_t gt_mask = 0; // mask of groups detected
  uint16_t end_mask = 0xFFFF;
  
  while(endtime < timeout)
  {
    if ((rt_mask == end_mask) && (ps_mask == 0x0F))
    {
      break;
    }
    si4703_readRegisters();
    
    if(si4703_registers[STATUSRSSI] & RDSR)
    {
      uint16_t gt = (si4703_registers[RDSB] >> 12) & 0x0F; // group type
      uint16_t ver = (si4703_registers[RDSB] >> 11) & 0x01; //version
      uint16_t tp = (si4703_registers[RDSB] >> 10) & 0x01; // traffic program
      uint16_t pty = (si4703_registers[RDSB] >> 5) & 0x1F;
      gt_mask != _BM(gt);
      
      if (_BM(gt))
      {
        //Serial.print("RDSA: ");
        //Serial.println(si4703_registers[RDSA]);
        //Serial.print("RDSB: ");
        //Serial.println(si4703_registers[RDSB]);
        //Serial.print("RDSC: ");
        //Serial.println(si4703_registers[RDSC]);
        //Serial.print("RDSD: ");
        //Serial.println(si4703_registers[RDSD]);
        //Serial.print("GT: ");
        //Serial.print(gt);
        //Serial.print(" Ver: ");
        //Serial.print(0x0A + ver);
        //Serial.print(" PTY: ");
        //Serial.print(pty);
        //Serial.print(" TP: ");
        //Serial.println(tp);
      }
      
      if (gt == 0)
      {
        uint16_t ta = (si4703_registers[RDSB] >> 4) & 0x01; // Traffic Announcement
        uint16_t ms = (si4703_registers[RDSB] >> 3) & 0x01; // Music/Speech
        uint16_t di = (si4703_registers[RDSB] >> 2) & 0x01; // Decoder control bit
        uint16_t idx = (si4703_registers[RDSB] & 0x03); // PS name
        if (di) di_mask |= _BM(3-idx);
        else di_mask &= ~_BM(3-idx);
        if (_BM(0))
        {
          //Serial.print("TA: ");
          //Serial.print(ta);
          //Serial.print(" MS: ");
          //Serial.print(ms);
          //Serial.print(" DI: ");
          //Serial.print(di_mask);
          //Serial.print(" idx: ");
          //Serial.println(idx);
        }
        ps_mask |= _BM(idx);
        idx <<= 1;
        ps_name[idx] = si4703_registers[RDSD] >> 8;
        if (ps_name[idx] < ' ') ps_name[idx] = '?';
        ps_name[idx+1] = si4703_registers[RDSD] & 0xFF;
        if (ps_name[idx+1] < ' ') ps_name[idx+1] = '?';
        if (_BM(0))
        {
          Serial.print("PS NAME: ");
          Serial.println(ps_name);
        }
      }
      
      if (gt == 2)
      {
        // Radiotext
        uint16_t ab = (si4703_registers[RDSB] >> 4) & 0x01;
        uint16_t seg = si4703_registers[RDSB] & 0x0F;
        if (_BM(2))
        {
          //Serial.print("A/B: ");
          //Serial.print(ab);
          //Serial.print(" Segment: ");
          //Serial.println(seg);
        }
        rt_mask |= _BM(seg);
        uint16_t idx = seg << 2;
        
        char ch = si4703_registers[RDSC] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx] = ch;
        ch = si4703_registers[RDSC] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+1] = ch;
        ch = si4703_registers[RDSD] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+2] = ch;
        ch = si4703_registers[RDSD] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+3] = ch;
        if (_BM(2))
        {
          Serial.print("RADIO TEXT: ");
          Serial.println(rt_text);
        }
      }
      //Serial.println();
      delay(40); //wait for the RDS bit to clear
      endtime += 40;
    }
    else
    {
      delay(30);
      endtime += 30;
    }
  }
}

void getRDS (int timeout)
{
  int endtime = 0;
  char ps_name[16];
  char rt_text[80];
  
  uint16_t ps_mask = 0; // mask of PS segments processed
  uint16_t rt_mask = 0; // mask of radiotext segments processed
  uint16_t di_mask = 0; // DI mask
  uint16_t gt_mask = 0; // mask of groups detected
  uint16_t end_mask = 0xFFFF;
  
  while(endtime < timeout)
  {
    if ((rt_mask == end_mask) && (ps_mask == 0x0F))
    {
      break;
    }
    si4703_readRegisters();
    
    if(si4703_registers[STATUSRSSI] & RDSR)
    {
      uint16_t gt = (si4703_registers[RDSB] >> 12) & 0x0F; // group type
      uint16_t ver = (si4703_registers[RDSB] >> 11) & 0x01; //version
      uint16_t tp = (si4703_registers[RDSB] >> 10) & 0x01; // traffic program
      uint16_t pty = (si4703_registers[RDSB] >> 5) & 0x1F;
      gt_mask != _BM(gt);
      
      if (_BM(gt))
      {
        Serial.print("RDSA: ");
        Serial.println(si4703_registers[RDSA]);
        Serial.print("RDSB: ");
        Serial.println(si4703_registers[RDSB]);
        Serial.print("RDSC: ");
        Serial.println(si4703_registers[RDSC]);
        Serial.print("RDSD: ");
        Serial.println(si4703_registers[RDSD]);
        Serial.print("GT: ");
        Serial.print(gt);
        Serial.print(" Ver: ");
        Serial.print(0x0A + ver);
        Serial.print(" PTY: ");
        Serial.print(pty);
        Serial.print(" TP: ");
        Serial.println(tp);
      }
      
      if (gt == 0)
      {
        uint16_t ta = (si4703_registers[RDSB] >> 4) & 0x01; // Traffic Announcement
        uint16_t ms = (si4703_registers[RDSB] >> 3) & 0x01; // Music/Speech
        uint16_t di = (si4703_registers[RDSB] >> 2) & 0x01; // Decoder control bit
        uint16_t idx = (si4703_registers[RDSB] & 0x03); // PS name
        if (di) di_mask |= _BM(3-idx);
        else di_mask &= ~_BM(3-idx);
        if (_BM(0))
        {
          Serial.print("TA: ");
          Serial.print(ta);
          Serial.print(" MS: ");
          Serial.print(ms);
          Serial.print(" DI: ");
          Serial.print(di_mask);
          Serial.print(" idx: ");
          Serial.println(idx);
        }
        ps_mask |= _BM(idx);
        idx <<= 1;
        ps_name[idx] = si4703_registers[RDSD] >> 8;
        if (ps_name[idx] < ' ') ps_name[idx] = '?';
        ps_name[idx+1] = si4703_registers[RDSD] & 0xFF;
        if (ps_name[idx+1] < ' ') ps_name[idx+1] = '?';
        if (_BM(0))
        {
          Serial.print("PS NAME: ");
          Serial.println(ps_name);
        }
      }
      
      if (gt == 2)
      {
        // Radiotext
        uint16_t ab = (si4703_registers[RDSB] >> 4) & 0x01;
        uint16_t seg = si4703_registers[RDSB] & 0x0F;
        if (_BM(2))
        {
          Serial.print("A/B: ");
          Serial.print(ab);
          Serial.print(" Segment: ");
          Serial.println(seg);
        }
        rt_mask |= _BM(seg);
        uint16_t idx = seg << 2;
        
        char ch = si4703_registers[RDSC] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx] = ch;
        ch = si4703_registers[RDSC] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+1] = ch;
        ch = si4703_registers[RDSD] >> 8;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+2] = ch;
        ch = si4703_registers[RDSD] & 0xFF;
        if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; }
        if (ch < ' ') ch = '?';
        rt_text[idx+3] = ch;
        if (_BM(2))
        {
          Serial.print("RADIO TEXT: ");
          Serial.println(rt_text);
        }
      }
      Serial.println();
      delay(40); //wait for the RDS bit to clear
      endtime += 40;
    }
    else
    {
      delay(30);
      endtime += 30;
    }
  }
}

void readRDS_PS(long timeout)
{ 
  char buffer[10];
  long endTime = millis() + timeout;
  boolean completed[] = {false, false, false, false};
  int completedCount = 0;
  while(completedCount < 4 && millis() < endTime) 
  {
	si4703_readRegisters();
	if(si4703_registers[STATUSRSSI] & (1<<RDSR))
        {
	  // ls 2 bits of B determine the 4 letter pairs
	  // once we have a full set return
	  // if you get nothing after 20 readings return with empty string
	  uint16_t b = si4703_registers[RDSB];
	  int index = b & 0x03;
	  if (! completed[index] && b < 500)
	  {
		completed[index] = true;
		completedCount ++;
	  	char Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
      	        char Dl = (si4703_registers[RDSD] & 0x00FF);
		buffer[index * 2] = Dh;
		buffer[index * 2 +1] = Dl;
		// Serial.print(si4703_registers[RDSD]); Serial.print(" ");
		// Serial.print(index);Serial.print(" ");
		// Serial.write(Dh);
		// Serial.write(Dl);
		// Serial.println();
          }
          delay(40); //Wait for the RDS bit to clear
	}
	else 
        {
	  delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
	}
    }
    if (millis() >= endTime) 
    {
	buffer[0] ='\0';
	return;
    }

  buffer[8] = '\0';
  
  Serial.print("PS: ");
  Serial.println(buffer);
}

void siRdsRtGet(void)
{
  char rdsRt[64] = {0};
  int  dt = 0;
  uint16_t rt_mask = 0; // mask of radiotext regments processed
  uint16_t end_mask = 0xFFFF; // mask of radioootext segments processed
  boolean breakOut  = false;
 
  while(breakOut == false)
  {
    if (rt_mask == end_mask)
    {
      breakOut = true;
    }
    
    if (breakOut == false)
    {
      si4703_readRegisters();
    
      if(si4703_registers[STATUSRSSI] & RDSR)
      {
        uint16_t gt = (si4703_registers[RDSB] >> 12) & 0x0F; // group type
        if (gt == 2)
        {
          uint16_t seg = si4703_registers[RDSB] & 0x0F;
          rt_mask |= _BM(seg);
          uint16_t idx = seg << 2;
          char ch = si4703_registers[RDSC] >> 8;
          if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; breakOut = true; }
          if (ch < ' ') ch = '?';
          rdsRt[idx] = ch;
          ch = si4703_registers[RDSC] & 0xFF;
          if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; breakOut = true; }
          if (ch < ' ') ch = '?';
          rdsRt[idx+1] = ch;
          ch = si4703_registers[RDSD] >> 8;
          if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; breakOut = true; }
          if (ch < ' ') ch = '?';
          rdsRt[idx+2] = ch;
          ch = si4703_registers[RDSD] & 0xFF;
          if (ch == '\r') { end_mask = 0xFFFF >> (15 - seg); ch = '^'; breakOut = true; }
          if (ch < ' ') ch = '?';
          rdsRt[idx+3] = ch;
        }
        delay(40);
      }
      else
      {
        delay(30);
      }
    }
  }
  //Serial.print("RADIO TEXT: ");
  Serial.println(rdsRt);    
}
