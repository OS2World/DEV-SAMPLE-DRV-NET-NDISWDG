/*
 * init.c - NDISWDG strategy entry, CONFIG.SYS initialization and registration
 *
 * Copyright (c) 2026, Martin Iturbide
 * All rights reserved. BSD 3-Clause License, see LICENSE.
 *
 * The device driver INIT command runs at ring 3 in protected mode, so the normal
 * DOS calls are available. This code:
 *   1. reads the options on the DEVICE= line (/V /Q /W /AA),
 *   2. opens the Protocol Manager (\DEV\PROTMAN$) and fetches the PROTOCOL.INI image,
 *   3. finds the [section] that has DRIVERNAME = NDISWDG$ and reads its BINDINGS,
 *   4. fills in the characteristics tables and registers the module.
 * After INIT nothing is left to do here: the Protocol Manager drives the binding
 * through SysReq() in ndis.c.
 *
 * Built with -zu (SS != DS): never pass the address of a local variable to a
 * function. All buffers used with DOS calls are static.
 */
#pragma code_seg("_TEXT", "CODE")

#include "ndis.h"
#include "version.h"

/* ---- symbols from hdr.asm / tail.asm ------------------------------------ */
extern WORD dgroup_sel;                 /* selector of our data group (loader fixup)  */
extern BYTE end_of_code;                /* last byte of the resident code             */
extern BYTE end_of_data;                /* last byte of the resident data             */

/* ---- DOS calls (DOSCALLS via os2.lib) ----------------------------------- */
extern WORD __far __pascal DosOpen(char __far *name, WORD __far *hf, WORD __far *action,
                                   DWORD size, WORD attr, WORD flags, WORD mode, DWORD rsvd);
extern WORD __far __pascal DosClose(WORD hf);
extern WORD __far __pascal DosRead(WORD hf, void __far *buf, WORD len, WORD __far *got);
extern WORD __far __pascal DosWrite(WORD hf, void __far *buf, WORD len, WORD __far *done);
extern WORD __far __pascal DosDevIOCtl(void __far *data, void __far *parm, WORD func, WORD cat, WORD hf);
extern WORD __far __pascal DosBeep(WORD freq, WORD ms);

#define MKFP(sel, off)  ((LPVOID)(((DWORD)(WORD)(sel) << 16) | (WORD)(off)))

/* ---- options ------------------------------------------------------------ */
static int fVerbose, fQuiet, fWait, fBeep;

/* ---- static work areas (no stack buffers, see the note above) ----------- */
static WORD     done;                   /* DosWrite result */
static WORD     got;                    /* DosRead result  */
static BYTE     key;
static WORD     hProtman, action;
static PMREQ    pmReq;
static char     line[100];
static char     hex[8];
static struct { WORD count; char name[16]; } bindList;
static MCASTLIST  placeholderMcast;
static MACSVC     placeholderSvc;
static MACSTAT    placeholderStat;
static const char vendorText[] = "NDISWDG pass-through";

