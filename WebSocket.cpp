#include "WebSocket.h"
#include "sha1.h"
#include "Base64.h"
#include <StandardCplusplus.h>
#include <serstream>

using namespace std;

struct Frame {
    bool isMasked;
    bool isFinal;
    byte opcode;
    byte mask[4];
    byte length;
    char data[MAX_RECEIVE_MESSAGE_SIZE];
} frame;

WebSocket::WebSocket(const char *urlPrefix, int inPort) :
    server(inPort),
    socket_urlPrefix(urlPrefix)
{
    state = DISCONNECTED;
    onConnect = NULL;
    onData = NULL;
    onDisconnect = NULL;
    onHeartbeatResponse = NULL;
}

void WebSocket::begin() {
    server.begin();      
}

void WebSocket::listen() {
    
    if (server.available()) {
        // No active connection available. Treat this as a handshake request.
        if (state == DISCONNECTED) {
            bool handshakeSuccessful = processHandshake();
            
            if (handshakeSuccessful) {
                state = CONNECTED;                    
                // Call the registered callback.
                if (onConnect) {
                    onConnect(*this);
                }
            } else {
                cout << F("WEBSOCKET: Handshake unsuccessful.\n");
            }
        } 
        else if (state == CONNECTED || state == CLOSING) {
            bool validFrame = getFrame();

            // We check the frame to see that it was valid, and disconnect
            // if it was not.
            if (!validFrame) {                
            	cout << F("WEBSOCKET: Got invalid frame. Disconnecting.\n");
                disconnect();
            }
        }
    }
}

bool WebSocket::isConnected() {
	return (state == CONNECTED) ? true : false;
}

void WebSocket::disconnect() {
    client.flush();
    delay(1);
    client.stop();

    state = DISCONNECTED;

    // Call the registered callback.
    if (onDisconnect) {
        onDisconnect(*this);
    }
}

