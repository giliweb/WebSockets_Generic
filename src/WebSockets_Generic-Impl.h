/****************************************************************************************************************************
  WebSockets_Generic-Impl.h - WebSockets Library for boards
  
  Based on and modified from WebSockets libarary https://github.com/Links2004/arduinoWebSockets
  to support other boards such as  SAMD21, SAMD51, Adafruit's nRF52 boards, etc.
  
  Built by Khoi Hoang https://github.com/khoih-prog/WebSockets_Generic
  Licensed under MIT license
  Version: 2.2.2
   
  @original file WebSockets.cpp
  @date 20.05.2015
  @author Markus Sattler

  Copyright (c) 2015 Markus Sattler. All rights reserved.
  This file is part of the WebSockets for Arduino.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  Version Modified By   Date      Comments
 ------- -----------  ---------- -----------
  2.1.3   K Hoang      15/05/2020 Initial porting to support SAMD21, SAMD51, nRF52 boards, such as AdaFruit Feather nRF52832, 
                                  nRF52840 Express, BlueFruit Sense, Itsy-Bitsy nRF52840 Express, Metro nRF52840 Express, etc.
  2.2.1   K Hoang      18/05/2020 Bump up to sync with v2.2.1 of original WebSockets library
  2.2.2   K Hoang      25/05/2020 Add support to Teensy, SAM DUE and STM32. Enable WebSocket Server for new supported boards.
 *****************************************************************************************************************************/

#ifndef WEBSOCKETS_GENERIC_IMPL_H_
#define WEBSOCKETS_GENERIC_IMPL_H_

#ifdef ESP8266
  #include <core_esp8266_features.h>
#endif

extern "C"
{
  #ifdef CORE_HAS_LIBB64
    #warning CORE_HAS_LIBB64
    #include <libb64/cencode.h>
  #else
    #warning No CORE_HAS_LIBB64
    #include "libb64/cencode_inc.h"
  #endif
}

#ifdef ESP8266
  #include <Hash.h>
#elif defined(ESP32)
  #include <hwcrypto/sha.h>
#else

extern "C"
{
  #include "libsha1/libsha1.h"
}

#endif

/**

   @param client WSclient_t *  ptr to the client struct
   @param code uint16_t see RFC
   @param reason ptr to the disconnect reason message
   @param reasonLen length of the disconnect reason message
*/
void WebSockets::clientDisconnect(WSclient_t * client, uint16_t code, char * reason, size_t reasonLen)
{
  LOGDEBUG2(client->num, "[handleWebsocket] clientDisconnect code:", code);
  
  if (client->status == WSC_CONNECTED && code)
  {
    if (reason)
    {
      sendFrame(client, WSop_close, (uint8_t *) reason, reasonLen);
    }
    else
    {
      uint8_t buffer[2];
      buffer[0] = ((code >> 8) & 0xFF);
      buffer[1] = (code & 0xFF);
      sendFrame(client, WSop_close, &buffer[0], 2);
    }
  }
  clientDisconnect(client);
}

/**

   @param buf uint8_t *         ptr to the buffer for writing
   @param opcode WSopcode_t
   @param length size_t         length of the payload
   @param mask bool             add dummy mask to the frame (needed for web browser)
   @param maskkey uint8_t[4]    key used for payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
*/
uint8_t WebSockets::createHeader(uint8_t * headerPtr, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin) 
{
  uint8_t headerSize;

  // calculate header Size
  if (length < 126)
  {
    headerSize = 2;
  }
  else if (length < 0xFFFF)
  {
    headerSize = 4;
  }
  else
  {
    headerSize = 10;
  }

  if (mask)
  {
    headerSize += 4;
  }

  // create header

  // byte 0
  *headerPtr = 0x00;

  if (fin)
  {
    *headerPtr |= bit(7);    ///< set Fin
  }

  *headerPtr |= opcode;    ///< set opcode
  headerPtr++;

  // byte 1
  *headerPtr = 0x00;

  if (mask)
  {
    *headerPtr |= bit(7);    ///< set mask
  }

  if (length < 126)
  {
    *headerPtr |= length;
    headerPtr++;
  }
  else if (length < 0xFFFF)
  {
    *headerPtr |= 126;
    headerPtr++;
    *headerPtr = ((length >> 8) & 0xFF);
    headerPtr++;
    *headerPtr = (length & 0xFF);
    headerPtr++;
  }
  else
  {
    // Normally we never get here (to less memory)
    *headerPtr |= 127;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = ((length >> 24) & 0xFF);
    headerPtr++;
    *headerPtr = ((length >> 16) & 0xFF);
    headerPtr++;
    *headerPtr = ((length >> 8) & 0xFF);
    headerPtr++;
    *headerPtr = (length & 0xFF);
    headerPtr++;
  }

  if (mask)
  {
    *headerPtr = maskKey[0];
    headerPtr++;
    *headerPtr = maskKey[1];
    headerPtr++;
    *headerPtr = maskKey[2];
    headerPtr++;
    *headerPtr = maskKey[3];
    headerPtr++;
  }
  
  return headerSize;
}

