/*
 * Andes Programmable Interval Timer, ATCPIT100.
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/ptimer.h"
#include "hw/qdev-properties.h"
#include "hw/timer/atcpit100.h"

/* #define DEBUG_ATCPIT100 */
/* #define MORE_HOOK */

#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#define zLOG(x...) printf(x)

#define LOG(x...) xLOG(x)
#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)

#ifdef DEBUG_ATCPIT100
#undef LOG
#define LOG(x...) yLOG(x)
#endif

enum {
    ID_PIT = 0x03031,
    ID_PIT_SHIFT = 12,
};

enum {
    VER_MAJOR = 0x00,
    VER_MAJOR_SHIFT = 4,
    VER_MINOR = 0x00,
    VER_MINOR_SHIFT = 0,
};

enum {
    REG_IDREV = 0x00,
    REG_CFG = 0x10,
    REG_INTEN = 0x14,
    REG_INTST = 0x18,
    REG_CHEN = 0x1c,
    REG_CH_BASE = 0x20,
    REG_CH_STRIDE = 0x10,
    REG_CH_STRIDE_SHIFT = 4,
    REG_CH_BIAS_MASK = 0xf,
    REG_SIZE = 0x60,
};

enum {
    REG_CH_CTRL = 0x00,
    REG_CH_RELOAD = 0x04,
    REG_CH_CNTR = 0x08,
};

#define DEVNAME "ATCPIT100"

#define CHANNEL_MODE(ctrl)  (extract32(ctrl, 0, 3))
#define CHANNEL_CLOCK(ctrl) (extract32(ctrl, 3, 1))
#define CHANNEL_PARK(ctrl)  (extract32(ctrl, 4, 1))

#define CHANNEL_ENABLE(ena, ch_no)    (extract32(ena, (ch_no << 2), 4))
#define TIMER_ENABLE(ena, ch_no, tmr) (extract32(ena, (ch_no << 2) + tmr, 1))

static void
enable_ptimer(ptimer_state *ptimer, uint64_t reload, uint32_t freq)
{
    /*
     * Pause the timer if it is running.  This may cause some
     * inaccuracy during to rounding, but avoids a whole lot of other
     * messiness.
     */
    ptimer_transaction_begin(ptimer);
    ptimer_stop(ptimer);
    ptimer_set_limit(ptimer, reload, 1);
    ptimer_set_freq(ptimer, freq);
    ptimer_run(ptimer, 0);
    ptimer_transaction_commit(ptimer);
}

static void
atcpit100_update_irq(Atcpit100State *s)
{
    unsigned int level = !!(s->int_en & s->int_st);

    LOG("%s: level %d\n", __func__, level);
    qemu_set_irq(s->irq, level);
}

static uint32_t
calculate_timer_count(Atcpit100Channel *ch)
{
    uint32_t count;
    Atcpit100Timer *t = ch->timers;
    unsigned int mode = CHANNEL_MODE(ch->control);

    switch (mode) {
    case 0: /* reserved */
    case 5: /* reserved */
        count = ch->counter;
        break;
    case 1: /* 32-bit-timer x1 */
        count = ptimer_get_count(t[0].ptimer);
        break;
    case 2: /* 16-bit-timer x2 */
        count = ((ptimer_get_count(t[0].ptimer) & 0xffffu) << 0) |
                ((ptimer_get_count(t[1].ptimer) & 0xffffu) << 16);
        break;
    case 3: /* 8-bit-timer x4 */
    case 7: /* Mixed PWM/8-bit timers */
        count = ((ptimer_get_count(t[0].ptimer) & 0xffu) << 0) |
                ((ptimer_get_count(t[1].ptimer) & 0xffu) << 8) |
                ((ptimer_get_count(t[2].ptimer) & 0xffu) << 16) |
                ((ptimer_get_count(t[3].ptimer) & 0xffu) << 24);
        break;
    case 4: /* PWM */
        count = ((ptimer_get_count(t[2].ptimer) & 0xffffu) << 0) |
                ((ptimer_get_count(t[3].ptimer) & 0xffffu) << 16);
        break;
    case 6: /* Mixed PWM/16-bit timer */
        count = ((ptimer_get_count(t[0].ptimer) & 0xffffu) << 0) |
                ((ptimer_get_count(t[2].ptimer) & 0x00ffu) << 16) |
                ((ptimer_get_count(t[3].ptimer) & 0x00ffu) << 24);
        break;
    }

    return count;
}

