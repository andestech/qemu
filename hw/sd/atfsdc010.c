/*
 * Andes Secure Digital (SD) host controller, ATFSDC010.
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"
#include "migration/vmstate.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/sd/sd.h"
#include "hw/sd/sdcard_legacy.h"
#include "hw/sd/atfsdc010.h"

/* #define DEBUG_ATFSDC010 */
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ATFSDC010
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

#define FTSDC_CMD_INDEX     0x3f
#define FTSDC_CMD_RESPONSE  (1 << 6)
#define FTSDC_CMD_LONGRESP  (1 << 7)
#define FTSDC_CMD_APPCMD    (1 << 8)
#define FTSDC_CMD_ENABLE    (1 << 9)
#define FTSDC_CMD_SDCRST    (1 << 10)

#define FTSDC_DATA_WRITE              (1 << 4)
#define FTSDC_DATA_DMAENABLE          (1 << 5)
#define FTSDC_DATA_ENABLE             (1 << 6)

#define FTSDC_STATUS_CMDCRCFAIL       (1 << 0)
#define FTSDC_STATUS_DATACRCFAIL      (1 << 1)
#define FTSDC_STATUS_CMDTIMEOUT       (1 << 2)
#define FTSDC_STATUS_DATATIMEOUT      (1 << 3)
#define FTSDC_STATUS_CMDRESPEND       (1 << 4)
#define FTSDC_STATUS_DATABLOCKEND     (1 << 5)
#define FTSDC_STATUS_CMDSENT          (1 << 6)
#define FTSDC_STATUS_DATAEND          (1 << 7)
#define FTSDC_STATUS_FIFO_URUN        (1 << 8)
#define FTSDC_STATUS_FIFO_ORUN        (1 << 9)
#define FTSDC_STATUS_CARD_CHANGE      (1 << 10)
#define FTSDC_STATUS_CARD_DETECT      (1 << 11)
#define FTSDC_STATUS_WRITE_PROT       (1 << 12)
#define FTSDC_STATUS_DATA0_STATUS     (1 << 17)

#define SET_BUSY_FTSDC_STATUS_DATA0(status) (status & ~FTSDC_STATUS_DATA0_STATUS)
#define SET_IDLE_FTSDC_STATUS_DATA0(status) (status | FTSDC_STATUS_DATA0_STATUS)

static void atfsdc010_reset_cmd(void *opaque)
{
    ATFSDC010State *s = (ATFSDC010State *)opaque;

    s->power = 0x10;
    s->clock = 0x1ff;
    s->buswidth = 0x9;  /* or 0x1 */
    s->mmc_irq_resptime = 0x42;
    s->cmdarg = 0;
    s->cmd = 0;
    s->datatimer = 0;
    s->datalength = 0;
    s->respcmd = 0;
    s->datatimer = 0;
    s->datactrl = 0;
    s->datacnt = 0;
    s->status = SET_IDLE_FTSDC_STATUS_DATA0(s->status);
    s->mask = 0x400;
    s->feature = 4;
    s->revision = ATFSDC_REVISION;
}

static void atfsdc010_update(ATFSDC010State *s)
{
    qemu_set_irq(s->irq, (s->status & s->mask) != 0);
}

static void atfsdc010_fifo_push(ATFSDC010State *s, uint32_t value)
{
    int n;

    if (s->fifo_len == ATFSDC010_FIFO_LEN) {
        LOGGE("%s: FIFO overflow\n", __func__);
        return;
    }
    n = (s->fifo_pos + s->fifo_len) & (ATFSDC010_FIFO_LEN - 1);
    s->fifo_len++;
    s->fifo[n] = value;
    LOG("FIFO push %08x\n", (int)value);
}

static uint32_t atfsdc010_fifo_pop(ATFSDC010State *s)
{
    uint32_t value;

    if (s->fifo_len == 0) {
        LOGGE("%s: FIFO underflow\n", __func__);
        return 0;
    }
    value = s->fifo[s->fifo_pos];
    s->fifo_len--;
    s->fifo_pos = (s->fifo_pos + 1) & (ATFSDC010_FIFO_LEN - 1);
    LOG("FIFO pop %08x\n", (int)value);
    return value;
}