/**

   @param client WSclient_t *   ptr to the client struct
   @param opcode WSopcode_t
   @param length size_t         length of the payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
   @return true if ok
*/
bool WebSockets::sendFrameHeader(WSclient_t * client, WSopcode_t opcode, size_t length, bool fin)
{
  uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

  uint8_t headerSize = createHeader(&buffer[0], opcode, length, client->cIsClient, maskKey, fin);

  if (write(client, &buffer[0], headerSize) != headerSize)
  {
    return false;
  }

  return true;
}

/**

   @param client WSclient_t *   ptr to the client struct
   @param opcode WSopcode_t
   @param payload uint8_t *     ptr to the payload
   @param length size_t         length of the payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
   @param headerToPayload bool  set true if the payload has reserved 14 Byte at the beginning to dynamically 
                                add the Header (payload neet to be in RAM!)
   @return true if ok
*/
bool WebSockets::sendFrame(WSclient_t * client, WSopcode_t opcode, uint8_t * payload, size_t length, bool fin, bool headerToPayload)
{
  if (client->tcp && !client->tcp->connected())
  {
    LOGDEBUG1(client->num, "[sendFrame] not Connected!?");
    
    
    return false;
  }

  if (client->status != WSC_CONNECTED)
  {
    LOGDEBUG1(client->num, "[sendFrame] not in WSC_CONNECTED state!?");
    
    return false;
  }

  LOGDEBUG1(client->num, "[sendFrame] ------- send message frame -------");
   
  LOGDEBUG1(client->num, "[sendFrame]");
  LOGDEBUG1("fin:",             fin); 
  LOGDEBUG1("opCode:",          opcode); 
  LOGDEBUG1("mask:",            client->cIsClient); 
  LOGDEBUG1("length:",          length);
  LOGDEBUG1("headerToPayload:", headerToPayload);              

  if (opcode == WSop_text)
  {                     
    LOGDEBUG2(client->num, "[sendFrame] text:", (char*) (payload + (headerToPayload ? 14 : 0)));
  }

  uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

  uint8_t headerSize;
  uint8_t * headerPtr;
  uint8_t * payloadPtr = payload;
  bool useInternBuffer = false;
  bool ret             = true;

  // calculate header Size
  if (length < 126)
  {
    headerSize = 2;
  }
  else if (length < 0xFFFF)
  {
    headerSize = 4;
  }
  else
  {
    headerSize = 10;
  }

  if (client->cIsClient)
  {
    headerSize += 4;
  }

#ifdef WEBSOCKETS_USE_BIG_MEM
  // only for ESP since AVR has less HEAP
  // try to send data in one TCP package (only if some free Heap is there)
  if (!headerToPayload && ((length > 0) && (length < 1400)) && (GET_FREE_HEAP > 6000))
  {
    LOGDEBUG1(client->num, "[sendFrame] pack to one TCP package...");
    
    uint8_t * dataPtr = (uint8_t *)malloc(length + WEBSOCKETS_MAX_HEADER_SIZE);

    if (dataPtr)
    {
      memcpy((dataPtr + WEBSOCKETS_MAX_HEADER_SIZE), payload, length);
      headerToPayload = true;
      useInternBuffer = true;
      payloadPtr      = dataPtr;
    }
  }
#endif

  // set Header Pointer
  if (headerToPayload)
  {
    // calculate offset in payload
    headerPtr = (payloadPtr + (WEBSOCKETS_MAX_HEADER_SIZE - headerSize));
  }
  else
  {
    headerPtr = &buffer[0];
  }

  if (client->cIsClient && useInternBuffer)
  {
    // if we use a Intern Buffer we can modify the data
    // by this fact its possible the do the masking
    for (uint8_t x = 0; x < sizeof(maskKey); x++)
    {
      maskKey[x] = random(0xFF);
    }
  }

  createHeader(headerPtr, opcode, length, client->cIsClient, maskKey, fin);

  if (client->cIsClient && useInternBuffer)
  {
    uint8_t * dataMaskPtr;

    if (headerToPayload)
    {
      dataMaskPtr = (payloadPtr + WEBSOCKETS_MAX_HEADER_SIZE);
    }
    else
    {
      dataMaskPtr = payloadPtr;
    }

    for (size_t x = 0; x < length; x++)
    {
      dataMaskPtr[x] = (dataMaskPtr[x] ^ maskKey[x % 4]);
    }
  }

#ifndef NODEBUG_WEBSOCKETS
  unsigned long start = micros();
#endif

  if (headerToPayload)
  {
    // header has be added to payload
    // payload is forced to reserved 14 Byte but we may not need all based on the length and mask settings
    // offset in payload is calculatetd 14 - headerSize
    if (write(client, &payloadPtr[(WEBSOCKETS_MAX_HEADER_SIZE - headerSize)], (length + headerSize)) != (length + headerSize))
    {
      ret = false;
    }
  }
  else
  {
    // send header
    if (write(client, &buffer[0], headerSize) != headerSize)
    {
      ret = false;
    }

    if (payloadPtr && length > 0)
    {
      // send payload
      if (write(client, &payloadPtr[0], length) != length)
      {
        ret = false;
      }
    }
  }

#ifndef NODEBUG_WEBSOCKETS
  LOGDEBUG2(client->num, "[sendFrame] sending Frame Done (us):", (micros() - start));       
#endif  

#ifdef WEBSOCKETS_USE_BIG_MEM
  if (useInternBuffer && payloadPtr)
  {
    free(payloadPtr);
  }
#endif

  return ret;
}