static void
update_single_channel_enable(Atcpit100Channel *ch, uint32_t toggles)
{
    LOG("%s: CH %d, toggles 0x%1x\n", __func__, ch->id, toggles);
    unsigned int ch_no, tmr_no, mask, mode, enables;
    uint32_t freq, reload;
    Atcpit100State *s;
    Atcpit100Timer *t;

    if (!toggles) {
        return;
    }

    s = ch->state;
    t = ch->timers;
    ch_no = ch->id;
    freq = CHANNEL_CLOCK(ch->control) ? s->pclk : s->extclk;
    mode = CHANNEL_MODE(ch->control);
    enables = CHANNEL_ENABLE(s->ch_en, ch_no);

    /* stop toggled timers first */
    mask = 1;
    for (tmr_no = 0; tmr_no < ATCPIT100_CHANNEL_TIMER_NUM; ++tmr_no) {
        if (toggles & mask) {
            ptimer_transaction_begin(t[tmr_no].ptimer);
            ptimer_stop(t[tmr_no].ptimer);
            ptimer_transaction_commit(t[tmr_no].ptimer);
        }
        mask <<= 1;
    }

    /* (re)start newly enabled timers now */
    switch (mode) {
    case 0: /* reserved */
    case 5: /* reserved */
        if (enables) {
            LOGGE("ATCPIT100: CH %d is enabled in RESERVED mode!\n", ch_no);
        }
        break;
    case 1: /* 32-bit-timer x1 */
        if ((toggles & 1) && (enables & 1)) {
            reload = ch->reload;
            enable_ptimer(t[0].ptimer, reload, freq);
        }
        break;
    case 2: /* 16-bit-timer x2 */
        if ((toggles & 1) && (enables & 1)) {
            reload = ch->reload & 0xffffu;
            enable_ptimer(t[0].ptimer, reload, freq);
        }
        if ((toggles & 2) && (enables & 2)) {
            reload = (ch->reload >> 16) & 0xffffu;
            enable_ptimer(t[1].ptimer, reload, freq);
        }
        break;
    case 3: /* 8-bit-timer x4 */
        if ((toggles & 1) && (enables & 1)) {
            reload = ch->reload & 0xffu;
            enable_ptimer(t[0].ptimer, reload, freq);
        }
        if ((toggles & 2) && (enables & 2)) {
            reload = (ch->reload >> 8) & 0xffu;
            enable_ptimer(t[1].ptimer, reload, freq);
        }
        if ((toggles & 4) && (enables & 4)) {
            reload = (ch->reload >> 16) & 0xffu;
            enable_ptimer(t[2].ptimer, reload, freq);
        }
        if ((toggles & 8) && (enables & 8)) {
            reload = (ch->reload >> 24) & 0xffu;
            enable_ptimer(t[3].ptimer, reload, freq);
        }
        break;
    case 4:
        if ((toggles & 8) && (enables & 8)) { /* PWM */
            reload = ch->reload & 0xffffu;
            enable_ptimer(t[2].ptimer, reload, freq);
            reload = (ch->reload >> 16) & 0xffffu;
            enable_ptimer(t[3].ptimer, reload, freq);
        }
        break;
    case 6: /* Mixed PWM/16-bit timer */
        if ((toggles & 1) && (enables & 1)) {
            reload = ch->reload & 0xffffu;
            enable_ptimer(t[0].ptimer, reload, freq);
        }
        if ((toggles & 8) && (enables & 8)) { /* PWM */
            reload = (ch->reload >> 16) & 0xffu;
            enable_ptimer(t[2].ptimer, reload, freq);
            reload = (ch->reload >> 24) & 0xffu;
            enable_ptimer(t[3].ptimer, reload, freq);
        }
        break;
    case 7: /* Mixed PWM/8-bit timers */
        if ((toggles & 1) && (enables & 1)) {
            reload = ch->reload & 0xffu;
            enable_ptimer(t[0].ptimer, reload, freq);
        }
        if ((toggles & 2) && (enables & 2)) {
            reload = (ch->reload >> 8) & 0xffu;
            enable_ptimer(t[1].ptimer, reload, freq);
        }
        if ((toggles & 8) && (enables & 8)) { /* PWM */
            reload = (ch->reload >> 16) & 0xffu;
            enable_ptimer(t[2].ptimer, reload, freq);
            reload = (ch->reload >> 24) & 0xffu;
            enable_ptimer(t[3].ptimer, reload, freq);
        }
        break;
    }
}

static inline void
atcpit100_update_channel_enable(Atcpit100State *s, uint64_t value)
{
    LOG("%s:\n", __func__);
    unsigned int ch_no;
    uint32_t toggle = s->ch_en ^ value;
    Atcpit100Channel *ch = s->channels;

    s->ch_en = value;
    for (ch_no = 0; ch_no < ATCPIT100_CHANNEL_NUM; ++ch_no) {
        uint32_t ch_toggle = toggle & 0xf;
        update_single_channel_enable(&ch[ch_no], ch_toggle);
        toggle >>= 4;
    }
}

