/*
* Copyright (c) 2012, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2012, Qualcomm Atheros Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <adf_os_perf.h>


#define MIPS_PERF_POS       5
#define MIPS_PERF_MSK       0xf
#define MIPS_EVNUM(x)       ((x << MIPS_PERF_POS)| MIPS_PERF_MSK)


#define MAX_NIBBLES         8 
#define HEX_DELIM           2
#define REG_VAL_MAX         (MAX_NIBBLES + HEX_DELIM + 1) 
#define REV_IDX(x, len)     (len - 1 - x)

#define IDX(x)              [CNTR_##x]

enum perf_init_val {
    INIT_CPU_CYCLES         = 0x000,
    INIT_MIPS24K_PERF0      = MIPS_EVNUM(0x9),  /**< ICACHE Misses */
    INIT_MIPS24K_PERF1      = MIPS_EVNUM(0xb),  /**< DCACHE Misses */
    INIT_MIPS74K_PERF0      = MIPS_EVNUM(0x17), /**< DCACHE loads */
    INIT_MIPS74K_PERF1      = MIPS_EVNUM(0x18), /**< DCACHE Misses */
    INIT_MIPS74K_PERF2      = MIPS_EVNUM(0x6),  /**< ICACHE Access */
    INIT_MIPS74K_PERF3      = MIPS_EVNUM(0x6),  /**< ICACHE Misses */
};

/*
 * -----------------------------------------
 *             Library Routines
 * -----------------------------------------
 */

#define UPDATE_PERF_REG(num, val)  ({   \
    uint32_t    __ret;  \
    write_c0_perfctrl##num(val);    \
    mdelay(1);  \
    __ret = read_c0_perfctrl##num();  \
    mdelay(1);  \
    write_c0_perfcntr##num(0);  \
    __ret;  \
}) 


/** 
 * @brief Load the perf event control with values
 * 
 * @param type
 * @param val
 */
static inline uint32_t 
program_perf_event(adf_os_perf_cntr_t  type, uint32_t  val)
{
    uint32_t    ret = 0;

    switch (type) {
        case CNTR_MIPS24K_CPU:
        case CNTR_MIPS74K_CPU:    
            write_c0_count(val);
            ret = read_c0_count();
            break;

        case CNTR_MIPS24K_PERF0:
        case CNTR_MIPS74K_PERF0:    
            ret = UPDATE_PERF_REG(0, val);
            break;

        case CNTR_MIPS24K_PERF1:
        case CNTR_MIPS74K_PERF1:    
            ret = UPDATE_PERF_REG(1, val);
            break;

        case CNTR_MIPS74K_PERF2:
            ret = UPDATE_PERF_REG(2, val);
            break;

        case CNTR_MIPS74K_PERF3:
            ret = UPDATE_PERF_REG(3, val);
            break;

        default:
            printk("%s:Invalid type\n", __func__);
            ret = 0;
            break;
    }

    printk("Type = %d, Val = %d, CTRL = 0x%x\n", type, val, ret);

    return ret;
}

/** 
 * @brief Record a perf event
 * 
 * @param type
 * 
 * @return 
 */
static inline uint32_t
record_perf_event(adf_os_perf_cntr_t   type)
{
    uint32_t    count = 0;

    switch (type) {
        case CNTR_MIPS24K_PERF0:
        case CNTR_MIPS74K_PERF0:
            count = read_c0_perfcntr0();
            break;

        case CNTR_MIPS24K_PERF1:
        case CNTR_MIPS74K_PERF1:
            count = read_c0_perfcntr1();
            break;

        case CNTR_MIPS74K_PERF2:
            count = read_c0_perfcntr2();
            break;

        case CNTR_MIPS74K_PERF3:
            count = read_c0_perfcntr3();
            break;

        case CNTR_MIPS24K_CPU:
        case CNTR_MIPS74K_CPU:
            count = read_c0_count();
            break;

        default:
            printk("%s:Invalid type\n", __func__);
            break;
    }

    return count;
}
/** 
 * @brief Hex Parsing routine
 * 
 * @param buf
 * @param len
 * 
 * @return 
 */
