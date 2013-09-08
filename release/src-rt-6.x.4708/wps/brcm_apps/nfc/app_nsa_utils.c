/*****************************************************************************
 **
 **  Name:           app_utils.c
 **
 **  Description:    Bluetooth utils functions
 **
 **  Copyright (c) 2009-2012, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "nsa_api.h"

//#include "app_xml_param.h"
#include "app_nsa_utils.h"

/*
 * Definitions
 */
/* Terminal Attribute definitions */
#define RESET       0
#define BRIGHT      1
#define DIM         2
#define UNDERLINE   3
#define BLINK       4
#define REVERSE     7
#define HIDDEN      8

/* Terminal Color definitions */
#define BLACK       0
#define RED         1
#define GREEN       2
#define YELLOW      3
#define BLUE        4
#define MAGENTA     5
#define CYAN        6
#define WHITE       7

/* Length of the string containing a TimeStamp */
#define APP_TIMESTAMP_LEN 80
/*
 * Local functions
 */

#ifdef APP_TRACE_COLOR
static void app_set_trace_color(int attr, int fg, int bg);
#endif
#ifdef APP_TRACE_TIMESTAMP
static char *app_get_time_stamp(char *p_buffer, int buffer_size);
#endif

#if (NSA != TRUE)
/*******************************************************************************
 **
 ** Function        app_get_cod_string
 **
 ** Description     This function is used to get readable string from Class of device
 **
 ** Parameters      class_of_device: The Class of device to decode
 **
 ** Returns         Pointer on string containing device type
 **
 *******************************************************************************/
char *app_get_cod_string(const DEV_CLASS class_of_device)
{
    UINT8 major;

    /* Extract Major Device Class value */
    BTM_COD_MAJOR_CLASS(major, class_of_device);

    switch (major)
    {
    case BTM_COD_MAJOR_MISCELLANEOUS:
        return "Misc device";
        break;
    case BTM_COD_MAJOR_COMPUTER:
        return "Computer";
        break;
    case BTM_COD_MAJOR_PHONE:
        return "Phone";
        break;
    case BTM_COD_MAJOR_LAN_ACCESS_PT:
        return "Access Point";
        break;
    case BTM_COD_MAJOR_AUDIO:
        return "Audio/Video";
        break;
    case BTM_COD_MAJOR_PERIPHERAL:
        return "Peripheral";
        break;
    case BTM_COD_MAJOR_IMAGING:
        return "Imaging";
        break;
    case BTM_COD_MAJOR_WEARABLE:
        return "Wearable";
        break;
    case BTM_COD_MAJOR_TOY:
        return "Toy";
        break;
    case BTM_COD_MAJOR_HEALTH:
        return "Health";
        break;
    default:
        return "Unknown device type";
        break;
    }
    return NULL;
}
#endif
/*******************************************************************************
 **
 ** Function        app_get_choice
 **
 ** Description     Wait for a choice from user
 **
 ** Parameters      The string to print before waiting for input
 **
 ** Returns         The number typed by the user, or -1 if the value type was
 **                 not parsable
 **
 *******************************************************************************/
int app_get_choice(const char *querystring)
{
    int neg, value, c, base;

    base = 10;
    neg = 1;
    printf("%s => ", querystring);
    value = 0;
    do
    {
        c = getchar();
        if ((c >= '0') && (c <= '9'))
        {
            value = (value * base) + (c - '0');
        }
        else if ((c >= 'a') && (c <= 'f'))
        {
            value = (value * base) + (c - 'a' + 10);
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            value = (value * base) + (c - 'A' + 10);
        }
        else if (c == '-')
        {
            neg *= -1;
        }
        else if (c == 'x')
        {
            base = 16;
        }

    } while ((c != EOF) && (c != '\n'));

    return value * neg;
}

/*******************************************************************************
 **
 ** Function        app_get_string
 **
 ** Description     Ask the user to enter a string value
 **
 ** Parameters      querystring: to print before waiting for input
 **                 str: the char buffer to fill with user input
 **                 len: the length of the char buffer
 **
 ** Returns         The length of the string entered not including last NULL char
 **                 negative value in case of error
 **
 *******************************************************************************/
int app_get_string(const char *querystring, char *str, int len)
{
    int c, index;

    if (querystring)
    {
        printf("%s => ", querystring);
    }
    
    index = 0;
    do
    {
        c = getchar();
        if (c == EOF)
        {
            return -1;
        }
        if ((c != '\n') && (index < (len - 1)))
        {
            str[index] = (char)c;
            index++;
        }
    } while (c != '\n');

    str[index] = '\0';
    return index;
}

