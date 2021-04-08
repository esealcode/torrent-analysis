#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include "functions.h"

int HTTPProtocolTracker_Upload( SOCKET trackerSocket, torrentBuild *tBuild, protocolInfo *pInfo ) {
    // Simple HTTP Request
	char GET_data[2083] = {0};
	unsigned char urlEncodedInfo_hash[128] = {0}, urlEncodedPeer_Id[128] = {0};
	int getPtr = 0, urlEncodedInfo_hash_length = 0, urlEncodedPeer_Id_length = 0;
	
	// Get urlencoded binary data
	urlEncodedInfo_hash_length = urlEncode(tBuild->info_hash, urlEncodedInfo_hash, 20);
	urlEncodedPeer_Id_length = urlEncode(tBuild->peer_id, urlEncodedPeer_Id, 20);
	
	getPtr += sprintf(GET_data, "?info_hash=%s", urlEncodedInfo_hash);
	getPtr += sprintf(GET_data+getPtr, "&peer_id=%s", urlEncodedPeer_Id);
	getPtr += sprintf(GET_data+getPtr, "&port=6889&uploaded=0&downloaded=0&left=0&compact=1&event=started&numwant=1000");
	
    char httpData[4096] = {0};
	
	sprintf(httpData, "GET %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: BitTorrent/795(254518154)(41866)\r\nAccept-Encoding: gzip\r\nConnection: Close\r\n\r\n", pInfo->path, GET_data, pInfo->hostname);
	printf("Request: %s\n", httpData);
    char recvBuffer[4096] = {0};

    printf("Sending HTTP Request\n");
	
    int retVar = send(trackerSocket, 
                  httpData, 
                  strlen(httpData), 
                  0);

    if ( retVar == SOCKET_ERROR ) {
        printf("Unable to send packets: 0x%x\n", WSAGetLastError());
        return -1;
    }

    int sizeData = recv(trackerSocket, 
                        recvBuffer, 
                        4096, 
                        0);

    if ( sizeData == SOCKET_ERROR || sizeData == 0 ) {
        printf("Unable to receive packets: 0x%x\n", WSAGetLastError());
        closesocket(trackerSocket);
        return -1;
    }
    http *httpResponse = (http *) calloc(4, sizeof(http));
    int POSTSize = HTTPTrackerResponseFillHeaderPOST(recvBuffer, httpResponse, sizeData);
	printf("Start Announce HTTP Request done ! :)\n");
	unChunckPOST(httpResponse);
	
	fileCtx *postCtx = (fileCtx *) calloc(7, sizeof(fileCtx));
	postCtx->stack = (char *) calloc(128, sizeof(char)); // Allocate 128 bytes for stack buffer
	postCtx->bytesLimit = httpResponse->sizeOfPost;
	postCtx->raw = 0; // Set the raw flag to 1, then the return of the function is the raw data without excluding data information => key:[value] => bytes:[string] and not key:[value] => [bytes:string]
	char *binaryPeers = (char *) calloc(httpResponse->sizeOfPost, sizeof(char));
	int binaryPeers_KeyLength = bencodeRead( httpResponse->httpPost, postCtx, "peers", binaryPeers );

	if ( binaryPeers_KeyLength == 0 ) {
		printf("No peers key/value found\n");
		return EXIT_FAILURE;
	}
	
	int  connectedPeers = binaryPeers_KeyLength/6, i = 0;
	uint32_t peerIP = 0;
	uint16_t peerPort = 0;
	
	printf("%d connected peers!\n", connectedPeers);
	
	for ( i=0; i < binaryPeers_KeyLength; ) {
		peerIP = *(int *)(binaryPeers+i);
		i += 4;
		peerPort = *(uint16_t *)(binaryPeers+i);
		i += 2;
		printf("(0x%x:%d) => %d.%d.%d.%d:%d\n", peerIP, peerPort, (peerIP & 0xFF000000) >> 24, (peerIP & 0x00FF0000) >> 16, (peerIP & 0x0000FF00) >> 8, peerIP & 0x000000FF, peerPort); 
	}
	
	free(postCtx);
    free(httpResponse);
}

int UDPProtocolTracker_Upload( SOCKET trackerSocket, torrentBuild *tBuild ) {

}

int HTTPProtocolTracker_Dump( SOCKET trackerSocket, torrentBuild *tBuild ) {

}

int UDPProtocolTracker_Dump( SOCKET trackerSocket, torrentBuild *tBuild ) {

}

