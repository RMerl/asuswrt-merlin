/* 
* headers.h
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


#ifndef __HEADERS_H__
#define __HEADERS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/if_arp.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/etherdevice.h>
#include <net/ip.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>

#include <linux/version.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/kthread.h>
#endif
#include <linux/tcp.h>
#include <linux/udp.h>
#ifndef BCM_SHM_INTERFACE
#include <linux/usb.h>
#endif
#ifdef BECEEM_TARGET

#include <mac_common.h>
#include <msg_Dsa.h>
#include <msg_Dsc.h>
#include <msg_Dsd.h>
#include <sch_definitions.h>
using namespace Beceem;
#ifdef ENABLE_CORRIGENDUM2_UPDATE
extern B_UINT32 g_u32Corr2MacFlags;
#endif
#endif

#include<Common/Typedefs.h>
#include<Common/Version.h>
#include<Common/Macros.h>
#include<Common/HostMIBSInterface.h>
#include<Common/cntrl_SignalingInterface.h>
#include<Common/PHSDefines.h>
#include<Common/led_control.h>
#include<Common/Ioctl.h>
#include<Common/nvm.h>
#include<Common/target_params.h>
#include<Common/Adapter.h>
#include<Common/CmHost.h>
#include<Common/DDRInit.h>
#include<Common/Debug.h>
#include<Common/HostMibs.h>
#include<Common/IPv6ProtocolHdr.h>
#include<Common/osal_misc.h>
#include<Common/PHSModule.h>
#include<Common/Protocol.h>
#include<Common/Prototypes.h>
#include<Common/Queue.h>
#include<Common/vendorspecificextn.h> 

#ifndef BCM_SHM_INTERFACE

#include<Interface/usb/InterfaceMacros.h>
#include<Interface/usb/InterfaceAdapter.h>
#include<Interface/usb/InterfaceIsr.h>
#include<Interface/usb/Interfacemain.h>
#include<Interface/usb/InterfaceMisc.h>
#include<Interface/usb/InterfaceRx.h>
#include<Interface/usb/InterfaceTx.h>
#endif
#include<Interface/InterfaceIdleMode.h>
#include<Interface/InterfaceInit.h>

#ifdef BCM_SHM_INTERFACE
#include <linux/cpe_config.h>

#ifdef GDMA_INTERFACE
#include <Interface/shm/GdmaInterface.h>
#include "../../../../common/include/symphony.h"
#else
#include <Interface/shm/virtual_interface.h>

#endif



#endif

#endif