/**
   callen when HTTP header is done
   @param client WSclient_t *  ptr to the client struct
*/
void WebSockets::headerDone(WSclient_t * client)
{
  client->status    = WSC_CONNECTED;
  client->cWsRXsize = 0;
  
  LOGDEBUG1(client->num, "[headerDone] Header Handling Done.");

#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
  client->cHttpLine = "";
  handleWebsocket(client);
#endif
}

/**
   handle the WebSocket stream
   @param client WSclient_t *  ptr to the client struct
*/
void WebSockets::handleWebsocket(WSclient_t * client)
{
  if (client->cWsRXsize == 0) 
  {
    handleWebsocketCb(client);
  }
}

/**
   wait for
   @param client
   @param size
*/
bool WebSockets::handleWebsocketWaitFor(WSclient_t * client, size_t size)
{
  if (!client->tcp || !client->tcp->connected())
  {
    return false;
  }

  if (size > WEBSOCKETS_MAX_HEADER_SIZE)
  {
    LOGDEBUG2(client->num, "[handleWebsocketWaitFor] size too big:", size);
    
    return false;
  }

  if (client->cWsRXsize >= size)
  {
    return true;
  }

  LOGDEBUG2(client->num, "[handleWebsocketWaitFor] size:", size); 
  LOGDEBUG1("cWsRXsize:", client->cWsRXsize);                  
                    
                    
  readCb(client, &client->cWsHeader[client->cWsRXsize], (size - client->cWsRXsize), 
      std::bind([](WebSockets * server, size_t size, WSclient_t * client, bool ok)
  {
    LOGDEBUG2(client->num, "[handleWebsocketWaitFor][readCb] size:", size); 
    LOGDEBUG1("ok:", ok);   

    if (ok)
    {
      client->cWsRXsize = size;
      server->handleWebsocketCb(client);
    }
    else
    {
      LOGDEBUG1(client->num, "[readCb] failed.");
      
      client->cWsRXsize = 0;
      // timeout or error
      server->clientDisconnect(client, 1002);
    }
  },
  this, size, std::placeholders::_1, std::placeholders::_2));
  return false;
}

