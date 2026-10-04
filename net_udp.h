//=============================================================================
// net_udp.h - UDP-only wrapper around UNAPI TCP/IP for MSX COCO multiplayer
//
// Phase B: thin layer over tcpip_* calls. Mirrors lib/network.h conventions
// but UDP-only (no TCP, no TLS).
//
// CRITICAL Z80/SDCC pattern (from msxai2/net_groq.c):
//   - tcpip_unapi_*_parms structs are NEVER passed/allocated on the stack.
//   - All asm helpers (tcpip_* in unapi_tcp.asm) are declared with
//     __sdcccall(0) via unapi_tcp.h (args on stack instead of registers).
//   - We allocate ONE module-scope global per struct (s_IpInfo, s_UdpParms,
//     s_UdpState) and reuse it across all calls.
//
// DOS2 .com build: NO page-2 dance. Running as an MSX-DOS 2 .com (the proven
// msxai2 recipe) we never switch our own page-2 slot at boot, so the UNAPI
// EXTBIO chain sees the machine in its normal DOS state and tcpip_* calls work
// when invoked plainly — exactly like msxai2/net_groq. ForceRamAddr=0x8000 keeps
// our DATA/BSS in page 2 (out of the page-1 segment the TSR swaps). The old
// ROM_KONAMI "page-2 dance" (Sys_GetPageSlot/Sys_SetPageSlot around every call)
// was a cartridge-only workaround and is intentionally removed here.
//
// Header-only and static — single translation unit (msx_coco.c) avoids
// linker duplicates without adding another file to AddSources.
//=============================================================================
#ifndef NET_UDP_H
#define NET_UDP_H

#include "msxgl.h"
#include "unapi_tcp.h"

// Return codes (mirror network.h spirit; bit ints kept distinct to ease debug).
#define NET_OK              0
#define NET_ERR_NO_NETWORK  1
#define NET_ERR_NO_CONN     2
#define NET_ERR_IO          3

// Global struct buffers — SDCC Z80: NEVER allocate these on the stack.
// 'static' at file scope keeps them per-translation-unit; with msx_coco.c as
// the only includer that means exactly one instance project-wide.
static tcpip_unapi_ip_info        s_IpInfo;
static tcpip_unapi_udp_dtg_parms  s_UdpParms;
static tcpip_unapi_udp_state      s_UdpState;

//-----------------------------------------------------------------------------
// Net_Init -- TCPIP_ENUMERATE; NET_OK iff at least one impl registered.
//-----------------------------------------------------------------------------
static u8 Net_Init(void)
{
	int count = tcpip_enumerate();   // resets active_implementation to 0
	return (count > 0) ? NET_OK : NET_ERR_NO_NETWORK;
}

//-----------------------------------------------------------------------------
// Net_GetLocalIP -- fills 4 bytes via tcpip_get_ipinfo (local_ip field).
// Returns NET_OK on success, NET_ERR_IO if the UNAPI call fails.
//-----------------------------------------------------------------------------
static u8 Net_GetLocalIP(u8* ipOut)
{
	if (tcpip_get_ipinfo(&s_IpInfo) != ERR_OK)
	{
		ipOut[0] = 0; ipOut[1] = 0; ipOut[2] = 0; ipOut[3] = 0;
		return NET_ERR_IO;
	}
	ipOut[0] = (u8)s_IpInfo.local_ip[0];
	ipOut[1] = (u8)s_IpInfo.local_ip[1];
	ipOut[2] = (u8)s_IpInfo.local_ip[2];
	ipOut[3] = (u8)s_IpInfo.local_ip[3];
	return NET_OK;
}

