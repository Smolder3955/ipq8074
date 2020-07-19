/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/signal.h>

#include <linux/kthread.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#include <linux/freezer.h>
#endif

#include <linux/smp_lock.h>
#include <wlan_mlme_dp_dispatcher.h>

/* use one thread to handle all devices or not. */
static int global_thread_used = 0;              
static struct eventq global_event_q;
static struct task_struct *global_athHTC_task;

/* TBD : change to one thread per device if need. */
struct htc_tx_q htc_tx_q;
static struct task_struct *athHTCTx_task;

static int ath_put_events(struct eventq *event_q, dummy_timer_func_t fn, void *arg)
{
    unsigned long flags;
    u_int16_t tail;

	if(event_q == NULL)
        return -1;

	spin_lock_irqsave(&event_q->event_lock, flags);

    if (event_q->count >= MAX_TIME_EVENT_NUM) {
        printk("%s: event queue %pK is full\n", __func__,event_q);
        spin_unlock_irqrestore(&event_q->event_lock, flags);
        return 1;
    }

    tail = event_q->tail;

    event_q->timerEvents[tail].func = fn;
    event_q->timerEvents[tail].arg = arg;
    event_q->tail = (event_q->tail + 1) & (MAX_TIME_EVENT_NUM-1);
    event_q->count++;

	spin_unlock_irqrestore(&event_q->event_lock, flags); 

    return 0;
}

static u_int16_t ath_get_events_num(struct eventq *event_q)
{
    unsigned long flags;
    int16_t num;

	if(event_q == NULL)
        return 0;

    spin_lock_irqsave(&event_q->event_lock, flags);
    num = event_q->count;
    spin_unlock_irqrestore(&event_q->event_lock, flags);

    return num;
}

int ath_put_defer_item(osdev_t osdev, defer_func_t func, int func_id, void *param1, void *param2, void *param3)
{
    unsigned long flags;
    u_int16_t tail;
    struct eventq *event_q;

    if (global_thread_used) {
        event_q = &global_event_q;      /* get from global */
    }
    else {
        event_q = osdev->event_q;       /* get from device scope */
    }

    if(event_q == NULL)
        return -1;

    spin_lock_irqsave(&event_q->event_lock, flags);

    if (event_q->defer_count >= MAX_DEFER_ITEM_NUM) {
        printk("%s: work item queue %pK is full\n", __func__,event_q);
        spin_unlock_irqrestore(&event_q->event_lock, flags);
        return 1;
    }

    tail = event_q->defer_tail;

    event_q->deferItems[tail].func = func;
    event_q->deferItems[tail].param1 = param1;
    event_q->deferItems[tail].param2 = param2;
    event_q->deferItems[tail].param3 = param3;
    event_q->deferItems[tail].func_id = func_id;
    event_q->defer_tail = (event_q->defer_tail + 1) & (MAX_DEFER_ITEM_NUM-1);
    event_q->defer_count++;

    /* schedule defer items */
    ath_wakeup_htc_thread(event_q);
    
    spin_unlock_irqrestore(&event_q->event_lock, flags);
    return 0;
}

static u_int16_t ath_get_defer_item_num(struct eventq *event_q)
{
    unsigned long flags;
    int16_t num;

	if(event_q == NULL)
        return 0;

    spin_lock_irqsave(&event_q->event_lock, flags);
    num = event_q->defer_count;
    spin_unlock_irqrestore(&event_q->event_lock, flags);

    return num;
}