static void atfsdc010_send_command(ATFSDC010State *s)
{
    SDRequest request;
    uint8_t response[16];
    int rlen;

    request.cmd = s->cmd & FTSDC_CMD_INDEX;
    request.arg = s->cmdarg;
    LOG("Command %d %08x\n", request.cmd, request.arg);
    rlen = sdbus_do_command(&s->sdbus, &request, response);
    if (rlen < 0) {
        goto error;
    }
    if (s->cmd & FTSDC_CMD_RESPONSE) {
#define RWORD(n) ((response[n] << 24) | (response[n + 1] << 16) \
                  | (response[n + 2] << 8) | response[n + 3])
        if (rlen == 0 || (rlen == 4 && (s->cmd & FTSDC_CMD_LONGRESP))) {
            goto error;
        }
        if (rlen != 4 && rlen != 16) {
            goto error;
        }
        if (rlen == 4) {
            s->response[0] = RWORD(0);
            s->response[1] = s->response[2] = s->response[3] = 0;
        } else {
            s->response[3] = RWORD(0);
            s->response[2] = RWORD(4);
            s->response[1] = RWORD(8);
            s->response[0] = RWORD(12);
        }
        LOG("Response received\n");
        s->status |= FTSDC_STATUS_CMDRESPEND;
#undef RWORD
    } else {
        LOG("Command sent\n");
        s->status |= FTSDC_STATUS_CMDSENT;
    }
    return;

error:
    LOG("Timeout\n");
    s->status |= FTSDC_STATUS_CMDTIMEOUT;
}

/*
 * Transfer data between the card and the FIFO.  This is complicated by
 * the FIFO holding 32-bit words and the card taking data in single byte
 * chunks.  FIFO bytes are transferred in little-endian order.
 */
static void atfsdc010_fifo_run(ATFSDC010State *s)
{
    uint32_t value;
    int n;
    int is_read;

    is_read = (s->datactrl & FTSDC_DATA_WRITE) == 0;
    if (s->datacnt != 0 && (!is_read || sdbus_data_ready(&s->sdbus))) {
        if (is_read) {
            while (s->datacnt > 0 && s->fifo_len < ATFSDC010_FIFO_LEN) {
                value = 0;
                for (n = 0; n < 4 && s->datacnt > 0; n++, s->datacnt--) {
                    value |= (uint32_t)sdbus_read_byte(&s->sdbus) << (n * 8);
                }
                atfsdc010_fifo_push(s, value);
            }
        } else {
            while (s->datacnt > 0 && s->fifo_len > 0) {
                value = atfsdc010_fifo_pop(s);
                for (n = 0; n < 4 && s->datacnt > 0; n++, s->datacnt--) {
                    sdbus_write_byte(&s->sdbus, value & 0xff);
                    value >>= 8;
                }
            }
        }
    }
    if (s->fifo_len == 0 && !is_read) {
        s->status |= FTSDC_STATUS_FIFO_URUN;
    } else if (s->fifo_len > 0 && is_read) {
        s->status |= FTSDC_STATUS_FIFO_ORUN;
    }
    if (s->datacnt == 0) {
        if (s->fifo_len == 0) {
            s->datactrl &= ~FTSDC_DATA_ENABLE;
            LOG("Data engine idle\n");
        }
        s->status |= FTSDC_STATUS_DATAEND;
        s->status |= FTSDC_STATUS_DATABLOCKEND;
        LOG("Transfer Complete\n");
    }
}

