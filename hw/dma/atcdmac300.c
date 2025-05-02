/*
 * Andes ATCDMAC300 (Andes Technology DMA Controller)
 *
 * Copyright (c) 2022 Andes Tech. Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/dma/atcdmac300.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "exec/memattrs.h"
#include "exec/address-spaces.h"
#include "hw/stream.h"
#include "hw/misc/riscv_iopmp_transaction_info.h"

/* #define DEBUG_ANDES_ATCDMAC300 */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ANDES_ATCDMAC300
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

static void atcdmac300_dma_int_stat_update(ATCDMAC300State *s, int status,
                                           int ch)
{
    s->IntStatus |= (1 << (status + ch));
}

static void atcdmac300_dma_reset_chan(ATCDMAC300State *s, int ch)
{
    if (s) {
        s->chan[ch].ChnCtrl &= ~(1 << CHAN_CTL_ENABLE);
        s->ChEN &= ~(1 << ch);
    }
}

static void atcdmac300_dma_reset(ATCDMAC300State *s)
{
    int ch;
    for (ch = 0; ch < ATCDMAC300_MAX_CHAN; ch++) {
        atcdmac300_dma_reset_chan(s, ch);
    }
}

static uint64_t atcdmac300_read(void *opaque, hwaddr offset, unsigned size)
{
    ATCDMAC300State *s = opaque;
    int ch = 0;
    uint64_t result = 0;

    if (offset >= 0x40) {
        ch = ATCDMAC300_GET_CHAN(offset);
        offset = ATCDMAC300_GET_OFF(offset, ch);
    }
    switch (offset) {
    case ATCDMAC300_DMA_IDREV:
        result = s->IdRev;
        break;
    case ATCDMAC300_DMA_CFG:
        result = s->DMACfg;
        break;
    case ATCDMAC300_DMAC_CTRL:
        break;
    case ATCDMAC300_CHN_ABT:
        break;
    case ATCDMAC300_INT_STATUS:
        result = s->IntStatus;
        break;
    case ATCDMAC300_CHAN_ENABLE:
        result = s->ChEN;
        break;
    case ATCDMAC300_CHAN_CTL:
        result = s->chan[ch].ChnCtrl;
        break;
    default:
        LOGGE("%s: Bad offset 0x%" HWADDR_PRIX "\n",
              __func__, offset);
        break;
    }
    LOG("### atcdmac300_read()=0x%lx, val=0x%lx\n", offset, result);
    return result;
}

static void transaction_info_push(StreamSink *sink, uint8_t *buf, bool eop)
{
    if (sink == NULL) {
        /* Do nothing if streamsink is not connected */
        return;
    }
    if (eop) {
        while (stream_push(sink, buf, sizeof(iopmp_transaction_info), true)
               == 0) {
            ;
        }
    } else {
        while (stream_push(sink, buf, sizeof(iopmp_transaction_info), false)
               == 0) {
            ;
        }
    }
}

static MemTxResult dma_iopmp_read(ATCDMAC300State *s, hwaddr addr, void *buf,
                                  hwaddr len,
                                  iopmp_transaction_info *transaction)
{
    MemTxResult result;
    if (s->iopmp_as) {
        if (s->transaction_info_sink) {
            transaction_info_push(s->transaction_info_sink,
                                  (uint8_t *)transaction, false);
        }
        MemTxAttrs dma_attrs = {.requester_id = transaction->sid};
        result = address_space_rw(s->iopmp_as, addr, dma_attrs,
                                  buf, len, false);
        if (s->transaction_info_sink) {
            transaction_info_push(s->transaction_info_sink,
                                  (uint8_t *)transaction, true);
        }
        return result;
    }
    cpu_physical_memory_read(addr, buf, len);
    return MEMTX_OK;
}

static MemTxResult dma_iopmp_write(ATCDMAC300State *s, hwaddr addr, void *buf,
                                   hwaddr len,
                                   iopmp_transaction_info *transaction)
{
    MemTxResult result = 0;
    if (s->iopmp_as) {
        if (s->transaction_info_sink) {
            transaction_info_push(s->transaction_info_sink,
                                  (uint8_t *)transaction, false);
        }
        MemTxAttrs dma_attrs = {.requester_id = transaction->sid};
        result = address_space_rw(s->iopmp_as, addr, dma_attrs,
                                  buf, len, true);
        if (s->transaction_info_sink) {
            transaction_info_push(s->transaction_info_sink,
                                  (uint8_t *)transaction, true);
        }
        return result;
    }
    cpu_physical_memory_write(addr, buf, len);
    return MEMTX_OK;
}

