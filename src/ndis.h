/*
 * ndis.h - NDIS 2.0.1 definitions used by the NDISWDG pass-through module
 *
 * Copyright (c) 2026, Martin Iturbide
 * All rights reserved. BSD 3-Clause License, see LICENSE.
 *
 * Written from the public 3Com/Microsoft LAN Manager NDIS 2.0.1 specification
 * (chapters 3 to 6 and appendix A). Structures are packed (build with -zp1).
 * All calls between modules use the FAR PASCAL convention of the specification.
 */
#ifndef NDIS_H
#define NDIS_H

typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
typedef void   __far   *LPVOID;
typedef BYTE   __far   *LPBYTE;
typedef WORD   __far   *LPWORD;

/* ---- return codes (appendix A) ------------------------------------------ */
#define NDIS_SUCCESS               0x0000
#define NDIS_WAIT_FOR_RELEASE      0x0001
#define NDIS_REQUEST_QUEUED        0x0002
#define NDIS_FRAME_NOT_RECOGNIZED  0x0003
#define NDIS_FRAME_REJECTED        0x0004
#define NDIS_FORWARD_FRAME         0x0005
#define NDIS_OUT_OF_RESOURCE       0x0006
#define NDIS_INVALID_PARAMETER     0x0007
#define NDIS_INVALID_FUNCTION      0x0008
#define NDIS_NOT_SUPPORTED         0x0009
#define NDIS_DRIVER_NOT_INIT       0x0022
#define NDIS_GENERAL_FAILURE       0x00FF

/* ---- system request opcodes --------------------------------------------- */
#define SYSREQ_INITIATE_BIND       1
#define SYSREQ_BIND                2
#define SYSREQ_INITIATE_PREBIND    3
#define SYSREQ_INITIATE_UNBIND     4
#define SYSREQ_UNBIND              5

/* ---- module function flags and protocol levels -------------------------- */
#define MODFLAG_BINDS_UPPER        0x00000001UL
#define MODFLAG_BINDS_LOWER        0x00000002UL
#define LEVEL_MAC                  1
#define IFACE_MAC                  1

/* ---- Protocol Manager (chapter 6, OS/2 IOCTL interface) ----------------- */
#define PM_IOCTL_CATEGORY          0x81
#define PM_IOCTL_FUNCTION          0x58
#define PM_GET_INFO                1
#define PM_REGISTER_MODULE         2

/* ---- function pointer types (FAR PASCAL, last parameter = callee's DS) --- */
typedef WORD (__far __pascal *PFN_SYSREQ)(LPVOID p1, LPVOID p2, WORD p3, WORD opcode, WORD ds);
typedef WORD (__far __pascal *PFN_REQUEST)(WORD protId, WORD reqHandle, WORD param1, DWORD param2, WORD opcode, WORD macDS);
typedef WORD (__far __pascal *PFN_TXCHAIN)(WORD protId, WORD reqHandle, LPVOID txDescr, WORD macDS);
typedef WORD (__far __pascal *PFN_XFER)(LPWORD bytesCopied, WORD frameOffset, LPVOID tdDescr, WORD macDS);
typedef WORD (__far __pascal *PFN_RXREL)(WORD reqHandle, WORD macDS);
typedef WORD (__far __pascal *PFN_INDCTL)(WORD macDS);

typedef WORD (__far __pascal *PFN_REQCONF)(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD request, WORD protDS);
typedef WORD (__far __pascal *PFN_TXCONF)(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD protDS);
typedef WORD (__far __pascal *PFN_RXLOOK)(WORD macId, WORD frameSize, WORD bytesAvail, LPVOID buffer, LPBYTE indicate, WORD protDS);
typedef WORD (__far __pascal *PFN_INDCOMP)(WORD macId, WORD protDS);
typedef WORD (__far __pascal *PFN_RXCHAIN)(WORD macId, WORD frameSize, WORD reqHandle, LPVOID rxDescr, LPBYTE indicate, WORD protDS);
typedef WORD (__far __pascal *PFN_STATUS)(WORD macId, WORD param1, LPBYTE indicate, WORD opcode, WORD protDS);

