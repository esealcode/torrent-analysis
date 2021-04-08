/*
    Http request each 5 minutes
*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// .torrent file parsing functions
int bencodeDump( char *bufferAddr, int *offsetAddr, int *bytesLimit );
void readString( char *bufferAddr, int *offsetAddr, int *bytesLimit );
int readInteger( char *bufferAddr, int *offsetAddr, int *bytesLimit );
void incOffset( int *offsetAddr, int *bytesLimit );

int main(int argc, char *argv[])
{
    // File open/read var
    HANDLE fHandle;
    int sizeReturn = 0;
    char *bufferAddr;
    int returnTmp = 0;
    PDWORD bytesReaded = 0;

    // bEncoded buffer parsing var
    int offsetAddr = 0+0;
    // sizeReturn

    // Reading .torrent file
    printf("Opening %s\n", argv[1]);
    fHandle = CreateFile(argv[1],
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

    printf("File length: %d\n", sizeReturn);

    bufferAddr = (char *) calloc(sizeReturn, sizeof(char));

    if ( bufferAddr == NULL ) {
        printf("Unable to alloc buffer memory\n");
        return EXIT_FAILURE;
    }

    returnTmp = ReadFile(fHandle,
                         bufferAddr,
                         sizeReturn,
                         &bytesReaded,
                         NULL
                         );
    if ( returnTmp == 0 ) {
        printf("Unable to read the file: 0x%x\n", (unsigned int)GetLastError());
        return EXIT_FAILURE;
    }

    printf("Buffer size: %d\n", (int)bytesReaded);

    bencodeDump( bufferAddr, &offsetAddr, &sizeReturn );

    free(bufferAddr);

    getchar();
    return 0;
}

int bencodeDump( char *bufferAddr, int *offsetAddr, int *bytesLimit ) {
    int cObjectType = '0'; // 0 == None, l == List, d == Dictionnary
    char endObjectStack[128] = {0}; // stack with LIFO working system
    int stackPtr = 0;

    char tabStack[128] = {0};

    for ( (*offsetAddr); (*offsetAddr) <= (*bytesLimit); ) {

        // cObjectType fix
        if ( endObjectStack[0] == '\0' ) cObjectType = '0';

        //printf("Stack state: %s\nStackptr: %d\ncObjectType: %c\nGlobal Offset: %d\n", endObjectStack, stackPtr, cObjectType, (*offsetAddr));

        // EOF
        if ( (*offsetAddr) >= (*bytesLimit) ) {
            if ( stackPtr == 0 && endObjectStack[0] == '\0' ) {
                printf("\nEOF\n");
                break;
            }
            else {
                printf("Bad bencoding\n");
                exit(EXIT_FAILURE);
            }
        }

        // End of object
        if ( bufferAddr[*offsetAddr] == 'e' ) {
            if ( stackPtr > 0 ) {

                tabStack[stackPtr-1] = '\0';
                if ( endObjectStack[stackPtr-1] == '}' ) printf("%sEnd Dictionnary\n", tabStack);
                else if ( endObjectStack[stackPtr-1] == ']' ) printf("%sEnd List\n", tabStack);

                endObjectStack[stackPtr-1] = '\0'; // Remove
                stackPtr--; // Dec Ptr
                cObjectType = endObjectStack[stackPtr-1] == '}' ? 'd' : 'l'; // Refresh the current object type

                // Inc offset properly
                if ( (*offsetAddr) >= bytesLimit ) {
                    if ( stackPtr == 0 && endObjectStack[0] == '\0' ) {
                        printf("\nEOF 2\n");
                        break;
                    }
                    else {
                        printf("Bad bencoding\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else (*offsetAddr)++; continue; // Inc + Jump to the start of loop to refresh 'e' byte check
            }
            else {
                printf("Bad bencoding 1\n");
                exit(EXIT_FAILURE);
            }
        }

        // Dictionnary purpose
        if ( cObjectType == 'd' ) {
            if ( bufferAddr[*offsetAddr] > 0x2F && bufferAddr[*offsetAddr] < 0x3A ) {
                printf("%s", tabStack);
                readString(bufferAddr, offsetAddr, bytesLimit);
                printf(":\n"); // Key:Value
            }
            else {
                printf("Bad bencoding: offset: %d -- %c\n", *offsetAddr, bufferAddr[*offsetAddr]);
                exit(EXIT_FAILURE);
            }
        }

        // Read string
        if ( bufferAddr[*offsetAddr] > 0x2F && bufferAddr[*offsetAddr] < 0x3A ) {
            printf("%s", tabStack);
            if ( cObjectType != 'd' ) printf("String: ");
            readString(bufferAddr, offsetAddr, bytesLimit);
        }

        // Read integer
        else if ( bufferAddr[*offsetAddr] == 'i' ) {
            printf("%s", tabStack);
            if ( cObjectType != 'd' ) printf("Integer: ");
            readInteger(bufferAddr, offsetAddr, bytesLimit);
        }

        // List entry
        else if ( bufferAddr[*offsetAddr] == 'l' ) {
            if ( stackPtr < 128 ) {
                printf("%sList entry", tabStack);
                tabStack[stackPtr] = '\t';
                endObjectStack[stackPtr] = ']';
                stackPtr++;
                cObjectType = 'l';
                incOffset(offsetAddr, bytesLimit);
            }
            else {
                printf("Too much objects\n");
                exit(EXIT_FAILURE);
            }
        }

        // Dictionnary Entry
        else if ( bufferAddr[*offsetAddr] == 'd' ) {
            if ( stackPtr < 128 ) {
                printf("%sDictionnary Entry", tabStack);
                tabStack[stackPtr] = '\t';
                endObjectStack[stackPtr] = '}';
                stackPtr++;
                cObjectType = 'd';
                incOffset(offsetAddr, bytesLimit);
            }
            else {
                printf("Too much objects\n");
                exit(EXIT_FAILURE);
            }
        }
        printf("\n");
    }

    return 0;
}

void readString( char *bufferAddr, int *offsetAddr, int *bytesLimit ) {
    int i=0;
    long int strSizeInt=0;
    char strSize[24] = { 0 };
    do {
        if ( bufferAddr[*offsetAddr] > 0x2F && bufferAddr[*offsetAddr] < 0x3A ) strSize[i] = bufferAddr[*offsetAddr];
        else {
            printf("Bad bencoding 3\n");
            exit(EXIT_FAILURE);
        }
        incOffset(offsetAddr, bytesLimit);
        if ( i+1 < 24 ) i++;
        else {
            printf("Too high entry length");
            exit(EXIT_FAILURE);
        }
    } while ( bufferAddr[*offsetAddr] != ':' );
    strSizeInt = strtol(strSize, NULL, 10);
    if ( strSizeInt == -1 ) {
        printf("Bad bencoding 4\n");
        exit(EXIT_FAILURE);
    }
    i=0;
    char *stringBuffer = (char *) calloc(strSizeInt+1  , sizeof(char)); // +1 for the null byte ( filled by calloc )
    do {
        incOffset(offsetAddr, bytesLimit);
        stringBuffer[i] = bufferAddr[*offsetAddr];
        i++;
    } while ( i < strSizeInt );
    printf("%s", stringBuffer);
    incOffset(offsetAddr, bytesLimit); // Return to the next byte to read
}

int readInteger( char *bufferAddr, int *offsetAddr, int *bytesLimit ) {
    incOffset(offsetAddr, bytesLimit);
    int i=0;
    long int integer=0;
    char strToInteger[24] = { 0 };
    do {
        if ( bufferAddr[*offsetAddr] > 0x2F && bufferAddr[*offsetAddr] < 0x3A ) strToInteger[i++] = bufferAddr[*offsetAddr];
        else {
            printf("Bad bencoding 5\n");
            exit(EXIT_FAILURE);
        }
        incOffset(offsetAddr, bytesLimit);
    } while ( bufferAddr[*offsetAddr] != 'e' );
    integer = strtol(strToInteger, NULL, 10);
    printf("%d", integer);
    incOffset(offsetAddr, bytesLimit); // Return to the next byte to read
    return integer;
}

void incOffset( int *offsetAddr, int *bytesLimit ) {
    if ( *(offsetAddr)+1 < *bytesLimit ) (*offsetAddr)++;
    else {
        printf("Bad bencoding 6\n");
        exit(EXIT_FAILURE);
    }
}
