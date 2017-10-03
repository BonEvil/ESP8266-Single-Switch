#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

typedef struct {
  String ap_ssid;
  String ap_pass;
  String st_ssid;
  String st_pass;
} NetworkVariables;

// SETUP FOR ALEXA/WEMO
WiFiUDP UDP;
boolean udpConnected = false;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
String serial;
String persistentUuid;

// SETUP FOR EEPROM SETTINGS
int memLen = 256;
NetworkVariables networkVariables;

// THIS WILL AUTOMATICALLY BE SET FROM ap_ssid
String localName;

// PINS FOR RELAY ACTIVATION AND SWITCH
const int relayPin = 5;
const int switchPin = 4;

// HOLDS CURRENT SWITCH STATE (true=ON, false=OFF)
boolean isSwitchOn;

// SET THE SERVER LISTENING PORT
ESP8266WebServer server(80);

void setup(){
  Serial.begin(115200);

  // SET UP THE PIN CONFIGURATIONS
  pinMode(relayPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);

  // SET INITIAL SWITCH STATE
  isSwitchOn = switchOn();

  // SET INITIAL RELAY STATE | TURN IT ON IF SWITCH STATE IS ON
  turnOffRelay();
  if (isSwitchOn){
    turnOnRelay();
  }

  // ALLOCATE THE MEMORY FOR PERSISTENCE
  EEPROM.begin(memLen);

  // PREPARE UNIQUE IDS FOR ALEXA CONTROL
  prepareIds();

  // INITIALIZE networkVariables TO DEFAULT OR FROM THE EEPROM
  initializeNetworkVariables();

  // SET UP THE AP IN ORDER TO DIRECTLY CONNECT TO THE ESP8266
  setupAccessPoint();

  // SET UP THE CONNECTION TO THE LOCAL NETWORK IF THE SSID AND PASSWORD ARE STORED IN EEPROM
  setupClientIfAvailable();

  // SET UP SERVER ROUTES
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
}

void initializeNetworkVariables(){
  String mem_ap_ssid = "";
  String mem_ap_pass = "";
  String mem_st_ssid = "";
  String mem_st_pass = "";

  int splitCount = 0;

  // THE NETWORK VARIABLES ARE STORED INLINE DELIMITED BY : AND ENDING WITH ;
  // THEREFORE, THESE ARE UNSAFE CHARACTERS FOR USE IN SSID AND PASSWORDS
  for (int i = 0; i < memLen ; ++i) {
    char c = char(EEPROM.read(i));
    if (c == ';'){ // WE'VE HIT THE END OF OUR STORED VARIABLES
      break;
    }

    if (c == ':'){ // WE'VE HIT A DELIMITER, MOVE TO THE NEXT VARIABLE
      ++splitCount;
    } else {
      switch (splitCount){
        case 0:
          mem_ap_ssid += c;
          break;
        case 1:
          mem_ap_pass += c;
          break;
        case 2:
          mem_st_ssid += c;
          break;
        default:
          mem_st_pass += c;
          break;
      }
    }
  }

  // MAKE SURE WE HAVE ALL OUR VARIABLES
  if (mem_st_ssid.length() > 0 &&
    mem_st_pass.length() > 0 &&
    mem_ap_ssid.length() > 0 &&
    mem_ap_pass.length() > 0){
      // SET OUR NETWORK VARIABLES
      networkVariables = {mem_ap_ssid,mem_ap_pass,mem_st_ssid,mem_st_pass};
      // NOW WE CREATE OUR LOCAL ACCESS NAME (Living Room Light becomes livingroomlight.local)
      String altName = mem_ap_ssid;
      altName.toLowerCase();
      altName.replace(" ","");
      localName = altName;
    } else {
      // SETUP DEFAULTS IF WE DON'T HAVE ANYTHING IN THE EEPROM
      setupDefaultNetworkVariables();
  }
}

void setupDefaultNetworkVariables(){
  // SET UP THE DEFAULT ACCESS POINT WITH NO PASSWORD
  networkVariables = {createSSID(),"","",""};
}

void setupClientIfAvailable(){ // IF WE HAVE NETWORK SSID AND PASSWORD, TRY TO CONNECT
  if (sizeof(networkVariables) > 0 &&
      networkVariables.st_ssid.length() > 0 &&
      networkVariables.st_pass.length() > 0){
    Serial.println("");
    Serial.println("ssid: "+networkVariables.st_ssid);
    Serial.println("pass: "+networkVariables.st_pass);

    if (WiFi.status() != WL_DISCONNECTED){
      Serial.println("disconnecting from network");
      WiFi.disconnect();
    }
    WiFi.begin (networkVariables.st_ssid.c_str(),networkVariables.st_pass.c_str());
    Serial.println ( "" );

    int count = 0;
    // Wait for connection
    while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );
      ++count;
      if (count == 40){
        Serial.println("Couldn't connect to network.");
        return;
      }
    }

    Serial.println ( "" );
    Serial.print ( "Connected to " );
    Serial.println ( WiFi.SSID() );
    Serial.print ( "IP address: " );
    Serial.println ( WiFi.localIP() );

    long rssi = WiFi.RSSI();
    Serial.print("Signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");

    Serial.println ( "HTTP server started" );

    if (MDNS.begin(localName.c_str())){
      Serial.println ("MDNS responder started");
    }

    // NOW SET UP THE WEMO SERVICES
    startHttpServer();

    // START OUR UDP CONNECTION
    connectUDP();
  }
}