uint32_t
string_to_hex(char   *buf, int   len)
{
    unsigned int    value   = 0;
    int             i       = 0, 
                    token   = 0;
        
    for (i = 0; i < len; i++) {

        token = buf[REV_IDX(i, len)] - '0';
        if (token < 10) goto add;

        token = buf[REV_IDX(i, len)] - 'a' + 10;
        if (token < 16) goto add;

        token = buf[REV_IDX(i, len)] - 'A' + 10;
        if (token < 16) goto add;
        
        token = 0;
add:
        value |= token << (i * 4);        
    }

    return value;
}

/*
 * -----------------------------------------
 *             Sub Routines
 * -----------------------------------------
 */


/** 
 * @brief Init the Perf Control(1) for DCACHE Miss events
 * 
 * @param entry
 */
void
init_perf_event(adf_os_perf_entry_t       *entry, uint32_t   def_val)
{
    unsigned long       flags;

    spin_lock_irqsave(&entry->lock_irq, flags);
    program_perf_event(entry->type, def_val);
    spin_unlock_irqrestore(&entry->lock_irq, flags);
}


/** 
 * @brief Sample the DCACHE Miss events
 * 
 * @param entry
 * @param done
 */
#define INT_32BIT_MAX_RANGE  0xffffffff 
void
sample_event(adf_os_perf_entry_t     *entry, uint8_t  done)
{
    uint32_t    idx = entry->sample_idx,
                old = 0;

    if (done) {
        old = entry->samples[idx];
        entry->sample_idx = INC_SAMPLES(idx);
    }

    /* account for overflow */
    entry->samples[idx] = (record_perf_event(entry->type) - old) & INT_32BIT_MAX_RANGE;
}

/** 
 * @brief Proc entry reader
 * 
 * @param page
 * @param start
 * @param off
 * @param count
 * @param eof
 * @param data
 * 
 * @return 
 */
int
read_perf_event(char *page, char **start, off_t off, int count, int *eof, 
                void *data)
{
    adf_os_perf_entry_t    *entry   = PERF_ENTRY(data), *te;
    uint32_t                len     = 0, 
                            limit, i;
    char                   *out     = page;

    adf_os_perf_entry_t     *root = entry->parent;
    struct list_head        *le;
    int                     j;

    limit = (count / PER_SAMPLE_SZ);

    /*
     * Just to make sure we are inside boundaries
     * XXX: Should be safe until we hit 3K buffer limit, 
     *      currently consuming around 600+ bytes or so
     */
    limit = limit > MAX_SAMPLES ? MAX_SAMPLES : limit;

    
    /*
     * XXX: locks
     */

    out += sprintf(out, "idx=%d\n", entry->sample_idx); 

    for (i = 0; i < limit; i++) {
        out += sprintf(out, "%d\t", i);
            
        j = 0;
        list_for_each(le, &root->child) {
            te = PERF_ENTRY(le);
            /*
             * First one is the CPU counter, and has to be multiplied by 2
             * since it runs at half the clock rate.
             */
            if (!j++)
                out += sprintf(out, "%d,", 2 * te->samples[i]);
            else
                out += sprintf(out, "%d,", te->samples[i]);
        }
        out += sprintf(out, "\n");
    }
    
    len = out - page;
    
    if (len < count)
        *eof = 1;
    else
        len = count;

    return len;
}

/** 
 * @brief 
 * 
 * @param file
 * @param buf
 * @param count
 * @param data
 * 
 * @return 
 */
