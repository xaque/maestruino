#include <SoftwareSerial.h>
#include <string.h>
#include "sound/musicdata.h"

/** Pins **/
int RX = 10;
int TX = 11;

boolean init_loop = true;

int timeout = 2;
String AP = "FellowshipOfThePing";
String PASS = "typefriendandenter";
SoftwareSerial esp8266(RX,TX);

void setup() {
  Serial.begin(9600);
  esp8266.begin(115200);
  esp8266.listen();
}

void loop() {
  if (init_loop){
    delay(20);
    esp8266.print("+++");
    delay(1000);
    sendCommand("AT", timeout, "OK");
    sendCommand("AT+CIPMODE=0", timeout, "OK");
    sendCommand("AT+CWMODE=1", timeout, "OK");
    wifi_join(AP, PASS);  
    init_loop = false;
  }
  delay(1000);
  boolean connected = wifi_tcp_connect("192.168.1.142", 19220);
  if (!connected){
    wifi_tcp_close();
  }
  else{
    //char msg[] = "MEGA_TEST_TCP\nMAESTRO IS KING";
    wifi_tcp_send(sounddata_data, sounddata_length);
    delay(50);
    wifi_tcp_close();
  }
}

/****** Wifi commands ******/
boolean wifi_join(String ssid, String passkey){
  return sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + passkey + "\"", 10, "OK");
}

boolean wifi_tcp_connect(String host, int port){
  return sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\"," + port, timeout, "OK");
}

boolean wifi_tcp_close(){
  return sendCommand("AT+CIPCLOSE", timeout, "OK");
}

boolean wifi_tcp_send(char message[], int len){
  int PACKET_SIZE = 512;
  if (len > PACKET_SIZE-2){
    //Change to Transparent Transmission Mode
    sendCommand("AT+CIPMODE=1", timeout, "");
    
    String ln1 = "AT+CIPSEND";
    boolean success = sendCommand(ln1, timeout, ">");

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
  boolean success = sendCommand(ln1, timeout, "");
  esp8266.print(message);
  esp8266.print("\r\n");
  return success;
}

boolean sendCommand(String command, int maxTime, char readReplay[]) {
  int countTime = 0;
  boolean found = false;
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
