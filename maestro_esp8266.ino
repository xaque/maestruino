#include <SoftwareSerial.h>

/** Pins **/
int RX = 10;
int TX = 11;


int timeout = 2;
String AP = "FellowshipOfThePing";
String PASS = "typefriendandenter";
SoftwareSerial esp8266(RX,TX);

void setup() {
  Serial.begin(9600);
  esp8266.begin(115200);
  esp8266.listen();
  sendCommand("AT", timeout, "OK");
  sendCommand("AT+CWMODE=1", timeout, "OK");
  wifi_join(AP, PASS);  
}

void loop() {
  delay(1000);
  boolean connected = wifi_tcp_connect("192.168.1.142", 19220);
  if (!connected){
    wifi_tcp_close();
  }
  else{
    wifi_tcp_send("MEGA_TEST_TCP\nMAESTRO IS KING");
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

boolean wifi_tcp_send(String message){
  //TODO break message larger than 2048 bytes into packets
  String ln1 = "AT+CIPSEND=";
  ln1.concat(message.length() + 2);
  sendCommand(ln1, timeout, "");
  return sendCommand(message, timeout, "OK");
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