#ifdef APP_TRACE_TIMESTAMP
/*******************************************************************************
 **
 ** Function        app_get_time_stamp
 **
 ** Description     This function is used to get a timestamp
 **
 ** Parameters:     p_buffer: buffer to write the timestamp
 **                 buffer_size: buffer size
 **
 ** Returns         pointer on p_buffer
 **
 *******************************************************************************/
static char *app_get_time_stamp(char *p_buffer, int buffer_size)
{
    char time_string[80];

    /* use GKI_get_time_stamp to have the same clock than the BSA client traces */
    GKI_get_time_stamp((INT8 *)time_string);

    snprintf(p_buffer, buffer_size, "%s", time_string);

    return p_buffer;
}
#endif

/*******************************************************************************
 **
 ** Function        app_print_info
 **
 ** Description     This function is used to print an application information message
 **
 ** Parameters:     format: Format string
 **                 optional parameters
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_print_info(char *format, ...)
{
    va_list ap;
#ifdef APP_TRACE_TIMESTAMP
    char time_stamp[APP_TIMESTAMP_LEN];
#endif

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, BLACK, WHITE);
#endif

#ifdef APP_TRACE_TIMESTAMP
    app_get_time_stamp(time_stamp, sizeof(time_stamp));
    printf("INFO@%s: ", time_stamp);
#endif

    va_start(ap, format);
    vprintf(format,ap);
    va_end(ap);

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, BLACK, WHITE);
#endif
}

/*******************************************************************************
 **
 ** Function        app_print_debug
 **
 ** Description     This function is used to print an application debug message
 **
 ** Parameters:     format: Format string
 **                 optional parameters
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_print_debug(char *format, ...)
{
    va_list ap;
#ifdef APP_TRACE_TIMESTAMP
    char time_stamp[APP_TIMESTAMP_LEN];
#endif

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, GREEN, WHITE);
#endif

#ifdef APP_TRACE_TIMESTAMP
    app_get_time_stamp(time_stamp, sizeof(time_stamp));
    printf("DEBUG@%s: ", time_stamp);
#else
    printf("DEBUG: ");
#endif

    va_start(ap, format);
    vprintf(format,ap);
    va_end(ap);

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, BLACK, WHITE);
#endif
}

/*******************************************************************************
 **
 ** Function        app_print_error
 **
 ** Description     This function is used to print an application error message
 **
 ** Parameters:     format: Format string
 **                 optional parameters
 **
 ** Returns         void
 **
 *******************************************************************************/
void app_print_error(char *format, ...)
{
    va_list ap;
#ifdef APP_TRACE_TIMESTAMP
    char time_stamp[APP_TIMESTAMP_LEN];
#endif

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, RED, WHITE);
#endif

#ifdef APP_TRACE_TIMESTAMP
    app_get_time_stamp(time_stamp, sizeof(time_stamp));
    printf("ERROR@%s: ", time_stamp);
#else
    printf("ERROR: ");
#endif

    va_start(ap, format);
    vprintf(format,ap);
    va_end(ap);

#ifdef APP_TRACE_COLOR
    app_set_trace_color(RESET, BLACK, WHITE);
#endif
}

#ifdef APP_TRACE_COLOR
/*******************************************************************************
 **
 ** Function        app_set_trace_color
 **
 ** Description     This function changes the text color
 **
 ** Parameters:     attribute: Text attribute (reset/Blink/etc)
 **                 foreground: foreground color
 **                 background: background color
 **
 ** Returns         void
 **
 *******************************************************************************/
static void app_set_trace_color(int attribute, int foreground, int background)
{
    char command[13];

    /* Command is the control command to the terminal */
    snprintf(command, sizeof(command), "%c[%d;%d;%dm", 0x1B, attribute, foreground + 30, background + 40);
    printf("%s", command);
}
#endif

/*******************************************************************************
 **
 ** Function        app_file_size
 **
 ** Description     Retrieve the size of a file identified by descriptor
 **
 ** Parameters:     fd: File descriptor
 **
 ** Returns         File size if successful or negative error number
 **
 *******************************************************************************/
int app_file_size(int fd)
{
    struct stat file_stat;
    int rc = -1;

    rc = fstat(fd, &file_stat);

    if (rc >= 0)
    {
        /* Retrieve the size of the file */
        rc = file_stat.st_size;
    }
    else
    {
        APP_ERROR1("could not fstat(fd=%d)", fd);
    }
    return rc;
}
