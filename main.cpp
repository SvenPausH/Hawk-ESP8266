#include <Arduino.h>
/*  1. Release Hawk ESP8266WiFi
 Compiled whith VsCode

Pin 5 and 4 for Software Serial (only Debug)

RX and TX Pin is HardwareSerial to Read Data from Hawk and KeyBoard
Pin 8 HardwareSerial to send Data to Hawk.

Please note Software Serial loses data.
*/



// Import required libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>

SoftwareSerial MySerial(5, 4); // 5=TX -> RX RS485| 4=RX -> TX TS485
//HardwareSerial1 MySerial1(2);

// Replace with your network credentials

const char *ssid = "your ssid";
const char *password = "your wifi password";

String message;
char messagec[60];
bool WebserverRun = false;


int SerialRead; // Byteweise einlesen
// Uptime
unsigned long TT=0,upM=0,upH=0,upD=0;

// 29 Byte
//char test[] = { "\x58\x53\x4D\x00\xF3\x1A\x00\x00\x01\x15\x58\x4D\x53\x00\x03\x6B\x00\x00\x01\x66\x58\x53\x4D\x00\x03\x1B\x27\x10\x01\x4D\x58\x4D\x53\x00\x01\x4B\x01\x44\x58\x53\x4D\x00\x03\x1A\x00\x00\x01\x15"};
//int testzahl = 0;
//const int testbyte = 48;

//int Serial1Debug;
//int supress_dup = 1;  // supress duplicate Output 0=NO 1=YES

typedef struct Struct_Data
{
  char device[3];   // device_strg XSM oder device_keyb XMS
  int len;          // Playload Datenlaenge
  char payload[20]; // Daten
  int crc;          // Pruefsumme
  int pos;          // Aktuelle Stringposition
  char data[30];    // kompletter Datensatz
  int last_kb_crc;  //unsigned int last_kb_crc; // letzte crc summe Keyboard
  int last_mb_crc;  //unsigned int last_mb_crc; // letzte crc summe Mainboard
  char display[11];
  char last_display[11];
} Struct_Data;
struct Struct_Data myPayload;

typedef struct Struct_SerBuf
{
  char buf[55];
  int len;
  int pos;
} Struct_SerBuf;

struct Struct_SerBuf mySerBuf;

typedef struct Struct_Config
{
  bool SetAllow;
  bool SerialDebug;
  bool Serial2Web;
  bool WebDebug;
  bool SuppressDuplicate;
  bool WebServer;
  unsigned int CountSetSup;
  bool DisplayOnOff;
  bool LastDrop;  
} Struct_Config;

struct Struct_Config myConfig;

int WebKey = 0; // Wenn auf der Webseite eine Taste gedrueckt wird.
int WebKeyNone = 0;
int WebKeyRepeat = 0;

typedef struct Struct_Text
{
  char Text[12];
  char Payload[15];
  int crc;
  int PayloadLen;
} Struct_Text;

Struct_Text MyText[] = {
    {" unknown  \0", "\0", 0, 0}, // ID=0
    // all Keyboard combinations
    {"KEYB UP   \0", "\x58\x4d\x53\x00\x03\x6b\x00\x02\x01\x68", 360, 10}, // Keyid=1
    {"KEYB DOWN \0", "\x58\x4d\x53\x00\x03\x6b\x00\x08\x01\x6e", 366, 10}, // Keyid=2
    {"KEYB JETS \0", "\x58\x4d\x53\x00\x03\x6b\x00\x10\x01\x76", 374, 10}, // Keyid=3
    {"KEYB SET  \0", "\x58\x4d\x53\x00\x03\x6b\x00\x04\x01\x6a", 362, 10}, // Keyid=4
    {"KEYB LIGHT\0", "\x58\x4d\x53\x00\x03\x6b\x00\x20\x01\x86", 390, 10}, // Keyid=5
    {"KEYB NONE \0", "\x58\x4d\x53\x00\x03\x6b\x00\x00\x01\x66", 358, 10}, // Keyid=6
    {"KEYB RESET\0", "\x58\x4d\x53\x00\x01\x4b\x01\x44", 324, 8},          // Reboard RESET?  ID=7
    // now all Mainbaord Payloads
    {"SET+RY ON \0", "\x58\x53\x4d\x00\x03\x1a\x00\x51\x01\x66", 358, 10},             //ID=8
    {"SET+RY OFF\0", "\x58\x53\x4d\x00\x03\x1a\x00\x01\x01\x16", 278, 10},             //ID=9
    {"   init   \0", "\x58\x53\x4d\x00\x01\x14\x01\x0d", 269, 8},                      //ID=10
    {" RY OFF   \0", "\x58\x53\x4d\x00\x03\x1a\x00\x00\x01\x15", 277, 10},             //ID=11
    {" RY ON    \0", "\x58\x53\x4d\x00\x03\x1a\x00\x50\x01\x65", 357, 10},             //ID=12
    {" MB RESET \0", "\x58\x53\x4d\x00\x03\x1b\x27\x10\x01\x4d", 333, 10},             //ID=13
    {" START    \0", "\x58\x4d\x53\x00\x06\x54\x48\x41\x57\x4b\x00\x02\x7d", 637, 13}, //ID=14
};

int text_count = sizeof MyText / sizeof MyText[0];