static uint64_t
atfsdc010_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t rz = 0;
    ATFSDC010State *s = opaque;

    switch (offset) {
    case 0x00: /* Command */
        rz = s->cmd;
        break;
    case 0x04: /* Argument */
        rz = s->cmdarg;
        break;
    case 0x08: /* Response0 */
        rz = s->response[0];
        break;
    case 0x0c: /* Response1 */
        rz = s->response[1];
        break;
    case 0x10: /* Response2 */
        rz = s->response[2];
        break;
    case 0x14: /* Response3 */
        rz = s->response[3];
        break;
    case 0x18: /* RespCmd */
        rz = s->respcmd;
        break;
    case 0x1c: /* DataCtrl */
        rz = s->datactrl;
        break;
    case 0x20: /* DataTimer */
        rz = s->datatimer;
        break;
    case 0x24: /* DataLength */
        rz = s->datalength;
        break;
    case 0x28: /* Status */
        rz = s->status;
        break;
    case 0x2c: /* Clear Write Only */
        rz = 0;
        break;
    case 0x30: /* Mask */
        rz = s->mask;
        break;
    case 0x34: /* Power Control */
        rz = s->power;
        break;
    case 0x38: /* Clock Control*/
        rz = s->clock;
        break;
    case 0x3c: /* Bus Width */
        rz = s->buswidth;
        break;
    case 0x40: /* Data Window */
        if (s->fifo_len == 0) {
            LOGGE("%s: Unexpected FIFO read\n", __func__);
        } else {
            uint32_t value;
            value = atfsdc010_fifo_pop(s);
            atfsdc010_fifo_run(s);
            atfsdc010_update(s);
            rz = value;
        }
        break;
    case 0x44: /* MMC interrupt response time register */
        rz = s->mmc_irq_resptime;
        break;
    case 0x48: /* general purpose output */
        rz = s->gpo;
        break;
    case 0x6c: /* SDIO control register 1 */
        rz = s->sdio_ctrl1;
        break;
    case 0x70: /* SDIO control register 2 */
        rz = s->sdio_ctrl2;
        break;
    case 0x74: /* SDIO status register */
        rz = s->sdio_status;
        break;
    case 0x9c: /* Feature */
        rz = s->feature;
        break;
    case 0xa0: /* Revision */
        rz = s->revision;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset %x\n", __func__, (int)offset);
    }

    LOG("%s:  offset %08lx, value %08lx\n", __func__, offset, rz);
    return rz;
}

static void
atfsdc010_write(void *opaque, hwaddr offset,
             uint64_t value, unsigned size)
{
    LOG("%s: offset %08lx, value %08lx\n", __func__, offset, value);
    ATFSDC010State *s = (ATFSDC010State *)opaque;

    switch (offset) {
    case 0x00: /* Command */
        s->cmd = value;
        if (s->cmd & FTSDC_CMD_SDCRST) {
            atfsdc010_reset_cmd(s);
        } else if (s->cmd & FTSDC_CMD_ENABLE) {
            atfsdc010_send_command(s);
            atfsdc010_fifo_run(s);
            /* The command has completed one way or the other.  */
            s->cmd &= ~FTSDC_CMD_ENABLE;
        }
        break;
    case 0x04: /* Argument */
        s->cmdarg = value;
        break;
    case 0x1c: /* DataCtrl */
        s->datactrl = value & 0x7f;
        if (value & FTSDC_DATA_ENABLE) {
            if (value & FTSDC_DATA_WRITE) {
                s->status |= FTSDC_STATUS_FIFO_URUN;
            }
            s->datacnt = s->datalength;
            atfsdc010_fifo_run(s);
        }
        break;
    case 0x20: /* DataTimer */
        s->datatimer = value;
        break;
    case 0x24: /* DataLength */
        s->datalength = value;
        break;
    case 0x2c: /* Clear */
        s->status &= ~(value & 0x7ff);
        break;
    case 0x30: /* Mask */
        s->mask = value & 0x7ff;
        break;
    case 0x34: /* Power */
        s->power = value & 0x1f;
        break;
    case 0x38: /* Clock */
        s->clock = value & 0x1ff;
        break;
    case 0x3c: /* Bus width */
        s->buswidth = value & 0x7;
        break;
    case 0x40: /* Data Window */
        if (s->datacnt == 0) {
            LOGGE("%s: Unexpected FIFO write\n", __func__);
        } else {
            /* we don't need to send value actually */
            atfsdc010_fifo_push(s, value);
            atfsdc010_fifo_run(s);
        }
        break;
    case 0x44: /* MMC interrupt response time register */
        s->mmc_irq_resptime = value & 0xff;
        break;
    case 0x48: /* general purpose output */
        s->gpo = value & 0xf;
        break;
    case 0x6c: /* SDIO control register 1 */
        s->sdio_ctrl1 = value;
        /* TODO: SDIO functions */
        break;
    case 0x70: /* SDIO control register 2 */
        s->sdio_ctrl2 = value & 0x3;
        /* TODO: SDIO functions */
        break;
    case 0x74: /* SDIO status register (RO) */
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad addr %"HWADDR_PRIx"\n",
                      __func__, offset);
    }
    atfsdc010_update(s);
}

