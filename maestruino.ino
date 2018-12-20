/*************************************************** 
  This is an example for the Adafruit VS1053 Codec Breakout

  Designed specifically to work with the Adafruit VS1053 Codec Breakout 
  ----> https://www.adafruit.com/products/1381

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// Don't forget to copy the v44k1q05.img patch to your micro SD 
// card before running this example!

#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <string.h>

/****** START maestro_record.ino variables ******/

// define the pins used
#define RESET 9      // VS1053 reset pin (output)
#define CS 10        // VS1053 chip select pin (output)
#define DCS 8        // VS1053 Data/command select pin (output)
#define CARDCS A0     // Card chip select pin
#define DREQ A1       // VS1053 Data request, ideally an Interrupt pin

bool record;
int thresholdCount;
int recordCount;
#define THRESHOLD_TIME 100
#define THRESHOLD 20
#define MAX 150
#define RECORD_TIME 5000

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, CS, DCS, DREQ, CARDCS);

File recording;  // the file we will save our recording to
#define RECBUFFSIZE 128  // 64 or 128 bytes.
uint8_t recording_buffer[RECBUFFSIZE];
uint8_t isRecording = false;
char lastFilename[15];
/****** END maestro_record.ino variables ******/

/****** START maestro_esp8266.ino variables ******/

#define RX 5  //RX to ESP8266
#define TX 6  //TX to ESP8266
#define PACKET_SIZE 512 //Max data to send in one chunk over esp8266

int timeout = 3; //Seconds for connection timeout
String AP = "FellowshipOfThePing";
String PASS = "typefriendandenter";
String SERVER_IP = "192.168.1.142";
int SERVER_PORT = 19220;
SoftwareSerial esp8266(RX,TX);

/****** END maestro_esp8266.ino variables ******/


void setup() {
  record_setup();
  esp8266_setup();
}

void record_setup() {
  // initialize digital pin for LED as an output.
  pinMode(4, OUTPUT);
  pinMode(7, OUTPUT);
  record = false;
  thresholdCount = 0;
  Serial.begin(9600);
  Serial.println("Adafruit VS1053 Ogg Recording Test");
  // initialise the music player
  if (!musicPlayer.begin()) {
    Serial.println("VS1053 not found");
    while (1);  // don't do anything more
  }

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    Serial.println("SD failed, or not present");
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
  
  // load plugin from SD card! We'll use mono 44.1KHz, high quality
  if (! musicPlayer.prepareRecordOgg("v44k1q05.img")) {
     Serial.println("Couldn't load plugin!");
     while (1);    
  }
}

void esp8266_setup() {
  Serial.begin(9600);
  esp8266.begin(115200);
  esp8266.listen();
  if (!sendCommand("AT", timeout, "OK")){
    delay(20);
    esp8266.print("+++");
    delay(1000);
  }
  sendCommand("AT+CIPMODE=0", timeout, "OK");
  sendCommand("AT+CWMODE=1", timeout, "OK");
  wifi_join(AP, PASS);
}



void loop() {
  record_loop();
}





/********** START maestro_record.ino functions **********/



void record_loop() {
  if (!isRecording && record) 
  {
    digitalWrite(7, HIGH);   // turn the LED on
    Serial.println("Begin recording");
    isRecording = true;
    musicPlayer.prepareRecordOgg("v44k1q05.img");
    // Check if the file exists already
    char filename[15];
    strcpy(filename, "RECORD00.OGG");
    for (uint8_t i = 0; i < 100; i++) 
    {
      filename[6] = '0' + i/10;
      filename[7] = '0' + i%10;
      // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) 
      {
        break;
      }
    }
    Serial.print("Recording to "); Serial.println(filename);
    strcpy(lastFilename, filename);
    recording = SD.open(filename, FILE_WRITE);
    if (! recording) 
    {
       Serial.println("Couldn't open file to record!");
       while (1);
    }
    digitalWrite(7, LOW);   // turn the LED on
    musicPlayer.startRecordOgg(true); // use microphone (for linein, pass in 'false')
    thresholdCount = 0;
    record = false;
    digitalWrite(4, HIGH);   // turn the LED on
  }
  else if(!isRecording && !record)
  {
    int sensorValue = analogRead(A2);
    if(sensorValue > THRESHOLD)
    {
      thresholdCount = thresholdCount + 5;
    }
    else if(thresholdCount > 0)
    {
      thresholdCount--;
    }
    if(sensorValue >= MAX)
    {
      thresholdCount = 0;
    }
    if(thresholdCount > THRESHOLD_TIME)
    {
      record = true;
    }
  }
  else if (isRecording && recordCount > RECORD_TIME) 
  {
    digitalWrite(4, LOW);   // turn the LED off
    recordCount = 0;
    Serial.println("End recording");
    musicPlayer.stopRecordOgg();
    isRecording = false;
    // flush all the data!
    saveRecordedData(isRecording);
    // close it up
    recording.close();
    delay(1000);
    wifi_tcp_send_file(lastFilename);
  }
  else if (isRecording)
  {
    saveRecordedData(isRecording);
    recordCount++;
  }
}