void ath_execute_defer_item(defer_t *work)
{
#if ZM_USB_STREAM_MODE
    osdev_t osdev;
#endif /*ZM_USB_STREAM_MODE*/

    /* Function ID is predefined */
    if (work->func_id == WORK_ITEM_SET_MULTICAST ) {
        work->func(work->param1);
    }
    else if ((work->func_id == WORK_ITEM_SINGLE_ARG_DEFERED )) {
        work->func(work->param1);
    }
    else if (work->func_id == WORK_ITEM_SET_BEACON_DEFERED) {
        work->func(work->param1);
    }
    else if (work->func_id == WORK_ITEM_SET_PS_DELIVER_EVENT) {
        ((defer_func_t_ps_deliver_event)work->func)(work->param1, work->param2);
    }
#if ZM_USB_STREAM_MODE
    else if (work->func_id == WORK_ITEM_SET_RX_STREAMMODE_TIMEOUT) {
        osdev = work->param1;
        ((defer_func_t_write_reg_single)work->func)(osdev->host_wmi_handle,osdev->upstream_reg ,osdev->upstream_reg_write_value);
    }
#endif /*ZM_USB_STREAM_MODE*/
    else if (work->func_id == WORK_ITEM_SET_TIMER_DELIVER_EVENT) {
        ((defer_func_t_timer_deliver_event)work->func)(work->param1, work->param2);
    }
    else if (work->func_id == WORK_ITEM_SET_OS_SCHE_EVENT) {
        ((defer_func_t_os_sche_event)work->func)(work->param1, work->param2);
    }
    else {
        printk("%s: Unknown Defer Item, enlist yourself\n", __func__);
    }
}

void ath_wakeup_htc_thread(struct eventq *event_q)
{
	if(event_q == NULL)
        return;

	event_q->msg_callback_flag = 1;
    wake_up_interruptible(&event_q->msg_wakeuplist);
}

static int ath_add_tasklets(osdev_t osdev, htc_tq_struct_t *tasklet, tasklet_callback_t fn, void *ctx)
{
    unsigned long flags;
    u_int16_t index;
    struct eventq *event_q;

    if (global_thread_used) {
        event_q = &global_event_q;      /* get from global */
    }
    else {
        event_q = osdev->event_q;      /* get from device scope */
    }
    
	if(event_q == NULL)
        return 1;

	spin_lock_irqsave(&event_q->tasklet_lock, flags);

    if (event_q->tasklet_count >= MAX_TASKLET_EVENT_NUM) {
        printk("%s: tasklet queue %pK is full\n", __func__,event_q);
        spin_unlock_irqrestore(&event_q->tasklet_lock, flags);
        return 1;
    }

    for (index = 0; index < MAX_TASKLET_EVENT_NUM; index++) {
        if (event_q->taskletEvents[index].is_used == 0){
            /* initialize tasklet structure */
            event_q->taskletEvents[index].tq = tasklet;
            event_q->taskletEvents[index].func = fn;
            event_q->taskletEvents[index].ctx = ctx;
            event_q->taskletEvents[index].is_scheduled = 0;
            event_q->taskletEvents[index].is_used = 1;
            
            event_q->tasklet_count++;
            break;
        }
    }

    if (index == MAX_TASKLET_EVENT_NUM) {
        KASSERT(0, ("event_q.tasklet_count mismatch!"));
    } 

    spin_unlock_irqrestore(&event_q->tasklet_lock, flags);
    return 0;
}

static int ath_del_tasklets(osdev_t osdev, htc_tq_struct_t *tasklet)
{
    unsigned long flags;
    u_int16_t index;
    struct eventq *event_q;

    if (global_thread_used) {
        event_q = &global_event_q;      /* get from global */
    }
    else {
        event_q = osdev->event_q;      /* get from device scope */
    }
    
	if(event_q == NULL)
        return 1;

	spin_lock_irqsave(&event_q->tasklet_lock, flags);

    for (index = 0; index < MAX_TASKLET_EVENT_NUM; index++) {
        if (event_q->taskletEvents[index].tq == tasklet) {
            printk("%s: tasklet[%d] is deleted!\n", __func__, index);
            
            event_q->taskletEvents[index].tq = NULL;
            event_q->taskletEvents[index].func = NULL;
            event_q->taskletEvents[index].ctx = NULL;
            event_q->taskletEvents[index].is_scheduled = 0;
            event_q->taskletEvents[index].is_used = 0;

            event_q->tasklet_count--;
            break;
        }
    }

    if (index == MAX_TASKLET_EVENT_NUM) {
        printk("%s: tasklet is not exist!\n", __func__);
    }    

    spin_unlock_irqrestore(&event_q->tasklet_lock, flags);
    return 0;
}