int unChunckPOST( http *httpStruct ) {
    char strHex[8] = {0};
    unsigned int intSize = 0;
    char *tmpPOST = (char *) calloc(httpStruct->sizeOfPost, sizeof(char)); // Allocate temporary buffer for POST data unchunked
    int tmpPOSTptr = 0, i = 0, y = 0;
	int newSizeOfPost = 0;
    for ( i=0, y=0; i < httpStruct->sizeOfPost; ) {
        /* Read hex size */
        while ( httpStruct->httpPost[i] != 0x0d && httpStruct->httpPost[i] != 0x00 ) {
            if ( i < httpStruct->sizeOfPost )
                if ( y < 8 )
                    if ( ((httpStruct->httpPost[i] > 0x2f && httpStruct->httpPost[i] < 0x3a) || (httpStruct->httpPost[i] > 0x60 && httpStruct->httpPost[i] < 0x67)) && (y < 8) ) strHex[y++] = httpStruct->httpPost[i++];
                    else return -1;
                else return -2;
            else return -3;
        }
        y = 0;

        intSize = strHex32ToInt(strHex); // Convert hex str to int
		
        memset(strHex, 0x00, 8);
        if ( intSize == 0 ) break; // End of string
		newSizeOfPost += intSize;

        i+=2;
        /* Verify following bytes */
        while ( y < intSize ) {
            if ( i < httpStruct->sizeOfPost ) {
                tmpPOST[tmpPOSTptr++] = httpStruct->httpPost[i++];
                y++;
            }
            else return -4;
        }
        y = 0;

        i+=2;
    }

    memset(httpStruct->httpPost, 0x00, httpStruct->sizeOfPost);
    memcpy(httpStruct->httpPost, tmpPOST, httpStruct->sizeOfPost);
	httpStruct->sizeOfPost = newSizeOfPost;

    free(tmpPOST);
    
    return 1;
}

unsigned int strHex32ToInt( char *hex ) {
    int hexLen = strlen(hex);
    if ( hexLen > 8 ) return -1; // > 32 bits
    int binaryHex = 0, i = 0;
    /*
        1234 -> 0x1234 -> 0001 0010 0011 0100 (binaryHex)
    */
    do {
        binaryHex <<= 4;
        if ( hex[i] > 0x2F && hex[i] < 0x3A ) { /* 0-9 */
            binaryHex = ( binaryHex + (hex[i++]-48) );
        }
        else if ( hex[i] > 0x60 && hex[i] < 0x67) { /* a-f */
            binaryHex = ( binaryHex + (hex[i++]-87) );
        }
        else return -1;
    } while ( i < hexLen );
    return binaryHex;
}

int isChunked( http *httpStruct ) {
    return strstr(httpStruct->httpHeader, "Transfer-Encoding: chunked") == NULL ? 0 : 1;
}

int urlEncode( unsigned char *baseUrl, unsigned char *urlEncodedUrl, int sizeOfUrl ) {
    int retBufOffset = 0, i = 0, y = 0;
	int bit = 0;
	char reserved[22] = "!*'();:@&=+$,/?#[]% ^";
    for ( i=0; i < sizeOfUrl; i++ ) {
		bit = 0;
		for ( y=0; y < 21; y++ ) {
			if ( baseUrl[i] == reserved[y] ) {
				bit++;
				break;
			}
		}
		
        if ( (baseUrl[i] < 0x21 || baseUrl[i] > 0x7e) || bit ) {
            retBufOffset += sprintf(urlEncodedUrl+retBufOffset, "%c", 0x25);
            retBufOffset += sprintf(urlEncodedUrl+retBufOffset, "%02x", baseUrl[i]);
        }
        else retBufOffset += sprintf(urlEncodedUrl+retBufOffset, "%c", baseUrl[i]);
    }
    return retBufOffset;
}

