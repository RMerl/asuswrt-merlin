#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "wsc_common.h"
#include "upnp.h"

static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";


extern int wsc_debug_level;

#ifdef RT_DEBUG
void DBGPRINTF(int level, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (level <= wsc_debug_level)
	{
		vprintf(fmt, ap);
	}
	va_end(ap);
}
#endif

void wsc_hexdump(char *title, char *ptr, int len)
{

	int32 i;
	char *tmp = ptr;

	if (RT_DBG_PKT <= wsc_debug_level)
	{
		printf("\n---StartOfMsgHexDump:%s\n", title);
		for(i = 0; i < len; i++)
		{
			if(i%16==0 && i!=0)
				printf("\n");
			printf("%02x ", tmp[i] & 0xff);
		}
		printf("\n---EndOfMsgHexDump!\n");
	}

}



void
wsc_PrintEventType( IN Upnp_EventType S )
{
    switch ( S ) {

        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
            printf( "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n" );
            break;
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
            printf( "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE\n" );
            break;
        case UPNP_DISCOVERY_SEARCH_RESULT:
            printf( "UPNP_DISCOVERY_SEARCH_RESULT\n" );
            break;
        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
            printf( "UPNP_DISCOVERY_SEARCH_TIMEOUT\n" );
            break;

            /*
               SOAP Stuff 
             */
        case UPNP_CONTROL_ACTION_REQUEST:
            printf( "UPNP_CONTROL_ACTION_REQUEST\n" );
            break;
        case UPNP_CONTROL_ACTION_COMPLETE:
            printf( "UPNP_CONTROL_ACTION_COMPLETE\n" );
            break;
        case UPNP_CONTROL_GET_VAR_REQUEST:
            printf( "UPNP_CONTROL_GET_VAR_REQUEST\n" );
            break;
        case UPNP_CONTROL_GET_VAR_COMPLETE:
            printf( "UPNP_CONTROL_GET_VAR_COMPLETE\n" );
            break;

            /*
               GENA Stuff 
             */
        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
            printf( "UPNP_EVENT_SUBSCRIPTION_REQUEST\n" );
            break;
        case UPNP_EVENT_RECEIVED:
            printf( "UPNP_EVENT_RECEIVED\n" );
            break;
        case UPNP_EVENT_RENEWAL_COMPLETE:
            printf( "UPNP_EVENT_RENEWAL_COMPLETE\n" );
            break;
        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
            printf( "UPNP_EVENT_SUBSCRIBE_COMPLETE\n" );
            break;
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
            printf( "UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n" );
            break;

        case UPNP_EVENT_AUTORENEWAL_FAILED:
            printf( "UPNP_EVENT_AUTORENEWAL_FAILED\n" );
            break;
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
            printf( "UPNP_EVENT_SUBSCRIPTION_EXPIRED\n" );
            break;

    }
}

