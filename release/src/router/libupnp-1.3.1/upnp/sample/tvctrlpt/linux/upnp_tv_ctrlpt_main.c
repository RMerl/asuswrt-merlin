///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "sample_util.h"
#include "upnp_tv_ctrlpt.h"
#include <string.h>

/*
   Tags for valid commands issued at the command prompt 
 */
enum cmdloop_tvcmds {
    PRTHELP = 0, PRTFULLHELP, POWON, POWOFF,
    SETCHAN, SETVOL, SETCOL, SETTINT, SETCONT, SETBRT,
    CTRLACTION, PICTACTION, CTRLGETVAR, PICTGETVAR,
    PRTDEV, LSTDEV, REFRESH, EXITCMD
};

/*
   Data structure for parsing commands from the command line 
 */
struct cmdloop_commands {
    char *str;                  // the string 
    int cmdnum;                 // the command
    int numargs;                // the number of arguments
    char *args;                 // the args
} cmdloop_commands;

/*
   Mappings between command text names, command tag,
   and required command arguments for command line
   commands 
 */
static struct cmdloop_commands cmdloop_cmdlist[] = {
    {"Help", PRTHELP, 1, ""},
    {"HelpFull", PRTFULLHELP, 1, ""},
    {"ListDev", LSTDEV, 1, ""},
    {"Refresh", REFRESH, 1, ""},
    {"PrintDev", PRTDEV, 2, "<devnum>"},
    {"PowerOn", POWON, 2, "<devnum>"},
    {"PowerOff", POWOFF, 2, "<devnum>"},
    {"SetChannel", SETCHAN, 3, "<devnum> <channel (int)>"},
    {"SetVolume", SETVOL, 3, "<devnum> <volume (int)>"},
    {"SetColor", SETCOL, 3, "<devnum> <color (int)>"},
    {"SetTint", SETTINT, 3, "<devnum> <tint (int)>"},
    {"SetContrast", SETCONT, 3, "<devnum> <contrast (int)>"},
    {"SetBrightness", SETBRT, 3, "<devnum> <brightness (int)>"},
    {"CtrlAction", CTRLACTION, 2, "<devnum> <action (string)>"},
    {"PictAction", PICTACTION, 2, "<devnum> <action (string)>"},
    {"CtrlGetVar", CTRLGETVAR, 2, "<devnum> <varname (string)>"},
    {"PictGetVar", PICTGETVAR, 2, "<devnum> <varname (string)>"},
    {"Exit", EXITCMD, 1, ""}
};

void
linux_print( const char *string )
{
    puts( string );
}

/********************************************************************************
 * TvCtrlPointPrintHelp
 *
 * Description: 
 *       Print help info for this application.
 ********************************************************************************/
void
TvCtrlPointPrintShortHelp( void )
{
    SampleUtil_Print( "Commands:" );
    SampleUtil_Print( "  Help" );
    SampleUtil_Print( "  HelpFull" );
    SampleUtil_Print( "  ListDev" );
    SampleUtil_Print( "  Refresh" );
    SampleUtil_Print( "  PrintDev      <devnum>" );
    SampleUtil_Print( "  PowerOn       <devnum>" );
    SampleUtil_Print( "  PowerOff      <devnum>" );
    SampleUtil_Print( "  SetChannel    <devnum> <channel>" );
    SampleUtil_Print( "  SetVolume     <devnum> <volume>" );
    SampleUtil_Print( "  SetColor      <devnum> <color>" );
    SampleUtil_Print( "  SetTint       <devnum> <tint>" );
    SampleUtil_Print( "  SetContrast   <devnum> <contrast>" );
    SampleUtil_Print( "  SetBrightness <devnum> <brightness>" );
    SampleUtil_Print( "  CtrlAction    <devnum> <action>" );
    SampleUtil_Print( "  PictAction    <devnum> <action>" );
    SampleUtil_Print( "  CtrlGetVar    <devnum> <varname>" );
    SampleUtil_Print( "  PictGetVar    <devnum> <action>" );
    SampleUtil_Print( "  Exit" );
}