static void atfsdc010_set_readonly(DeviceState *dev, bool level)
{
    ATFSDC010State *s = (ATFSDC010State *)dev;

    qemu_set_irq(s->card_readonly, level);
}

static void atfsdc010_set_inserted(DeviceState *dev, bool level)
{
    ATFSDC010State *s = (ATFSDC010State *)dev;

    qemu_set_irq(s->card_inserted, level);
}

static void
atfsdc010_reset(DeviceState *d)
{
    ATFSDC010State *s = ATFSDC010(d);

    s->power = 0x10;
    s->clock = 0x1ff;
    s->buswidth = 0x9; /* or 0x1 */
    s->mmc_irq_resptime = 0x42;
    s->cmdarg = 0;
    s->cmd = 0;
    s->datatimer = 0;
    s->datalength = 0;
    s->respcmd = 0;
    s->response[0] = 0;
    s->response[1] = 0;
    s->response[2] = 0;
    s->response[3] = 0;
    s->datatimer = 0;
    s->datactrl = 0;
    s->datacnt = 0;
    s->status = 0;
    s->mask = 0x400;
    s->feature = 4;
    s->revision = ATFSDC_REVISION;

    atfsdc010_set_inserted(DEVICE(s), sdbus_get_inserted(&s->sdbus));
    atfsdc010_set_readonly(DEVICE(s), sdbus_get_readonly(&s->sdbus));
}

static void
atfsdc010_realize(DeviceState *dev, Error **errp)
{
    DeviceState *card;
    DriveInfo *dinfo;

    /* FIXME use a qdev drive property instead of drive_get_next() */
    card = qdev_new(TYPE_SD_CARD);
    dinfo = drive_get(IF_SD, 0, 0);
    qdev_prop_set_drive_err(card, "drive", blk_by_legacy_dinfo(dinfo),
                            &error_fatal);
    qdev_realize_and_unref(card,
                           qdev_get_child_bus(dev, "sd-bus"),
                           &error_fatal);
}

static const VMStateDescription vmstate_atfsdc010 = {
    .name = TYPE_ATFSDC010,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        /* TODO */
        VMSTATE_END_OF_LIST()
    }
};

static void
atfsdc010_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    k->vmsd = &vmstate_atfsdc010;
    /* Reason: init() method uses drive_get_next() */
    k->user_creatable = false;
    k->realize = atfsdc010_realize;
    device_class_set_legacy_reset(k, atfsdc010_reset);
}

static const MemoryRegionOps atfsdc010_ops = {
    .read = atfsdc010_read,
    .write = atfsdc010_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void
atfsdc010_init(Object *obj)
{
    ATFSDC010State *s = ATFSDC010(obj);
    DeviceState *dev = DEVICE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &atfsdc010_ops, s,
                            TYPE_ATFSDC010, 0x100);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);
    qdev_init_gpio_out_named(dev, &s->card_readonly, "card-read-only", 1);
    qdev_init_gpio_out_named(dev, &s->card_inserted, "card-inserted", 1);

    qbus_init(&s->sdbus, sizeof(s->sdbus), TYPE_ATFSDC010_BUS, dev, "sd-bus");
}

static const TypeInfo atfsdc010_info = {
    .name          = TYPE_ATFSDC010,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ATFSDC010State),
    .instance_init = atfsdc010_init,
    .class_init    = atfsdc010_class_init,
};

static void atfsdc010_bus_class_init(ObjectClass *klass, void *data)
{
    SDBusClass *sbc = SD_BUS_CLASS(klass);

    sbc->set_inserted = atfsdc010_set_inserted;
    sbc->set_readonly = atfsdc010_set_readonly;
}

static const TypeInfo atfsdc010_bus_info = {
    .name = TYPE_ATFSDC010_BUS,
    .parent = TYPE_SD_BUS,
    .instance_size = sizeof(SDBus),
    .class_init = atfsdc010_bus_class_init,
};

static void atfsdc010_register_types(void)
{
    type_register_static(&atfsdc010_info);
    type_register_static(&atfsdc010_bus_info);
}

type_init(atfsdc010_register_types)

/*
 * Create SDC device.
 */
DeviceState *atfsdc010_create(hwaddr addr, qemu_irq irq)
{
    DeviceState *dev;

    dev = sysbus_create_varargs(TYPE_ATFSDC010, addr, irq, NULL);

    return dev;
}