#define SCREENFLAG_SET 0x01
#define SCREENFLAG_FREE 0x02
#define SCREENFLAG_F2 0x04
#define SCREENFLAG_JETS 0x08
#define SCREENFLAG_FREE2 0x10
#define SCREENFLAG_LIGHT 0x20
#define SCREENFLAG_READY 0x40
#define SCREENFLAG_CLEAN 0x80
#define SCREENFLAG_F1 0x100
#define SCREENFLAG_FREE3 0x200
#define SCREENFLAG_LUNAR 0x400
#define SCREENFLAG_COLON 0x800 //---:--
#define SCREENFLAG_LDOT 0x1000
#define SCREENFLAG_FREE4 0x2000
#define SCREENFLAG_FREE5 0x4000
#define SCREENFLAG_FREE6 0x4000

void StorePayload(int SerialData);
void FindPayload();
void TrimPayload(int pos);
void PrintPayload();
void SetDisplay(int crc);
void ConsoleRead(int data);
void PrintConfig();
void SendPayload(int ConstID);
void SendPayloadMB(int ConstID);
void SendInitialWeb(); // Immer wenn ein Client connected
void Uptime();
/* WEBSERVER */

int clients_connected = 0;
unsigned long previousMillis = 0; // will store last time LED was updated

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en-GB">
<head>
  <title>ESP HotTube</title>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial, Helvetica, sans-serif;
      text-align: center;
    }
    h1 {
      font-size: 1.8rem;
      color: white;
    }
    h2 {
      font-size: 1.3rem;
      font-weight: bold;
      color: #143642;
    }
    .topnav {
      overflow: hidden;
      background-color: #143642;
    }
    .main {
      padding: 5px;
      max-width: 600px;
      margin: 0 auto;
    }
    /* The switch - the box around the slider */
    .switch {
      position: relative;
      display: inline-block;
      width: 60px;
      height: 34px;
    }
    /* Hide default HTML checkbox */
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    .switch .onoff {
      color: #ccc;
      text-indent: 70px;
      font-size: 1.5em;
      text-align: left;
      display: flex;
      color: rgb(24, 24, 24);
    }

    /* The slider */
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
    }

    .slider:before {
      position: absolute;
      content: "off";
      height: 26px;
      width: 26px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
    }

    input:checked+.slider {
      background-color: #0f8b8d;
    }

    input:focus+.slider {
      box-shadow: 0 0 1px #0f8b8d;
    }

    input:checked+.slider:before {
      content: "on";
      transform: translateX(26px);
    }

    /* Rounded sliders */
    .slider.round {
      border-radius: 34px;
    }

    .slider.round:before {
      border-radius: 50%;
    }

    section {
      background-color: #F8F7F9;
      ;
      box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
      padding-top: 2px;
      padding-bottom: 10px;
      border-radius: 5px;
      margin: 10px;
    }

    .button {
      padding: 10px 30px;
      margin-bottom: 5px;
      font-size: 16px;
      text-align: center;
      outline: none;
      color: #fff;
      background-color: #0f8b8d;
      border: none;
      border-radius: 5px;
      user-select: none;
    }

    .button:active {
      background-color: #0f8b8d;
      box-shadow: 0 2px #cdcdcd;
      top: 2px;
      position: relative;
    }

    .state {
      font-size: 1.5rem;
      color: #8c8c8c;
      font-weight: bold;
    }

    table {
      border: thin solid #a0a0a0;
      font-weight: bold;
      color: rgb(190, 190, 190);
      font-family: Verdana;
      font-size: 1em;
      text-align: center;
      padding: 5px;
      margin-left: auto;
      margin-right: auto;
      margin-top: auto;
      margin-bottom: auto;
    }

    .markiertm0 {
      font-family: Monospace;
      font-weight: bold;
      font-size: 2em
    }

    td.markiert1 {
      color: red;
      font-weight: bold;
    }

    td.markiert0 {
      font-weight: bold;
      color: gray;
    }

    td.markiertm1 {
      color: red;
      font-weight: bold;
      font-family: Monospace;
      font-size: 2em
    }
  </style>
</head>