static int ath_schedule_tasklets(osdev_t osdev, htc_tq_struct_t *tasklet)
{
    struct eventq *event_q;
    unsigned long flags;
    int index;

    if (global_thread_used) {
        event_q = &global_event_q;      /* get from global */
    }
    else {
        event_q = osdev->event_q;      /* get from device scope */
    }
    
	if(event_q == NULL)
        return 1;

	spin_lock_irqsave(&event_q->tasklet_lock, flags);

    for (index = 0; index < event_q->tasklet_count; index++)
    {
        if (event_q->taskletEvents[index].tq == tasklet)
        {
            event_q->taskletEvents[index].is_scheduled = 1;
            ath_wakeup_htc_thread(event_q);
            break;
        }
    }

    spin_unlock_irqrestore(&event_q->tasklet_lock, flags);
    return 0;
}

static void
ath_eventq_flush_timers(struct eventq *event_q)
{
	if(event_q == NULL)
        return;

	event_q->head    = 0;
    event_q->tail    = 0;
    event_q->count   = 0;
}

static void
ath_eventq_flush_defer(struct eventq *event_q)
{
	if(event_q == NULL)
        return;

	event_q->defer_head  = 0;
    event_q->defer_tail  = 0;
    event_q->defer_count = 0;
}

static void
ath_eventq_flush_tasklet(struct eventq *event_q)
{
    int index;

	if(event_q == NULL)
        return;
	
    for (index = 0; index < MAX_TASKLET_EVENT_NUM; index++) {
        event_q->taskletEvents[index].tq = NULL;
        event_q->taskletEvents[index].func = NULL;
        event_q->taskletEvents[index].ctx = NULL;
        event_q->taskletEvents[index].is_scheduled = 0;
        event_q->taskletEvents[index].is_used = 0;
    }

    event_q->tasklet_count = 0;
}

static void ath_initilialize_eventq(struct eventq *event_q)
{
    /* initialise wait queue */
    init_waitqueue_head(&event_q->msg_wakeuplist);

    /* initialize spin lock */
    spin_lock_init(&event_q->event_lock);
    spin_lock_init(&event_q->tasklet_lock);

    /* initialize event queue structure */
    event_q->msg_callback_flag = 0;
    event_q->terminate = 0;
    event_q->stopping = 0;

    /* Init the timers */
    ath_eventq_flush_timers(event_q);

    /* Init the tasklet events */
    ath_eventq_flush_tasklet(event_q);

    /* Init defered tasks */
    ath_eventq_flush_defer(event_q);
}

/* private functions */
int ath_create_htc_thread(osdev_t osdev)
{
    struct eventq *event_q;
    struct task_struct **athHTC_task;

    /*
     * If osdev is NULL and means the driver use only one glbal kthread to handle all devices.
     * Else, one device use one kthread.
     */    
    if(osdev) {
        osdev->event_q = (struct eventq *)OS_MALLOC(osdev, sizeof(struct eventq), GFP_ATOMIC);
        if(osdev->event_q == NULL) {
            printk(KERN_ERR "%s: allocate struct eventq fail\n", __func__);
            return 1;
        }

        /* get from device scope */
        event_q = osdev->event_q;   
        athHTC_task = &osdev->athHTC_task; 
    }
    else {
        global_thread_used = 1;

        /* get from global */
        event_q = &global_event_q;  
        athHTC_task = &global_athHTC_task;
    }

    lock_kernel();

    /* Initialize the eventq first */
    ath_initilialize_eventq(event_q);

    *athHTC_task = kthread_run(ath_htc_thread, (void *)event_q, "athHTCThread");

    if (!IS_ERR(*athHTC_task))
    {
        printk(KERN_INFO "%s: kthread_run successfully, thread %pK\n", __func__, *athHTC_task);
        unlock_kernel();

        return 0;
    }

    if (!global_thread_used) {
        OS_FREE(osdev->event_q);
    }
    else {
        global_thread_used = 0;     /* reset after kthread stop */
    }

    printk(KERN_ERR "%s: kthread_run fail\n", __func__);
    unlock_kernel();

    return 1;
}