static void atcdmac300_thread_run_channel(void *opaque, int ch)
{
    ATCDMAC300State *s = opaque;
    int result;
    uint64_t src_addr, dst_addr;
    /* End address for AXI_BOUNDARY check */
    uint64_t src_end_addr, dst_end_addr;
    /* DMA register bit field */
    uint32_t src_addr_ctl, dst_addr_ctl, int_tc_mask, int_err_mask,
             int_abort_mask, burst_size, src_width, dst_width, src_inf, dst_inf;
    /* Internal computation */
    uint32_t remain_size_byte, dst_remain_byte, burst_size_transfer,
             src_burst_remain, src_width_byte, dst_width_byte,
             burst_size_byte, dma_remain_transfer_size, buf_index;
    uint32_t axi_src_len = 0, axi_dst_len = 0;
    uint8_t buf[ATCDMAC300_MAX_BURST_SIZE * 32];
    iopmp_transaction_info src_transaction, dst_transaction;

    if (((s->chan[ch].ChnCtrl >> CHAN_CTL_ENABLE) & 0x1) != 0x1) {
        return;
    }
    bql_lock();
    /* Read again after locked */
    if (((s->chan[ch].ChnCtrl >> CHAN_CTL_ENABLE) & 0x1) != 0x1) {
        bql_unlock();
        return;
    }
    src_inf = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_BUS_IDX) &
                CHAN_CTL_SRC_BUS_IDX_MASK;
    dst_inf = (s->chan[ch].ChnCtrl >> CHAN_CTL_DST_BUS_IDX) &
                CHAN_CTL_DST_BUS_IDX_MASK;
    src_transaction.sid = s->inf_sid[src_inf];
    dst_transaction.sid = s->inf_sid[dst_inf];

    src_width = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_WIDTH) &
                CHAN_CTL_SRC_WIDTH_MASK;
    dst_width = (s->chan[ch].ChnCtrl >> CHAN_CTL_DST_WIDTH) &
                CHAN_CTL_DST_WIDTH_MASK;
    burst_size = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_BURST_SZ) &
                 CHAN_CTL_SRC_BURST_SZ_MASK;
    src_addr = (s->chan[ch].ChnSrcAddrH << 32) |
                s->chan[ch].ChnSrcAddr;
    dst_addr = (s->chan[ch].ChnDstAddrH << 32) |
                s->chan[ch].ChnDstAddr;
    src_addr_ctl = (s->chan[ch].ChnCtrl >> CHAN_CTL_SRC_ADDR_CTL) &
                   CHAN_CTL_SRC_ADDR_CTL_MASK;
    dst_addr_ctl = (s->chan[ch].ChnCtrl >> CHAN_CTL_DST_ADDR_CTL) &
                   CHAN_CTL_DST_ADDR_CTL_MASK;

    src_width_byte = 1 << src_width;
    dst_width_byte = 1 << dst_width;
    dma_remain_transfer_size = s->chan[ch].ChnTranSize;
    remain_size_byte = dma_remain_transfer_size * src_width_byte;
    int_tc_mask = (s->chan[ch].ChnCtrl >> CHAN_CTL_INT_TC_MASK_POS) & 0x1;
    int_err_mask = (s->chan[ch].ChnCtrl >> CHAN_CTL_INT_ERR_MASK_POS) & 0x1;
    int_abort_mask = (s->chan[ch].ChnCtrl >> CHAN_CTL_INT_ABT_MASK_POS) & 0x1;
    burst_size_transfer = (1 << burst_size);
    burst_size_byte = burst_size_transfer * src_width_byte;
    if (remain_size_byte && burst_size < 11 &&
        src_width < 6 && dst_width < 6 &&
        (src_addr & (src_width_byte - 1)) == 0 &&
        (dst_addr & (dst_width_byte - 1)) == 0 &&
        (remain_size_byte & (dst_width_byte - 1)) == 0 &&
        (burst_size_byte & (dst_width_byte - 1)) == 0) {
        while (remain_size_byte > 0) {
            if (s->ChAbort & (1 << ch)) {
                /* check abort status before a dma brust start */
                s->ChAbort &= ~(1 << ch);
                atcdmac300_dma_reset_chan(s, ch);
                atcdmac300_dma_int_stat_update(s, INT_STATUS_ABT, ch);
                if (!int_abort_mask) {
                    qemu_irq_raise(s->irq);
                }
                bql_unlock();
                return;
            }
            int i;
            src_burst_remain = MIN(burst_size_transfer,
                                   dma_remain_transfer_size);
            dst_remain_byte = src_burst_remain * src_width_byte;
            buf_index = 0;
            memset(buf, 0, sizeof(buf));
            /* One DMA burst may need mutiple AXI bursts */
            while (src_burst_remain) {
                if (src_addr_ctl == 0) {
                    axi_src_len = MIN(src_burst_remain,
                                      AXI_BURST_INC_LEN_MAX + 1);
                    src_end_addr = src_width_byte * axi_src_len + src_addr;
                    if ((src_addr / AXI_BOUNDARY) !=
                         (src_end_addr / AXI_BOUNDARY)) {
                            src_end_addr = (src_end_addr / AXI_BOUNDARY)
                                           * AXI_BOUNDARY;
                            axi_src_len = (src_end_addr - src_addr) /
                                          src_width_byte;
                        }
                    /* Convert AXI signal to general IOPMP transaction */
                    src_transaction.start_addr = src_addr;
                    src_transaction.end_addr = src_end_addr - 1;
                }
                if (src_addr_ctl == 1) {
                    /* AXI does not support decrement type, use fixed type */
                    axi_src_len = 1;
                    src_transaction.start_addr = src_addr;
                    src_transaction.end_addr = src_addr + src_width_byte - 1;
                }
                if (src_addr_ctl == 2) {
                    axi_src_len = 1;
                    src_transaction.start_addr = src_addr;
                    src_transaction.end_addr = src_addr + src_width_byte - 1;
                }
                /* src_burst */
                for (i = 0; i < axi_src_len; i++) {
                    result = dma_iopmp_read(s, src_addr, &buf[buf_index],
                                            src_width_byte, &src_transaction);
                    buf_index += src_width_byte;
                    if (result != MEMTX_OK) {
                        s->ChAbort &= ~(1 << ch);
                        atcdmac300_dma_int_stat_update(s,
                            INT_STATUS_ERR, ch);
                        atcdmac300_dma_reset_chan(s, ch);
                        if (!int_err_mask) {
                            qemu_irq_raise(s->irq);
                        }
                        bql_unlock();
                        return;
                    }
                    if (src_addr_ctl == 0) {
                        src_addr += src_width_byte;
                    }
                    if (src_addr_ctl == 1) {
                        src_addr -= src_width_byte;
                    }
                }
                src_burst_remain -= axi_src_len;
                dma_remain_transfer_size -= axi_src_len;
                remain_size_byte -= axi_src_len * src_width_byte;
            }
            buf_index = 0;
            /* One src burst may need mutiple dst bursts*/
            while (dst_remain_byte > 0) {
                if (dst_addr_ctl == 0) {
                    axi_dst_len = (dst_remain_byte / dst_width_byte);
                    axi_dst_len = MIN(axi_dst_len, AXI_BURST_INC_LEN_MAX + 1);
                    dst_end_addr = dst_width_byte * axi_dst_len + dst_addr;
                    if ((dst_addr / AXI_BOUNDARY) !=
                        (dst_end_addr / AXI_BOUNDARY)) {
                        dst_end_addr = (dst_end_addr / AXI_BOUNDARY)
                                       * AXI_BOUNDARY;
                        axi_dst_len = (dst_end_addr - dst_addr) /
                                      dst_width_byte;
                    }
                    dst_transaction.start_addr = dst_addr;
                    dst_transaction.end_addr = dst_end_addr - 1;
                }
                if (dst_addr_ctl == 1) {
                    axi_dst_len = 1;
                    dst_transaction.start_addr = dst_addr;
                    dst_transaction.end_addr = dst_addr + dst_width_byte - 1;
                }
                if (dst_addr_ctl == 2) {
                    axi_dst_len = 1;
                    dst_transaction.start_addr = dst_addr;
                    dst_transaction.end_addr = dst_addr + dst_width_byte - 1;
                }
                for (i = 0; i < axi_dst_len; i++) {
                    result = dma_iopmp_write(s, dst_addr, &buf[buf_index],
                                             dst_width_byte, &dst_transaction);
                    buf_index += dst_width_byte;
                    if (result != MEMTX_OK) {
                        s->ChAbort &= ~(1 << ch);
                        atcdmac300_dma_int_stat_update(s,
                            INT_STATUS_ERR, ch);
                        atcdmac300_dma_reset_chan(s, ch);
                        if (!int_err_mask) {
                            qemu_irq_raise(s->irq);
                        }
                        bql_unlock();
                        return;
                    }
                    if (dst_addr_ctl == 0) {
                        dst_addr += dst_width_byte;
                    }
                    if (dst_addr_ctl == 1) {
                        dst_addr -= dst_width_byte;
                    }
                }
                dst_remain_byte -= dst_width_byte * axi_dst_len;
            }
        }
        /* DMA transfer complete */
        s->ChAbort &= ~(1 << ch);
        atcdmac300_dma_reset_chan(s, ch);
        atcdmac300_dma_int_stat_update(s, INT_STATUS_TC, ch);
        if (!int_tc_mask) {
            qemu_irq_raise(s->irq);
        }
        bql_unlock();
        return;
    } else {
        s->ChAbort &= ~(1 << ch);
        atcdmac300_dma_int_stat_update(s, INT_STATUS_ERR, ch);
        atcdmac300_dma_reset_chan(s, ch);
        if (!int_err_mask) {
            qemu_irq_raise(s->irq);
        }
        bql_unlock();
    }
}

