/*
 * ndis.c - NDISWDG run-time part: binding and pass-through of the MAC interface
 *
 * Copyright (c) 2026, Martin Iturbide
 * All rights reserved. BSD 3-Clause License, see LICENSE.
 *
 * NDISWDG is a "MAC-layer entity" (NDIS 2.0.1, chapter 6): it binds at its lower
 * boundary to a real MAC driver, and presents a MAC interface at its upper
 * boundary to a protocol. Every call and indication that crosses it is forwarded
 * unchanged; only the module identifiers are exchanged:
 *   - towards the MAC the wedge is the protocol and uses its own module ID,
 *   - towards the protocol the wedge is the MAC and reports its own module ID.
 * Request handles are passed through untouched, so no handle table is needed.
 *
 * Everything here may run at interrupt time: no blocking, no C library, and no
 * large stack use. The entry points are FAR PASCAL (callee removes the parameters,
 * the last parameter is the caller's idea of our data selector, which is ignored
 * because __loadds reloads DS from the one data group this module has).
 */
#pragma code_seg("_TEXT", "CODE")

#include "ndis.h"

/* ---- tables the other modules see (filled in by init.c) ----------------- */
CCT        g_cct;
UPPERDISP  g_upperDisp;
LOWERDISP  g_lowerDisp;

/* ---- what we copied out of the neighbours at bind time ------------------ */
static struct {
    int          bound;
    WORD         macId;
    WORD         macDS;
    PFN_REQUEST  request;
    PFN_TXCHAIN  transmitChain;
    PFN_XFER     transferData;
    PFN_RXREL    receiveRelease;
    PFN_INDCTL   indicationOn;
    PFN_INDCTL   indicationOff;
} low;

static struct {
    int          bound;
    WORD         protId;
    WORD         protDS;
    PFN_REQCONF  requestConfirm;
    PFN_TXCONF   transmitConfirm;
    PFN_RXLOOK   receiveLookahead;
    PFN_INDCOMP  indicationComplete;
    PFN_RXCHAIN  receiveChain;
    PFN_STATUS   status;
} up;

/* slot the lower MAC writes the pointer to its own table into (Bind) */
static LPVOID  tabSlot;

/* traffic counters (diagnostics, read through the status IOCTL); incremented without
 * locking, so a count may be off by one if an interrupt hits between the two halves */
enum { C_REQUEST, C_TXCHAIN, C_XFER, C_RXREL, C_INDON, C_INDOFF,
       C_REQCONF, C_TXCONF, C_RXLOOK, C_INDCOMP, C_RXCHAIN, C_STATUS, C_REFUSED };
static DWORD  counter[WDG_NCOUNTERS];
#define COUNT(i)  (counter[i]++)

void wdgGetStatus(WDGSTATUS __far *out)
{
    int i;

    out->size     = sizeof(WDGSTATUS);
    out->version  = 0x0200;
    out->flags    = (low.bound ? 1 : 0) | (up.bound ? 2 : 0);
    out->moduleId = g_cct.moduleId;
    for (i = 0; i < 16; i++)
        out->name[i] = g_cct.name[i];
    out->macId    = low.macId;
    out->protId   = up.protId;
    for (i = 0; i < WDG_NCOUNTERS; i++)
        out->counter[i] = counter[i];
}

/* ======================================================================== */
/*  System requests (called by the Protocol Manager and by the protocol)    */
/* ======================================================================== */

/* InitiateBind from the Protocol Manager: bind to the MAC described by 'lowerTab'. */
static WORD initiateBind(LPVOID lowerTab, WORD lastBind)
{
    CCT __far       *mac;
    CCT __far       *macOut;
    UPPERDISP __far *ud;
    WORD             rc;

    (void)lastBind;

    if (lowerTab == 0 || low.bound)
        return NDIS_GENERAL_FAILURE;        /* nothing to bind to, or bound already */

    mac = (CCT __far *)lowerTab;
    if (mac->sysReq == 0)
        return NDIS_GENERAL_FAILURE;

    tabSlot = 0;
    /* Bind (opcode 2): give the MAC our table, get its table back through tabSlot. */
    rc = mac->sysReq((LPVOID)&g_cct, (LPVOID)&tabSlot, 0, SYSREQ_BIND, mac->moduleDS);
    if (rc != NDIS_SUCCESS)
        return rc;

    macOut = (tabSlot != 0) ? (CCT __far *)tabSlot : mac;
    ud = (UPPERDISP __far *)macOut->upperDisp;
    if (ud == 0 || ud->request == 0)
        return NDIS_GENERAL_FAILURE;

    /* copy the entry points and data selector now (spec: do not use them before the bind succeeded) */
    low.macId          = macOut->moduleId;
    low.macDS          = macOut->moduleDS;
    low.request        = ud->request;
    low.transmitChain  = ud->transmitChain;
    low.transferData   = ud->transferData;
    low.receiveRelease = ud->receiveRelease;
    low.indicationOn   = ud->indicationOn;
    low.indicationOff  = ud->indicationOff;

    /* from now on the protocol above sees the MAC's own characteristics and status */
    g_cct.svcChars  = macOut->svcChars;
    g_cct.svcStatus = macOut->svcStatus;

    low.bound = 1;
    return NDIS_SUCCESS;
}