void ath_htc_thread_stopping(osdev_t osdev)
{
    struct eventq *event_q;
    unsigned long flags;

    if(osdev) {      
        /* get from device scope */
        event_q = osdev->event_q;           
    }
    else {
        /* get from global */
        event_q = &global_event_q;  
    }

    if(event_q == NULL) {
        printk(KERN_ERR "%s: stop htc thread fail\n", __func__);
        return;
    } 

    spin_lock_irqsave(&event_q->event_lock, flags);
    event_q->stopping = 1;
    spin_unlock_irqrestore(&event_q->event_lock, flags);
}

void ath_terminate_htc_thread(osdev_t osdev)
{
    struct task_struct *athHTC_task;
    struct eventq *event_q;
    unsigned long flags;

    if(osdev) {      
        /* get from device scope */
        event_q = osdev->event_q;           
        athHTC_task = osdev->athHTC_task; 
    }
    else {
        /* get from global */
        event_q = &global_event_q;  
        athHTC_task = global_athHTC_task;
    }

	if((athHTC_task == NULL) ||
       (event_q == NULL)) {
        printk(KERN_ERR "%s: terminate htc thread fail\n", __func__);
        return;
    } 

    spin_lock_irqsave(&event_q->event_lock, flags);

    /* set terminate flag */
    event_q->terminate = 1;

    /* Flush all queued timers to avoid a reschedule */
    ath_eventq_flush_timers(event_q);
    ath_eventq_flush_defer(event_q);

    spin_unlock_irqrestore(&event_q->event_lock, flags);
    
    /* terminate kernel thread */
    kthread_stop(athHTC_task);

    printk(KERN_INFO "%s: stop the thread\n", __func__);
}

#if 0
void stop_kthread(void)
{
    printk(KERN_INFO "%s: stop the thread\n", __func__);
    kthread_stop(athHTC_task);
}
#endif

/* this is the thread function that we are executing */
int ath_htc_thread(void *data)
{
    struct eventq *event_q = (struct eventq *)data;

    /* an endless loop in which we are doing our work */
    do
    {
        int tasklet_index;

        wait_event_interruptible(event_q->msg_wakeuplist,
                event_q->msg_callback_flag != 0 || kthread_should_stop());

        event_q->msg_callback_flag = 0;

        /* We need to do a memory barrier here to be sure that
           the flags are visible on all CPUs. 
        */
        mb();

        /* Timer handler */
        while(ath_get_events_num(event_q) > 0 && event_q->stopping == 0)
        {
            unsigned long flags;
            u_int8_t head;
            timer_event_t *pTimer;

            spin_lock_irqsave(&event_q->event_lock, flags);

            head = event_q->head;
            pTimer = &(event_q->timerEvents[head]);
            event_q->head = (event_q->head + 1) & (MAX_TIME_EVENT_NUM-1);
            event_q->count--;

            spin_unlock_irqrestore(&event_q->event_lock, flags);

            /* Execute the timer callback function */
            pTimer->func((unsigned long)pTimer->arg);
        }

        /* Tasklet handler */
        /* TODO: make the tasklet interation more efficient */
        for (tasklet_index = 0; tasklet_index < event_q->tasklet_count && event_q->stopping == 0; tasklet_index++)
        {
            if ((event_q->taskletEvents[tasklet_index].is_used == 1) &&
                (event_q->taskletEvents[tasklet_index].is_scheduled == 1))
            {
                tasklet_t *pTasklet;

                pTasklet = (tasklet_t *)&(event_q->taskletEvents[tasklet_index]);

                /* Execute the tasklet callback function */
                pTasklet->func(pTasklet->ctx);
            }
        }

        /* Defer WorkItem */
        while(ath_get_defer_item_num(event_q) > 0 && event_q->stopping == 0)
        {
            unsigned long flags;
            u_int8_t head;
            defer_t *work_item;

            spin_lock_irqsave(&event_q->event_lock, flags);

            head = event_q->defer_head;
            work_item = &(event_q->deferItems[head]);
            event_q->defer_head = (event_q->defer_head + 1) & (MAX_DEFER_ITEM_NUM-1);
            event_q->defer_count--;

            spin_unlock_irqrestore(&event_q->event_lock, flags);

            /* Execute the defer work item */
            ath_execute_defer_item(work_item);
        }

        try_to_freeze();
    } while(!kthread_should_stop());

    /* for checking */
    if (event_q->terminate != 1)
    {
        printk(KERN_ERR "%s HTC Thread is not terminated by terminate funcion\n", __func__);
        BUG();
    }
    
    /* free resource */
    if (!global_thread_used) {
        OS_FREE(event_q);
    }

    /* returning from the thread here calls the exit functions */
    return 0;
}