int fillProtocolInfo( char *url, protocolInfo *pInfo ) {
	int retVar_=0;
	WSADATA wsa;
	retVar_ = WSAStartup(MAKEWORD(2, 2), &wsa);
	if ( retVar_ != 0 ) {
		printf("Unable to WSAStartup: 0x%x\n", retVar_);
		return -1;
	}
	
    int urlLen = strlen(url);
    if ( urlLen < 4 ) {
		WSACleanup(); 
		return -2;
	}	// a.io, the smallest URL possible 

    char *hostnameStart = url, *hostnameEnd = NULL;
	char hostname[2083] = {0};
    if ( urlLen > 7 && *(int *)url == 0x70747468 ) { // HTTP signature
        hostnameStart += 4;
        if ( url[4] == 0x73 ) {
			WSACleanup(); 
			return -1;
		}	// check the https case 0x73 == 's', and return error because we don't handle HTTPS connections
        hostnameStart += 3; // We assume the rest of the url start is "://"
		hostnameEnd = strchr(hostnameStart, 0x2f); // 0x2f = '/'
		
		memcpy(hostname, hostnameStart, hostnameEnd-hostnameStart);
	
		printf("Trying to resolve: %s\n", hostname);
		
		struct hostent *resolveHost;
		resolveHost = gethostbyname(hostname);
		
		pInfo->ip = *(u_long *)resolveHost->h_addr_list[0];
		pInfo->port = 80;
		pInfo->protocol = HTTP; // HTTP Protocol
		
		int pathLen = strlen(hostnameEnd);
		pInfo->path = (char *) calloc(pathLen+1, sizeof(char));
		memcpy(pInfo->path, hostnameEnd, pathLen); // Copy the following path
		
		pInfo->hostname = (char *) calloc(hostnameEnd-hostnameStart, sizeof(char));
		memcpy(pInfo->hostname, hostnameStart, hostnameEnd-hostnameStart);
    }
	else if ( urlLen > 7 && (*(int *)url & 0x00FFFFFF) == 0x00706475 ) { // UDP signature
		hostnameStart += 6; // udp:// -> 6 characters
		hostnameEnd = strchr(hostnameStart, 0x3a); // 0x3a = ':'
		
		memcpy(hostname, hostnameStart, hostnameEnd-hostnameStart);
		
		printf("Trying to resolve: %s\n", hostname);
		
		struct hostent *resolveHost = gethostbyname(hostname);
		if ( resolveHost == NULL ) {
			printf("Unable to resolve hostname\n");
			WSACleanup();
			return -1;
		}
		
		hostnameEnd += 1; // Go after :

		char strPort[7] = {0};
		char *slashHost = strchr(hostnameEnd, 0x2f);
		if ( slashHost != NULL ) 
			if ( slashHost-hostnameEnd < 6 )
				memcpy(strPort, hostnameEnd, slashHost-hostnameEnd);
		else memcpy(strPort, hostnameEnd, strlen(hostnameEnd));
		
		pInfo->ip = *(u_long *)resolveHost->h_addr_list[0];
		pInfo->port = strtol(strPort, NULL, 10);
		pInfo->protocol = UDP; // UDP Protocol
	}
	
	WSACleanup();
	return 1;
}

int HTTPTrackerResponseFillHeaderPOST( char *bufferAddr, http* httpStruct, int sizeData ) {

    // Write Header
    char *endOfHeader = strstr(bufferAddr, "\x0d\x0a\x0d\x0a") + 4; // Go after \r\n\r\n
	if ( endOfHeader == NULL ) {
		printf("Bad header\n");
		return EXIT_FAILURE;
	}
    httpStruct->sizeOfHeader = endOfHeader-bufferAddr;
    httpStruct->httpHeader = (char *) calloc(httpStruct->sizeOfHeader+1, sizeof(char)); // +1 add NULL Bytes
    memcpy(httpStruct->httpHeader, bufferAddr, httpStruct->sizeOfHeader);

    // Write POST 
    char *endOfResponse = bufferAddr+sizeData;
	if ( endOfResponse == NULL ) {
		printf("Bad header\n");
		return EXIT_FAILURE;
	}
    httpStruct->sizeOfPost = endOfResponse-endOfHeader;
    httpStruct->httpPost = (char *) calloc(httpStruct->sizeOfPost+1, sizeof(char)); // +1 add NULL Bytes
    memcpy(httpStruct->httpPost, endOfHeader, httpStruct->sizeOfPost);

    return 1;
}

void buildPeerId ( char *returnLocation ) {
    // -BT7950-%f1%a2%dbto-%ba%d8Wi%ae%f3
    // -BT7950-@>wN'7a"/H'u

    sprintf(returnLocation, "-BT7950-"); // Start of Peer_ID ( fake BitTorrent Peer_ID )
    int i=0, rValue=0;
    char payload[13] = {0};
    
    srand(time(NULL));
    for ( i=0; i < 12; i++ ) {
        rValue = rand() % 0x7E;
        if ( rValue == 0x25 ) rValue++; // Avoid % char for url encoding purpose
        payload[i] = rValue;
    }
    
    sprintf(returnLocation+8, payload);
}

