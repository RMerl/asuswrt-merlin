/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the Stanford.txt file.
**
*/
/**
*   confread.c
*
*   Generic config file reader. Used to open a config file,
*   and read the tokens from it. The parser is really simple,
*   and does no backlogging. This means that no form of
*   text escaping and qouting is currently supported.
*   '#' chars are read as comments, and the comment lasts until 
*   a newline or EOF
*
*/

#include "igmpproxy.h"

#define     READ_BUFFER_SIZE    512   // Inputbuffer size...
        
#ifndef MAX_TOKEN_LENGTH
  #define MAX_TOKEN_LENGTH  30     // Default max token length
#endif
                                     
FILE            *confFilePtr;       // File handle pointer
char            *iBuffer;           // Inputbuffer for reading...
unsigned int    bufPtr;             // Buffer position pointer.
unsigned int    readSize;           // Number of bytes in buffer after last read...
char    cToken[MAX_TOKEN_LENGTH];   // Token buffer...
short   validToken;

/**
*   Opens config file specified by filename.
*/    
int openConfigFile(char *filename) {

    // Set the buffer to null initially...
    iBuffer = NULL;

    // Open the file for reading...
    confFilePtr = fopen(filename, "r");
    
    // On error, return false
    if(confFilePtr == NULL) {
        return 0;
    }
    
    // Allocate memory for inputbuffer...
    iBuffer = (char*) malloc( sizeof(char) * READ_BUFFER_SIZE );
    
    if(iBuffer == NULL) {
        closeConfigFile();
        return 0;
    }
    
    // Reset bufferpointer and readsize
    bufPtr = 0;
    readSize = 0;

    return 1;
}

/**
*   Closes the currently open config file.
*/
void closeConfigFile() {
    // Close the file.
    if(confFilePtr!=NULL) {
        fclose(confFilePtr);
    }
    // Free input buffer memory...
    if(iBuffer != NULL) {
        free(iBuffer);
    }
}

/**
*   Returns the next token from the configfile. The function
*   return NULL if there are no more tokens in the file.    
*/
char *nextConfigToken() {

    validToken = 0;

    // If no file or buffer, return NULL
    if(confFilePtr == NULL || iBuffer == NULL) {
        return NULL;
    }

    {
        unsigned int tokenPtr       = 0;
        unsigned short finished     = 0;
        unsigned short commentFound = 0;

        // Outer buffer fill loop...
        while ( !finished ) {
            // If readpointer is at the end of the buffer, we should read next chunk...
            if(bufPtr == readSize) {
                // Fill up the buffer...
                readSize = fread (iBuffer, sizeof(char), READ_BUFFER_SIZE, confFilePtr);
                bufPtr = 0;

                // If the readsize is 0, we should just return...
                if(readSize == 0) {
                    return NULL;
                }
            }

            // Inner char loop...
            while ( bufPtr < readSize && !finished ) {

                //printf("Char %s", iBuffer[bufPtr]);

                // Break loop on \0
                if(iBuffer[bufPtr] == '\0') {
                    break;
                }

                if( commentFound ) {
                    if( iBuffer[bufPtr] == '\n' ) {
                        commentFound = 0;
                    }
                } else {

                    // Check current char...
                    switch(iBuffer[bufPtr]) {
                    case '#':
                        // Found a comment start...
                        commentFound = 1;
                        break;

                    case '\n':
                    case '\r':
                    case '\t':
                    case ' ':
                        // Newline, CR, Tab and space are end of token, or ignored.
                        if(tokenPtr > 0) {
                            cToken[tokenPtr] = '\0';    // EOL
                            finished = 1;
                        }
                        break;

                    default:
                        // Append char to token...
                        cToken[tokenPtr++] = iBuffer[bufPtr];
                        break;
                    }
                
                }

                // Check end of token buffer !!!
                if(tokenPtr == MAX_TOKEN_LENGTH - 1) {
                    // Prevent buffer overrun...
                    cToken[tokenPtr] = '\0';
                    finished = 1;
                }

                // Next char...
                bufPtr++;
            }
            // If the readsize is less than buffersize, we assume EOF.
            if(readSize < READ_BUFFER_SIZE && bufPtr == readSize) {
                if (tokenPtr > 0)
                    finished = 1;
                else
                    return NULL;
            }
        }
        if(tokenPtr>0) {
            validToken = 1;
            return cToken;
        }
    }
    return NULL;
}


/**
*   Returns the currently active token, or null
*   if no tokens are availible.
*/
char *getCurrentConfigToken() {
    return validToken ? cToken : NULL;
}