void setupAccessPoint(){ // SET UP OUR ACCESS POINT FROM OUR STORED EEPROM OR DEFAULT SETTINGS
  WiFi.mode(WIFI_AP_STA);
  if (sizeof(networkVariables) > 0){
    if(networkVariables.ap_ssid.length() > 0 &&
       networkVariables.ap_pass.length() > 0){
       WiFi.softAP(networkVariables.ap_ssid.c_str(),networkVariables.ap_pass.c_str());
    } else {
      WiFi.softAP(networkVariables.ap_ssid.c_str());
    }
  }
}

void loop(){
  server.handleClient();
  delay(1);

  // WE ARE WATCHING FOR A STATE CHANGE FROM THE SWITCH IN ORDER TO TURN ON OR OFF THE RELAY
  // DEPENDENT ON THE RELAY'S CURRENT STATE
  if (switchOn() != isSwitchOn){
    Serial.println("changing switch state");
    isSwitchOn = !isSwitchOn;
    if(relayOn()){
      turnOffRelay();
    } else {
      turnOnRelay();
    }
  }

  int packetSize = UDP.parsePacket();

  if(packetSize) {
    Serial.println("");
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = UDP.remoteIP();

    for (int i =0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }

    Serial.print(", port ");
    Serial.println(UDP.remotePort());

    int len = UDP.read(packetBuffer, 255);

    if (len > 0) {
        packetBuffer[len] = 0;
    }

    String request = packetBuffer;

    if(request.indexOf('M-SEARCH') > 0) {
        if(request.indexOf("urn:Belkin:device:**") > 0) {
            Serial.println("Responding to search request ...");
            respondToSearch();
        }
    }
  }

  delay(10);
}

void setupNewNetworkVariables(NetworkVariables variables){ // SETS THE NEW ACCESS POINT AND NETWORK VARIABLES
  int ap_ssidLen = variables.ap_ssid.length();
  int ap_passLen = variables.ap_pass.length();
  int st_ssidLen = variables.st_ssid.length();
  int st_passLen = variables.st_pass.length();

  if (ap_ssidLen > 0 && ap_passLen > 0 && st_ssidLen > 0 && st_passLen > 0){
    int totalLen = (ap_ssidLen + ap_passLen + st_ssidLen + st_passLen + 5); // 3x : and 1x ; and 1x \0

    String store = variables.ap_ssid+":"+variables.ap_pass+":"+variables.st_ssid+":"+variables.st_pass+";";

    clearMemory();

    for (int i=0;i<totalLen;++i){
      EEPROM.write(i,store[i]);
    }
    EEPROM.commit();
    setup();
  }
}

void clearMemory(){ // CLEAR THE EEPROM MEMORY BEFORE SETTING THE NEW NETWORK VARIABLES OR TO RESET TO THE DEFAULT
  for (int i=0;i<memLen;++i){
    EEPROM.write(i,0);
  }
  EEPROM.commit();
}

// THE INDEX PAGE SERVED UP FROM 192.168.4.1 OF THE ACCESS POINT OR FROM ROOT OF THE .local CLIENT
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>ESP8266 Connection</title>
<style>
"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
</style>
</head>
<body>
<div>
<h1>Setup Access Point and Station Connection</h1>
<p>
Access Point SSID is what this device will advertise. It is also the 'friendly' name that Alexa will use to identify this device.</br>*The password is mandatory.
</p>
<p>
Station SSID and Password are to connect to your local network.
</p>
<form action="/" method="post">
<input name="ap_ssid" placeholder="Access Point SSID"/><br/>
<input name="ap_pass" type="password" placeholder="Access Point Password"/><br/>
<input name="st_ssid" placeholder="Station SSID"/><br/>
<input name="st_pass" type="password" placeholder="Station Password"/><br/>
<input name="submit" type="submit" value="Submit"/>
</form>
</div>
<div>
<h1>Reset</h1>
<p>
Use this to reset back to factory settings.
</p>
<form action="/" method="post">
<input name="submit" type="submit" value="Reset"/>
</form>
</div>
</body>
</html>
)rawliteral";

static const char PROGMEM INDEX2_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>ESP8266 Connection</title>
<style>
"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
</style>
</head>
<body>
<div>
<h1>Updating network settings.</h1>
<form action="/" method="post">
<input name="submit" type="submit" value="OK"/>
</form>
</div>
</body>
</html>
)rawliteral";