<body>
  <div class="main">
    <section>
      <div class="topnav">
        <h1> HotTube Hawk</h1>
      </div>
    </section>
    <section>
      <div class="display">
        <h2>Display</h2>
        <table id="tabelle">
          <tbody>
            <tr>
              <td>SET</td>
              <td></td>
              <td></td>
              <td>&bigstar;&#9789;&bigstar;</td>
              <td>&cir;</td>
              <td>READY</td>
            </tr>
            <tr>
              <td>F1</td>
              <td class="markiertm1" colspan="5" rowspan="2">_________</td>
            </tr>
            <tr>
              <td>F2</td>
            </tr>
            <tr>
              <td>&#9884;</td>
              <td></td>
              <td>&cir;</td>
              <td>&#9728;</td>
              <td></td>
              <td>&#9851;</td>
            </tr>
          </tbody>
        </table>
      </div>
      <div class="infotext">
        <p id="infotext">Display: ON Suppressed SET:0</p>
      </div>
    </section>
    <section>
      <div class="keypad">
        <h2>Tastatur</h2>
        <button id="button1" class="button">Temp &#9651;</button>
        <button id="button2" class="button">Temp &#9661;</button>
        <br>
        <button id="button3" class="button">Jets</button>
        <button id="button4" class="button">Set</button>
        <button id="button5" class="button">Light</button>
      </div>
    </section>
    <section>
      <div class="switch">
        <h2>Config</h2>
        <label class="switch">
          <span class="onoff">AllowSET</span>
          <input id="configset" type="checkbox">
          <span class="slider round"></span>
        </label>
        <label class="switch">
          <span class="onoff">SerialLog</span>
          <input id="configserial" type="checkbox">
          <span class="slider round"></span>
        </label>
        </label>
        <label class="switch">
          <span class="onoff">Serial2Web</span>
          <input id="configserial2web" type="checkbox">
          <span class="slider round"></span>
        </label>
        <label class="switch">
          <span class="onoff">WebLog</span>
          <input id="configweb" type="checkbox">
          <span class="slider round"></span>
        </label>
        <label class="switch">
          <span class="onoff">NoDUPS</span>
          <input id="configdups" type="checkbox">
          <span class="slider round"></span>
        </label>
        <div class="keypad">
          <button id="button6" class="button">reset</button>
        </div>
        <div class="keypad">
          <button id="button7" class="button">reboot</button>
        </div>        
      </div>
    </section>
    <script>
      var gateway = `ws://${window.location.hostname}/ws`;
      var websocket;
      var SCREEN_SET = 0x01;
      var SCREEN_FREE = 0x02;
      var SCREEN_F2 = 0x04;
      var SCREEN_JETS = 0x08;
      var SCREEN_FREE2 = 0x10;
      var SCREEN_LIGHT = 0x20;
      var SCREEN_READY = 0x40;
      var SCREEN_CLEAN = 0x80;
      var SCREEN_F1 = 0x100;
      var SCREEN_FREE3 = 0x200;
      var SCREEN_LUNAR = 0x400;
      var SCREEN_COLON = 0x800;
      var SCREEN_LDOT = 0x1000;
      var SCREEN_FREE4 = 0x2000;
      var SCREEN_FREE5 = 0x4000;
      var WebLogging = 0;

      window.addEventListener('load', onLoad);

      function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
      }
      function onOpen(event) {
        console.log('Connection opened');
      }
      function onClose(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
      }
      function onMessage(event) {
        var state;
        var elem = document.getElementById('configweb');
        var onoff = elem.parentNode.querySelector(".onoff");
        if (elem.checked) {
          console.log(event.data);
        }
        myArray = event.data.split("$$");
        if (myArray[0] == "msg") {
          console.log(myArray[1]);
        }

        if (myArray[0] == "infotext") {
          SetInfoText(myArray[1], myArray[2],myArray[3],myArray[4],myArray[5]);
        }
        if (myArray[0] == "displconf") {
          //document.getElementById('tabelle').getElementsByTagName('tbody')[0].getElementsByTagName('td')[myArray[1]].classList=('markiert'+myArray[2]);
          var item = Number(myArray[1]);
          if (item & SCREEN_SET) { setScreen(0, 1) }
          else { setScreen(0, 0); }
          //if (item & SCREEN_FREE)  {console.log('bit 2 set');}
          if (item & SCREEN_F2) { setScreen(8, 1) }
          else { setScreen(8, 0); }
          if (item & SCREEN_JETS) { setScreen(9, 1) }
          else { setScreen(9, 0); }
          //if (item & SCREEN_FREE2) {console.log('bit 5 set');}
          if (item & SCREEN_LIGHT) { setScreen(12, 1) }
          else { setScreen(12, 0); }
          if (item & SCREEN_READY) { setScreen(5, 1) }
          else { setScreen(5, 0); }
          if (item & SCREEN_CLEAN) { setScreen(14, 1) }
          else { setScreen(14, 0); }
          if (item & SCREEN_F1) { setScreen(6, 1) }
          else { setScreen(6, 0); }
          //if (item & SCREEN_FREE3) {console.log('bit 10 set');}
          if (item & SCREEN_LUNAR) { setScreen(3, 1) }
          else { setScreen(3, 0); }
          if (item & SCREEN_COLON) { setScreen(4, 1) }
          else { setScreen(4, 0); }
          if (item & SCREEN_LDOT) { setScreen(11, 1) }
          else { setScreen(11, 0); }

          document.getElementById('tabelle').getElementsByTagName('tbody')[0].getElementsByTagName('td')[7].innerHTML = myArray[2];
          if (myArray[3] == "1") {
            SetMySwitch("configset", 1);
          }
          else {
            SetMySwitch("configset", 0);
          }
          if (myArray[4] == "1") {
            SetMySwitch("configserial", 1);
          }
          else {
            SetMySwitch("configserial", 0);
          }
          if (myArray[5] == "1") {
            SetMySwitch("configserial2web", 1);
          }
          else {
            SetMySwitch("configserial2web", 0);
          }
          if (myArray[7] == "1") {
            SetMySwitch("configdups", 1);
          }
          else {
            SetMySwitch("configdups", 0);
          }
        }
      }
      function WebLog(daten) {
        if (WebLogging) {
          console.log(daten);
        }
      }
      function SetInfoText(OnOff, SetCount, Day, H, M) {
        var text = document.getElementById("infotext");
        text.textContent = "Display: " + OnOff + " Suppressed SET: " + SetCount + " UP: " + Day + " Days  "+ String(H).padStart(2,'0') + ":" + String(M).padStart(2,'0');
      }
      function setScreen(itemm, mark) {
        //console.log('item:' + itemm + ' mark:'+mark);
        document.getElementById('tabelle').getElementsByTagName('tbody')[0].getElementsByTagName('td')[itemm].classList = ('markiert' + mark);
      }
      function onLoad(event) {
        initWebSocket();
        initButtons();
        initSwitch();
      }
      function initButtons() {
        document.getElementById('button1').addEventListener('click', function (e) { SendMyData('tup') });
        document.getElementById('button2').addEventListener('click', function (e) { SendMyData('tdown') });
        document.getElementById('button3').addEventListener('click', function (e) { SendMyData('jets') });
        document.getElementById('button4').addEventListener('click', function (e) { SendMyData('set') });
        document.getElementById('button5').addEventListener('click', function (e) { SendMyData('light') });
        document.getElementById('button6').addEventListener('click', function (e) { SendMyData('reset') });
        document.getElementById('button7').addEventListener('click', function (e) { SendMyData('reboot') });        
      }
      function initSwitch() {
        document.getElementById('configset').addEventListener('click', function (e) { SendMySwitch('configset') });
        document.getElementById('configserial').addEventListener('click', function (e) { SendMySwitch('configserial') });
        document.getElementById('configserial2web').addEventListener('click', function (e) { SendMySwitch('configserial2web') });
        document.getElementById('configweb').addEventListener('click', function (e) { SendMySwitch('configweb') });
        document.getElementById('configdups').addEventListener('click', function (e) { SendMySwitch('configdups') });
      }
      function SendMyData(data) {
        console.log('Send: ' + data);
        websocket.send('button:' + data + '\0');
      }
      function SetMySwitch(data, state) {
        const toggle = document.getElementById(data);
        var onoff = toggle.parentNode.querySelector('.onoff');
        toggle.checked = state;
      }
      function SendMySwitch(data) {
        const toggle = document.getElementById(data);
        var onoff = toggle.parentNode.querySelector('.onoff');
        var state = toggle.checked ? '1' : '0';
        console.log('Send: ' + data + ':' + state);
        websocket.send('config:' + data + ':' + state + '\0');
      }
    </script>
  </div>
