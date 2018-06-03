// P0: LED out
// P1: SDA
// P2: SCL
// P3: V- programming
// P4: V+ programming
// P5: IR in / Disgispark programming

#include <EEPROM.h>

#define SCL_PIN 2 
#define SCL_PORT PORTB 
#define SDA_PIN 1 
#define SDA_PORT PORTB
#include <SoftI2CMaster.h>

//instructions
#define PT2257_ADDR 0x88        //Chip address
#define EVC_OFF     0b11111111  //Function OFF (-79dB)
#define EVC_2CH_1   0b11010000  //2-Channel, -1dB/step
#define EVC_2CH_10  0b11100000  //2-Channel, -10dB/step
#define EVC_L_1     0b10100000  //Left Channel, -1dB/step
#define EVC_L_10    0b10110000  //Left Channel, -10dB/step
#define EVC_R_1     0b00100000  //Right Channel, -1dB/step
#define EVC_R_10    0b00110000  //Right Channel, -10dB/step
#define EVC_MUTE    0b01111000  //2-Channel MUTE

//IRremote lib compatible with ATTiny
#include <tiny_IRremote.h>
IRrecv irrecv(5);
decode_results results;  

unsigned long cmdVolDown=0x33B840BF;
unsigned long cmdVolUp=0x33B8708F;
unsigned long cmdMute=0x33B830CF;
unsigned long lastCommand=0;
unsigned long lastTime=0;
uint8_t att;    //attenuation
bool mute=0;
bool stored=0;  //flag to remember if attenuation is tored on eeprom after change
unsigned long t=0;

void setup() {
  i2c_init();  

  //recover the attenuation from eeprom previously saved
  att = EEPROM.read(0);
  if (att>79) att=30;
  evc_setVolume(att);
  evc_mute(0);

  //recover IR codes from eeprom previously programmed
  EEPROM.get(2, cmdVolDown);
  EEPROM.get(6, cmdVolUp);
  EEPROM.get(10, cmdMute);

  irrecv.enableIRIn();

  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(0, OUTPUT);
  //ready
  blink();
  blink();
  blink();
}


void loop() {
  
  if (irrecv.decode(&results)) {

    if (!digitalRead(3) && !digitalRead(4)) {
      //program Mute (pin 3 and 4 both to ground)
      if (results.value!=0xFFFFFFFF) EEPROM.put(10, results.value);
      EEPROM.get(10, cmdMute);
      digitalWrite(0, HIGH);
      delay(300);
      digitalWrite(0, LOW);
      delay(300);
    } else if (!digitalRead(3)) {
      //program VolDown (pin 3 to ground)
      if (results.value!=0xFFFFFFFF) EEPROM.put(2, results.value);
      EEPROM.get(2, cmdVolDown);
      digitalWrite(0, HIGH);
      delay(600);
      digitalWrite(0, LOW);
      delay(600);
    } else if (!digitalRead(4)) {
      //program VolUp (pin 4 to ground)
      if (results.value!=0xFFFFFFFF) EEPROM.put(6, results.value);
      EEPROM.get(6, cmdVolUp);
      digitalWrite(0, HIGH);
      delay(1200);
      digitalWrite(0, LOW);
      delay(1200);
    } else if (results.value==cmdVolDown || results.value==cmdVolUp || results.value==cmdMute) {
      //store command for repeat while keep pressing remote control buttons
      lastCommand=results.value;
      lastTime=millis();
      Command();
    } else if (results.value==0xFFFFFFFF && results.value!=cmdMute) {
      //repeat command while keep pressing remote control buttons
      lastTime=millis();
      Command();
    } else {
      lastCommand=0;
    }
      
    irrecv.resume(); // Receive the next value
  }

  if (!stored && millis()>lastTime+10000) {
    //after 10 s from last attenuation change, store it on eeprom and blink led
    EEPROM.update(0, att);
    stored=1;
    blink();
  }

}

void Command() {

  //volume down
  if (lastCommand==cmdVolDown) {
    if (att<79) att++;
    evc_setVolume(att);
    if (mute) {
      mute=0;
      evc_mute(mute);
    }
    stored=0;
    blink();
  }

  //volume up
  if (lastCommand==cmdVolUp) {
    if (att>0) att--;
    evc_setVolume(att);
    if (mute) {
      mute=0;
      evc_mute(mute);
    }
    stored=0;
    blink();
  }

  //mute
  if (lastCommand==cmdMute) {
    mute=!mute;
    evc_mute(mute);
    blink();
  }
  
}

//blink led
void blink() {
  digitalWrite(0, HIGH);
  delay(30);
  digitalWrite(0, LOW);
  delay(30);
}

//calculate level
byte evc_level(uint8_t dB){
    
    if(dB>79) dB=79;
    
    uint8_t b = dB/10;  //get the most significant digit (eg. 79 gets 7)
    uint8_t a = dB%10;  //get the least significant digit (eg. 79 gets 9)
    
    b = b & 0b0000111; //limit the most significant digit to 3 bit (7)
    
    return (b<<4) | a; //return both numbers in one byte (0BBBAAAA)
}

//set volume
void evc_setVolume(uint8_t dB){
    byte bbbaaaa = evc_level(dB);
    
    byte aaaa = bbbaaaa & 0b00001111;
    byte bbb = (bbbaaaa>>4) & 0b00001111;
   
    if (i2c_start(PT2257_ADDR | I2C_WRITE)){
        i2c_write(EVC_2CH_10 | bbb);
        i2c_write(EVC_2CH_1 | aaaa);
        i2c_stop();
    }

}

//set/reset mute
void evc_mute(bool toggle){
    if (i2c_start(PT2257_ADDR | I2C_WRITE)){
        i2c_write(EVC_MUTE | (toggle & 0b00000001));
        i2c_stop();
    }
}


