/*
    Http request each 5 minutes
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <winsock2.h>
#include "sha1.h"
#include "functions.h"

int main(int argc, char *argv[])
{

    // Handle parameters
    if ( argc != 3 ) {
        printf("Usage: tool <--upload|--dump> <torrent_destination>\n");
        exit(EXIT_FAILURE);
    }

    if ( strcmp(argv[1], "--upload") == 0 ) {
        printf("Let's upload !\n");
    }
    else if ( strcmp(argv[1], "--dump") == 0 ) {
        printf("Let's dump this clients\n");
    }
    else {
        printf("Usage: tool <upload|dump> <torrent_destination>\n");
        exit(EXIT_FAILURE);
    }

    // File open/read var
    HANDLE fHandle;
    int sizeReturn = 0;
    char *bufferAddr;
    int returnTmp = 0;
    int bytesReaded = 0;

    // Reading .torrent file
    fHandle = CreateFile(argv[2],
                             GENERIC_READ,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL
                             );
    if ( fHandle == INVALID_HANDLE_VALUE ) {
        printf("Unable to open the file: 0x%x\n", (unsigned int)GetLastError());
        return EXIT_FAILURE;
    }

    sizeReturn = GetFileSize(fHandle,
                                 NULL
                                );

    if ( sizeReturn == INVALID_FILE_SIZE ) {
        printf("Unable to get size of the file: 0x%x\n", (unsigned int)GetLastError());
        return EXIT_FAILURE;
    }

    bufferAddr = (char *) calloc(sizeReturn, sizeof(char));

    if ( bufferAddr == NULL ) {
        printf("Unable to alloc buffer memory\n");
        return EXIT_FAILURE;
    }

    returnTmp = ReadFile(fHandle,
                         bufferAddr,
                         sizeReturn,
                         (LPDWORD)&bytesReaded,
                         NULL
                         );
    if ( returnTmp == 0 ) {
        printf("Unable to read the file: 0x%x\n", (unsigned int)GetLastError());
        return EXIT_FAILURE;
    }

    int keyVal_length = -1;
    char *announce_text = (char *) calloc(sizeReturn, sizeof(char));

    printf("Initialize file parsing context...\n");
    fileCtx *ctx = (fileCtx *) calloc(6, sizeof(fileCtx));
    ctx->stack = (char *) calloc(128, sizeof(char)); // Allocate 128 bytes for stack buffer
    ctx->bytesLimit = sizeReturn;
    ctx->raw = 0; // Set the raw flag to 1, then the return of the function is the raw data without excluding data information => key:[value] => bytes:[string] and not key:[value] => [bytes:string]

    keyVal_length = bencodeRead( bufferAddr, ctx, "announce", announce_text );
    if ( keyVal_length == 0 ) { 
        printf("No announce key\n");
		return EXIT_FAILURE;
    }

    
    free(ctx->stack); // Free the memory allocated for our stack
    free(ctx); // And free the memory allocated for the context structure

    // Socket part
    int retVar=0;
    WSADATA wsa;
    retVar = WSAStartup(MAKEWORD(2, 2), &wsa);
    if ( retVar != 0 ) {
        printf("Unable to WSAStartup: 0x%x\n", retVar);
        return EXIT_FAILURE;
    }

	protocolInfo *pInfo = (protocolInfo *) calloc(5, sizeof(protocolInfo));
	fillProtocolInfo(announce_text, pInfo);
	printf("Protocol: %s -- IP: 0x%x -- Port: %d\n", pInfo->protocol == 15 ? "HTTP" : "UDP", pInfo->ip, pInfo->port);
	
	free(announce_text);
	
    SOCKADDR_IN ProtocolSocket;
	SOCKET trackerSocket;
	
	if ( pInfo->protocol == HTTP ) {
		trackerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // HTTP Over TCP
		printf("Initializing TCP socket\n");
	}
	else if ( pInfo->protocol == UDP ) {
		trackerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		printf("Initializing UDP socket\n");
	}
    if ( trackerSocket == INVALID_SOCKET ) {
        printf("Socket error: 0x%x\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    printf("Socket created !\n");

    ProtocolSocket.sin_addr.s_addr = pInfo->ip;
    printf("Ip resolved: %s\n", inet_ntoa(ProtocolSocket.sin_addr));

    ProtocolSocket.sin_family = AF_INET;
    ProtocolSocket.sin_port = htons(pInfo->port);

    retVar = connect(trackerSocket, 
                    (SOCKADDR *)&ProtocolSocket, 
                    sizeof(ProtocolSocket));
    if ( retVar != 0 ) {
        closesocket(trackerSocket);
        printf("Error on connecting socket: 0x%x\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("Connected to %s:%d !\n", inet_ntoa(ProtocolSocket.sin_addr), pInfo->port);
	
	// Fill the basic needed things structure
	fileCtx *ctx_ = (fileCtx *) calloc(7, sizeof(fileCtx));
	ctx_->stack = (char *) calloc(128, sizeof(char)); // Allocate 128 bytes for stack buffer
	ctx_->bytesLimit = sizeReturn;
	ctx_->raw = 1; // Set the raw flag to 1, then the return of the function is the raw data without excluding data information => key:[value] => bytes:[string] and not key:[value] => [bytes:string]
	char *raw_info = (char *) calloc(sizeReturn, sizeof(char));

	keyVal_length = bencodeRead( bufferAddr, ctx_, "info", raw_info );
	if ( keyVal_length == 0 ) {
		printf("No info key found\n");
		return EXIT_FAILURE;
	}
	
	sha1nfo info_hashStruct;
	sha1_init(&info_hashStruct);
	sha1_write(&info_hashStruct, raw_info, keyVal_length);
	uint8_t *uint_info_hash = sha1_result(&info_hashStruct);
	
	free(raw_info);
	
	if ( uint_info_hash == NULL ) {
		printf("Unable to hash info\n");
		return EXIT_FAILURE;
	}
		
	torrentBuild *tBuild = (torrentBuild *) calloc(2, sizeof(torrentBuild));
	memcpy(tBuild->info_hash, uint_info_hash, 20);
	buildPeerId(tBuild->peer_id);
	
	free(bufferAddr); // Free the memory allocated for the .torrent file content, because we don't care of it now
	
	/* Let's play */
	
	if ( pInfo->protocol == UDP ) {
		printf("UDP Place !\n");
		UDPProtocolTracker_Upload( trackerSocket, tBuild );
	}
	else if ( pInfo->protocol == HTTP ) {
		printf("HTTP Place !\n");
		HTTPProtocolTracker_Upload( trackerSocket, tBuild, pInfo );
	}
	else {
		printf("No protocol\n");
		return(EXIT_FAILURE);
	}

    // End socket connection
	free(pInfo);
    closesocket(trackerSocket);
    printf("Socket closed\n");
    WSACleanup();
    printf("WSA Cleaned\n");

    getchar();
    return 0;
}