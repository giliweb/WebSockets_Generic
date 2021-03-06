/****************************************************************************************************************************
   Generic_WebSocketServerFragmentation_W5500.ino
   For Generic boards using W5x00 Ethernet Shield/Module

   Based on and modified from WebSockets libarary https://github.com/Links2004/arduinoWebSockets
   to support other boards such as  SAMD21, SAMD51, Adafruit's nRF52 boards, etc.

   Built by Khoi Hoang https://github.com/khoih-prog/WebSockets_Generic
   Licensed under MIT license
   Version: 2.2.2

   Originally created on: 22.05.2015
   Original Author: Markus Sattler

   Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
   2.1.3   K Hoang      15/05/2020 Initial porting to support SAMD21, SAMD51, nRF52 boards, such as AdaFruit Feather nRF52832,
                                  nRF52840 Express, BlueFruit Sense, Itsy-Bitsy nRF52840 Express, Metro nRF52840 Express, etc.
   2.2.1   K Hoang      18/05/2020 Bump up to sync with v2.2.1 of original WebSockets library
   2.2.2   K Hoang      25/05/2020 Add support to Teensy, SAM DUE and STM32. Enable WebSocket Server for new supported boards.
 *****************************************************************************************************************************/

#define _WEBSOCKETS_LOGLEVEL_     3
#define WEBSOCKETS_NETWORK_TYPE   NETWORK_W5100

#include <WebSocketsServer_Generic.h>

WebSocketsServer webSocket = WebSocketsServer(81);

String fragmentBuffer = "";

uint8_t mac[6] =  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0A };

// Select the IP address according to your local network
IPAddress ip(192, 168, 2, 222);

// Only for W5100
#define SDCARD_CS       4

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch (type)
  {
    case WStype_DISCONNECTED:
      Serial.println( "[" + String(num) + "] Disconnected!");
      break;
      
    case WStype_CONNECTED:
      {
        //IPAddress ip = webSocket.remoteIP(num);
        //Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
      
    case WStype_TEXT:
      Serial.println( "[" + String(num) + "] get Text: " + String((char *) payload));
      break;
      
    case WStype_BIN:
      Serial.println( "[" + String(num) + "] get binary length: " + String(length));
      //hexdump(payload, length);

      // send message to client
      webSocket.sendBIN(num, payload, length);
      break;

    // Fragmentation / continuation opcode handling
    // case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT_TEXT_START:
      fragmentBuffer = (char*)payload;
      Serial.println( "[" + String(num) + "] get start start of Textfragment: " + String((char *) payload));
      break;
      
    case WStype_FRAGMENT:
      fragmentBuffer += (char*)payload;
      Serial.println( "[" + String(num) + "] get Textfragment: " + String((char *) payload));
      break;
      
    case WStype_FRAGMENT_FIN:
      fragmentBuffer += (char*)payload;
      Serial.println( "[" + String(num) + "] get end of Textfragment: " + String((char *) payload));
      Serial.println( "[" + String(num) + "] full frame: " + fragmentBuffer);
      break;

    default:
      break;
      
  }
}

void setup()
{
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH); // Deselect the SD card
  
  // Serial.begin(921600);
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nStart Generic_WebSocketServerFragmentation_W5500");

  for (uint8_t t = 4; t > 0; t--)
  {
    Serial.println("[SETUP] BOOT WAIT " + String(t));
    Serial.flush();
    delay(1000);
  }

  // start the ethernet connection and the server:
  // Use Static IP
  Ethernet.begin(mac, ip);
  //Configure IP address via DHCP
  //Ethernet.begin(mac);


  // server address, port and URL
  Serial.print("WebSockets Server IP address: ");
  Serial.println(Ethernet.localIP());
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop()
{
  webSocket.loop();
}