void wsc_printEvent(IN Upnp_EventType EventType,
                       IN void *Event)
{
    printf( "\n\n\n======================================================================\n" );
    printf( "----------------------------------------------------------------------\n" );
    wsc_PrintEventType( EventType );

    switch ( EventType ) {

            /*
               SSDP 
             */
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        case UPNP_DISCOVERY_SEARCH_RESULT:
            {
                struct Upnp_Discovery *d_event = ( struct Upnp_Discovery * )Event;

                printf( "ErrCode     =  %d\n", d_event->ErrCode );
                printf( "Expires     =  %d\n", d_event->Expires );
                printf( "DeviceId    =  %s\n", d_event->DeviceId );
                printf( "DeviceType  =  %s\n", d_event->DeviceType );
                printf( "ServiceType =  %s\n", d_event->ServiceType );
                printf( "ServiceVer  =  %s\n", d_event->ServiceVer );
                printf( "Location    =  %s\n", d_event->Location );
                printf( "OS          =  %s\n", d_event->Os );
                printf( "Ext         =  %s\n", d_event->Ext );
            }
            break;

        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
            // Nothing to print out here
            break;

            /*
               SOAP 
             */
        case UPNP_CONTROL_ACTION_REQUEST:
            {
                struct Upnp_Action_Request *a_event = ( struct Upnp_Action_Request * )Event;
                char *xmlbuff = NULL;

                printf( "ErrCode     =  %d\n", a_event->ErrCode );
                printf( "ErrStr      =  %s\n", a_event->ErrStr );
                printf( "ActionName  =  %s\n", a_event->ActionName );
                printf( "UDN         =  %s\n", a_event->DevUDN );
                printf( "ServiceID   =  %s\n", a_event->ServiceID );
                if( a_event->ActionRequest ) {
                    xmlbuff = ixmlPrintDocument( a_event->ActionRequest );
                    if( xmlbuff )
                        printf( "ActRequest  =  %s\n", xmlbuff );
                    if( xmlbuff )
                        ixmlFreeDOMString( xmlbuff );
                    xmlbuff = NULL;
                } else {
                    printf( "ActRequest  =  (null)\n" );
                }

                if( a_event->ActionResult ) {
                    xmlbuff = ixmlPrintDocument( a_event->ActionResult );
                    if( xmlbuff )
                        printf( "ActResult   =  %s\n", xmlbuff );
                    if( xmlbuff )
                        ixmlFreeDOMString( xmlbuff );
                    xmlbuff = NULL;
                } else {
                    printf( "ActResult   =  (null)\n" );
                }
            }
            break;

        case UPNP_CONTROL_ACTION_COMPLETE:
            {
                struct Upnp_Action_Complete *a_event = ( struct Upnp_Action_Complete * )Event;
                char *xmlbuff = NULL;

                printf( "ErrCode     =  %d\n", a_event->ErrCode );
                printf( "CtrlUrl     =  %s\n", a_event->CtrlUrl );
                if( a_event->ActionRequest ) {
                    xmlbuff = ixmlPrintDocument( a_event->ActionRequest );
                    if( xmlbuff )
                        printf( "ActRequest  =  %s\n", xmlbuff );
                    if( xmlbuff )
                        ixmlFreeDOMString( xmlbuff );
                    xmlbuff = NULL;
                } else {
                    printf( "ActRequest  =  (null)\n" );
                }

                if( a_event->ActionResult ) {
                    xmlbuff = ixmlPrintDocument( a_event->ActionResult );
                    if( xmlbuff )
                        printf( "ActResult   =  %s\n", xmlbuff );
                    if( xmlbuff )
                        ixmlFreeDOMString( xmlbuff );
                    xmlbuff = NULL;
                } else {
                    printf( "ActResult   =  (null)\n" );
                }
            }
            break;

        case UPNP_CONTROL_GET_VAR_REQUEST:
            {
                struct Upnp_State_Var_Request *sv_event = ( struct Upnp_State_Var_Request * )Event;

                printf( "ErrCode     =  %d\n", sv_event->ErrCode );
                printf( "ErrStr      =  %s\n", sv_event->ErrStr );
                printf( "UDN         =  %s\n", sv_event->DevUDN );
                printf( "ServiceID   =  %s\n", sv_event->ServiceID );
                printf( "StateVarName=  %s\n", sv_event->StateVarName );
                printf( "CurrentVal  =  %s\n", sv_event->CurrentVal );
            }
            break;

        case UPNP_CONTROL_GET_VAR_COMPLETE:
            {
                struct Upnp_State_Var_Complete *sv_event =
                    ( struct Upnp_State_Var_Complete * )Event;

                printf( "ErrCode     =  %d\n", sv_event->ErrCode );
                printf( "CtrlUrl     =  %s\n", sv_event->CtrlUrl );
                printf( "StateVarName=  %s\n", sv_event->StateVarName );
                printf( "CurrentVal  =  %s\n", sv_event->CurrentVal );
            }
            break;

            /*
               GENA 
             */
        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
            {
                struct Upnp_Subscription_Request *sr_event = ( struct Upnp_Subscription_Request * )Event;

                printf( "ServiceID   =  %s\n", sr_event->ServiceId );
                printf( "UDN         =  %s\n", sr_event->UDN );
                printf( "SID         =  %s\n", sr_event->Sid );
            }
            break;

        case UPNP_EVENT_RECEIVED:
            {
                struct Upnp_Event *e_event = ( struct Upnp_Event * )Event;
                char *xmlbuff = NULL;

                printf( "SID         =  %s\n", e_event->Sid );
                printf( "EventKey    =  %d\n", e_event->EventKey );
                xmlbuff = ixmlPrintDocument( e_event->ChangedVariables );
                printf( "ChangedVars =  %s\n", xmlbuff );
                ixmlFreeDOMString( xmlbuff );
                xmlbuff = NULL;
            }
            break;

        case UPNP_EVENT_RENEWAL_COMPLETE:
            {
                struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

                printf( "SID         =  %s\n", es_event->Sid);
                printf( "ErrCode     =  %d\n", es_event->ErrCode);
                printf( "TimeOut     =  %d\n", es_event->TimeOut);
            }
            break;

        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
            {
                struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

                printf( "SID         =  %s\n", es_event->Sid );
                printf( "ErrCode     =  %d\n", es_event->ErrCode );
                printf( "PublisherURL=  %s\n", es_event->PublisherUrl );
                printf( "TimeOut     =  %d\n", es_event->TimeOut );
            }
            break;

        case UPNP_EVENT_AUTORENEWAL_FAILED:
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
            {
                struct Upnp_Event_Subscribe *es_event = ( struct Upnp_Event_Subscribe * )Event;

                printf( "SID         =  %s\n", es_event->Sid );
                printf( "ErrCode     =  %d\n", es_event->ErrCode );
                printf( "PublisherURL=  %s\n", es_event->PublisherUrl );
                printf( "TimeOut     =  %d\n", es_event->TimeOut );
            }
            break;

    }
    printf( "----------------------------------------------------------------------\n" );
    printf( "======================================================================\n\n\n\n" );

}