</body>
</html>
)rawliteral";

/*
  0=SET
  3=moon
  4=udot
  5=ready
  6=f1
  7=TEXT
  8=F2
  9=jets
  11=ldot
  12=sun
  14=clean
*/
void notifyClients(const String &var)
{
  if (!myConfig.WebServer)
  {
    return;
  }
  //& (clients_connected > 0)
  //if ((myConfig.SerialDebug) & (clients_connected > 0) ) {
  //  MySerial.printf("notifyClients %s message %s clients connected %u \n",var, message, clients_connected);
  //}
  // Nur Daten senden wenn Clients verbunden Sind.
  if (clients_connected > 0)
  {
    ws.textAll(var + "$$" + message);
    /*
    if (var == "displ")
    {
      ws.textAll(var + ":" + message);
    }
    if (var == "config")
    {
      ws.textAll(var + ":" + message);
    }
    if (var == "msg")
    {
      ws.textAll(var + ":" + message);
    }
    
    */
    //else
    //  ws.textAll(var + ":"+  String(ledState));
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    //if (strcmp((char*)data, "button:toggle") == 0) {
    if (strcmp((char *)data, "button:tup") == 0)
    {
      WebKey = 1;
      WebKeyRepeat = 3;
    }
    if (strcmp((char *)data, "button:tdown") == 0)
    {
      WebKey = 2;
      WebKeyRepeat = 3;
    };
    if (strcmp((char *)data, "button:jets") == 0)
    {
      WebKey = 3;
      WebKeyRepeat = 3;
    };
    if (strcmp((char *)data, "button:set") == 0)
    {
      WebKey = 4;
      WebKeyRepeat = 3;
    };
    if (strcmp((char *)data, "button:light") == 0)
    {
      WebKey = 5;
      WebKeyRepeat = 3;
      //WebKeyNone=15;
    };
    if (strcmp((char *)data, "button:reset") == 0)
    {
      WebKey = 14;
      WebKeyRepeat = 0;
      myPayload.crc = 358;
      SendPayload(0);
    };
    if (strcmp((char *)data, "button:reboot") == 0)
    {
      MySerial.println("ESP Reboot");
      message ="ESP Reboot";
      notifyClients("msg");
      ESP.restart;
    };    
    if (strcmp((char *)data, "config:configdups:1") == 0)
    {
      myConfig.SuppressDuplicate = true;
      SetDisplay(0);
    }
    if (strcmp((char *)data, "config:configdups:0") == 0)
    {
      myConfig.SuppressDuplicate = false;
      SetDisplay(0);
    }
    if (strcmp((char *)data, "config:configserial:1") == 0)
    {
      myConfig.SerialDebug = true;
      SetDisplay(0);
    }
    if (strcmp((char *)data, "config:configserial:0") == 0)
    {
      myConfig.SerialDebug = false;
      SetDisplay(0);
      MySerial.println("Serial Debug off");
    }
    if (strcmp((char *)data, "config:configset:1") == 0)
    {
      myConfig.SetAllow = true;
      SetDisplay(0);
    }
    if (strcmp((char *)data, "config:configset:0") == 0)
    {
      myConfig.SetAllow = false;
      SetDisplay(0);
      MySerial.println("Set Allow off");
    }
    if (strcmp((char *)data, "config:configserial2web:1") == 0)
    {
      myConfig.Serial2Web = true;
      SetDisplay(0);
    
    }
    if (strcmp((char *)data, "config:configserial2web:0") == 0)
    {
      myConfig.Serial2Web = false;
      SetDisplay(0);
      //MySerial.println("Set Allow off");
    }    
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    MySerial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    clients_connected++;
    SendInitialWeb();
    break;
  case WS_EVT_DISCONNECT:
    MySerial.printf("WebSocket client #%u disconnected\n", client->id());
    clients_connected--;
    break;
  case WS_EVT_DATA:
    MySerial.printf("event Data %s size %d\n", data, len);
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String &var)
{
  MySerial.println(var);
  /*
  if(var == "STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  */
  return String();
}

void initWebserver(int warten){
  if (myConfig.WebServer)
  {
    // Connect to Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      MySerial.println("Connecting to WiFi..");
      WebserverRun = false;  
    }      
//    while (WiFi.status() != WL_CONNECTED)
    delay(warten);
    if (WiFi.status() != WL_CONNECTED) {
      MySerial.println("Wifi timeout");
      return;
    }    
    if (WebserverRun){
      return;
    }
    
    // Print ESP Local IP Address
    MySerial.println(WiFi.localIP());
    initWebSocket();
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html, processor); });
    // Start server
    server.begin();
    WebserverRun = true;
  }
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);   //  UART0 (TX = GPIO1, RX = GPIO3) used for RS485 TTL Input
  Serial1.begin(115200);  // UART1 (TX = GPIO2), use the Serial11 object. Send Data to Mainboard.
  MySerial.begin(115200); //used for Console Debug Output.

  mySerBuf.len = 0;
  mySerBuf.pos = 0;
  myConfig.WebServer = true;

  myConfig.SetAllow = false;
  myConfig.SerialDebug = false;
  myConfig.Serial2Web = false;
  myConfig.WebDebug = false;
  myConfig.SuppressDuplicate = true;
  myConfig.CountSetSup = 0;
  myConfig.DisplayOnOff = true;
  myConfig.LastDrop = false;
  

  myPayload.crc = 0;
  myPayload.display[0] = 'I';
  myPayload.display[1] = ' ';
  myPayload.display[2] = 'N';
  myPayload.display[3] = ' ';
  myPayload.display[4] = 'I';
  myPayload.display[5] = ' ';
  myPayload.display[6] = 'T';
  myPayload.display[7] = ' ';
  myPayload.display[8] = '\0';

  Serial1.println("Starte ESP HAWK auf UART1");
  MySerial.println("Starte ESP HAWK Software Serial");
  PrintConfig();

  initWebserver(1000);


}