bool WebSocket::processHandshake() {
    char temp[128];
    char key[80];
    char bite;
    
    bool hasUpgrade = false;
    bool hasConnection = false;
    bool isSupportedVersion = false;
    bool hasHost = false;
    bool hasOrigin = false;
    bool hasKey = false;

    byte counter = 0;
    
    cout << F("WEBSOCKET: Processing handshake\n");
    while ((bite = client.read()) != -1) {
        temp[counter++] = bite;

        if (bite == '\n' || counter > 127) { // EOL got, or too long header. temp should now contain a header string
            temp[counter - 2] = 0; // Terminate string before CRLF                    

            // Ignore case when comparing and allow 0-n whitespace after ':'. See the spec:
            // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html
            if (!hasUpgrade && strstr(temp, "Upgrade:")) {
                // OK, it's a websockets handshake for sure
                hasUpgrade = true;	
            } else if (!hasConnection && strstr(temp, "Connection: ")) {
                hasConnection = true;
            } else if (!hasOrigin && strstr(temp, "Origin:")) {
                hasOrigin = true;
            } else if (!hasHost && strstr(temp, "Host: ")) {
                hasHost = true;
            } else if (!hasKey && strstr(temp, "Sec-WebSocket-Key: ")) {
                hasKey = true;
                strtok(temp, " ");
                strcpy(key, strtok(NULL, " "));
            } else if (!isSupportedVersion && strstr(temp, "Sec-WebSocket-Version: ") && strstr(temp, "13")) {
                isSupportedVersion = true;
            }
            
            cout << temp;
            counter = 0; // Start saving new header string
        }
    }

    if (!hasUpgrade)
        cout << F("Handshake has no \"Upgrade\" entry.");
    if (!hasConnection)
        cout << F("Handshake has no \"Connection\" entry.");
    if (!isSupportedVersion)
        cout << F("Handshake has no \"Sec-Websocket-Version\" entry.");
    if (!hasHost)
        cout << F("Handshake has no \"Host\" entry.");
    if (!hasOrigin)
        cout << F("Handshake has no \"Origin\" entry.");
    if (!hasKey)
        cout << F("Handshake has no \"Sec-Websocket-Key\" entry.");


    // Assert that we have all headers that are needed. If so, go ahead and
    // send response headers.
    if (hasUpgrade && hasConnection && isSupportedVersion && hasHost && hasOrigin && hasKey) {
        strcat(key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"); // Add the omni-valid GUI
        Sha1.init();
        Sha1.print(key);
        uint8_t *hash = Sha1.result();
        base64_encode(temp, (char*)hash, 20);
        client.print("HTTP/1.1 101 Switching Protocols\r\n");
        client.print("Upgrade: websocket\r\n");
        client.print("Connection: Upgrade\r\n");
        client.print("Sec-WebSocket-Accept: ");
        client.print(temp);
        client.print(CRLF);
        client.print(CRLF);
    } else {
        // Nope, failed handshake. Disconnect
        return false;
    }
    
    return true;
}


bool WebSocket::getFrame() {
    byte bite;
    unsigned short payloadLength = 0;
    
    bite = client.read();        
    frame.opcode = bite & 0xf; // Opcode
    frame.isFinal = bite & 0x80; // Final frame?
    // Determine length (only accept <= 126, i.e. maximum 65535 bytes. Need to cap this in-code.)
    bite = client.read();
    frame.length = bite & 0x7f; // Length of payload        
    frame.isMasked = bite & 0x80;

    // Frame complete!
    if (!frame.isFinal) {
        // We don't handle fragments! Close and disconnect.
        cout << F("WEBSOCKET: Non-final frames not supported.");
        close(WS_POLICY_VIOLATION, "Non-final frames not supported.");

        return false;
    }

    // First check if the frame size is within our limits.
    if (frame.length > 126) {
        if (frame.length == 127) {
            cout << F("WEBSOCKET: Buffer only allows a message with a payload size of ")
                 << MAX_RECEIVE_MESSAGE_SIZE;
        } else if (frame.length > 127) {
            cout << F("WEBSOCKET: Bad payload-length field value: ")
                 << frame.length;
        }

        close(WS_PAYLOAD_TOO_BIG, "Maximum payload is 1024 bytes.");
       
        return false;
    }

    // If the length part of the header is 126, it means it contains an extended length field.
    // Next two bytes contain the actual payload size, so we need to get the "true" length.
     if (frame.length == 126) {
        byte exLengthByte1 = client.read();
        byte exLengthByte2 = client.read();
        payloadLength = (exLengthByte1 << 8) + exLengthByte2;
     } 
     // If frame length is less than 126, that is the size of the payload.
     else {
        payloadLength = frame.length;        
     }

     // Check if our buffer can store the payload.
     if (payloadLength > MAX_RECEIVE_MESSAGE_SIZE) {
        cout << F("WEBSOCKET: The frame has a payload size of ")
             << payloadLength << F(", but buffer size can only store ")
             << MAX_RECEIVE_MESSAGE_SIZE << F(" bytes.\n");

        close(WS_PAYLOAD_TOO_BIG, "Maximum payload is 2048 bytes.");
        
        return false;
     }

    // Client should always send mask, but check just to be sure    
    if (frame.isMasked) {
        frame.mask[0] = client.read();
        frame.mask[1] = client.read();
        frame.mask[2] = client.read();
        frame.mask[3] = client.read();
    }
    
    // Get message bytes and unmask them if necessary
    for (int i = 0; i < payloadLength; i++) {
        if (frame.isMasked) {
            frame.data[i] = client.read() ^ frame.mask[i % 4];
        } else {
            frame.data[i] = client.read();
        }
    }

    switch (frame.opcode) {
        
        case FRAME_OP_PONG:            
            if (onHeartbeatResponse && (strstr(frame.data, HEARTBEAT_MESSAGE) != NULL)) {            
                onHeartbeatResponse(*this);
            }
            break;

        case FRAME_OP_TEXT:
            // Call the user provided function
            if (onData) {
                onData(*this, frame.data, payloadLength);
            }
            break;
            
        case FRAME_OP_CLOSE:

            // If we are already closing, the received close frame is hopefully a reply to a previous
            // close frame we sent. 
            // TODO: Check that the received close frame is a copy of ours.
            if (state == CLOSING) {
                disconnect();
                cout << F("WEBSOCKET: Close response received. Terminating connection.\n");    
            }
            // In this case, the client wants to close, so we echo back the close frame and 
            // disconnect.     
            else {
                cout << F("WEBSOCKET: Close frame received. Closing in answer.\n");                
                // TODO: How do we echo back the close frame?
                // First two bytes in payload is a two-byte close code.
                uint16_t closeStatusCode = frame.data[0] << 8 | frame.data[1];
                close(closeStatusCode, "");
                disconnect();
            }
            
            break;

        //Unhandled
        case FRAME_OP_CONT:
        case FRAME_OP_BINARY:
        case FRAME_OP_PING:        
            cout << F("WEBSOCKET: Unsupported frame.\n");
            return false;
            
        default:
            // Unexpected.
    		cout << F("WEBSOCKET: Unhandled frame.\n");
			return false;
    }
    return true;
}


void WebSocket::registerConnectCallback(Callback *callback) {
    onConnect = callback;
}
void WebSocket::registerDataCallback(DataCallback *callback) {
    onData = callback;
}
void WebSocket::registerDisconnectCallback(Callback *callback) {
    onDisconnect = callback;
}
void WebSocket::registerHeartbeatResponseCallback(Callback *callback) {
    onHeartbeatResponse = callback;
}

void WebSocket::close(uint16_t statusCode, char *reason) {    
    
    client.write(FRAME_OP_CLOSE);
    if (statusCode) {
        client.write(statusCode);
    }
    if (reason) {
        for (int i = 0; i < strlen(reason); i++) {
            client.write(reason[i]);            
        }
    }
    state = CLOSING;
    cout << F("Close frame sent.\n");
}

void WebSocket::gracelessClose(uint16_t statusCode, char *reason) {
    close(statusCode, reason);
    disconnect();
    cout << F("Connection terminated without waiting for close-frame reply from client.\n");
}

bool WebSocket::sendMessage(char *payload, uint16_t payloadLength) {
    send(FRAME_OP_TEXT, payload, payloadLength);
}

bool WebSocket::sendHeartbeat() {
    ping(HEARTBEAT_MESSAGE, strlen(HEARTBEAT_MESSAGE));
}


bool WebSocket::send(uint8_t opCode, char *payload, uint16_t payloadLength) {
    if (state == CONNECTED) {
        //
        // First byte, always FIN, as we don't support anything else.
        //
        server.write((uint8_t)FRAME_FLAG_FIN | opCode); 
        
        //
        // Second byte, mask and length of payload.            
        //
        
        // If no payload, make sure the length of the payload is 0.
        if (payload == NULL) {
            payloadLength = 0;
        }

        // If payload size is 125 or less, that is the length we add in the  payload-length header field.
        if (payloadLength <= 125) {
            server.write((uint8_t) payloadLength);
        }
        // If payload size is between 126 and 65535, 126 is the value to add in the payload-length header field.
        // In this case, the following 2 bytes in the header contain the  actual payload length. 
        else if ((payloadLength >= 126) && (payloadLength <= 65535)) {
            server.write(126);
            byte exLengthByte2 = (byte)(payloadLength & 0xFF);
            byte exLengthByte1 = (byte)((payloadLength >> 8) & 0xFF);            
            server.write(exLengthByte1);
            server.write(exLengthByte2);
        }
        // We definately don't handle messages that are bigger than 65535.
        else {
            cout << F("WEBSOCKET: Message is too big to send.")
                 << F("Only messages up to 65535 bytes in size are supported.\n");            
            return false;
        }                
        
        //
        // Header is finished, now we write the actual payload.
        //
        for (int i = 0; i < payloadLength ; i++) {
            server.write(payload[i]);            
        }
        return true;
    }
    cout << F("WEBSOCKET: No connection to client, no data sent.\n");   
    return false;
}

void WebSocket::ping(char *payload, uint16_t payloadLength) {
    send(FRAME_OP_PING, payload, payloadLength);
}
void WebSocket::pong(char *payload, uint16_t payloadLength) {
    send(FRAME_OP_PONG, payload, payloadLength);
}