/* Bind from the protocol above: exchange tables, return our table through 'tabAddr'. */
static WORD bindUpper(LPVOID protTab, LPVOID tabAddr)
{
    CCT __far       *prot;
    LOWERDISP __far *ld;

    if (!low.bound)
        return NDIS_DRIVER_NOT_INIT;        /* the lower MAC is bound first */
    if (up.bound || protTab == 0 || tabAddr == 0)
        return NDIS_GENERAL_FAILURE;        /* like a MAC: one upper bind only */

    prot = (CCT __far *)protTab;
    ld = (LOWERDISP __far *)prot->lowerDisp;
    if (ld == 0)
        return NDIS_GENERAL_FAILURE;

    up.protId             = prot->moduleId;
    up.protDS             = prot->moduleDS;
    up.requestConfirm     = ld->requestConfirm;
    up.transmitConfirm    = ld->transmitConfirm;
    up.receiveLookahead   = ld->receiveLookahead;
    up.indicationComplete = ld->indicationComplete;
    up.receiveChain       = ld->receiveChain;
    up.status             = ld->status;

    *(LPVOID __far *)tabAddr = (LPVOID)&g_cct;
    up.bound = 1;
    return NDIS_SUCCESS;
}

WORD __far __pascal __loadds SysReq(LPVOID p1, LPVOID p2, WORD p3, WORD opcode, WORD ds)
{
    (void)ds;
    switch (opcode) {
    case SYSREQ_INITIATE_BIND:      /* p1 = pad, p2 = lower module's table, p3 = last-bind flag */
        return initiateBind(p2, p3);
    case SYSREQ_BIND:               /* p1 = caller's table, p2 = where to return ours */
        return bindUpper(p1, p2);
    default:                        /* prebind, unbind: this module is static */
        return NDIS_INVALID_FUNCTION;
    }
}

/* ======================================================================== */
/*  Protocol -> MAC direction: the MAC upper dispatch table                 */
/* ======================================================================== */

WORD __far __pascal __loadds WdgRequest(WORD protId, WORD reqHandle, WORD param1, DWORD param2, WORD opcode, WORD macDS)
{
    (void)protId; (void)macDS;
    COUNT(C_REQUEST);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.request(g_cct.moduleId, reqHandle, param1, param2, opcode, low.macDS);
}

WORD __far __pascal __loadds WdgTransmitChain(WORD protId, WORD reqHandle, LPVOID txDescr, WORD macDS)
{
    (void)protId; (void)macDS;
    COUNT(C_TXCHAIN);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.transmitChain(g_cct.moduleId, reqHandle, txDescr, low.macDS);
}

WORD __far __pascal __loadds WdgTransferData(LPWORD bytesCopied, WORD frameOffset, LPVOID tdDescr, WORD macDS)
{
    (void)macDS;
    COUNT(C_XFER);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.transferData(bytesCopied, frameOffset, tdDescr, low.macDS);
}

WORD __far __pascal __loadds WdgReceiveRelease(WORD reqHandle, WORD macDS)
{
    (void)macDS;
    COUNT(C_RXREL);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.receiveRelease(reqHandle, low.macDS);
}

WORD __far __pascal __loadds WdgIndicationOn(WORD macDS)
{
    (void)macDS;
    COUNT(C_INDON);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.indicationOn(low.macDS);
}

WORD __far __pascal __loadds WdgIndicationOff(WORD macDS)
{
    (void)macDS;
    COUNT(C_INDOFF);
    if (!low.bound) { COUNT(C_REFUSED); return NDIS_GENERAL_FAILURE; }
    return low.indicationOff(low.macDS);
}

/* ======================================================================== */
/*  MAC -> protocol direction: the protocol lower dispatch table            */
/* ======================================================================== */

WORD __far __pascal __loadds WdgRequestConfirm(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD request, WORD protDS)
{
    (void)protId; (void)macId; (void)protDS;
    COUNT(C_REQCONF);
    if (!up.bound)
        return NDIS_SUCCESS;
    return up.requestConfirm(up.protId, g_cct.moduleId, reqHandle, status, request, up.protDS);
}

WORD __far __pascal __loadds WdgTransmitConfirm(WORD protId, WORD macId, WORD reqHandle, WORD status, WORD protDS)
{
    (void)protId; (void)macId; (void)protDS;
    COUNT(C_TXCONF);
    if (!up.bound)
        return NDIS_SUCCESS;
    return up.transmitConfirm(up.protId, g_cct.moduleId, reqHandle, status, up.protDS);
}

WORD __far __pascal __loadds WdgReceiveLookahead(WORD macId, WORD frameSize, WORD bytesAvail, LPVOID buffer, LPBYTE indicate, WORD protDS)
{
    (void)macId; (void)protDS;
    COUNT(C_RXLOOK);
    if (!up.bound)
        return NDIS_FRAME_NOT_RECOGNIZED;
    return up.receiveLookahead(g_cct.moduleId, frameSize, bytesAvail, buffer, indicate, up.protDS);
}

WORD __far __pascal __loadds WdgIndicationComplete(WORD macId, WORD protDS)
{
    (void)macId; (void)protDS;
    COUNT(C_INDCOMP);
    if (!up.bound)
        return NDIS_SUCCESS;
    return up.indicationComplete(g_cct.moduleId, up.protDS);
}

WORD __far __pascal __loadds WdgReceiveChain(WORD macId, WORD frameSize, WORD reqHandle, LPVOID rxDescr, LPBYTE indicate, WORD protDS)
{
    (void)macId; (void)protDS;
    COUNT(C_RXCHAIN);
    if (!up.bound)
        return NDIS_FRAME_NOT_RECOGNIZED;
    return up.receiveChain(g_cct.moduleId, frameSize, reqHandle, rxDescr, indicate, up.protDS);
}

WORD __far __pascal __loadds WdgStatus(WORD macId, WORD param1, LPBYTE indicate, WORD opcode, WORD protDS)
{
    (void)macId; (void)protDS;
    COUNT(C_STATUS);
    if (!up.bound)
        return NDIS_SUCCESS;
    return up.status(g_cct.moduleId, param1, indicate, opcode, up.protDS);
}