void loop()
{
  if (myConfig.WebServer)
  {
    ws.cleanupClients();
  }
  
  if (Serial.available())
  {
    StorePayload(Serial.read());
  }
  /*
  if (MySerial.available())
  {
    ConsoleRead(MySerial.read());
  }
  */
  Uptime();
} // end loop

void Uptime()
{
  if ((millis() - TT) > 60000)
  {
    TT += 60000;
    upM++;
    if (WebserverRun != true) {
      initWebserver(1);
    }  
    if (upM == 60)
    {
      upM = 0;
      upH++;
    }
    if (upH == 24)
    {
      upH = 0;
      upD++;
    }
    MySerial.println("Uptime " + String(upD) + " Days  " +  " HH24:mi " + String(upH)+ ":" + String(upM));
    SetDisplay(5);
  }
  if (TT > millis())
    TT=0;
} // end Uptime

void PrintConfig()
{
  MySerial.printf("Wifi mode ist %s\n", myConfig.WebServer ? "an" : "aus");
  MySerial.printf("Set Taste erlaubt %s\n", myConfig.SetAllow ? "true" : "false");
  MySerial.printf("Dups unterdruecken %s\n", myConfig.SuppressDuplicate ? "true" : "false");
  MySerial.printf("Debug Serial %s\n", myConfig.SerialDebug ? "true" : "false");
  MySerial.printf("Degug Web %s\n", myConfig.WebDebug ? "true" : "false");
  MySerial.print("Meine IP ");
  MySerial.println(WiFi.localIP());
}
void ConsoleRead(int data)
{
  MySerial.printf("Console gelesen %d", data);
  /*
   switch (data)
    {
    case 63:  //?
      myConfig.Serial_Debug = false;
      MySerial.println("HELP");
      MySerial.println("Serial Debug disabled");
      MySerial.print("My IP Adress ");
      MySerial.println(WiFi.localIP());
      MySerial.println("");
      MySerial.println("");
      MySerial.println("");

      break;  
    default:
      MySerial.printf("Console gelesen %d", data);
      break;  
    }  
    */
}
void StorePayload(int SerialData)
{
  //MySerial.printf("l:%d %02X ",mySerBuf.len, SerialData);
  if (mySerBuf.len < 49)
  {
    mySerBuf.buf[mySerBuf.len] = SerialData;
    mySerBuf.len++;
    if (mySerBuf.len > 7)
      FindPayload();
  }
  else
  {
    MySerial.println("Store Payload Serial Buffer overflow");
    TrimPayload(0);
  }
}