void WebSockets::handleWebsocketCb(WSclient_t * client)
{
  if (!client->tcp || !client->tcp->connected())
  {
    return;
  }

  uint8_t * buffer = client->cWsHeader;

  WSMessageHeader_t * header = &client->cWsHeaderDecode;
  uint8_t * payload          = NULL;

  uint8_t headerLen = 2;

  if (!handleWebsocketWaitFor(client, headerLen))
  {
    return;
  }

  // split first 2 bytes in the data
  header->fin    = ((*buffer >> 7) & 0x01);
  header->rsv1   = ((*buffer >> 6) & 0x01);
  header->rsv2   = ((*buffer >> 5) & 0x01);
  header->rsv3   = ((*buffer >> 4) & 0x01);
  header->opCode = (WSopcode_t)(*buffer & 0x0F);
  buffer++;

  header->mask       = ((*buffer >> 7) & 0x01);
  header->payloadLen = (WSopcode_t)(*buffer & 0x7F);
  buffer++;

  if (header->payloadLen == 126)
  {
    headerLen += 2;

    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    header->payloadLen = buffer[0] << 8 | buffer[1];
    buffer += 2;
  }
  else if (header->payloadLen == 127)
  {
    headerLen += 8;

    // read 64bit integer as length
    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    if (buffer[0] != 0 || buffer[1] != 0 || buffer[2] != 0 || buffer[3] != 0)
    {
      // really too big!
      header->payloadLen = 0xFFFFFFFF;
    }
    else
    {
      header->payloadLen = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];
    }

    buffer += 8;
  }

  LOGDEBUG1(client->num, "[handleWebsocket] ------- read massage frame -------"); 
  LOGDEBUG2(client->num, "[handleWebsocket] fin:", header->fin); 
  LOGDEBUG1("rsv1:",    header->rsv1);
  LOGDEBUG1("rsv2:",    header->rsv2);  
  LOGDEBUG1("rsv3:",    header->rsv3);  
  LOGDEBUG1("opCode:",  header->opCode);               
                    
  LOGDEBUG2(client->num, "[handleWebsocket] mask:", header->mask); 
  LOGDEBUG1("payloadLen:", header->payloadLen);
  
  
  if (header->payloadLen > WEBSOCKETS_MAX_DATA_SIZE)
  {
    LOGDEBUG2(client->num, "[handleWebsocket] payload too big:", header->payloadLen); 
    
    clientDisconnect(client, 1009);
    return;
  }

  if (header->mask)
  {
    headerLen += 4;
    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    header->maskKey = buffer;
    buffer += 4;
  }

  if (header->payloadLen > 0)
  {
    // if text data we need one more
    payload = (uint8_t *)malloc(header->payloadLen + 1);

    if (!payload)
    {
      LOGDEBUG2(client->num, "[handleWebsocket] to less memory to handle payload", header->payloadLen); 
      
      clientDisconnect(client, 1011);
      return;
    }

    readCb(client, payload, header->payloadLen, std::bind(&WebSockets::handleWebsocketPayloadCb, 
            this, std::placeholders::_1, std::placeholders::_2, payload));
  }
  else
  {
    handleWebsocketPayloadCb(client, true, NULL);
  }
}