int 
write_perf_event(struct file *file, const char *buf, unsigned long count, 
                  void *data)
{
    adf_os_perf_entry_t    *entry                   = PERF_ENTRY(data);
    char                    temp_buf[REG_VAL_MAX]   = {0};
    uint32_t                val                     = 0,
                            ret                     = 0;
    unsigned long           flags                   = 0;

    if (!capable(CAP_SYS_ADMIN))
        return -EACCES;

    count = ((count > REG_VAL_MAX) ? REG_VAL_MAX : count);

    if (copy_from_user(temp_buf, buf, count))
        return -EFAULT;

    if (temp_buf[1] == 'x')
        val = string_to_hex(&temp_buf[2], count - 3);
    else if (temp_buf[0] == 'x')
        val = string_to_hex(&temp_buf[1], count - 2);
    else
        val = string_to_hex(&temp_buf[0], count - 1);

    /* printk("Received reg_val = 0x%x, count = %d\n", reg_val, count); */
    
    spin_lock_irqsave(&entry->lock_irq, flags);

    memset(&entry->samples, 0, MAX_SAMPLE_SZ);

    ret = program_perf_event(entry->type, MIPS_EVNUM(val));

    spin_unlock_irqrestore(&entry->lock_irq, flags);

    return count; 
}


void
init_cpu_cycles(adf_os_perf_entry_t       *entry, uint32_t      def_val)
{
    return;
}


int
read_cpu_cycles(char *page, char **start, off_t off, int count, int *eof, 
                void *data)
{
    adf_os_perf_entry_t    *entry   = PERF_ENTRY(data);
    uint32_t                len     = 0,
                            limit   = 0, 
                            i       = 0;
    char                   *out     = page;
    
    limit = (count / PER_SAMPLE_SZ);

    /*
     * Just to make sure we are inside boundaries
     * XXX: Should be safe until we hit 3K buffer limit, 
     *      currently consuming around 600+ bytes or so
     */      
    limit = (limit > MAX_SAMPLES) ? MAX_SAMPLES : limit;
    
    /*
     * XXX: locks
     */
    out += sprintf(out, "idx = %d\n", entry->sample_idx);
     
    /*
     * C0 counts are 1/2 the pipeline clock rate
     */
    for (i = 0; i < limit; i++) 
        out   += sprintf(out, "%d\t%d\n", i, (entry->samples[i] * 2));
    
    len = out - page;
    
    if (len < count)
        *eof = 1;
    else
        len = count;

    return len;
}

/** 
 * @brief Proc write
 * 
 * @param file
 * @param buf
 * @param count
 * @param data
 * 
 * @return 
 */
int 
write_cpu_cycles(struct file *file, const char *buf, unsigned long count, 
                  void *data)
{
    adf_os_perf_entry_t    *entry   = PERF_ENTRY(data);
    unsigned long           flags;

    if (!capable(CAP_SYS_ADMIN))
        return -EACCES;
    
    printk("!!!! Reseting C0 counter, Why !!!!\n");

    spin_lock_irqsave(&entry->lock_irq, flags);
    program_perf_event(entry->type, 0x0);
    spin_unlock_irqrestore(&entry->lock_irq, flags);
     
    return count; 
}
/*
 * -----------------------------------------
 *              Main Routines
 * -----------------------------------------
 */

proc_api_tbl_t          api_tbl[CNTR_LAST] = {
    IDX(MIPS24K_CPU)    = INIT_API(cpu_cycles, INIT_CPU_CYCLES),
    IDX(MIPS24K_PERF0)  = INIT_API(perf_event, INIT_MIPS24K_PERF0),
    IDX(MIPS24K_PERF1)  = INIT_API(perf_event, INIT_MIPS24K_PERF1),
    IDX(MIPS74K_CPU)    = INIT_API(cpu_cycles, INIT_CPU_CYCLES),
    IDX(MIPS74K_PERF0)  = INIT_API(perf_event, INIT_MIPS74K_PERF0),
    IDX(MIPS74K_PERF1)  = INIT_API(perf_event, INIT_MIPS74K_PERF1),
    IDX(MIPS74K_PERF2)  = INIT_API(perf_event, INIT_MIPS74K_PERF2),
    IDX(MIPS74K_PERF3)  = INIT_API(perf_event, INIT_MIPS74K_PERF3),
};