void handleRoot() // ACCEPTS THE NETWORK VARIABLES FORM OR SENDS THE DEFAULT INDEX HTML
{
  String ap_ssid = "";
  String ap_pass = "";
  String st_ssid = "";
  String st_pass = "";

  boolean reset = false;

  for (uint8_t i=0; i<server.args(); i++){
    String formName = server.argName(i);
    String formValue = server.arg(i);

    Serial.println(formName + ": " + formValue);
    if (formName == "ap_ssid"){
      ap_ssid = formValue;
    } else if (formName == "ap_pass"){
      ap_pass = formValue;
    } else if (formName == "st_ssid"){
      st_ssid = formValue;
    } else if (formName == "st_pass"){
      st_pass = formValue;
    } else if (formValue == "Reset"){
      reset = true;
    }
  }

  if (reset){
    server.send_P(200, "text/html",INDEX2_HTML);
    clearMemory();
    initializeNetworkVariables();
    setup();
  } else if (ap_ssid.length() > 0 &&
      ap_pass.length() > 0 &&
      st_ssid.length() > 0 &&
      st_pass.length() > 0){

      server.send_P(200, "text/html",INDEX2_HTML);
      NetworkVariables vars = {ap_ssid,ap_pass,st_ssid,st_pass};
      setupNewNetworkVariables(vars);
  } else {
    server.send_P(200, "text/html",INDEX_HTML);
  }
}

void handleNotFound()
{
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

String createSSID(){ // CREATES THE DEFAULT SSID
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  return "ESP8266 Thing " + macID;
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistentUuid = "Socket-1_0-" + serial;
}

boolean connectUDP(){
  boolean state = false;

  Serial.println("");
  Serial.println("Connecting to UDP");

  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }

  return state;
}

void startHttpServer() {
    server.on("/index.html", HTTP_GET, [](){
      Serial.println("Got Request index.html ...\n");
      server.send(200, "text/plain", "Hello World!");
    });

    server.on("/upnp/control/basicevent1", HTTP_POST, []() {
      Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");

      String request = server.arg(0);
      Serial.print("request:");
      Serial.println(request);

      if(request.indexOf("<BinaryState>1</BinaryState>") > 0) {
          Serial.println("Got Turn on request");
          turnOnRelay();
      }

      if(request.indexOf("<BinaryState>0</BinaryState>") > 0) {
          Serial.println("Got Turn off request");
          turnOffRelay();
      }

      server.send(200, "text/plain", "");
    });

    server.on("/eventservice.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to eventservice.xml ... ########\n");
      String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
            "<actionList>"
              "<action>"
                "<name>SetBinaryState</name>"
                "<argumentList>"
                  "<argument>"
                    "<retval/>"
                    "<name>BinaryState</name>"
                    "<relatedStateVariable>BinaryState</relatedStateVariable>"
                    "<direction>in</direction>"
                  "</argument>"
                "</argumentList>"
                 "<serviceStateTable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>BinaryState</name>"
                    "<dataType>Boolean</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>level</name>"
                    "<dataType>string</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                "</serviceStateTable>"
              "</action>"
            "</scpd>\r\n"
            "\r\n";

      server.send(200, "text/plain", eventservice_xml.c_str());
    });

    server.on("/setup.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to setup.xml ... ########\n");

      IPAddress localIP = WiFi.localIP();
      char s[16];
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

      String setup_xml = "<?xml version=\"1.0\"?>"
            "<root>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ networkVariables.ap_ssid +"</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Emulated Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<UDN>uuid:"+ persistentUuid +"</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                  "<service>"
                      "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                      "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
              "</serviceList>"
              "</device>"
            "</root>\r\n"
            "\r\n";

        server.send(200, "text/xml", setup_xml.c_str());

        Serial.print("Sending :");
        Serial.println(setup_xml);
    });

    Serial.println("HTTP Server started ..");
}

void respondToSearch() {
  Serial.println("");
  Serial.print("Sending response to ");
  Serial.println(UDP.remoteIP());
  Serial.print("Port : ");
  Serial.println(UDP.remotePort());

  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  String response =
       "HTTP/1.1 200 OK\r\n"
       "CACHE-CONTROL: max-age=86400\r\n"
       "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
       "EXT:\r\n"
       "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
       "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
       "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
       "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
       "ST: urn:Belkin:device:**\r\n"
       "USN: uuid:" + persistentUuid + "::urn:Belkin:device:**\r\n"
       "X-User-Agent: redsonic\r\n\r\n";

  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(response.c_str());
  UDP.endPacket();

  Serial.println("Response sent !");
}

// CONTROLS

boolean switchOn(){
  boolean state = false;
  if (digitalRead(switchPin) == LOW) {
    state = true;
  }
  return state;
}

boolean relayOn(){
  boolean state = false;
  if (digitalRead(relayPin) == LOW){
    state = true;
  }
  return state;
}

void turnOnRelay() {
 digitalWrite(relayPin, LOW);
}

void turnOffRelay() {
  digitalWrite(relayPin, HIGH);
}