void WebSockets::handleWebsocketPayloadCb(WSclient_t * client, bool ok, uint8_t * payload)
{
  WSMessageHeader_t * header = &client->cWsHeaderDecode;

  if (ok)
  {
    if (header->payloadLen > 0)
    {
      payload[header->payloadLen] = 0x00;

      if (header->mask)
      {
        //decode XOR
        for (size_t i = 0; i < header->payloadLen; i++)
        {
          payload[i] = (payload[i] ^ header->maskKey[i % 4]);
        }
      }
    }

    switch (header->opCode)
    {
      case WSop_text:
        LOGDEBUG2(client->num, "[handleWebsocket] text:", (char *) payload); 
        
      // no break here!
      case WSop_binary:
      case WSop_continuation:
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
      case WSop_ping:
        // send pong back
        LOGDEBUG2(client->num, "[handleWebsocket] ping received", payload ? (const char *)payload : "");                   
                          
        sendFrame(client, WSop_pong, payload, header->payloadLen);
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
      case WSop_pong:
        LOGDEBUG2(client->num, "[handleWebsocket] get pong", payload ? (const char *)payload : "");                  
                          
        client->pongReceived = true;
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
      case WSop_close:
        {
#ifndef NODEBUG_WEBSOCKETS
          uint16_t reasonCode = 1000;

          if (header->payloadLen >= 2)
          {
            reasonCode = payload[0] << 8 | payload[1];
          }

          LOGDEBUG2(client->num, "[handleWebsocket] get ask for close. Code:", reasonCode);
#endif
          
          
          if (header->payloadLen > 2)
          {
            LOGDEBUG1("==>", (char *) (payload + 2));
          }

          clientDisconnect(client, 1000);
        } break;
      default:
        clientDisconnect(client, 1002);
        break;
    }

    if (payload)
    {
      free(payload);
    }

    // reset input
    client->cWsRXsize = 0;
#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
    //register callback for next message
    handleWebsocketWaitFor(client, 2);
#endif

  }
  else
  {
    LOGDEBUG1(client->num, "[handleWebsocket] missing data!");
    
    free(payload);
    clientDisconnect(client, 1002);
  }
}

