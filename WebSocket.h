/*
Websocket-Arduino, a simple websocket implementation for Arduino
Copyright 2012 Per Ejeklint

Based on previous implementations by
Copyright 2010 Ben Swanson
and
Copyright 2010 Randall Brewer
and
Copyright 2010 Oliver Smith

Some code and concept based off of Webduino library
Copyright 2009 Ben Combee, Ran Talbott

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

-------------
Now based off version 13
http://datatracker.ietf.org/doc/rfc6455/?include_text=1

Modified by Alexandros Baltas, 2013
www.codebender.cc
-------------

Modified by Axis Communications, 2014.
*/

#include <Arduino.h> // Arduino 1.0 or greater is required
#include <stdlib.h>
#include <SPI.h>
#include <Ethernet.h>

#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#define CRLF "\r\n" // CRLF characters to terminate lines/handshakes in headers.

// Status codes for close frames; see spec for details.
#define WS_NORMAL_CLOSE 1000
#define WS_ENDPOINT_GOING_AWAY 1001
#define WS_PROTOCOL_ERROR 1002
#define WS_PAYLOAD_NOT_ACCEPTABLE 1003
#define WS_RESERVED 1004
#define WS_RESERVED_NO_STATUS_CODE 1005
#define WS_RESERVED_ABNORMAL_CLOSE 1006
#define WS_MALFORMED_PAYLOAD 1007
#define WS_POLICY_VIOLATION 1008
#define WS_PAYLOAD_TOO_BIG 1009
#define WS_EXTENSION_REQUIRED 1010
#define WS_UNEXPECTED_CONDITION 1011
#define WS_RESERVED_TLS_FAILURE 1015

// Frame header flags.
#define FRAME_FLAG_FIN 0x80 // FIN bit. Is this the final fragment of a multi-fragment message?

//Frame header opcodes.
#define FRAME_OP_CONT 0x00 // Continuation frame
#define FRAME_OP_TEXT 0x01 // Text frame
#define FRAME_OP_BINARY 0x02 // Binary frame
#define FRAME_OP_CLOSE 0x08 // Close connection
#define FRAME_OP_PING 0x09 // Ping frame
#define FRAME_OP_PONG 0x0A // Pong frame
#define FRAME_OP_BITMASK 0x0F // Bit mask for opcodes

#define MAX_RECEIVE_MESSAGE_SIZE 256

#define HEARTBEAT_MESSAGE "Heartbeat"

class WebSocket {
    public:
        // Constructor.
        WebSocket(const char *urlPrefix = "/", int inPort = 80);
        
        // Callback functions definition.
        typedef void DataCallback(WebSocket &socket, char* socketString, unsigned short frameLength);
        typedef void Callback(WebSocket &socket);
        
        // Start listening for connections.
        void begin();
        
        // Main listener for incoming data. Should be called from the loop.
        void listen();
        
        void close(uint16_t, char*);
        void gracelessClose(uint16_t, char*);

        // Callbacks
        void registerDataCallback(DataCallback *callback);
        void registerConnectCallback(Callback *callback);
        void registerDisconnectCallback(Callback *callback);
        void registerHeartbeatResponseCallback(Callback *callback);
        
    	// Are we connected?
    	bool isConnected();
    	
        bool sendMessage(char*, uint16_t);
        bool sendHeartbeat();

        void ping(char*, uint16_t);
        void pong(char*, uint16_t);
        bool send(uint8_t opCode, char*, uint16_t);

    private:
        EthernetServer server;
        EthernetClient client;
        enum State {DISCONNECTED, CONNECTED, CLOSING} state;

        const char *socket_urlPrefix;

        // Pointer to the callback function the user should provide
        DataCallback *onData;
        Callback *onConnect;
        Callback *onDisconnect;
        Callback *onHeartbeatResponse;

        // Discovers if the client's header is requesting an upgrade to a
        // websocket connection.
        bool processHandshake();
        
        // Reads a frame from client. Returns false if user disconnects, 
        // or unhandled frame is received. Server must then disconnect, or an error occurs.
        bool getFrame();

        // Disconnect user gracefully.
        void disconnect();
};

#endif