static void htcTimerHandler(unsigned long arg)
{
    htc_timer_t *pTimer = (htc_timer_t *) arg;
    osdev_t osdev;
    struct eventq *event_q;;

    if (global_thread_used) {
        event_q = &global_event_q;  /* get from global */
    }
    else {
        osdev = (osdev_t)pTimer->osdev;
        event_q = osdev->event_q;   /* get from device scope */
    }

    ath_put_events(event_q, pTimer->func, pTimer->arg);
    ath_wakeup_htc_thread(event_q);
}

void ath_register_htc_thread_callback(osdev_t osdev)
{
    /* Register the thread and timer callback functions */
    osdev->htcTimerHandler = htcTimerHandler;
    osdev->htcAddTasklet = ath_add_tasklets;
    osdev->htcDelTasklet = ath_del_tasklets;
    osdev->htcScheduleTasklet = ath_schedule_tasklets;
    osdev->htcPutDeferItem = ath_put_defer_item;
}

void ath_wakeup_htc_tx_thread(void)
{
    htc_tx_q.msg_callback_flag = 1;
    wake_up_interruptible(&htc_tx_q.msg_wakeuplist);
}

int ath_put_txbuf(struct sk_buff *skb)
{
    unsigned long flags;
    u_int16_t tail;

    spin_lock_irqsave(&htc_tx_q.htc_txbuf_lock, flags);

    if (htc_tx_q.htc_txbuf_count >= MAX_TXQ_BUF_NUM) {
        printk("%s: txbuf queue is full\n", __func__);
        spin_unlock_irqrestore(&htc_tx_q.htc_txbuf_lock, flags);
        return 1;
    }

    tail = htc_tx_q.htc_txbuf_tail;

    htc_tx_q.htc_txbuf_q[tail] = skb;
    htc_tx_q.htc_txbuf_tail = (htc_tx_q.htc_txbuf_tail + 1) & (MAX_TXQ_BUF_NUM-1);
    htc_tx_q.htc_txbuf_count++;

    spin_unlock_irqrestore(&htc_tx_q.htc_txbuf_lock, flags);

    ath_wakeup_htc_tx_thread();

    return 0;
}

static u_int16_t ath_get_txq_num(void)
{
    unsigned long flags;
    int16_t num;

    spin_lock_irqsave(&htc_tx_q.htc_txbuf_lock, flags);
    num = htc_tx_q.htc_txbuf_count;
    spin_unlock_irqrestore(&htc_tx_q.htc_txbuf_lock, flags);

    return num;
}