void TrimPayload(int pos)
{
  if ((pos == 0) | (pos >= mySerBuf.len))
  { // wenn pos 0 oder pos >= buffer
    mySerBuf.len = 0;
    mySerBuf.pos = 0;
    //MySerial.println("trim 0");
  }
  else
  {
    MySerial.printf("\n Start Trim pos %d buflen: %d\n Old Buffer: ", pos, mySerBuf.len);
    for (int i = 0; i < mySerBuf.len; i++)
    {
      MySerial.printf("%02X ", mySerBuf.buf[i]);
    }
    memcpy(mySerBuf.buf, mySerBuf.buf + pos, mySerBuf.len - pos);
    mySerBuf.len = mySerBuf.len - pos;
    mySerBuf.pos = 0;
    MySerial.printf("\n Now Trim pos %d buflen: %d\n New Buffer: ", pos, mySerBuf.len);
    for (int i = 0; i < mySerBuf.len; i++)
    {
      MySerial.printf("%02X ", mySerBuf.buf[i]);
    }
    MySerial.println("\nTrim END");
  }
}

void FindPayload()
// SerialData = gelesene Daten
// SerialPayload = Payload Buffer
// SerialPlayloadPos = Payload Buffer Pos
//struct Struct_Data myPayload
{
  //buflen--;
  //printf("\nChar groesse %d\n", buflen);
  /*sucht in einem char array den naechsten Datensatz*/
  // printf("datenpos %d buflen %d",datenpos, buflen);
  // printf("BUFFER RAW DATA->");
  // for ( int i = datenpos; i < buflen; i++ ) {
  // printf("%02X ",text[i]);
  // }
  // printf("\n");
  int PayloadLen = 0;
  //if (mySerBuf.len < 8) {
  //  return; // Payload zu kurz Payload min. 7 Byte
  //}
  for (int i = 0; i < mySerBuf.len; i++)
  {
    //printf("datenpos : %d ", datenpos);
    if (mySerBuf.buf[i] == 'X')
    //if ( (((mySerBuf.buf[i] == 'X') & (mySerBuf.buf[i + 1] == 'M') & (mySerBuf.buf[i + 2] == 'S'))) | ( ((mySerBuf.buf[i] == 'X') & (mySerBuf.buf[i + 1] == 'S') & (mySerBuf.buf[i + 2] == 'M'))) )
    {
      if ((mySerBuf.len - i) < 7)
        return; // Zu wenig Daten

      //MySerial.printf("\n Anfang gefunden an stelle %d Zeichenkette %c%c%c\n", i,mySerBuf.buf[i],mySerBuf.buf[i+1],mySerBuf.buf[i+2]);
      //MySerial.printf("buflen:%d datenpos: %d ",mySerBuf.len, mySerBuf.pos);
      //PayloadLen = ((mySerBuf.buf[i + 3] * 256) + mySerBuf.buf[i + 4]) ;
      PayloadLen = (mySerBuf.buf[i + 3] << 8) + mySerBuf.buf[i + 4];
      if (PayloadLen > 7)
      {
        MySerial.println("Payload to long: "); // + myPayload.len);
        TrimPayload(i + 3);
        return;
      }
      if ((mySerBuf.len) < PayloadLen + i + 7)
      {
        //MySerial.println("abbruch nicht genug daten");
        //myPayload.len = -1;
        return;
      }
      // Daten in das Struct kopieren.
      memcpy(myPayload.device, mySerBuf.buf + i, 3); // Stelle 1-3  XMS ODER XMS
      myPayload.len = PayloadLen;
      memcpy(myPayload.payload, mySerBuf.buf + i + 5, myPayload.len); // Die Nutzdaten Stelle 6... laenge
      //myPayload.crc = (mySerBuf.buf[i + 4 + myPayload.len + 1] * 256) + mySerBuf.buf[i + 4 + myPayload.len + 2];
      myPayload.crc = (mySerBuf.buf[i + 4 + myPayload.len + 1] << 8) + mySerBuf.buf[i + 4 + myPayload.len + 2];

      myPayload.pos = i + 7 + myPayload.len; // merken der wo wir stehen
      mySerBuf.pos = myPayload.pos;
      memcpy(myPayload.data, mySerBuf.buf + i, myPayload.len + 7); // Kompletter Datensatz
      if (myPayload.payload[0] == 5)                               // Beginnt der Payload mit 5 ist es eine Bildschirmausgabe
      {
        //memcpy(myPayload.last_display, SerialPayload + i + 5, myPayload.len);
        int z = 0;
        for (int x = 0; x < myPayload.len; x++)
        {
          if ((mySerBuf.buf[i + 5 + x]) < 128)
          {
            myPayload.display[x + z] = mySerBuf.buf[i + 5 + x];
            z++;
            myPayload.display[x + z] = 32; // Blank
          }
          else
          {
            myPayload.display[x + z] = mySerBuf.buf[i + 5 + x] - 128;
            z++;
            myPayload.display[x + z] = 46; // Dezimapunkt
          }
          myPayload.display[x + z + 1] = 0; // Null Byte Terminator
        }
        //memcpy(myPayload.display, mySerBuf.buf + i + 5, myPayload.len);
      }

      TrimPayload(mySerBuf.pos);
      /**/

      PrintPayload();
      return;
    }
    //printf("%02X ", text[i]);
  }
  // printf(" buffer null\”");
  myPayload.len = -1;
  return;
}

