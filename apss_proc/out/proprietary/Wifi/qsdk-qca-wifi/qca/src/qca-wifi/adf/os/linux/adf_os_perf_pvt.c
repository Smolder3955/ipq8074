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
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include <adf_os_perf.h>


extern proc_api_tbl_t          api_tbl[];

adf_os_perf_entry_t     perf_root = {{0,0}}; 

/** 
 * @brief Module init
 * 
 * @return 
 */
int
adf_os_perfmod_init(void)
{
    printk("Perf Debug Module Init\n");
    
    INIT_LIST_HEAD(&perf_root.list);
    INIT_LIST_HEAD(&perf_root.child);

    perf_root.proc = proc_mkdir(PROCFS_PERF_DIRNAME, 0);

    return 0;
}


/** 
 * @brief Module exit
 */
void
adf_os_perfmod_exit(void)
{
    printk("Perf Debug Module Exit\n");
    
    remove_proc_entry(PROCFS_PERF_DIRNAME, 0);
}

/** 
 * @brief Create the perf entry
 * 
 * @param parent
 * @param id_name
 * @param type
 * 
 * @return 
 */
adf_os_perf_id_t
__adf_os_perf_init(adf_os_perf_id_t       parent, uint8_t     *id_name,
                   adf_os_perf_cntr_t    type)
{
    adf_os_perf_entry_t    *entry  = NULL;
    adf_os_perf_entry_t    *pentry = PERF_ENTRY(parent);

    if (type >= CNTR_LAST) {
        printk("%s:%s Invalid perf-type\n", __FILE__, __func__);
        goto done;
    }

    if (!pentry) pentry = &perf_root;
    
    entry = kmalloc(sizeof(struct adf_os_perf_entry), GFP_ATOMIC);

    if (!entry) {
        printk(" Out of Memory,:%s\n", __func__);
        return NULL;
    }
    
    memset(entry, 0, sizeof(struct adf_os_perf_entry));

    INIT_LIST_HEAD(&entry->list);
    INIT_LIST_HEAD(&entry->child);

    spin_lock_init(&entry->lock_irq);

    list_add_tail(&entry->list, &pentry->child);

    entry->name = id_name;
    entry->type = type;

    if (type == CNTR_GROUP) {
        entry->proc = proc_mkdir(id_name, pentry->proc);
        goto done;
    }

    
    entry->parent   = pentry;
    entry->proc     = create_proc_entry(id_name, S_IFREG|S_IRUGO|S_IWUSR, 
                                        pentry->proc);
    entry->proc->data       = entry;
    entry->proc->read_proc  = api_tbl[type].proc_read;
    entry->proc->write_proc = api_tbl[type].proc_write;

    /* 
     * Initialize the Event with default values
     */
    api_tbl[type].init(entry, api_tbl[type].def_val);

done:
    return entry;
}
EXPORT_SYMBOL(__adf_os_perf_init);

/** 
 * @brief Destroy the perf entry
 *
 * @param id
 *
 * @return 
 */
a_bool_t
__adf_os_perf_destroy(adf_os_perf_id_t  id)
{
    adf_os_perf_entry_t     *entry  = PERF_ENTRY(id),
                            *parent = entry->parent;

    if (!list_empty(&entry->child)) {
        printk("Child's are alive, Can't delete \n");
        return A_FALSE;
    }

    remove_proc_entry(entry->name, parent->proc);

    list_del(&entry->list);

    vfree(entry);

    return A_TRUE;
}
EXPORT_SYMBOL(__adf_os_perf_destroy);

/** 
 * @brief Start the sampling
 * 
 * @param id
 */
void
__adf_os_perf_start(adf_os_perf_id_t    id)
{
    adf_os_perf_entry_t     *entry = PERF_ENTRY(id);

    api_tbl[entry->type].sample(entry, 0); 
}
EXPORT_SYMBOL(__adf_os_perf_start);

/** 
 * @brief Stop sampling
 *
 * @param id
 */
void
__adf_os_perf_end(adf_os_perf_id_t    id)
{
    adf_os_perf_entry_t     *entry = PERF_ENTRY(id);

    api_tbl[entry->type].sample(entry, 1); 
}
EXPORT_SYMBOL(__adf_os_perf_end);