static int ath_htc_tx_thread(void *data)
{
    do
    {
        wait_event_interruptible(htc_tx_q.msg_wakeuplist,
                htc_tx_q.msg_callback_flag != 0 || kthread_should_stop());

        htc_tx_q.msg_callback_flag = 0;

        /* We need to do a memory barrier here to be sure that
           the flags are visible on all CPUs. 
        */
        mb();

        while(ath_get_txq_num() > 0)
        {
            unsigned long flags;
            u_int16_t head;
            osif_dev *dev;
            wlan_if_t vap;
            struct sk_buff *skb;

            spin_lock_irqsave(&htc_tx_q.htc_txbuf_lock, flags);

            head = htc_tx_q.htc_txbuf_head;
            skb = htc_tx_q.htc_txbuf_q[head];
            dev = ath_netdev_priv(skb->dev);
            vap = dev->os_if;
            htc_tx_q.htc_txbuf_head = (htc_tx_q.htc_txbuf_head + 1) & (MAX_TXQ_BUF_NUM-1);
            htc_tx_q.htc_txbuf_count--;

            spin_unlock_irqrestore(&htc_tx_q.htc_txbuf_lock, flags);

            wlan_vap_send(vap, skb);
        }

        try_to_freeze();
    } while(!kthread_should_stop());

    /* for checking */
    if (htc_tx_q.terminate != 1)
    {
        printk(KERN_ERR "%s HTC Thread is not terminated by terminate funcion\n", __func__);
        BUG();
    }

    /* returning from the thread here calls the exit functions */
    return 0;
}

void
ath_htc_tx_flush_txbuf(void)
{
    int i, head;
    struct sk_buff *skb;

    head = htc_tx_q.htc_txbuf_head;
    for (i = 0; i < htc_tx_q.htc_txbuf_count; i++) {
        skb = htc_tx_q.htc_txbuf_q[(head + i) & (MAX_TXQ_BUF_NUM-1)];
        if (skb != NULL)
            dev_kfree_skb(skb);
    }
    
    htc_tx_q.htc_txbuf_head  = 0;
    htc_tx_q.htc_txbuf_tail  = 0;
    htc_tx_q.htc_txbuf_count = 0;
}

int ath_create_htc_tx_thread(struct net_device *netdev)
{
    lock_kernel();

    /* initialise wait queue */
    init_waitqueue_head(&htc_tx_q.msg_wakeuplist);

    /* initialize spin lock */
    spin_lock_init(&htc_tx_q.htc_txbuf_lock);

    /* initialize event queue structure */
    htc_tx_q.msg_callback_flag = 0;
    htc_tx_q.terminate = 0;

    htc_tx_q.htc_txbuf_head  = 0;
    htc_tx_q.htc_txbuf_tail  = 0;
    htc_tx_q.htc_txbuf_count = 0;

    athHTCTx_task = kthread_run(ath_htc_tx_thread, netdev, "athHTCTxThread");

    if (!IS_ERR(athHTCTx_task))
    {
        printk(KERN_INFO "%s: Tx Thread creates successfully\n", __func__);
        unlock_kernel();
        return 0;
    }

    printk(KERN_ERR "%s: Tx Thread creates fail\n", __func__);
    unlock_kernel();

    return 1;
}

void ath_terminate_htc_tx_thread(void)
{
    unsigned long flags;
    
    printk(KERN_INFO "%s: stop the thread\n", __func__);

    spin_lock_irqsave(&htc_tx_q.htc_txbuf_lock, flags);
    htc_tx_q.terminate = 1;
    
    ath_htc_tx_flush_txbuf();
    spin_unlock_irqrestore(&htc_tx_q.htc_txbuf_lock, flags);

    /* set terminate flag */
    kthread_stop(athHTCTx_task);
}
#ifdef ATH_USB
EXPORT_SYMBOL(ath_terminate_htc_tx_thread);
EXPORT_SYMBOL(ath_terminate_htc_thread);
EXPORT_SYMBOL(ath_htc_thread_stopping);
EXPORT_SYMBOL(ath_register_htc_thread_callback);
EXPORT_SYMBOL(ath_create_htc_thread);
EXPORT_SYMBOL(ath_create_htc_tx_thread);
#endif
