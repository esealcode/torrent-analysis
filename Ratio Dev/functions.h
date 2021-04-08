#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>

#define FILE_EOF 0x42
#define HTTP 15
#define UDP 16

typedef int bool;

typedef struct http {
    char *httpHeader;
    int sizeOfHeader;
    char *httpPost;
    int sizeOfPost;
} http;


typedef struct fileCtx {
    int cObjectType; // Current object type: dictionnary/list/none
    int stackPtr; // Pointer to the end of the stack
    char *stack; // Stack buffer
    int fileOffset;
    int byteCopied; // Number of bytes copied
    int bytesLimit; // Maximum of bytes to read/copy
    bool raw;
} fileCtx;


typedef struct protocolInfo {
	u_long ip; // 00.00.00.00 32 bits
	int port; // HTTP = 80, UDP = ?
	int protocol; // 0 = HTTP, 1 = UDP
	char *path;
	char *hostname;
} protocolInfo;


typedef struct UDPConnectPacket {
	int64_t connection_id;
	int action;
	int transaction_id;
} UDPConnectPacket;

typedef struct torrentBuild {
	unsigned char info_hash[21];
	unsigned char peer_id[21];
} torrentBuild;

typedef struct UDPAnnouncePacket {
	int64_t connection_id;
	int action;
	int transaction_id;
	char info_hash[20];
	char peer_id[20];
	int64_t downloaded;
	int64_t left;
	int64_t uploaded;
	int event;
	int ip_addr;
	int key;
	int num_want;
	int16_t port;
} UDPAnnouncePacket;


typedef struct HTTPAnnouncePacket {
    char info_hash[20]; // SHA1 hash of info in .torrent file : 20 bytes of SHA1 + 1 null byte
    char peer_id[20]; // Random ID generated
    char ip[16]; // 000.000.000.000 + null byte
    char port[6]; // Peer port : 00000 + null byte
    char uploaded[32]; // Data uploaded since last update
    char downloaded[32]; // Data downloaded since last update
    char left[32]; // Data which we still have to download
    char event[24]; // Event
} HTTPAnnouncePacket;


// Tracker Communication
int HTTPProtocolTracker_Upload( SOCKET trackerSocket, torrentBuild *tBuild, protocolInfo *pInfo );
/* */
int UDPProtocolTracker_Upload( SOCKET trackerSocket, torrentBuild *tBuild );
/* */
int HTTPProtocolTracker_Dump( SOCKET trackerSocket, torrentBuild *tBuild );
/* */
int UDPProtocolTracker_Dump( SOCKET trackerSocket, torrentBuild *tBuild );


// Build functions
void buildPeerId( char *returnLocation ); /* Return nothing, just write generated peer_id in buffer passed to the function */
int urlEncode( unsigned char *baseUrl, unsigned char *urlEncodedUrl, int sizeOfUrl ); /* Return size of data and write encoded url in the new buffer */
int fillProtocolInfo( char *url, protocolInfo *pInfo ); /* Return 1 if succeed, -1 if the url does not have good format */
int unChunckPOST( http *httpStruct ); /* Return error/success */
unsigned int strHex32ToInt( char *hex ); /* Return an unsigned int which is the 32bits hex value converted */


// Check functions
int isChunked( http *httpStruct ); /* Return 1 if YES, 0 if NO */


// Tracker HTTP Request
int HTTPTrackerResponseFillHeaderPOST( char *bufferAddr, http* httpStruct, int sizeData ); /* Return the POST data Size */


// .torrent file parsing functions
int bencodeRead( char *bufferAddr, fileCtx *ctx, const char *key, char *returnValueBuffer ); /* Return number of bytes writed */
/* */
int readString( char *bufferAddr, fileCtx *ctx, char *returnLocation ); /* Return size of the string */
/* */
int readInteger( char *bufferAddr, fileCtx *ctx ); /* Return an int which is the Integer readed */
/* */
void incOffset( fileCtx *ctx ); /* Return nothing, just check overflows */

#endif