void PrintPayload()
{
  //printf("## device %c%c%c dup:%d", mydata.device[0], mydata.device[1], mydata.device[2], print_dup);
  //char payload [15] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
  //memcpy(payload, mydata.payload, mydata.len-1);
  //if (!myConfig.Serial_Debug)
  //  return;
  bool CallSetDisplay = false;
  //bool CallSendPayload = false;
  message = "";
  if (myPayload.device[2] == 'M')
  {
    //SendPayloadMB(0);
    if ((myPayload.last_mb_crc == myPayload.crc) & (myConfig.SuppressDuplicate))
      return;
    
    if (((myPayload.last_mb_crc + myPayload.crc) == 526) & (!myConfig.DisplayOnOff)) {
      return;
    }
    else if (((myPayload.last_mb_crc + myPayload.crc) == 526) & (myConfig.DisplayOnOff)) {
      myConfig.DisplayOnOff = false;
      MySerial.println("Display Off");
    }
    else if (((myPayload.last_mb_crc + myPayload.crc) != 526) & (!myConfig.DisplayOnOff))
    {
      myConfig.DisplayOnOff = true;
    }
    
    if (myConfig.SerialDebug)
    {
      MySerial.printf("MB last_crc:%6d cur_crc:%6d", myPayload.last_mb_crc, myPayload.crc);
    }
    message = "";
    if (myConfig.Serial2Web)
    {
      sprintf(messagec, "MB last_crc:%6d cur_crc:%6d", myPayload.last_mb_crc, myPayload.crc);
      message = String(messagec);
    }
    myPayload.last_mb_crc = myPayload.crc;
    CallSetDisplay = true;
  }
  else
  {
    /* Daten senden*/
    SendPayload(0);
    if ((myPayload.last_kb_crc == myPayload.crc) & (myConfig.SuppressDuplicate))
      return;
    if (myConfig.SerialDebug)
    {
      MySerial.printf("KB last_crc:%6d cur_crc:%6d", myPayload.last_kb_crc, myPayload.crc);
    }
    if (myConfig.Serial2Web)
    {
      sprintf(messagec, "KB last_crc:%6d cur_crc:%6d", myPayload.last_kb_crc, myPayload.crc);
      message = String(messagec);
    }
    myPayload.last_kb_crc = myPayload.crc;
  }

    myPayload.last_mb_crc = myPayload.crc;
  if (myConfig.SerialDebug)
  {
    MySerial.printf(" %c%c%c Bytes:%d  Payload: ", myPayload.device[0], myPayload.device[1], myPayload.device[2], myPayload.len);
  }
  if (myConfig.Serial2Web)  {
    sprintf(messagec," %c%c%c Bytes:%d  Payload: ", myPayload.device[0], myPayload.device[1], myPayload.device[2], myPayload.len);
    message += String(messagec);
  }
  for (int i = 0; i <= myPayload.len - 1; i++)
  {
    if (myConfig.SerialDebug) {
      MySerial.printf("%02X", myPayload.payload[i]);
    }
    if (myConfig.Serial2Web)  {
      printf(messagec,"%02X", myPayload.payload[i]);
      message += String(messagec);
    }  
  }
  //printf(" CRC %ld",mydata.crc);
  /* KEYBOARD MAINBOARD TEXT DATA */
  for (int i = 1; i <= text_count; i++)
  {
    if ((myPayload.device[2] == 'M') & (i < 7))
    {
      continue;
    }
    if (myPayload.crc == MyText[i].crc)
    {
      if (myConfig.Serial2Web) {
        sprintf(messagec," %s", MyText[i].Text);
        message += String(messagec);
      }
      if (myConfig.SerialDebug)
        MySerial.printf(" %s", MyText[i].Text);
      break;
    }
    if (i == text_count)
    {
      if (myConfig.SerialDebug)
        MySerial.printf(" %s", MyText[0].Text);
      
      if (myConfig.Serial2Web) {
        sprintf(messagec," %s", MyText[0].Text);
        message += String(messagec);
      }        
    }
  }

  if (myConfig.SerialDebug)
  {
    MySerial.printf(" ASCII:");
    for (int i = 0; i <= myPayload.len - 1; i++)
    {
      if (myPayload.payload[i] == 0)
      {
        MySerial.printf("°");
      }
      else if (
          (myPayload.payload[i] == 13) ||
          (myPayload.payload[i] == 10) ||
          (myPayload.payload[i] == 11) ||
          (myPayload.payload[i] == 12) ||
          (myPayload.payload[i] == 16) ||
          (myPayload.payload[i] == 26) ||
          (myPayload.payload[i] == 28))
      {
        MySerial.printf("%c", 94);
      }
      else
      {
        MySerial.printf("%c", myPayload.payload[i]);
      }
    } // end for
    MySerial.printf(" Display: %s", myPayload.display);
    MySerial.printf("  rawdata: ");
    for (int i = 0; i < myPayload.len + 7; i++)
    {
      MySerial.printf("%02x", myPayload.data[i]);
    } // end for
    MySerial.printf("\n");
  } // end if SerialDebug
 if (myConfig.Serial2Web)
  {
    message += " ASCII:";
    for (int i = 0; i <= myPayload.len - 1; i++)
    {
      if (myPayload.payload[i] == 0)
      {
        message += "°";
      }
      else if (
          (myPayload.payload[i] == 13) ||
          (myPayload.payload[i] == 10) ||
          (myPayload.payload[i] == 11) ||
          (myPayload.payload[i] == 12) ||
          (myPayload.payload[i] == 16) ||
          (myPayload.payload[i] == 26) ||
          (myPayload.payload[i] == 28))
      {
        message += "^";
      }
      else
      {
        sprintf(messagec,"%c", myPayload.payload[i]);
        message += String(messagec);
      }
    } // end for
    sprintf(messagec," Display: %s", myPayload.display);
    message += String(messagec);
    message += "  rawdata: ";
    for (int i = 0; i < myPayload.len + 7; i++)
    {
      sprintf(messagec,"%02x", myPayload.data[i]);
      message += String(messagec);
    } // end for
    notifyClients("msg");
  } // end if SerialDebug

  if (myPayload.payload[0] == 5)
    SetDisplay(5);
  if (CallSetDisplay)
    SetDisplay(myPayload.crc);
}; // end void PrintPayload

