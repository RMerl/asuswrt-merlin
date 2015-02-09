/* 
* IPv6ProtocolHdr.h
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


#ifndef _IPV6_PROTOCOL_DEFINES_
#define _IPV6_PROTOCOL_DEFINES_


#define IPV6HDR_TYPE_HOPBYHOP 0x0
#define IPV6HDR_TYPE_ROUTING 0x2B
#define IPV6HDR_TYPE_FRAGMENTATION 0x2C
#define IPV6HDR_TYPE_DESTOPTS 0x3c
#define IPV6HDR_TYPE_AUTHENTICATION 0x33
#define IPV6HDR_TYPE_ENCRYPTEDSECURITYPAYLOAD 0x34
#define MASK_IPV6_CS_SPEC 0x2


#define TCP_HEADER_TYPE 0x6
#define UDP_HEADER_TYPE 0x11
#define IPV6_ICMP_HDR_TYPE 0x2
#define IPV6_FLOWLABEL_BITOFFSET 9 

#define IPV6_MAX_CHAINEDHDR_BUFFBYTES 0x64
/*
// Size of Dest Options field of Destinations Options Header
// in bytes.
*/
#define IPV6_DESTOPTS_HDR_OPTIONSIZE 0x8 

//typedef  unsigned char UCHAR;
//typedef  unsigned short USHORT;
//typedef  unsigned long int ULONG;

typedef struct IPV6HeaderFormatTag
{
	UCHAR  ucVersionPrio;
	UCHAR  aucFlowLabel[3];
	USHORT usPayloadLength;
	UCHAR  ucNextHeader;
	UCHAR  ucHopLimit;
	ULONG  ulSrcIpAddress[4];
	ULONG  ulDestIpAddress[4];
}IPV6Header;

typedef struct IPV6RoutingHeaderFormatTag
{
	UCHAR ucNextHeader;
	UCHAR ucRoutingType;
	UCHAR ucNumAddresses;
	UCHAR ucNextAddress;
	ULONG ulReserved;
	//UCHAR aucAddressList[0];

}IPV6RoutingHeader;

typedef struct IPV6FragmentHeaderFormatTag
{
	UCHAR ucNextHeader;
	UCHAR ucReserved;
	USHORT usFragmentOffset;
	ULONG  ulIdentification;
}IPV6FragmentHeader;

typedef struct IPV6DestOptionsHeaderFormatTag
{
	UCHAR ucNextHeader;
	UCHAR ucHdrExtLen;
	UCHAR ucDestOptions[6];
	//UCHAR udExtDestOptions[0];
}IPV6DestOptionsHeader;

typedef struct IPV6HopByHopOptionsHeaderFormatTag
{
	UCHAR ucNextHeader;
	UCHAR ucMisc[3];
	ULONG ulJumboPayloadLen;
}IPV6HopByHopOptionsHeader;

typedef struct IPV6AuthenticationHeaderFormatTag
{
	UCHAR ucNextHeader;
	UCHAR ucLength;
	USHORT usReserved;
	ULONG  ulSecurityParametersIndex;
	//UCHAR  ucAuthenticationData[0]; 

}IPV6AuthenticationHeader;

typedef struct IPV6IcmpHeaderFormatTag
{
	UCHAR ucType;
	UCHAR ucCode;
	USHORT usChecksum;
	//UCHAR  ucIcmpMsg[0];

}IPV6IcmpHeader;

typedef enum _E_IPADDR_CONTEXT
{
	eSrcIpAddress,
	eDestIpAddress

}E_IPADDR_CONTEXT;



//Function Prototypes
BOOLEAN MatchSrcIpv6Address(S_CLASSIFIER_RULE *pstClassifierRule,IPV6Header *pstIpv6Header);
BOOLEAN MatchDestIpv6Address(S_CLASSIFIER_RULE *pstClassifierRule,IPV6Header *pstIpv6Header);

USHORT	IpVersion6(PMINI_ADAPTER Adapter, /**< Pointer to the driver control structure */
					PVOID pcIpHeader, /**<Pointer to the IP Hdr of the packet*/
					S_CLASSIFIER_RULE *pstClassifierRule );

VOID DumpIpv6Address(ULONG *puIpv6Address);
VOID DumpIpv6Header(IPV6Header *pstIpv6Header);

extern BOOLEAN MatchSrcPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushSrcPort);
extern BOOLEAN MatchDestPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushSrcPort);
extern BOOLEAN MatchProtocol(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucProtocol);


#endif
