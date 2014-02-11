## Arduino Websocket Server for Door Controller Test Tool

This library implements a Websocket server running on an Arduino. It's based on the [proposed standard][1] published December 2011 which is supported in the current versions (January 2014) of Firefox, Chrome, and Safari.

The library is used by the Door Controller Test Tool.

### Limitations

The implementation in this library has restrictions as the Arduino platform resources are very limited:

* The server **only** handles TXT frames.
* The server **only** handles **single byte** chars. The Arduino just can't handle UTF-8 to it's full.
* The server **only** accepts **final** frames with maximum payload length of 256 bytes. No fragmented data, in other words.
* For now, the server silently ignores all frames except TXT, CLOSE and PONG.
* The server **only** handles one client at a time. 

_Required headers (example):_

	GET /whatever HTTP/1.1
	Host: server.example.com
	Upgrade: websocket
	Connection: Upgrade
	Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
	Origin: http://example.com
	Sec-WebSocket-Version: 13

The server checks that all of these headers are present, but only cares that the version is 13 and origin is not null.

_Response example:_

	HTTP/1.1 101 Switching Protocols
	Upgrade: websocket
	Connection: Upgrade
	Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

The last line is the Base64 encoded SHA-1 hash of the key with a concatenated GUID, as specified by the standard.

### Requirements

* Arduino IDE 1.0.1 or greater. You should not use 1.0 since it has a bug in the Ethernet library that will affect this library.
* An Arduino Duemilanove or greater with Ethernet shield. An Arduino Ethernet should work too, but it has not been tested.
* A Websocket client that conforms to version 13 of the protocol.

[1]: http://datatracker.ietf.org/doc/rfc6455/?include_text=1 "Protol version implemented here"