int bencodeRead( char *bufferAddr, fileCtx *ctx, const char *key, char *returnValueBuffer ) {

    /* Extract var */
    int context = 0xFF, returnBufferOffset = 0;

    /* Stack var 
    int cObjectType = '0', ctx->stackPtr = 0; // 0 == None, l == List, d == Dictionnary
    char endObjectStack[128] = {0}; // stack with LIFO working system*/
    
    /* Read var */
    char *ptrString = (char *) calloc(ctx->bytesLimit, sizeof(char));
    if ( ptrString == NULL ) {
        printf("Error, NULL Allocation return\n");
        exit(EXIT_FAILURE); 
    }
    int intVar = 0, bytesReaded = 0;

    /* Ptr file 
    int fileOffset = 0;*/

    memset(returnValueBuffer, 0x00, sizeof(returnValueBuffer)); // Fill the buffer with NULL Bytes

    for ( ctx->fileOffset; ctx->fileOffset <= ctx->bytesLimit; ) {

        // cObjectType fix
        if ( ctx->stack[0] == '\0' ) ctx->cObjectType = '0';

        // EOF
        if ( ctx->fileOffset >= ctx->bytesLimit ) {
            if ( ctx->stackPtr == 0 && ctx->stack[0] == '\0' ) {
                return returnBufferOffset;
            }
            else {
                printf("Bad bencoding\n");
                exit(EXIT_FAILURE);
            }
        }

        // End of object
        if ( bufferAddr[ctx->fileOffset] == 'e' ) {

            if ( ctx->stackPtr > 0 ) {
                if ( ctx->stackPtr > context ) {
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)"e", 1);
                    returnBufferOffset++;
                }

                ctx->stack[(ctx->stackPtr)-1] = '\0'; // Remove
                ctx->stackPtr--; // Dec Ptr
                ctx->cObjectType = ctx->stack[ctx->stackPtr-1] == '}' ? 'd' : 'l'; // Refresh the current object type

                // Inc offset properly
                if ( ctx->fileOffset+1 >= ctx->bytesLimit ) {
                    if ( ctx->stackPtr == 0 && ctx->stack[0] == '\0' ) {
                        (ctx->fileOffset)++;
                        continue; // Pass to let EOF part end the function
                    }
                    else {
                        printf("Bad bencoding\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else (ctx->fileOffset)++; continue; // Inc + Jump to the start of loop to refresh 'e' byte check
            }
            else {
                printf("Bad bencoding 1\n");
                exit(EXIT_FAILURE);
            }
        }

        // Dictionnary purpose
        if ( ctx->cObjectType == 'd' ) {
            if ( bufferAddr[ctx->fileOffset] > 0x2F && bufferAddr[ctx->fileOffset] < 0x3A ) {
                if ( ctx->stackPtr == context ) {
                    context = 0xFF; // Set no context
                    return returnBufferOffset; // Return num of bytes copied
                }

                bytesReaded = readString(bufferAddr, ctx, ptrString);

                if ( strcmp(key, ptrString) == 0 && context == 0xFF ) {
                    context = ctx->stackPtr; // Set new context
                }
                else if ( ctx->stackPtr >= context ) {
                    if ( ctx->raw == 1 ) {
                        returnBufferOffset += sprintf(returnValueBuffer+returnBufferOffset, "%d", bytesReaded);
                        memcpy(returnValueBuffer+returnBufferOffset, (void *)":", 1);
                        returnBufferOffset++;
                        memcpy(returnValueBuffer+returnBufferOffset, ptrString, bytesReaded);
                        returnBufferOffset += bytesReaded;
                    }
                    else {
                        memcpy(returnValueBuffer+returnBufferOffset, ptrString, bytesReaded);
                        returnBufferOffset += bytesReaded;
                    }
                }
                memset(ptrString, 0x00, ctx->bytesLimit);
            }   
            else {
                printf("Bad bencoding: offset: %d -- %c\n", ctx->fileOffset, bufferAddr[ctx->fileOffset]);
                exit(EXIT_FAILURE);
            }
        }

        // Read string
        if ( bufferAddr[ctx->fileOffset] > 0x2F && bufferAddr[ctx->fileOffset] < 0x3A ) {
            bytesReaded = readString(bufferAddr, ctx, ptrString);
            if ( ctx->stackPtr >= context ) {
                if ( ctx->raw == 1 ) {
                    returnBufferOffset += sprintf(returnValueBuffer+returnBufferOffset, "%d", bytesReaded);
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)":", 1);
                    returnBufferOffset++;
                    memcpy(returnValueBuffer+returnBufferOffset, ptrString, bytesReaded);
                    returnBufferOffset += bytesReaded;
                }
                else {
                    memcpy(returnValueBuffer+returnBufferOffset, ptrString, bytesReaded);
                    returnBufferOffset += bytesReaded;
                }            }
            memset(ptrString, 0x00, ctx->bytesLimit);
        }

        // Read integer
        else if ( bufferAddr[ctx->fileOffset] == 'i' ) {
            intVar = readInteger(bufferAddr, ctx);
            if ( ctx->stackPtr >= context ) {
                if ( ctx->raw == 1 ) {
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)"i", 1);
                    returnBufferOffset++;
                    returnBufferOffset += sprintf(returnValueBuffer+returnBufferOffset, "%d", intVar);
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)"e", 1);
                    returnBufferOffset++;
                }
                else {
                    returnBufferOffset += sprintf(returnValueBuffer+returnBufferOffset, "%d", intVar);
                }
            }
        }

        // List entry
        else if ( bufferAddr[ctx->fileOffset] == 'l' ) {
            if ( ctx->stackPtr < 128 ) {
                ctx->stack[ctx->stackPtr] = ']';
                ctx->stackPtr++;
                ctx->cObjectType = 'l';
                incOffset(ctx);
                if ( ctx->stackPtr >= context ) {
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)"l", 1);
                    returnBufferOffset++;
                }
            }
            else {
                printf("Too much objects\n");
                exit(EXIT_FAILURE);
            }
        }

        // Dictionnary Entry
        else if ( bufferAddr[ctx->fileOffset] == 'd' ) {
            if ( ctx->stackPtr < 128 ) {
                ctx->stack[ctx->stackPtr] = '}';
                ctx->stackPtr++;
                ctx->cObjectType = 'd';
                incOffset(ctx);
                if ( ctx->stackPtr >= context ) {
                    memcpy(returnValueBuffer+returnBufferOffset, (void *)"d", 1);
                    returnBufferOffset++; 
                }
            }
            else {
                printf("Too much objects\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;
}

int readString( char *bufferAddr, fileCtx *ctx, char *returnLocation ) {
    int i=0;
    long int strSizeInt=0;
    char strSize[16] = { 0 };
    do {
        if ( bufferAddr[ctx->fileOffset] > 0x2F && bufferAddr[ctx->fileOffset] < 0x3A ) strSize[i] = bufferAddr[ctx->fileOffset];
        else {
            printf("Bad bencoding 3\n");
            exit(EXIT_FAILURE);
        }
        incOffset(ctx);
        if ( i+1 < 16 ) i++;
        else {
            printf("Too high entry length");
            exit(EXIT_FAILURE);
        }
    } while ( bufferAddr[ctx->fileOffset] != ':' );
    strSizeInt = strtol(strSize, NULL, 10);
    if ( strSizeInt == -1 ) {
        printf("Bad bencoding 4\n");
        exit(EXIT_FAILURE);
    }
	else if ( strSizeInt == 0 ) {
		incOffset(ctx); // Return to the next byte to read
		return strSizeInt;
	}
    i=0;
    do {
        incOffset(ctx);
        returnLocation[i] = bufferAddr[ctx->fileOffset];
        i++;
    } while ( i < strSizeInt );
    incOffset(ctx); // Return to the next byte to read
    return strSizeInt;
}

int readInteger( char *bufferAddr, fileCtx *ctx ) {
    incOffset(ctx); // Skip 'i'
    char strToInt[16] = {0};
    int vInt = 0;
    int i=0;
    do {
        if ( bufferAddr[ctx->fileOffset] > 0x2F && bufferAddr[ctx->fileOffset] < 0x3A ) {
            if ( i+1 < 16 ) strToInt[i++] = bufferAddr[ctx->fileOffset];
            else {
                printf("Too high number\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            printf("Bad bencoding 5\n");
            exit(EXIT_FAILURE);
        }
        incOffset(ctx);
    } while ( bufferAddr[ctx->fileOffset] != 'e' );
    incOffset(ctx); // Return to the next byte to read ( after 'e' )
    vInt = strtol(strToInt, NULL, 10);
    return vInt;
}

void incOffset( fileCtx *ctx ) {
    if ( (ctx->fileOffset)+1 < ctx->bytesLimit ) (ctx->fileOffset)++;
    else {
        printf("Bad bencoding 6\n");
        exit(EXIT_FAILURE);
    }
}