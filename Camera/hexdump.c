/* 
 * This file is part of the positron distribution (https://github.com/radredgreen/positron).
 * Copyright (c) 2024 RadRedGreen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "hexdump.h"

void hexDump (
    const char * desc,
    const void * addr,
    const int len,
    int perLine
) {
    // Silently ignore silly per-line values.

    if (perLine < 4 || perLine > 64) perLine = 16;

    int i;
    unsigned char buff[perLine+1];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL) printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of perLine means new or first line (with line offset).

        if ((i % perLine) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.

            if (i != 0) printf ("  %s\n", buff);

            // Output the offset of current line.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.

        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % perLine] = '.';
        else
            buff[i % perLine] = pc[i];
        buff[(i % perLine) + 1] = '\0';
    }

    // Pad out last line if not exactly perLine characters.

    while ((i % perLine) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}


void hexDumpCompare (
    const char * desc,
    const void * addr,
    const void * addr2,
    const int len,
    int perLine
) {
    // Silently ignore silly per-line values.

    if (perLine < 4 || perLine > 64) perLine = 16;

    int i;
    unsigned char buff[perLine+1];
    const unsigned char * pc = (const unsigned char *)addr;

    int i2;
    unsigned char buff2[perLine+1];
    const unsigned char * pc2 = (const unsigned char *)addr2;

    // Output description if given.

    if (desc != NULL) printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }


    // Process every byte in the data.
    i2 = 0;
    for (i = 0; i < len; i++) {
        // Multiple of perLine means new or first line (with line offset).

        //pre
        if ((i % perLine) == 0) {
            // Output the offset of current line.
            printf ("  %04x ", i);
        }

        //in
            // Now the hex code for the specific character.

            if (pc[i]!=pc2[i]) printf("\033[0;31m");
            printf (" %02x", pc[i]);
            if (pc[i]!=pc2[i]) printf("\033[0m"); 

            // And buffer a printable ASCII character for later.

            if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
                buff[i % perLine] = '.';
            else
                buff[i % perLine] = pc[i];
            buff[(i % perLine) + 1] = '\0';


        //post
        if (((i + 1) % perLine) == 0) {
            printf ("  %s", buff);
                for ( ; i2 <= i; i2++) {
                    // Multiple of perLine means new or first line (with line offset).
                    //pre
                    if ((i2 % perLine) == 0) {
                        // Output the offset of current line.

                        printf ("  %04x ", i2);
                    }
                    //in
                    // Now the hex code for the specific character.

                    if (pc[i2]!=pc2[i2]) printf("\033[0;31m");
                    printf (" %02x", pc2[i2]);
                    if (pc[i2]!=pc2[i2]) printf("\033[0m"); 

                    // And buffer a printable ASCII character for later.

                    if ((pc2[i2] < 0x20) || (pc2[i2] > 0x7e)) // isprint() may be better.
                        buff2[i2 % perLine] = '.';
                    else
                        buff2[i2 % perLine] = pc2[i2];
                    buff2[(i2 % perLine) + 1] = '\0';
                    if (((i2+1) % perLine) == 0) {

                        printf ("  %s \n", buff2);
                    }
                }
        }

    }
    // Pad out last line if not exactly perLine characters.

    while ((i % perLine) != 0) {
        printf ("   ");
        i++;
    }
    printf("\n");
}


