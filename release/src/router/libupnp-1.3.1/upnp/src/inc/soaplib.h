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

#ifndef SOAPLIB_H
#define SOAPLIB_H 


//SOAP module API to be called in Upnp-Dk API
/****************************************************************************
*	Function :	soap_device_callback
*
*	Parameters :
*		  IN http_parser_t *parser : Parsed request received by the device
*		  IN http_message_t* request :	HTTP request 
*		  INOUT SOCKINFO *info :	socket info
*
*	Description :	This is a callback called by minisever after receiving 
*		the request from the control point. This function will start 
*		processing the request. It calls handle_invoke_action to handle the
*		SOAP action
*
*	Return :	void
*
*	Note :
****************************************************************************/
void soap_device_callback( 
						  IN http_parser_t *parser, 
						  IN http_message_t* request, 
						  INOUT SOCKINFO *info );


/****************************************************************************
*	Function :	SoapSendAction
*
*	Parameters :
*		IN char* action_url :	device contrl URL 
*		IN char *service_type :	device service type
*		IN IXML_Document *action_node : SOAP action node	
*		OUT IXML_Document **response_node :	SOAP response node
*
*	Description :	This function is called by UPnP API to send the SOAP 
*		action request and waits till it gets the response from the device
*		pass the response to the API layer
*
*	Return :	int
*		returns UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
int SoapSendAction( 
		IN char* action_url, 
		IN char *service_type,
		IN IXML_Document *action_node,
		OUT IXML_Document **response_node );

/****************************************************************************
*	Function :	SoapSendActionEx
*
*	Parameters :
*		IN char* action_url :	device contrl URL 
*		IN char *service_type :	device service type
		IN IXML_Document *Header: Soap header
*		IN IXML_Document *action_node : SOAP action node ( SOAP body)	
*		OUT IXML_Document **response_node :	SOAP response node
*
*	Description :	This function is called by UPnP API to send the SOAP 
*		action request and waits till it gets the response from the device
*		pass the response to the API layer. This action is similar to the 
*		the SoapSendAction with only difference that it allows users to 
*		pass the SOAP header along the SOAP body ( soap action request)
*
*	Return :	int
*		returns UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
int SoapSendActionEx(
		IN char * ActionURL, 
		IN char *ServiceType, 
		IN IXML_Document *Header,
		IN IXML_Document *ActNode , 
		OUT IXML_Document **RespNode) ;

/****************************************************************************
*	Function :	SoapGetServiceVarStatus
*
*	Parameters :
*			IN  char * action_url :	Address to send this variable 
*									query message.
*			IN  char *var_name : Name of the variable.
*			OUT char **var_value :	Output value.
*
*	Description :	This function creates a status variable query message 
*		send it to the specified URL. It also collect the response.
*
*	Return :	int
*
*	Note :
****************************************************************************/
int SoapGetServiceVarStatus(
				IN char * ActionURL, 
				IN DOMString VarName, 
				OUT DOMString *StVar) ;   

extern const char* ContentTypeHeader;

#endif //SOAPLIB_H