/* encode 3 8-bit binary bytes as 4 '6-bit' characters */
void ILibencodeblock( unsigned char in[3], unsigned char out[4], int len )
{
	out[0] = cb64[ in[0] >> 2 ];
	out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
	out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
	out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

/*! \fn ILibBase64Encode(unsigned char* input, const int inputlen, unsigned char** output)
	\brief Base64 encode a stream adding padding and line breaks as per spec.
	\par
	\b Note: The encoded stream must be freed
	\param input The stream to encode
	\param inputlen The length of \a input
	\param output The encoded stream
	\returns The length of the encoded stream
*/
int ILibBase64Encode(unsigned char* input, const int inputlen, unsigned char** output)
{
	unsigned char* out;
	unsigned char* in;
	
	*output = (unsigned char*)malloc(((inputlen * 4) / 3) + 5);
	out = *output;
	in  = input;
	
	if (input == NULL || inputlen == 0)
	{
		*output = NULL;
		return 0;
	}
	
	while ((in+3) <= (input+inputlen))
	{
		ILibencodeblock(in, out, 3);
		in += 3;
		out += 4;
	}
	if ((input+inputlen)-in == 1)
	{
		ILibencodeblock(in, out, 1);
		out += 4;
	}
	else
	if ((input+inputlen)-in == 2)
	{
		ILibencodeblock(in, out, 2);
		out += 4;
	}
	*out = 0;
	
	return (int)(out-*output);
}

/* Decode 4 '6-bit' characters into 3 8-bit binary bytes */
void ILibdecodeblock( unsigned char in[4], unsigned char out[3] )
{
	out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
	out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
	out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

/*! \fn ILibBase64Decode(unsigned char* input, const int inputlen, unsigned char** output)
	\brief Decode a base64 encoded stream discarding padding, line breaks and noise
	\par
	\b Note: The decoded stream must be freed
	\param input The stream to decode
	\param inputlen The length of \a input
	\param output The decoded stream
	\returns The length of the decoded stream
*/
int ILibBase64Decode(unsigned char* input, const int inputlen, unsigned char** output)
{
	unsigned char* inptr;
	unsigned char* out;
	unsigned char v;
	unsigned char in[4];
	int i, len;
	
	if (input == NULL || inputlen == 0)
	{
		*output = NULL;
		return 0;
	}
	
	*output = (unsigned char*)malloc(((inputlen * 3) / 4) + 4);
	out = *output;
	inptr = input;
	
	while( inptr <= (input+inputlen) )
	{
		for( len = 0, i = 0; i < 4 && inptr <= (input+inputlen); i++ )
		{
			v = 0;
			while( inptr <= (input+inputlen) && v == 0 ) {
				v = (unsigned char) *inptr;
				inptr++;
				v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
				if( v ) {
					v = (unsigned char) ((v == '$') ? 0 : v - 61);
				}
			}
			if( inptr <= (input+inputlen) ) {
				len++;
				if( v ) {
					in[ i ] = (unsigned char) (v - 1);
				}
			}
			else {
				in[i] = 0;
			}
		}
		if( len )
		{
			ILibdecodeblock( in, out );
			out += len-1;
		}
	}
	*out = 0;
	return (int)(out-*output);
}