static void *atcdmac300_thread_run(void *opaque)
{
    ATCDMAC300State *s = opaque;
    while (1) {
        qemu_mutex_lock(&s->thr_mutex);
        while (!s->ChEN) {
            qemu_cond_wait(&s->thr_cond, &s->thr_mutex);
        }
        qemu_mutex_unlock(&s->thr_mutex);
        for (int ch = 0; ch < ATCDMAC300_MAX_CHAN; ch++) {
            atcdmac300_thread_run_channel(opaque, ch);
        }
    }
    return NULL;
}

static void atcdmac300_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    ATCDMAC300State *s = opaque;
    int ch = 0;

    LOG("@@@ atcdmac300_write()=0x%lx, value=0x%lx\n", offset, value);

    if (offset >= 0x40) {
        ch = ATCDMAC300_GET_CHAN(offset);
        offset = ATCDMAC300_GET_OFF(offset, ch);
    }
    switch (offset) {
    case ATCDMAC300_INT_STATUS:
        /* Write 1 to clear */
        s->IntStatus &= ~value;
        if (!s->IntStatus) {
            qemu_irq_lower(s->irq);
        }
        break;
    case ATCDMAC300_DMAC_CTRL:
        atcdmac300_dma_reset(s);
        break;
    case ATCDMAC300_CHN_ABT:
        for (int i = 0; i < ATCDMAC300_MAX_CHAN; i++) {
            if (value & 0x1 && (s->chan[i].ChnCtrl & (1 << CHAN_CTL_ENABLE))) {
                s->ChAbort |= (0x1 << i);
            }
            value >>= 1;
        }
        break;
    case ATCDMAC300_CHAN_CTL:
        qemu_mutex_lock(&s->thr_mutex);
        /* Also write alias register ChEN */
        if (value & (1 << CHAN_CTL_ENABLE)) {
            s->ChEN |= (1 << ch);
        } else {
            s->ChEN &= ~(1 << ch);
        }
        s->chan[ch].ChnCtrl = value;
        /* Wakeup run thread */
        if (s->ChEN) {
            qemu_cond_signal(&s->thr_cond);
        }
        qemu_mutex_unlock(&s->thr_mutex);
        break;
    case ATCDMAC300_CHAN_TRAN_SZ:
        s->chan[ch].ChnTranSize = value;
        break;
    case ATCDMAC300_CHAN_SRC_ADDR:
        s->chan[ch].ChnSrcAddr = value;
        break;
    case ATCDMAC300_CHAN_SRC_ADDR_H:
        s->chan[ch].ChnSrcAddrH = value;
        break;
    case ATCDMAC300_CHAN_DST_ADDR:
        s->chan[ch].ChnDstAddr = value;
        break;
    case ATCDMAC300_CHAN_DST_ADDR_H:
        s->chan[ch].ChnDstAddrH = value;
        break;
    case ATCDMAC300_CHAN_LL_POINTER:
        s->chan[ch].ChnLLPointer = value;
        break;
    case ATCDMAC300_CHAN_LL_POINTER_H:
        s->chan[ch].ChnLLPointerH = value;
        break;
    default:
        LOGGE("%s: Bad offset 0x%" HWADDR_PRIX "\n",
              __func__, offset);
        break;
    }
}

