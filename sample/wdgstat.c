/*
 * wdgstat.c - show the status of the NDISWDG module (bound state and traffic counters)
 *
 * Copyright (c) 2026, Martin Iturbide
 * All rights reserved. BSD 3-Clause License, see LICENSE.
 *
 * 32-bit OS/2 program. It opens \\DEV\\NDISWDG$ and sends the generic IOCTL
 * category 0xA5, function 1 (see src\ndis.h, WDGSTATUS).
 */
#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>

#pragma pack(1)
typedef struct {
    USHORT  size;
    USHORT  version;
    USHORT  flags;
    USHORT  moduleId;
    char    name[16];
    USHORT  macId;
    USHORT  protId;
    ULONG   counter[13];
} WDGSTATUS;
#pragma pack()

static const char *names[13] = {
    "protocol -> MAC  Request",
    "protocol -> MAC  TransmitChain",
    "protocol -> MAC  TransferData",
    "protocol -> MAC  ReceiveRelease",
    "protocol -> MAC  IndicationOn",
    "protocol -> MAC  IndicationOff",
    "MAC -> protocol  RequestConfirm",
    "MAC -> protocol  TransmitConfirm",
    "MAC -> protocol  ReceiveLookahead",
    "MAC -> protocol  IndicationComplete",
    "MAC -> protocol  ReceiveChain",
    "MAC -> protocol  Status",
    "refused (not bound yet)"
};

int main(void)
{
    HFILE     hf;
    ULONG     action, dataLen = 0;
    APIRET    rc;
    WDGSTATUS st;
    int       i;

    for (i = 0; i < (int)sizeof(st); i++)
        ((char *)&st)[i] = 0;
    rc = DosOpen("\\DEV\\NDISWDG$", &hf, &action, 0, 0, OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);
    if (rc != 0) {
        printf("wdgstat: cannot open \\DEV\\NDISWDG$ (rc=%lu). Is NDISWDG.OS2 loaded?\n", (unsigned long)rc);
        return 1;
    }
    rc = DosDevIOCtl(hf, 0xA5, 0x01, NULL, 0, NULL, &st, sizeof(st), &dataLen);
    DosClose(hf);
    if (rc != 0) {
        printf("wdgstat: status IOCTL failed (rc=%lu)\n", (unsigned long)rc);
        return 1;
    }

    if (st.size != sizeof(st)) {
        printf("wdgstat: the loaded driver does not answer the status IOCTL (old build?). Reboot with the new NDISWDG.OS2.\n");
        return 1;
    }
    printf("NDISWDG version %x.%02x, module %s (id %u)\n",
           st.version >> 8, st.version & 0xFF, st.name, (unsigned)st.moduleId);
    printf("  bound to the MAC below:   %s (MAC module id %u)\n", (st.flags & 1) ? "yes" : "NO", (unsigned)st.macId);
    printf("  bound to the protocol above: %s (protocol module id %u)\n", (st.flags & 2) ? "yes" : "NO", (unsigned)st.protId);
    for (i = 0; i < 13; i++)
        printf("  %-36s %10lu\n", names[i], (unsigned long)st.counter[i]);
    return 0;
}