static inline void
update_channel_control(Atcpit100Channel *ch, uint32_t value)
{
    Atcpit100State *s = ch->state;
    uint32_t toggles = ch->control ^ value;
    uint32_t enables = CHANNEL_ENABLE(s->ch_en, ch->id);
    uint32_t redo_enable = 0;

    ch->control = value;

    if (CHANNEL_MODE(toggles)) { /* mode changed */
        if (enables) {
            LOGGE("%s: CH %d changes Mode when enabled!\n", DEVNAME, ch->id);
        }
        redo_enable = 1;
    }

    if (CHANNEL_CLOCK(toggles)) { /* clock changed */
        redo_enable = 1;
    }

    if (CHANNEL_PARK(toggles)) { /* park changed */
        /* PWM not supported yet */
    }

    if (redo_enable) {
        update_single_channel_enable(ch, 0xf); /* as all enables are toggled */
    }
}

static uint64_t
atcpit100_read_channel(Atcpit100Channel *ch, hwaddr addr)
{
    uint64_t rz = 0;

    switch (addr) {
    case REG_CH_CTRL:
        rz = ch->control;
        break;
    case REG_CH_RELOAD:
        rz = ch->reload;
        break;
    case REG_CH_CNTR: /* RO */
        rz = calculate_timer_count(ch);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad access (ch %d, addr %x)\n",
                      __func__, ch->id, (int)addr);
    }

    LOG("\e[96m%s: ch %d, addr %08x, value %08x\e[0m\n", __func__, ch->id,
        (int)addr, (int)rz);
    return rz;
}

static void
atcpit100_write_channel(Atcpit100Channel *ch, hwaddr addr, uint64_t value)
{
    LOG("\e[96m%s: ch %d, addr %08x, value %08x\e[0m\n", __func__, ch->id,
        (int)addr, (int)value);

    switch (addr) {
    case REG_CH_CTRL:
        update_channel_control(ch, (uint32_t)value);
        break;
    case REG_CH_RELOAD:
        ch->reload = value;
        break;
    case REG_CH_CNTR: /* RO */
        break;
    default:
        LOGGE("%s: Bad ch %d, addr %x\n", __func__, ch->id, (int)addr);
    }
}

static uint64_t
atcpit100_read(void *opaque, hwaddr addr, unsigned size)
{
    Atcpit100State *s = ATCPIT100(opaque);
    uint64_t rz = 0;

    switch (addr) {
    case REG_IDREV ... REG_CH_BASE - 1:
        switch (addr) {
        case REG_IDREV: /* RO */
            rz = ID_PIT << ID_PIT_SHIFT | VER_MAJOR << VER_MAJOR_SHIFT |
                 VER_MINOR << VER_MINOR_SHIFT;
            break;
        case REG_CFG: /* RO */
            rz = ATCPIT100_CHANNEL_NUM;
            break;
        case REG_INTEN:
            rz = s->int_en;
            break;
        case REG_INTST: /* W1C */
            rz = s->int_st;
            break;
        case REG_CHEN:
            rz = s->ch_en;
            break;
        default:
            LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
            break;
        }
        LOG("\e[95m%s: addr %08x, value %08x\e[0m\n", __func__, (int)addr,
            (int)rz);
        break;
    case REG_CH_BASE ... REG_SIZE - 1:
        if (1) {
            int ch_num = (addr - REG_CH_BASE) >> REG_CH_STRIDE_SHIFT;
            int bias = addr & REG_CH_BIAS_MASK;
            rz = atcpit100_read_channel(&s->channels[ch_num], bias);
        }
        break;
    default:
        LOGGE("%s: Bad addr %x\n", __func__, (int)addr);
    }

    return rz;
}

static inline void
atcpit100_update_int_enable(Atcpit100State *s, uint64_t value)
{
    LOG("%s:\n", __func__);
    s->int_en = value;
    atcpit100_update_irq(s);
}

static void
atcpit100_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    Atcpit100State *s = ATCPIT100(opaque);

    LOG("\e[97m%s: addr %08x, value %08x\e[0m\n", __func__, (int)addr,
        (int)value);
    switch (addr) {
    case REG_IDREV ... REG_CH_BASE - 1:
        switch (addr) {
        case REG_IDREV: /* RO */
            break;
        case REG_CFG: /* RO */
            break;
        case REG_INTEN:
            atcpit100_update_int_enable(s, value);
            break;
        case REG_INTST: /* W1C */
            s->int_st &= ~(uint32_t)value;
            atcpit100_update_irq(s);
            break;
        case REG_CHEN:
            atcpit100_update_channel_enable(s, value);
            break;
        default:
            /* reserved bits are written as no effect */
            LOGGE("%s: Bad addr %x (value %x)\n", __func__, (int)addr,
                  (int)value);
            break;
        }
        break;
    case REG_CH_BASE ... REG_SIZE - 1:
        if (1) {
            int ch_num = (addr - REG_CH_BASE) >> REG_CH_STRIDE_SHIFT;
            int bias = addr & REG_CH_BIAS_MASK;
            atcpit100_write_channel(&s->channels[ch_num], bias, value);
        }
        break;
    default:
        LOGGE("%s: Bad addr %x (value %x)\n", __func__, (int)addr, (int)value);
    }
}