static const MemoryRegionOps atcdmac300_ops = {
    .read = atcdmac300_read,
    .write = atcdmac300_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8
    }
};

static void atcdmac300_realize(DeviceState *dev, Error **errp)
{
    ATCDMAC300State *s = ATCDMAC300(dev);
    SysBusDevice *sbus = SYS_BUS_DEVICE(dev);
    memory_region_init_io(&s->mmio, OBJECT(dev), &atcdmac300_ops, s,
                          TYPE_ATCDMAC300, s->mmio_size);
    sysbus_init_mmio(sbus, &s->mmio);
    qemu_mutex_init(&s->thr_mutex);
    qemu_cond_init(&s->thr_cond);
    qemu_thread_create(&s->thread, "atcdmac_thread", atcdmac300_thread_run,
                       s, QEMU_THREAD_JOINABLE);
}

static void atcdmac300_init(Object *obj)
{
    ATCDMAC300State *s = ATCDMAC300(obj);
    SysBusDevice *sbus = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbus, &s->irq);
}

static Property atcdmac300_properties[] = {
    DEFINE_PROP_UINT32("mmio-size", ATCDMAC300State, mmio_size, 0x100000),
    DEFINE_PROP_UINT32("id-and-revision", ATCDMAC300State, IdRev,
                    (ATCDMAC300_PRODUCT_ID  << 8) |
                    ((ATCDMAC300_REV_MAJOR & 0xF) << 4) |
                    ((ATCDMAC300_REV_MINOR & 0xF))),
    DEFINE_PROP_UINT32("inturrupt-status", ATCDMAC300State, IntStatus, 0),
    DEFINE_PROP_UINT32("dmac-configuration", ATCDMAC300State,
                       DMACfg, 0xc3404108),
    DEFINE_PROP_END_OF_LIST(),
};

