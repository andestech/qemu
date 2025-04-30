/*
 * Andes ACE GDB stub
 *
 * Copyright (c) 2023 Andes Technology Corp.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _ANDES_ACE_GDB_
#define _ANDES_ACE_GDB_
#include "gdbstub/andes_ace_gdb.h"

__attribute__((weak)) int32_t qemu_ace_get_filename_for_gdb(CPUState *x, char *y)
{
    return -1;
}

int gdb_handle_query_rcmd_andes_query(GArray *params, void *user_ctx)
{
    /* nds query target only can be 'SID' or 'OCD' */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_TARGET_CMD)) {
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)NDS_QUERY_TARGET_RES,
                     strlen(NDS_QUERY_TARGET_RES));
        gdb_put_strbuf();
        return 0;
    }
    /* nds query endian only can be '[target]: [LE|BE]' */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_ENDIAN_CMD)) {
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)NDS_QUERY_ENDIAN_RES,
                     strlen(NDS_QUERY_ENDIAN_RES));
        gdb_put_strbuf();
        return 0;
    }
    /* return empty id like ICEman */
    if (!g_strcmp0((const char *)gdbserver_state.mem_buf->data,
        NDS_QUERY_CPUID_CMD)) {
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)NDS_QUERY_CPUID_RES,
                     strlen(NDS_QUERY_CPUID_RES));
        gdb_put_strbuf();
        return 0;
    }
    /* gdb client may send 'nds ace os.name', os.name may 'Linux' */
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_ACE_CMD, strlen(NDS_ACE_CMD))) {
        /*
         * Should put libacetool.so file length as
         * filename since ICEman did it
         */
        char filename[64];
        if (qemu_ace_get_filename_for_gdb(gdbserver_state.g_cpu, filename)) {
            gdb_put_packet("E04");
            return 0;
        }
        gdb_memtohex(gdbserver_state.str_buf,
                     (guint8 *)filename, strlen(filename));
        gdb_put_strbuf();
        return 0;
    }
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_VA_CMD, strlen(NDS_VA_CMD))) {
        /*
         * This command is implemented as a hmp command so we will let
         * qemu monitor handle the command.
         */
        return -1;
    }
    if (!strncmp((const char *)gdbserver_state.mem_buf->data,
        NDS_OTHER_CMD, strlen(NDS_OTHER_CMD))) {
        gdb_put_packet("OK");
        return 0;
    }
    /*
     * Returning -1 will let the caller(gdb_handle_query_rcmd) knows
     * the command was not handled here and will write this command
     * to the chardev of qemu monitor.
     */
    return -1;
}
#endif