/* ---- common characteristics table (spec chapter 4) ---------------------- */
typedef struct {
    WORD        size;               /* size of this table in bytes (64)          */
    BYTE        ndisMajor;          /* BCD, 02                                    */
    BYTE        ndisMinor;          /* BCD, 00                                    */
    WORD        reserved;
    BYTE        modMajor;           /* module version, BCD                        */
    BYTE        modMinor;
    DWORD       flags;              /* MODFLAG_*                                  */
    char        name[16];           /* module name, ASCIIZ                        */
    BYTE        upperLevel;
    BYTE        upperIface;
    BYTE        lowerLevel;
    BYTE        lowerIface;
    WORD        moduleId;           /* filled in by the Protocol Manager          */
    WORD        moduleDS;           /* data selector of this module               */
    PFN_SYSREQ  sysReq;             /* system request entry point                 */
    LPVOID      svcChars;           /* service-specific characteristics           */
    LPVOID      svcStatus;          /* service-specific status                    */
    LPVOID      upperDisp;          /* upper dispatch table                       */
    LPVOID      lowerDisp;          /* lower dispatch table                       */
    LPVOID      reserved1;
    LPVOID      reserved2;
} CCT;

/* ---- MAC upper dispatch table: entry points a protocol calls ------------ */
typedef struct {
    LPVOID      cct;                /* back pointer to the common table           */
    PFN_REQUEST request;
    PFN_TXCHAIN transmitChain;
    PFN_XFER    transferData;
    PFN_RXREL   receiveRelease;
    PFN_INDCTL  indicationOn;
    PFN_INDCTL  indicationOff;
} UPPERDISP;

/* ---- protocol lower dispatch table: entry points a MAC calls ------------ */
typedef struct {
    LPVOID      cct;
    DWORD       ifaceFlags;
    PFN_REQCONF requestConfirm;
    PFN_TXCONF  transmitConfirm;
    PFN_RXLOOK  receiveLookahead;
    PFN_INDCOMP indicationComplete;
    PFN_RXCHAIN receiveChain;
    PFN_STATUS  status;
} LOWERDISP;

/* ---- MAC service-specific tables (spec chapter 4) ------------------------ */
/* The wedge starts with valid placeholder tables so that nobody ever meets a NULL
 * pointer; at bind time it points to the real MAC's tables instead. */
typedef struct {
    WORD        maxAddrs;
    WORD        curAddrs;
} MCASTLIST;

typedef struct {                    /* MAC service-specific characteristics        */
    WORD        length;
    char        typeName[16];
    WORD        addrLen;
    BYTE        permAddr[16];
    BYTE        curAddr[16];
    DWORD       funcAddr;
    LPVOID      mcastList;
    DWORD       linkSpeed;
    DWORD       flags;
    WORD        maxFrame;
    DWORD       txBufCap;
    WORD        txBlock;
    DWORD       rxBufCap;
    WORD        rxBlock;
    BYTE        vendorCode[3];
    BYTE        vendorAdapter;
    LPVOID      vendorDesc;
    WORD        irq;
    WORD        txQueueDepth;
    WORD        maxBlocks;
} MACSVC;

typedef struct {                    /* MAC service-specific status                 */
    WORD        length;
    DWORD       diagTime;
    DWORD       macStatus;
    WORD        filter;
    LPVOID      mediaStats;
    DWORD       clearTime;
    DWORD       stat[24];           /* statistics, 0xFFFFFFFF = not supported      */
} MACSTAT;

/* ---- PROTOCOL.INI memory image (spec chapter 4) ------------------------- */
typedef struct {                    /* one [module] section                       */
    LPVOID      next;
    LPVOID      prev;
    char        name[16];
    /* keyword entries follow */
} MODCFG;

typedef struct {                    /* one keyword = value line                   */
    LPVOID      next;
    LPVOID      prev;
    char        keyword[16];
    WORD        numParams;
    /* parameters follow: WORD type (0 numeric, 1 string), WORD len, value[len] */
} KEYWORD;

#define PARAM_NUMERIC  0
#define PARAM_STRING   1

typedef struct {                    /* BindingsList passed to RegisterModule      */
    WORD        count;
    char        name[1][16];
} BINDLIST1;

/* Protocol Manager request block */
typedef struct {
    WORD        opcode;
    WORD        status;
    LPVOID      pointer1;
    LPVOID      pointer2;
    WORD        word1;
} PMREQ;