/* ---- tiny helpers (no C library in a device driver) --------------------- */
static int slen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void scopy(char *d, const char *s, int max)       /* copy at most max-1, always terminate */
{
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static int upc(int c)
{
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

static int sieq(const char __far *a, const char *b)       /* case-insensitive compare */
{
    while (*a && *b) {
        if (upc(*a) != upc(*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Print one message: "NDISWDG: text", wrapped at 79 columns, prefix on every line. */
static void emit(const char *text)
{
    int len, n, cut, i, k;
    const int prefix = sizeof(DRIVER_NAME ": ") - 1;      /* 9 */

    if (fQuiet) return;
    len = slen(text);
    while (len > 0) {
        n = len;
        if (n > 79 - prefix - 2) {
            cut = 79 - prefix - 2;
            while (cut > 0 && text[cut] != ' ') cut--;
            n = cut ? cut : 79 - prefix - 2;
        }
        k = 0;
        for (i = 0; i < prefix; i++) line[k++] = (DRIVER_NAME ": ")[i];
        for (i = 0; i < n; i++)      line[k++] = text[i];
        line[k++] = '\r';
        line[k++] = '\n';
        DosWrite(1, (LPVOID)line, k, (LPWORD)&done);
        text += n; len -= n;
        if (len > 0 && *text == ' ') { text++; len--; }
    }
}

static void emitCode(const char *what, WORD code)
{
    static const char digits[] = "0123456789ABCDEF";
    static char       msgbuf[160];
    int               i;

    scopy(msgbuf, what, 140);
    i = slen(msgbuf);
    msgbuf[i++] = ' ';
    msgbuf[i++] = '0'; msgbuf[i++] = 'x';
    hex[0] = digits[(code >> 12) & 15]; hex[1] = digits[(code >> 8) & 15];
    hex[2] = digits[(code >> 4) & 15];  hex[3] = digits[code & 15];
    msgbuf[i++] = hex[0]; msgbuf[i++] = hex[1]; msgbuf[i++] = hex[2]; msgbuf[i++] = hex[3];
    msgbuf[i++] = '.';
    msgbuf[i] = 0;
    emit(msgbuf);
}

#define FAILED  "NDIS pass-through module could not be loaded. "

/* ---- DEVICE= line options: /V verbose, /Q quiet, /W wait, /AA beep ------- */
static void parseArgs(const char __far *p)
{
    if (p == 0) return;
    while (*p == ' ' || *p == '\t') p++;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;     /* skip the driver file name */
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '\r' || *p == '\n') break;
        if (*p == '/' || *p == '-') {
            p++;
            if (upc(p[0]) == 'A' && upc(p[1]) == 'A') { fBeep = 1; p += 2; }
            else {
                switch (upc(*p)) {
                case 'V': fVerbose = 1; break;
                case 'Q': fQuiet = 1;   break;
                case 'W': fWait = 1;    break;
                default:  break;                       /* unknown options are ignored */
                }
                if (*p) p++;
            }
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
    }
}

/* ---- PROTOCOL.INI image -------------------------------------------------- */

/* first parameter of a keyword when it is a string, else NULL */
static const char __far *firstString(KEYWORD __far *k)
{
    WORD __far *p;
    if (k->numParams == 0) return 0;
    p = (WORD __far *)((BYTE __far *)k + sizeof(KEYWORD));
    return (p[0] == PARAM_STRING) ? (const char __far *)(p + 2) : 0;
}

static KEYWORD __far *firstKeyword(MODCFG __far *m)
{
    KEYWORD __far *k = (KEYWORD __far *)((BYTE __far *)m + sizeof(MODCFG));
    return (m->next == (LPVOID)k) ? 0 : k;      /* a section without keywords is followed by the next section */
}

static KEYWORD __far *findKeyword(MODCFG __far *m, const char *name)
{
    KEYWORD __far *k;
    for (k = firstKeyword(m); k != 0; k = (KEYWORD __far *)k->next)
        if (sieq(k->keyword, name))
            return k;
    return 0;
}

/* the section whose DRIVERNAME is our device name, or NULL */
static MODCFG __far *findOurSection(LPVOID image)
{
    MODCFG __far  *m;
    KEYWORD __far *k;
    const char __far *s;

    for (m = (MODCFG __far *)image; m != 0; m = (MODCFG __far *)m->next) {
        k = findKeyword(m, "DRIVERNAME");
        if (k != 0 && (s = firstString(k)) != 0 && sieq(s, DEVICE_NAME))
            return m;
    }
    return 0;
}

/* ---- Protocol Manager request through the generic IOCTL ------------------ */
static WORD pmCall(WORD opcode, LPVOID p1, LPVOID p2)
{
    WORD rc;

    pmReq.opcode   = opcode;
    pmReq.status   = 0;
    pmReq.pointer1 = p1;
    pmReq.pointer2 = p2;
    pmReq.word1    = 0;
    rc = DosDevIOCtl(0, (LPVOID)&pmReq, PM_IOCTL_FUNCTION, PM_IOCTL_CATEGORY, hProtman);
    if (rc != 0)
        return rc;
    return pmReq.status;
}

/* ---- tables --------------------------------------------------------------- */
/* valid placeholder service-specific tables (replaced by the MAC's own at bind time) */
static void buildPlaceholders(void)
{
    int i;

    placeholderMcast.maxAddrs = 0;
    placeholderMcast.curAddrs = 0;

    placeholderSvc.length   = sizeof(MACSVC);
    scopy(placeholderSvc.typeName, "DIX+802.3", 16);
    placeholderSvc.addrLen  = 6;
    placeholderSvc.mcastList = MKFP(dgroup_sel, (WORD)&placeholderMcast);
    placeholderSvc.flags    = 0x00000001UL;     /* broadcast */
    placeholderSvc.maxFrame = 1514;
    placeholderSvc.vendorCode[0] = 0xFF; placeholderSvc.vendorCode[1] = 0xFF; placeholderSvc.vendorCode[2] = 0xFF;
    placeholderSvc.vendorDesc = MKFP(dgroup_sel, (WORD)vendorText);
    placeholderSvc.txQueueDepth = 1;
    placeholderSvc.maxBlocks    = 8;

    placeholderStat.length    = sizeof(MACSTAT);
    placeholderStat.diagTime  = 0xFFFFFFFFUL;
    placeholderStat.macStatus = 0x00000007UL | 0x00000008UL | 0x00000010UL;  /* operational, bound, open */
    placeholderStat.clearTime = 0xFFFFFFFFUL;
    for (i = 0; i < 24; i++)
        placeholderStat.stat[i] = 0xFFFFFFFFUL;
}

static void buildTables(void)
{
    buildPlaceholders();

    g_cct.size       = sizeof(CCT);
    g_cct.ndisMajor  = 0x02;
    g_cct.ndisMinor  = 0x00;
    g_cct.modMajor   = VER_MAJOR_BCD;
    g_cct.modMinor   = VER_MINOR_BCD;
    g_cct.flags      = MODFLAG_BINDS_UPPER | MODFLAG_BINDS_LOWER;
    g_cct.upperLevel = LEVEL_MAC;
    g_cct.upperIface = IFACE_MAC;
    g_cct.lowerLevel = LEVEL_MAC;
    g_cct.lowerIface = IFACE_MAC;
    g_cct.moduleDS   = dgroup_sel;
    g_cct.sysReq     = (PFN_SYSREQ)fptab[0];    /* fptab: addresses fixed up by the loader (Ring 0 selector) */
    g_cct.svcChars   = MKFP(dgroup_sel, (WORD)&placeholderSvc);    /* replaced by the MAC's at bind time */
    g_cct.svcStatus  = MKFP(dgroup_sel, (WORD)&placeholderStat);
    g_cct.upperDisp  = MKFP(dgroup_sel, (WORD)&g_upperDisp);
    g_cct.lowerDisp  = MKFP(dgroup_sel, (WORD)&g_lowerDisp);

    g_upperDisp.cct            = MKFP(dgroup_sel, (WORD)&g_cct);
    g_upperDisp.request        = (PFN_REQUEST)fptab[1];
    g_upperDisp.transmitChain  = (PFN_TXCHAIN)fptab[2];
    g_upperDisp.transferData   = (PFN_XFER)fptab[3];
    g_upperDisp.receiveRelease = (PFN_RXREL)fptab[4];
    g_upperDisp.indicationOn   = (PFN_INDCTL)fptab[5];
    g_upperDisp.indicationOff  = (PFN_INDCTL)fptab[6];

    g_lowerDisp.cct                = MKFP(dgroup_sel, (WORD)&g_cct);
    g_lowerDisp.ifaceFlags         = 7;         /* handles non-LLC, specific and nonspecific LLC frames */
    g_lowerDisp.requestConfirm     = (PFN_REQCONF)fptab[7];
    g_lowerDisp.transmitConfirm    = (PFN_TXCONF)fptab[8];
    g_lowerDisp.receiveLookahead   = (PFN_RXLOOK)fptab[9];
    g_lowerDisp.indicationComplete = (PFN_INDCOMP)fptab[10];
    g_lowerDisp.receiveChain       = (PFN_RXCHAIN)fptab[11];
    g_lowerDisp.status             = (PFN_STATUS)fptab[12];
}

/* ---- read the configuration and register with the Protocol Manager -------- */
static int registerModule(void)
{
    MODCFG __far     *sec;
    KEYWORD __far    *kb;
    const char __far *s;
    WORD              rc, nb, i;
    static char       text[80];

    if (DosOpen((char __far *)"\\DEV\\PROTMAN$", (LPWORD)&hProtman, (LPWORD)&action, 0, 0, 0x0001, 0x0040, 0) != 0) {
        emit(FAILED "Unable to open the Protocol Manager (PROTMAN$). PROTMAN.OS2 must be loaded before this driver.");
        return 0;
    }

    rc = pmCall(PM_GET_INFO, 0, 0);
    if (rc != 0 || pmReq.pointer1 == 0) {
        emitCode(FAILED "GetProtocolManagerInfo failed, code", rc);
        DosClose(hProtman);
        return 0;
    }

    sec = findOurSection(pmReq.pointer1);
    if (sec == 0) {
        emit(FAILED "PROTOCOL.INI has no section with DRIVERNAME = " DEVICE_NAME ".");
        DosClose(hProtman);
        return 0;
    }
    for (i = 0; i < 15 && sec->name[i]; i++)
        g_cct.name[i] = sec->name[i];
    g_cct.name[i] = 0;

    /* BINDINGS = <name of the MAC to wrap> */
    kb = findKeyword(sec, "BINDINGS");
    s  = (kb != 0) ? firstString(kb) : 0;
    if (s == 0) {
        emit(FAILED "The PROTOCOL.INI section has no BINDINGS keyword naming the MAC to wrap.");
        DosClose(hProtman);
        return 0;
    }
    nb = 0;
    for (i = 0; i < 15 && s[i]; i++)
        bindList.name[i] = s[i];
    bindList.name[i] = 0;
    nb = 1;
    bindList.count = nb;
    if (kb->numParams > 1)
        emit("Only the first BINDINGS entry is used, the others are ignored.");

    buildTables();

    rc = pmCall(PM_REGISTER_MODULE, MKFP(dgroup_sel, (WORD)&g_cct), MKFP(dgroup_sel, (WORD)&bindList));
    DosClose(hProtman);
    if (rc != 0) {
        emitCode(FAILED "RegisterModule failed, code", rc);
        return 0;
    }

    if (fVerbose) {
        scopy(text, "Registered module ", 40);
        scopy(text + slen(text), g_cct.name, 16);
        scopy(text + slen(text), ", it will bind to ", 20);
        scopy(text + slen(text), bindList.name, 16);
        scopy(text + slen(text), ".", 2);
        emit(text);
    }
    return 1;
}

/* ---- INIT command --------------------------------------------------------- */
static void initDriver(RPINIT_IN __far *rpIn)
{
    RPINIT_OUT __far *rpOut = (RPINIT_OUT __far *)rpIn;
    int ok;

    parseArgs((const char __far *)rpIn->args);

    if (fVerbose) {
        emit(DRIVER_NAME " " VER_STRING " - NDIS 2.0.1 pass-through (wedge) module");
    }

    ok = registerModule();

    if (ok) {
        rpOut->units   = 0;
        rpOut->endCode = (WORD)&end_of_code;
        rpOut->endData = (WORD)&end_of_data;
        rpOut->h.status = RPSTAT_DONE;
        if (fVerbose)
            emit("Driver loaded without warnings.");
        if (fWait && !fQuiet) {
            emit("Press any key to continue.");
            DosRead(0, (LPVOID)&key, 1, (LPWORD)&got);
        }
        if (fBeep)
            DosBeep(1000, 120);
    } else {
        rpOut->units   = 0;
        rpOut->endCode = 0;                     /* nothing stays resident */
        rpOut->endData = 0;
        rpOut->h.status = RPSTAT_DONE | RPSTAT_ERROR | RPERR_GENERAL;
        if (fWait && !fQuiet) {
            emit("Press any key to continue.");
            DosRead(0, (LPVOID)&key, 1, (LPWORD)&got);
        }
    }
}

/* ---- strategy (called from hdr.asm with the request packet on the stack) -- */
void __cdecl strategy(RPHEADER __far *rp)
{
    RPIOCTL __far *io;

    switch (rp->command) {
    case RPCMD_INIT:
        initDriver((RPINIT_IN __far *)rp);
        break;

    case RPCMD_IOCTL:                           /* generic IOCTL: category 0xA5, function 1 = get status */
        io = (RPIOCTL __far *)rp;
        if (io->category == WDG_IOCTL_CATEGORY && io->function == WDG_IOCTL_GETSTATUS
            && io->data != 0 && io->dataLen >= sizeof(WDGSTATUS)) {
            wdgGetStatus((WDGSTATUS __far *)io->data);
            rp->status = RPSTAT_DONE;
        } else {
            rp->status = RPSTAT_DONE | RPSTAT_ERROR | 0x0003;      /* unknown command */
        }
        break;

    default:                                    /* open, close and the rest: nothing to do */
        rp->status = RPSTAT_DONE;
        break;
    }
}