void
TvCtrlPointPrintLongHelp( void )
{
    SampleUtil_Print( "" );
    SampleUtil_Print( "******************************" );
    SampleUtil_Print( "* TV Control Point Help Info *" );
    SampleUtil_Print( "******************************" );
    SampleUtil_Print( "" );
    SampleUtil_Print
        ( "This sample control point application automatically searches" );
    SampleUtil_Print
        ( "for and subscribes to the services of television device emulator" );
    SampleUtil_Print
        ( "devices, described in the tvdevicedesc.xml description document." );
    SampleUtil_Print( "" );
    SampleUtil_Print( "Commands:" );
    SampleUtil_Print( "  Help" );
    SampleUtil_Print( "       Print this help info." );
    SampleUtil_Print( "  ListDev" );
    SampleUtil_Print
        ( "       Print the current list of TV Device Emulators that this" );
    SampleUtil_Print
        ( "         control point is aware of.  Each device is preceded by a" );
    SampleUtil_Print
        ( "         device number which corresponds to the devnum argument of" );
    SampleUtil_Print( "         commands listed below." );
    SampleUtil_Print( "  Refresh" );
    SampleUtil_Print
        ( "       Delete all of the devices from the device list and issue new" );
    SampleUtil_Print
        ( "         search request to rebuild the list from scratch." );
    SampleUtil_Print( "  PrintDev       <devnum>" );
    SampleUtil_Print
        ( "       Print the state table for the device <devnum>." );
    SampleUtil_Print
        ( "         e.g., 'PrintDev 1' prints the state table for the first" );
    SampleUtil_Print( "         device in the device list." );
    SampleUtil_Print( "  PowerOn        <devnum>" );
    SampleUtil_Print
        ( "       Sends the PowerOn action to the Control Service of" );
    SampleUtil_Print( "         device <devnum>." );
    SampleUtil_Print( "  PowerOff       <devnum>" );
    SampleUtil_Print
        ( "       Sends the PowerOff action to the Control Service of" );
    SampleUtil_Print( "         device <devnum>." );
    SampleUtil_Print( "  SetChannel     <devnum> <channel>" );
    SampleUtil_Print
        ( "       Sends the SetChannel action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the channel to be changed" );
    SampleUtil_Print( "         to <channel>." );
    SampleUtil_Print( "  SetVolume      <devnum> <volume>" );
    SampleUtil_Print
        ( "       Sends the SetVolume action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the volume to be changed" );
    SampleUtil_Print( "         to <volume>." );
    SampleUtil_Print( "  SetColor       <devnum> <color>" );
    SampleUtil_Print
        ( "       Sends the SetColor action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the color to be changed" );
    SampleUtil_Print( "         to <color>." );
    SampleUtil_Print( "  SetTint        <devnum> <tint>" );
    SampleUtil_Print
        ( "       Sends the SetTint action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the tint to be changed" );
    SampleUtil_Print( "         to <tint>." );
    SampleUtil_Print( "  SetContrast    <devnum> <contrast>" );
    SampleUtil_Print
        ( "       Sends the SetContrast action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the contrast to be changed" );
    SampleUtil_Print( "         to <contrast>." );
    SampleUtil_Print( "  SetBrightness  <devnum> <brightness>" );
    SampleUtil_Print
        ( "       Sends the SetBrightness action to the Control Service of" );
    SampleUtil_Print
        ( "         device <devnum>, requesting the brightness to be changed" );
    SampleUtil_Print( "         to <brightness>." );
    SampleUtil_Print( "  CtrlAction     <devnum> <action>" );
    SampleUtil_Print
        ( "       Sends an action request specified by the string <action>" );
    SampleUtil_Print
        ( "         to the Control Service of device <devnum>.  This command" );
    SampleUtil_Print
        ( "         only works for actions that have no arguments." );
    SampleUtil_Print
        ( "         (e.g., \"CtrlAction 1 IncreaseChannel\")" );
    SampleUtil_Print( "  PictAction     <devnum> <action>" );
    SampleUtil_Print
        ( "       Sends an action request specified by the string <action>" );
    SampleUtil_Print
        ( "         to the Picture Service of device <devnum>.  This command" );
    SampleUtil_Print
        ( "         only works for actions that have no arguments." );
    SampleUtil_Print
        ( "         (e.g., \"PictAction 1 DecreaseContrast\")" );
    SampleUtil_Print( "  CtrlGetVar     <devnum> <varname>" );
    SampleUtil_Print
        ( "       Requests the value of a variable specified by the string <varname>" );
    SampleUtil_Print
        ( "         from the Control Service of device <devnum>." );
    SampleUtil_Print( "         (e.g., \"CtrlGetVar 1 Volume\")" );
    SampleUtil_Print( "  PictGetVar     <devnum> <action>" );
    SampleUtil_Print
        ( "       Requests the value of a variable specified by the string <varname>" );
    SampleUtil_Print
        ( "         from the Picture Service of device <devnum>." );
    SampleUtil_Print( "         (e.g., \"PictGetVar 1 Tint\")" );
    SampleUtil_Print( "  Exit" );
    SampleUtil_Print( "       Exits the control point application." );
}