/**
   generate the key for Sec-WebSocket-Accept
   @param clientKey String
   @return String Accept Key
*/
String WebSockets::acceptKey(String & clientKey)
{
  uint8_t sha1HashBin[20] = { 0 };

#ifdef ESP8266
  sha1(clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", &sha1HashBin[0]);
#elif defined(ESP32)
  String data = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  esp_sha(SHA1, (unsigned char *)data.c_str(), data.length(), &sha1HashBin[0]);
#else
  clientKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  SHA1_CTX ctx;
  SHA1Init(&ctx);
  SHA1Update(&ctx, (const unsigned char *)clientKey.c_str(), clientKey.length());
  SHA1Final(&sha1HashBin[0], &ctx);
#endif

  String key = base64_encode(sha1HashBin, 20);
  key.trim();

  return key;
}

/**
   base64_encode
   @param data uint8_t
   @param length size_t
   @return base64 encoded String
*/
String WebSockets::base64_encode(uint8_t * data, size_t length)
{
  size_t size   = ((length * 1.6f) + 1);
  char * buffer = (char *)malloc(size);

  if (buffer)
  {
    base64_encodestate _state;
    base64_init_encodestate(&_state);
    int len = base64_encode_block((const char *)&data[0], length, &buffer[0], &_state);
    len     = base64_encode_blockend((buffer + len), &_state);

    String base64 = String(buffer);
    free(buffer);
    return base64;
  }
  return String("-FAIL-");
}

/**
   read x byte from tcp or get timeout
   @param client WSclient_t
   @param out  uint8_t * data buffer
   @param n size_t byte count
   @return true if ok
*/
bool WebSockets::readCb(WSclient_t * client, uint8_t * out, size_t n, WSreadWaitCb cb)
{
#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
  if (!client->tcp || !client->tcp->connected())
  {
    return false;
  }

  client->tcp->readBytes(out, n, std::bind([](WSclient_t * client, bool ok, WSreadWaitCb cb)
  {
    if (cb)
    {
      cb(client, ok);
    }
  },
  client, std::placeholders::_1, cb));

#else
  unsigned long t = millis();
  ssize_t len;

  LOGDEBUG1("[readCb] n:", n);
  LOGDEBUG1("t:", t);

  while (n > 0)
  {
    if (client->tcp == NULL)
    {
      LOGDEBUG("[readCb] tcp is null!");

      if (cb)
      {
        cb(client, false);
      }
      return false;
    }

    if (!client->tcp->connected())
    {
      LOGDEBUG("[readCb] not connected!");

      if (cb)
      {
        cb(client, false);
      }
      return false;
    }

    if ((millis() - t) > WEBSOCKETS_TCP_TIMEOUT)
    {
      LOGDEBUG1("[readCb] receive TIMEOUT (ms):", (millis() - t));

      if (cb)
      {
        cb(client, false);
      }
      return false;
    }

    if (!client->tcp->available())
    {
      WEBSOCKETS_YIELD();
      continue;
    }

    len = client->tcp->read((uint8_t *)out, n);

    if (len > 0)
    {
      t = millis();
      out += len;
      n -= len;
    }
    
    LOGDEBUG1("[readCb] Receive Length :", len);
    LOGDEBUG1("[readCb] Left :", n);
      
    WEBSOCKETS_YIELD();
  }

  if (cb)
  {
    cb(client, true);
  }
  WEBSOCKETS_YIELD();
#endif
  return true;
}

/**
   write x byte to tcp or get timeout
   @param client WSclient_t
   @param out  uint8_t * data buffer
   @param n size_t byte count
   @return bytes send
*/
size_t WebSockets::write(WSclient_t * client, uint8_t * out, size_t n)
{
  if (out == NULL)
    return 0;

  if (client == NULL)
    return 0;

  unsigned long t = millis();
  size_t len      = 0;
  size_t total    = 0;

  LOGDEBUG1("[write] n:", n);
  LOGDEBUG1("t:", t);

  while (n > 0)
  {
    if (client->tcp == NULL)
    {
      LOGDEBUG("[write] tcp is null!");
      break;
    }

    if (!client->tcp->connected())
    {
      LOGDEBUG("[write] not connected!");
      break;
    }

    if ((millis() - t) > WEBSOCKETS_TCP_TIMEOUT)
    {
      LOGDEBUG1("[write] receive TIMEOUT (ms):", (millis() - t));
      break;
    }

    len = client->tcp->write((const uint8_t *)out, n);

    if (len)
    {
      t = millis();
      out += len;
      n -= len;
      total += len;

      LOGDEBUG1("[write] Write Length :", len);
      LOGDEBUG1("[write] Left :", n);
    }
    else
    {
      LOGDEBUG1("[write] Failed Write Length :", len);
      LOGDEBUG1("[write] Left :", n);
    }
       
    WEBSOCKETS_YIELD();
  }
  
  WEBSOCKETS_YIELD();
  return total;
}

size_t WebSockets::write(WSclient_t * client, const char * out)
{
  if (client == NULL)
    return 0;

  if (out == NULL)
    return 0;

  return write(client, (uint8_t *)out, strlen(out));
}

/**
   enable ping/pong heartbeat process
   @param client WSclient_t
   @param pingInterval uint32_t how often ping will be sent
   @param pongTimeout uint32_t millis after which pong should timout if not received
   @param disconnectTimeoutCount uint8_t how many timeouts before disconnect, 0=> do not disconnect
*/
void WebSockets::enableHeartbeat(WSclient_t * client, uint32_t pingInterval, uint32_t pongTimeout, uint8_t disconnectTimeoutCount)
{
  if (client == NULL)
    return;

  client->pingInterval           = pingInterval;
  client->pongTimeout            = pongTimeout;
  client->disconnectTimeoutCount = disconnectTimeoutCount;
  client->pongReceived           = false;
}

/**
   handle ping/pong heartbeat timeout process
   @param client WSclient_t
*/
void WebSockets::handleHBTimeout(WSclient_t * client)
{
  if (client->pingInterval)
  {
    // if heartbeat is enabled
    uint32_t pi = millis() - client->lastPing;

    if (client->pongReceived)
    {
      client->pongTimeoutCount = 0;
    }
    else
    {
      if (pi > client->pongTimeout)
      {
        // pong not received in time
        client->pongTimeoutCount++;
        client->lastPing = millis() - client->pingInterval - 500;    // force ping on the next run

        LOGDEBUG1("[HBtimeout] pong TIMEOUT! lp=", client->lastPing); 
        LOGDEBUG1("millis:",  millis());
        LOGDEBUG1("pi:",      pi);  
        LOGDEBUG1("count:",   client->pongTimeoutCount);  
               

        if (client->disconnectTimeoutCount && client->pongTimeoutCount >= client->disconnectTimeoutCount)
        {
          LOGDEBUG1("[HBtimeout] DISCONNECTING, count=", client->pongTimeoutCount); 
          
          clientDisconnect(client);
        }
      }
    }
  }
}

#endif  //WEBSOCKETS_GENERIC_IMPL_H_