//-----------------------------------------------------------------------------
// Net_GetBroadcastIP -- computes localIP | ~mask using the same s_IpInfo
// fetched by tcpip_get_ipinfo. Returns NET_OK on success.
//-----------------------------------------------------------------------------
static u8 Net_GetBroadcastIP(u8* bcastOut)
{
	u8 i;
	if (tcpip_get_ipinfo(&s_IpInfo) != ERR_OK)
	{
		bcastOut[0] = 0; bcastOut[1] = 0; bcastOut[2] = 0; bcastOut[3] = 0;
		return NET_ERR_IO;
	}
	for (i = 0; i < 4; ++i)
	{
		u8 ip   = (u8)s_IpInfo.local_ip[i];
		u8 mask = (u8)s_IpInfo.subnet_mask[i];
		bcastOut[i] = (u8)(ip | (u8)~mask);
	}
	return NET_OK;
}

//-----------------------------------------------------------------------------
// Net_UdpOpen -- TCPIP_UDP_OPEN with CONNTYPE_RESIDENT (stays open until
// explicit close). Returns conn number (>=0) or -1 on error.
//-----------------------------------------------------------------------------
static i16 Net_UdpOpen(u16 port)
{
	int connNum = 0;
	if (tcpip_udp_open((int)port, CONNTYPE_RESIDENT, &connNum) != ERR_OK)
		return (i16)-1;
	return (i16)connNum;
}

//-----------------------------------------------------------------------------
// Net_UdpClose -- TCPIP_UDP_CLOSE.
//-----------------------------------------------------------------------------
static u8 Net_UdpClose(i16 conn)
{
	return (tcpip_udp_close((int)conn) == ERR_OK) ? NET_OK : NET_ERR_IO;
}

//-----------------------------------------------------------------------------
// Net_UdpSend -- TCPIP_UDP_SEND. Fills the GLOBAL s_UdpParms (NEVER stack).
//-----------------------------------------------------------------------------
static u8 Net_UdpSend(i16 conn, const u8* dstIp, u16 dstPort, const u8* data, u16 len)
{
	s_UdpParms.dest_ip[0] = dstIp[0];
	s_UdpParms.dest_ip[1] = dstIp[1];
	s_UdpParms.dest_ip[2] = dstIp[2];
	s_UdpParms.dest_ip[3] = dstIp[3];
	s_UdpParms.dest_port  = (int)dstPort;
	s_UdpParms.data_length = (int)len;
	return (tcpip_udp_send((int)conn, &s_UdpParms, (char*)data) == ERR_OK) ? NET_OK : NET_ERR_IO;
}

//-----------------------------------------------------------------------------
// Net_UdpRecv -- TCPIP_UDP_RCV. Reuses GLOBAL s_UdpParms (NEVER stack).
// Returns bytes read (0 if none/error). On success, fills fromIp/fromPort.
//-----------------------------------------------------------------------------
static u16 Net_UdpRecv(i16 conn, u8* buf, u16 maxLen, u8* fromIp, u16* fromPort)
{
	if (tcpip_udp_rcv((int)conn, (char*)buf, (int)maxLen, &s_UdpParms) != ERR_OK)
		return 0;
	fromIp[0]  = (u8)s_UdpParms.dest_ip[0];
	fromIp[1]  = (u8)s_UdpParms.dest_ip[1];
	fromIp[2]  = (u8)s_UdpParms.dest_ip[2];
	fromIp[3]  = (u8)s_UdpParms.dest_ip[3];
	*fromPort  = (u16)s_UdpParms.dest_port;
	return (u16)s_UdpParms.data_length;
}

//-----------------------------------------------------------------------------
// Net_UdpHasData -- TCPIP_UDP_STATE; returns num_of_pend_dtg (>0 if data
// waiting). Uses GLOBAL s_UdpState (NEVER stack).
//-----------------------------------------------------------------------------
static u8 Net_UdpHasData(i16 conn)
{
	if (tcpip_udp_state((int)conn, &s_UdpState) != ERR_OK)
		return 0;
	return (u8)s_UdpState.num_of_pend_dtg;
}

#endif // NET_UDP_H
