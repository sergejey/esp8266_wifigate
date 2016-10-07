#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>

const int EEPROM_HTTP_SERVER=50;
const int EEPROM_DATA_FILTER=100;
const int EEPROM_STRING_MAX=50;

#include <Ticker.h>
Ticker ticker;
ESP8266WebServer server(80);

String http_server;
String data_filter;
String latestSerialData;

const byte numChars = 200;
char receivedChars[numChars]; // an array to store the received data
boolean newData = false;

String URLEncode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
return encodedString;
}

void handleRoot() {
  String message = "<html><head><title>WiFi Gate</title></head><body><h1>WiFi Gate</h1>\n";
  message += "<form action='/' method='get'>Send data to serial:<br/><input type='text' name='data' id='data_field' value=''><input type='button' value='Send' onClick='sendData();'></form>";
  message += "<p>Data from serial: <span id='latestData'>";
  message += latestSerialData;
  message += "</span>\n";
  message += "<h2>Settings</h2><form action='/save' method='get'>Serial data hook URL:<br/><input type='text' name='server' value='";
  message += http_server;
  message += "' size='60'><br/>Serial data filter:<br/><input type='text' name='filter' value='";
  message += data_filter;  
  message += "'size='60'><br/><input type='submit' value='Save settings'></form>";
  message += "</p>";  
  message += "<script language='javascript' src='https://ajax.googleapis.com/ajax/libs/jquery/2.2.4/jquery.min.js'></script>";  
  message += "<script language=\"javascript\">\n";
  message += "var status_timer;\n";
  message += "function sendData() {\n";
  message += "$.ajax({  url: \"/send?data=\"+encodeURIComponent($('#data_field').val())  }).done(function(data) { $('#data_field').val(''); });";
  message += "}\n";  
  message += "function updateStatus() {\n";
  message += "$.ajax({  url: \"/data\"  }).done(function(data) {  $('#latestData').html(data);status_timer=setTimeout('updateStatus();', 500); });";
  message += "}\n";
  message += "$(document).ready(function() {  updateStatus();   });\n";      
  message += "</script>";  
  message += "</body></html>";    
  server.send(200, "text/html", message);
}

void handleData() {
  String message = "";
  message += latestSerialData;
  server.send(200, "text/plain", message);
}

String readStringEEPROM(int startIdx) {
    String res="";
    for (int i = 0; i < EEPROM_STRING_MAX; ++i)
    {
      byte sm=EEPROM.read(startIdx+i);
      if (sm>0) {
       res+= char(sm);        
      } else {
        break;
      }
    }
    //Serial.print("Read string: ");
    //Serial.println(res);
    return res;
}

void writeStringEEPROM(String str, int startIdx) {
          for (int i = 0; i < EEPROM_STRING_MAX; ++i)
          {
            EEPROM.write(startIdx+i, 0);
          }    
          for (int i = 0; i < str.length(); ++i)
            {
              EEPROM.write(startIdx+i, str[i]);
            }    
          EEPROM.commit();  
}

void handleUpdateConfig() {
  for (uint8_t i=0; i<server.args(); i++){
    if (server.argName(i)=="server") {
      http_server=server.arg(i);
    }
    if (server.argName(i)=="filter") {
      data_filter=server.arg(i);
    }    
  }
  String message = "<html><body><script language='javascript'>tm=setTimeout(\"window.location.href='/';\",2000);</script>Data saved!</body></html>";  

  writeStringEEPROM(data_filter,EEPROM_DATA_FILTER);
  writeStringEEPROM(http_server,EEPROM_HTTP_SERVER);
  
  server.send(200, "text/html", message);
}

void handleSend() {
  String message = "OK";
  for (uint8_t i=0; i<server.args(); i++){
    if (server.argName(i)=="data") {
      message += "; Sent: "+server.arg(i);      
      Serial.println(server.arg(i));
    }
  }
  server.send(200, "text/plain", message);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void tick()
{
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //Serial.println("Entered config mode");
  //Serial.println(WiFi.softAPIP());
  //Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}




void setup() {
    Serial.begin(9600);

    pinMode(BUILTIN_LED, OUTPUT);
    //ticker.attach(0.5, tick);
    
    //WiFiManager
    WiFiManager wifiManager;
    wifiManager.setAPCallback(configModeCallback);
    if (!wifiManager.autoConnect("ESPGate")) {
      //Serial.println("failed to connect and hit timeout");
      ESP.reset();
      delay(1000);
    }

    ticker.detach();
    digitalWrite(BUILTIN_LED, HIGH);

    EEPROM.begin(512);

    data_filter=readStringEEPROM(EEPROM_DATA_FILTER);
    http_server=readStringEEPROM(EEPROM_HTTP_SERVER);
    
    latestSerialData = "None";

  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.on("/data", handleData);
  server.on("/save", handleUpdateConfig);

  server.onNotFound(handleNotFound);

  server.begin();
  //Serial.println("HTTP server started");    

}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
  recvWithEndMarker();
  showNewData();  
}

void recvWithEndMarker() {
 static byte ndx = 0;
 char endMarker = '\n';
 char rc;
 while (Serial.available() > 0 && newData == false) {
  rc = Serial.read();
  if (rc != endMarker) {
  receivedChars[ndx] = rc;
  ndx++;
  if (ndx >= numChars) {
  ndx = numChars - 1;
  }
  }
  else {
  receivedChars[ndx] = '\0'; // terminate the string
  ndx = 0;
  newData = true;
  }
 }
}

void showNewData() {
 if (newData == true) {
  latestSerialData=receivedChars;
  if (latestSerialData.indexOf(data_filter)>=0) {
    //Serial.println("Matched!");
    HTTPClient http;
    String url = http_server;
    url += URLEncode(latestSerialData);
    http.begin(url);
    int httpCode = http.GET();
    http.end();
  }
  newData = false;
 }
}