uint16_t saveRecordedData(bool isrecord) {
  uint16_t written = 0;
  
    // read how many words are waiting for us
  uint16_t wordswaiting = musicPlayer.recordedWordsWaiting();
  
  // try to process 256 words (512 bytes) at a time, for best speed
  while (wordswaiting > 256) {
    //Serial.print("Waiting: "); Serial.println(wordswaiting);
    // for example 128 bytes x 4 loops = 512 bytes
    for (int x=0; x < 512/RECBUFFSIZE; x++) {
      // fill the buffer!
      for (uint16_t addr=0; addr < RECBUFFSIZE; addr+=2) {
        uint16_t t = musicPlayer.recordedReadWord();
        //Serial.println(t, HEX);
        recording_buffer[addr] = t >> 8; 
        recording_buffer[addr+1] = t;
      }
      if (! recording.write(recording_buffer, RECBUFFSIZE)) {
            Serial.print("Couldn't write "); Serial.println(RECBUFFSIZE); 
            while (1);
      }
    }
    // flush 512 bytes at a time
    recording.flush();
    written += 256;
    wordswaiting -= 256;
  }
  
  wordswaiting = musicPlayer.recordedWordsWaiting();
  if (!isrecord) {
    Serial.print(wordswaiting); Serial.println(" remaining");
    // wrapping up the recording!
    uint16_t addr = 0;
    for (int x=0; x < wordswaiting-1; x++) {
      // fill the buffer!
      uint16_t t = musicPlayer.recordedReadWord();
      recording_buffer[addr] = t >> 8; 
      recording_buffer[addr+1] = t;
      if (addr > RECBUFFSIZE) {
          if (! recording.write(recording_buffer, RECBUFFSIZE)) {
                Serial.println("Couldn't write!");
                while (1);
          }
          recording.flush();
          addr = 0;
      }
    }
    if (addr != 0) {
      if (!recording.write(recording_buffer, addr)) {
        Serial.println("Couldn't write!"); while (1);
      }
      written += addr;
    }
    musicPlayer.sciRead(VS1053_SCI_AICTRL3);
    if (! (musicPlayer.sciRead(VS1053_SCI_AICTRL3) & _BV(2))) {
       recording.write(musicPlayer.recordedReadWord() & 0xFF);
       written++;
    }
    recording.flush();
  }

  return written;
}

/********** END maestro_record.ino functions **********/



/********** START maestro_esp8266.ino functions **********/

void esp8266_loop() {
  bool connected = wifi_tcp_connect(SERVER_IP, SERVER_PORT);
  if (!connected){
    wifi_tcp_close();
  }
  else{
    char msg[] = "MEGA_TEST_TCP\nMAESTRO IS KING";
    wifi_tcp_send(msg, 29);
    delay(50);
    wifi_tcp_close();
  }
  delay(1000);
}

/****** Wifi commands ******/
bool wifi_join(String ssid, String passkey){
  return sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + passkey + "\"", 10, "OK");
}

bool wifi_tcp_connect(String host, int port){
  return sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\"," + port, timeout, "OK");
}

bool wifi_tcp_close(){
  return sendCommand("AT+CIPCLOSE", timeout, "OK");
}

bool wifi_tcp_send(char message[], int len){
  if (len > PACKET_SIZE-2){
    //Change to Transparent Transmission Mode
    sendCommand("AT+CIPMODE=1", timeout, "");
    
    String ln1 = "AT+CIPSEND";
    bool success = sendCommand(ln1, timeout, ">");

    //Break up into packets of 2048 bytes
    int packets = len / PACKET_SIZE;
    for (int i = 0; i < packets; i++){
      char packet[PACKET_SIZE];
      memcpy_P(packet, &message[i*PACKET_SIZE], PACKET_SIZE);
      esp8266.print(packet);
      delay(20);
    }
    
    //End of transmission
    esp8266.print("+++");
    delay(1000);
    
    //Change to Normal mode
    sendCommand("AT+CIPMODE=0", timeout, "");

    return success;
  }
  String ln1 = "AT+CIPSEND=";
  ln1.concat(len + 2);
  bool success = sendCommand(ln1, timeout, "");
  esp8266.print(message);
  esp8266.print("\r\n");
  return success;
}

bool wifi_tcp_send_file(char filename[]){
  File file = SD.open(filename);
  if (!file){
    Serial.println("error reading file");
    return false;
  }
  sendCommand("AT+CIPMODE=1", timeout, "");
  String ln1 = "AT+CIPSEND";
  bool success = sendCommand(ln1, timeout, ">");
  while (file.available()){
    char buf[PACKET_SIZE];
    file.read(buf, PACKET_SIZE);
    esp8266.print(buf);
    delay(20);
  }
  esp8266.print("+++");
  delay(1000);
  file.close();
  sendCommand("AT+CIPMODE=0", timeout, "");
  return success;
}

bool sendCommand(String command, int maxTime, char readReplay[]) {
  int countTime = 0;
  bool found = false;
  Serial.print("AT$ ");
  Serial.print(command);
  Serial.print(" :");
  while(countTime < maxTime){
    esp8266.print(command + (String)"\r\n");
    if(esp8266.find(readReplay)){
      found = true;
      break;
    }
    countTime++;
  }

  if(found == true){
    Serial.println("Success");
  }
  else{
    Serial.println("Fail");
  }
  
  clearReadBuffer();
  return found;
}

void clearReadBuffer(){
  while (esp8266.available()){
    esp8266.read();
  }
}

/********** END maestro_esp8266.ino functions **********/