static const MemoryRegionOps atcpit100_ops = {
    .read = atcpit100_read,
    .write = atcpit100_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {.min_access_size = 4, .max_access_size = 4}
};

static void
atcpit100_reset(DeviceState *dev)
{
    int i, j;
    Atcpit100State *s = ATCPIT100(dev);
    for (i = 0; i < ATCPIT100_CHANNEL_NUM; ++i) {
        Atcpit100Channel *ch = &s->channels[i];
        for (j = 0; j < ATCPIT100_CHANNEL_TIMER_NUM; ++j) {
            Atcpit100Timer *tmr = &ch->timers[j];
            ptimer_transaction_begin(tmr->ptimer);
            ptimer_stop(tmr->ptimer);
            ptimer_set_freq(tmr->ptimer, s->pclk);
            ptimer_transaction_commit(tmr->ptimer);
        }
        ch->control = 0;
        ch->reload = 0;
    }
    s->int_en = 0;
    s->int_st = 0;
    s->ch_en = 0;
}

#ifdef MORE_HOOK
static void
atcpit100_realize(DeviceState *dev, Error **errp)
{
    LOG("%s:\n", __func__);
}
#endif

static void
timer_hit(void *opaque)
{
    Atcpit100Timer *tmr = opaque;
    Atcpit100Channel *ch = tmr->channel;
    Atcpit100State *s = ch->state;
    LOG("%s: CH %d, TMR %d\n", __func__, tmr->id, ch->id);
    uint32_t next = s->int_st | (1 << (tmr->id + (ch->id << 2)));

    if (s->int_st ^ next) {
        s->int_st = next;
        atcpit100_update_irq(s);
    } else {
        /* nothing new!  */
    }
}

static void
atcpit100_init(Object *obj)
{
    LOG("%s:\n", __func__);
    Atcpit100State *s = ATCPIT100(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    int i, j;

    /* Init all channels.  */
    for (i = 0; i < ATCPIT100_CHANNEL_NUM; ++i) {
        Atcpit100Channel *ch = &s->channels[i];
        ch->id = i;
        ch->state = s;
        /* Init all timers of one channel.  */
        for (j = 0; j < ATCPIT100_CHANNEL_TIMER_NUM; ++j) {
            Atcpit100Timer *tmr = &ch->timers[j];
            tmr->id = j;
            tmr->channel = ch;
            tmr->ptimer = ptimer_init(timer_hit, tmr,
                                      PTIMER_POLICY_WRAP_AFTER_ONE_PERIOD |
                                      PTIMER_POLICY_NO_IMMEDIATE_TRIGGER |
                                      PTIMER_POLICY_NO_IMMEDIATE_RELOAD);
            ptimer_transaction_begin(tmr->ptimer);
            ptimer_set_freq(tmr->ptimer, s->pclk);
            ptimer_transaction_commit(tmr->ptimer);
        }
    }

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->mmio, obj, &atcpit100_ops, s, TYPE_ATCPIT100,
                          0x100);
    sysbus_init_mmio(sbd, &s->mmio);
}

static Property atcpit100_properties[] = {
    DEFINE_PROP_UINT32("pclk", Atcpit100State, pclk, 60 * 1000 * 1000),
    DEFINE_PROP_UINT32("extclk", Atcpit100State, extclk, 20 * 1000 * 1000),
    DEFINE_PROP_END_OF_LIST(),
};

static void
atcpit100_class_init(ObjectClass *klass, void *data)
{
    LOG("%s:\n", __func__);
    DeviceClass *k = DEVICE_CLASS(klass);

#ifdef MORE_HOOK
    k->realize = atcpit100_realize;
#endif
    device_class_set_legacy_reset(k, atcpit100_reset);
    device_class_set_props(k, atcpit100_properties);
}

static const TypeInfo atcpit100_info = {
    .name = TYPE_ATCPIT100,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Atcpit100State),
    .instance_init = atcpit100_init,
    .class_init = atcpit100_class_init,
};

static void
atcpit100_register_types(void)
{
    LOG("%s:\n", __func__);
    type_register_static(&atcpit100_info);
}

type_init(atcpit100_register_types);

/*
 * Create PIT device.
 */
DeviceState *
atcpit100_create(hwaddr addr, qemu_irq irq)
{
    LOG("%s:\n", __func__);
    DeviceState *dev;

    dev = sysbus_create_simple(TYPE_ATCPIT100, addr, irq);

    return dev;
}