/********************************************************************************
 * TvCtrlPointPrintCommands
 *
 * Description: 
 *       Print the list of valid command line commands to the user
 *
 * Parameters:
 *   None
 *
 ********************************************************************************/
void
TvCtrlPointPrintCommands(  )
{
    int i;
    int numofcmds = sizeof( cmdloop_cmdlist ) / sizeof( cmdloop_commands );

    SampleUtil_Print( "Valid Commands:" );
    for( i = 0; i < numofcmds; i++ ) {
        SampleUtil_Print( "  %-14s %s", cmdloop_cmdlist[i].str,
                          cmdloop_cmdlist[i].args );
    }
    SampleUtil_Print( "" );
}

/********************************************************************************
 * TvCtrlPointCommandLoop
 *
 * Description: 
 *       Function that receives commands from the user at the command prompt
 *       during the lifetime of the control point, and calls the appropriate
 *       functions for those commands.
 *
 * Parameters:
 *    None
 *
 ********************************************************************************/
void *
TvCtrlPointCommandLoop( void *args )
{
    char cmdline[100];

    while( 1 ) {
        SampleUtil_Print( "\n>> " );
        fgets( cmdline, 100, stdin );
        TvCtrlPointProcessCommand( cmdline );
    }
}

int
TvCtrlPointProcessCommand( char *cmdline )
{
    char cmd[100];
    char strarg[100];
    int arg_val_err = -99999;
    int arg1 = arg_val_err;
    int arg2 = arg_val_err;
    int cmdnum = -1;
    int numofcmds = sizeof( cmdloop_cmdlist ) / sizeof( cmdloop_commands );
    int cmdfound = 0;
    int i,
      rc;
    int invalidargs = 0;
    int validargs;

    validargs = sscanf( cmdline, "%s %d %d", cmd, &arg1, &arg2 );

    for( i = 0; i < numofcmds; i++ ) {
        if( strcasecmp( cmd, cmdloop_cmdlist[i].str ) == 0 ) {
            cmdnum = cmdloop_cmdlist[i].cmdnum;
            cmdfound++;
            if( validargs != cmdloop_cmdlist[i].numargs )
                invalidargs++;
            break;
        }
    }

    if( !cmdfound ) {
        SampleUtil_Print( "Command not found; try 'Help'" );
        return TV_SUCCESS;
    }

    if( invalidargs ) {
        SampleUtil_Print( "Invalid arguments; try 'Help'" );
        return TV_SUCCESS;
    }

    switch ( cmdnum ) {
        case PRTHELP:
            TvCtrlPointPrintShortHelp(  );
            break;

        case PRTFULLHELP:
            TvCtrlPointPrintLongHelp(  );
            break;

        case POWON:
            TvCtrlPointSendPowerOn( arg1 );
            break;

        case POWOFF:
            TvCtrlPointSendPowerOff( arg1 );
            break;

        case SETCHAN:
            TvCtrlPointSendSetChannel( arg1, arg2 );
            break;

        case SETVOL:
            TvCtrlPointSendSetVolume( arg1, arg2 );
            break;

        case SETCOL:
            TvCtrlPointSendSetColor( arg1, arg2 );
            break;

        case SETTINT:
            TvCtrlPointSendSetTint( arg1, arg2 );
            break;

        case SETCONT:
            TvCtrlPointSendSetContrast( arg1, arg2 );
            break;

        case SETBRT:
            TvCtrlPointSendSetBrightness( arg1, arg2 );
            break;

        case CTRLACTION:
            /*
               re-parse commandline since second arg is string 
             */
            validargs = sscanf( cmdline, "%s %d %s", cmd, &arg1, strarg );
            if( 3 == validargs )
                TvCtrlPointSendAction( TV_SERVICE_CONTROL, arg1, strarg,
                                       NULL, NULL, 0 );
            else
                invalidargs++;
            break;

        case PICTACTION:
            /*
               re-parse commandline since second arg is string 
             */
            validargs = sscanf( cmdline, "%s %d %s", cmd, &arg1, strarg );
            if( 3 == validargs )
                TvCtrlPointSendAction( TV_SERVICE_PICTURE, arg1, strarg,
                                       NULL, NULL, 0 );
            else
                invalidargs++;
            break;

        case CTRLGETVAR:
            /*
               re-parse commandline since second arg is string 
             */
            validargs = sscanf( cmdline, "%s %d %s", cmd, &arg1, strarg );
            if( 3 == validargs )
                TvCtrlPointGetVar( TV_SERVICE_CONTROL, arg1, strarg );
            else
                invalidargs++;
            break;

        case PICTGETVAR:
            /*
               re-parse commandline since second arg is string 
             */
            validargs = sscanf( cmdline, "%s %d %s", cmd, &arg1, strarg );
            if( 3 == validargs )
                TvCtrlPointGetVar( TV_SERVICE_PICTURE, arg1, strarg );
            else
                invalidargs++;
            break;

        case PRTDEV:
            TvCtrlPointPrintDevice( arg1 );
            break;

        case LSTDEV:
            TvCtrlPointPrintList(  );
            break;

        case REFRESH:
            TvCtrlPointRefresh(  );
            break;

        case EXITCMD:
            rc = TvCtrlPointStop(  );
            exit( rc );
            break;

        default:
            SampleUtil_Print( "Command not implemented; see 'Help'" );
            break;
    }

    if( invalidargs )
        SampleUtil_Print( "Invalid args in command; see 'Help'" );

    return TV_SUCCESS;
}

int
main( int argc,
      char **argv )
{
    int rc;
    ithread_t cmdloop_thread;
    int sig;
    sigset_t sigs_to_catch;
    int code;

    rc = TvCtrlPointStart( linux_print, NULL );
    if( rc != TV_SUCCESS ) {
        SampleUtil_Print( "Error starting UPnP TV Control Point" );
        exit( rc );
    }
    // start a command loop thread
    code =
        ithread_create( &cmdloop_thread, NULL, TvCtrlPointCommandLoop,
                        NULL );

    /*
       Catch Ctrl-C and properly shutdown 
     */
    sigemptyset( &sigs_to_catch );
    sigaddset( &sigs_to_catch, SIGINT );
    sigwait( &sigs_to_catch, &sig );

    SampleUtil_Print( "Shutting down on signal %d...", sig );
    rc = TvCtrlPointStop(  );
    exit( rc );
}