/*
void SendPayloadMB(int ConstID)
{
  // {" RY OFF   \0", "\x58\x53\x4d\x00\x03\x1a\x00\x00\x01\x15",277, 10}, //ID=11
  // {" RY ON    \0", "\x58\x53\x4d\x00\x03\x1a\x00\x50\x01\x65",357, 10}, //ID=12
  // {" MB RESET \0", "\x58\x53\x4d\x00\x03\x1b\x27\x10\x01\x4d",333, 10}, //ID=13
  if ((WebKey == 2) & (KeyBoard > 15))
    KeyBoard--;
  if ((WebKey == 1) & (KeyBoard < 45))
    KeyBoard++;

  if ((myPayload.crc == 357) | (myPayload.crc == 277))
  {
    if (WebKeyNone > 0)
    {
      MySerial.printf("Send %s", MyText[WebKeyNone].Text);
      for (int i = 0; i < MyText[WebKeyNone].PayloadLen; i++)
      {
        Serial1.write(MyText[WebKeyNone].Payload[i]);
        MySerial.printf(" %02X ", MyText[WebKeyNone].Payload[i]);
      }
      WebKeyNone = 0;
    }
    else
    {
      MySerial.printf("Send %s", MyText[KeyBoard].Text);
      for (int i = 0; i < MyText[KeyBoard].PayloadLen; i++)
      {
        Serial1.write(MyText[KeyBoard].Payload[i]);
        MySerial.printf(" %02X ", MyText[KeyBoard].Payload[i]);
      }
    }
    MySerial.println("");
  }
  else
  {
    for (int i = 0; i < myPayload.len + 7; i++)
    {
      Serial1.write(myPayload.data[i]);
      MySerial.printf("%02X ", myPayload.data[i]);
    }
    MySerial.println("");
  }

  WebKey = 0;
}
*/
void SendPayload(int ConstID)
{
  if ((myPayload.crc == 362) & (!myConfig.SetAllow))
  {
    if (!myConfig.LastDrop) {
      MySerial.printf("KEY SET DROP!!!! Send None bytes %u \n", MyText[6].PayloadLen);
      myConfig.CountSetSup++;
      myConfig.LastDrop = true;
      SetDisplay(5);
    }
    for (int i = 0; i < MyText[6].PayloadLen; i++)
    {
      Serial1.write(MyText[6].Payload[i]);
      //MySerial.printf("%02X ", MyText[6].Payload[i]);
    }
    //MySerial.println("");
  }
  else if ((myPayload.crc == 358) & (WebKey > 0))
  {
    // Tastatur sendet default Wert. Wenn im Web eine Taste gedrückt wurde diese senden
    MySerial.printf(" WebKey pressed %u : ", WebKey);
    myConfig.LastDrop = false;
    for (int i = 0; i < MyText[WebKey].PayloadLen; i++)
    {
      Serial1.write(MyText[WebKey].Payload[i]);
      MySerial.printf("%02X ", MyText[WebKey].Payload[i]);
    }
    MySerial.println("");
    message = "Webkey send " + String(WebKey);
    notifyClients("msg");
    if (WebKeyRepeat > 0) {
      WebKeyRepeat--;
    }
    else {
      WebKey = 0;
    }
  }
  else
  {
    myConfig.LastDrop = false;
    for (int i = 0; i < myPayload.len + 7; i++)
    {
      Serial1.write(myPayload.data[i]);
      //MySerial.printf("%02X ", myPayload.data[i]);
    }
  }
};
void SendInitialWeb()
{
  SetDisplay(5);
  SetDisplay(0);
}

void SetDisplay(int crc)
{
  switch (crc)
  {
  case 5:
    message  =  String(myConfig.DisplayOnOff ? "ON" : "OFF") + "$$" + String(myConfig.CountSetSup) + "$$" + String(upD) + "$$" + String(upH) + "$$" + String(upM); 
    notifyClients("infotext");    
    break;
  default:
    message = String((myPayload.payload[1] << 8) + myPayload.payload[2]) 
      + "$$" + String(myPayload.display) 
      + "$$" + String(myConfig.SetAllow ? "1" : "0")  
      + "$$" + String(myConfig.SerialDebug ? "1" : "0") 
      + "$$" + String(myConfig.Serial2Web ? "1" : "0") 
      + "$$" + String(myConfig.WebDebug ? "1" : "0")
      + "$$" + String(myConfig.SuppressDuplicate ? "1" : "0");
    notifyClients("displconf");
    break;
  }
}