/* ---- OS/2 request packet header and INIT packet ------------------------- */
typedef struct {
    BYTE        length;
    BYTE        unit;
    BYTE        command;
    WORD        status;
    BYTE        reserved[8];
} RPHEADER;

typedef struct {
    RPHEADER    h;
    BYTE        units;              /* in: number of units (out: units)           */
    LPVOID      devHelp;            /* in: DevHlp entry point                     */
    LPVOID      args;               /* in: pointer to the DEVICE= line arguments  */
    BYTE        drive;
} RPINIT_IN;

typedef struct {
    RPHEADER    h;
    BYTE        units;
    WORD        endCode;            /* out: end of resident code                  */
    WORD        endData;            /* out: end of resident data                  */
    LPVOID      bpb;
    WORD        status2;
} RPINIT_OUT;

#define RPSTAT_DONE   0x0100
#define RPSTAT_ERROR  0x8000
#define RPERR_GENERAL 0x000C

/* ---- status interface (generic IOCTL, used by sample\wdgstat.c) ----------- */
#define WDG_IOCTL_CATEGORY  0xA5
#define WDG_IOCTL_GETSTATUS 0x01
#define WDG_NCOUNTERS       13

typedef struct {
    WORD    size;                   /* size of this structure                    */
    WORD    version;                /* BCD, 0x0200 = 2.00                        */
    WORD    flags;                  /* bit 0: bound to the MAC, bit 1: protocol bound */
    WORD    moduleId;               /* our Protocol Manager module ID            */
    char    name[16];               /* our module name                           */
    WORD    macId;                  /* module ID of the MAC below                */
    WORD    protId;                 /* module ID of the protocol above           */
    DWORD   counter[WDG_NCOUNTERS]; /* see wdg_counter_names in the sample       */
} WDGSTATUS;

/* generic IOCTL request packet (OS/2 2.x device driver interface) */
typedef struct {
    RPHEADER    h;
    BYTE        category;
    BYTE        function;
    LPVOID      parm;
    LPVOID      data;
    WORD        sfn;
    WORD        parmLen;
    WORD        dataLen;
} RPIOCTL;

#define RPCMD_INIT      0
#define RPCMD_OPEN      13
#define RPCMD_CLOSE     14
#define RPCMD_IOCTL     16

void wdgGetStatus(WDGSTATUS __far *out);

/* ---- shared between the modules ----------------------------------------- */
extern LPVOID     fptab[13];        /* entry points with loader fixups, see hdr.asm */
extern CCT        g_cct;
extern UPPERDISP  g_upperDisp;
extern LOWERDISP  g_lowerDisp;

WORD __far __pascal __loadds SysReq(LPVOID p1, LPVOID p2, WORD p3, WORD opcode, WORD ds);

/* protocol -> MAC direction (upper dispatch table) */
WORD __far __pascal __loadds WdgRequest(WORD protId, WORD reqHandle, WORD param1, DWORD param2, WORD opcode, WORD macDS);
WORD __far __pascal __loadds WdgTransmitChain(WORD protId, WORD reqHandle, LPVOID txDescr, WORD macDS);
WORD __far __pascal __loadds WdgTransferData(LPWORD bytesCopied, WORD frameOffset, LPVOID tdDescr, WORD macDS);
WORD __far __pascal __loadds WdgReceiveRelease(WORD reqHandle, WORD macDS);
WORD __far __pascal __loadds WdgIndicationOn(WORD macDS);
WORD __far __pascal __loadds WdgIndicationOff(WORD macDS);

/* MAC -> protocol direction (lower dispatch table) */
WORD __far __pascal __loadds WdgRequestConfirm(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD request, WORD protDS);
WORD __far __pascal __loadds WdgTransmitConfirm(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD protDS);
WORD __far __pascal __loadds WdgReceiveLookahead(WORD macId, WORD frameSize, WORD bytesAvail, LPVOID buffer, LPBYTE indicate, WORD protDS);
WORD __far __pascal __loadds WdgIndicationComplete(WORD macId, WORD protDS);
WORD __far __pascal __loadds WdgReceiveChain(WORD macId, WORD frameSize, WORD reqHandle, LPVOID rxDescr, LPBYTE indicate, WORD protDS);
WORD __far __pascal __loadds WdgStatus(WORD macId, WORD param1, LPBYTE indicate, WORD opcode, WORD protDS);

#endif
