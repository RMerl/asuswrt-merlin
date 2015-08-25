// pherr.h: interface for the CBaseThread class.
//
//////////////////////////////////////////////////////////////////////
/*! \file pherr.h
*  \author skyvense
*  \date   2009-09-14
*  \brief PHDDNS 客户端实现	
*/

#ifndef __PHERR__H__
#define __PHERR__H__



//! 客户端状态
enum MessageCodes
{
	okConnected = 0,		//tcp已经连接上服务器
	okAuthpassed,			//验证已经通过
	okDomainListed,			//拿到域名列表（仅已激活DDNS花生壳的）
	okDomainsRegistered,	//所有域名已注册，下面开始定时keepalive udp
	okKeepAliveRecved,		//收到一个
	okConnecting,			//正在连接服务器
	okRetrievingMisc,		//no use
	okRedirecting,			//正在转向另一个服务器

	errorConnectFailed = 0x100,	//tcp连接失败，同时也暗示登陆失败
	errorSocketInitialFailed,	//socket初始化出错 ，同时也暗示登陆失败
	errorAuthFailed,			//用户名或者密码错误，下面还有个服务器忙，也是在这个时候可能返回
	errorDomainListFailed,		//列出域名列表出错（仅已激活DDNS花生壳的），同时也暗示登陆失败
	errorDomainRegisterFailed,	//某条域名注册出错，同时也暗示登陆失败
	errorUpdateTimeout,			//no use
	errorKeepAliveError,		//no use
	errorRetrying,
	errorAuthBusy,				//服务器忙
	errorStatDetailInfoFailed,  //获取用户信息或者详细域名列表失败，同时也暗示登陆失败
	

	okNormal = 0x120,
	okNoData,					//keepalive错误
	okServerER,					//keepalive错误
	errorOccupyReconnect,		//正在重新连接服务器
};

#endif // __PHERR__H__