static void atcdmac300_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    device_class_set_props(k, atcdmac300_properties);
    k->realize = atcdmac300_realize;
}

static const TypeInfo atcdmac300_info = {
    .name          = TYPE_ATCDMAC300,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ATCDMAC300State),
    .class_init    = atcdmac300_class_init,
    .instance_init = atcdmac300_init,
};

void
atcdmac300_create(ATCDMAC300State *atcdmac, const char *name,
                  hwaddr addr, hwaddr mmio_size, qemu_irq irq)
{
    qdev_prop_set_uint32(DEVICE(atcdmac), "mmio-size", mmio_size);

    sysbus_realize(SYS_BUS_DEVICE(atcdmac), NULL);
    sysbus_mmio_map(SYS_BUS_DEVICE(atcdmac), 0, addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(atcdmac), 0, irq);
    return;
}

static void atcdmac300_register_types(void)
{
    type_register_static(&atcdmac300_info);
}

void atcdmac300_connect_iopmp(DeviceState *dev, AddressSpace *iopmp_as,
                              StreamSink *transaction_info_sink,
                              uint32_t inf0_sid, uint32_t inf1_sid)
{
    ATCDMAC300State *s = ATCDMAC300(dev);
    s->iopmp_as = iopmp_as;
    s->transaction_info_sink = transaction_info_sink;
    s->inf_sid[0] = inf0_sid;
    s->inf_sid[1] = inf1_sid;
}

type_init(atcdmac300_register_types)
