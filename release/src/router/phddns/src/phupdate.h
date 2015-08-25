// phupdate.h: interface for the CBaseThread class.
//
//////////////////////////////////////////////////////////////////////
/*! \file phupdate.h
*  \author skyvense
*  \date   2009-09-14
*  \brief PHDDNS 客户端实现	
*/

#ifndef _CUPDATECORE_H
#define _CUPDATECORE_H

#include "phglobal.h"

//! 花生壳DDNS客户端实现基类
/*!
*/
	

	//! 步进调用，配置好参数后需要立即进入此函数，函数返回下次需要执行本函数的时间（秒数）
	int phddns_step(PHGlobal *phglobal);	

	//! 停止花生壳DDNS更新，重新配置参数后可进入另一个
	void phddns_stop(PHGlobal *phglobal);

	//! 初始化socket
	BOOL InitializeSockets(PHGlobal *phglobal);
	//! 关闭所有socket
	BOOL DestroySockets(PHGlobal *phglobal);
	//! 与DDNS服务器连接的TCP主过程
	int ExecuteUpdate(PHGlobal *phglobal);
	//! 启动UDP“连接”
	BOOL BeginKeepAlive(PHGlobal *phglobal);
	//! 发送一个UDP心跳包
	BOOL SendKeepAlive(PHGlobal *phglobal, int opCode);
	//! 接收心跳包返回
	int RecvKeepaliveResponse(PHGlobal *phglobal);

#endif