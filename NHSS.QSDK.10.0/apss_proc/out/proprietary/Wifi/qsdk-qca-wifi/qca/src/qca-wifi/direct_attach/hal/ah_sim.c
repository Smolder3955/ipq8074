/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include "ah_sim.h"

#if ATH_DRIVER_SIM

#include "ah_internal.h"
#include "ah_devid.h"

static bool sim_info_init = false;

static PWSTR sim_id_path = L"athrsimctrl";
static PWSTR sim_mach_id = L"SimulationMachineID";
static PWSTR sim_mgr_id = L"SimulationEngineID";
static PWSTR sim_mode_id = L"SimulationModeStandAlone";
static PWSTR sim_promiscuous_id = L"SimulationModeIfIntersimPromiscuous";

struct AHSIM_sim_info_t AHSIM_sim_info;

void AHSIM_TxMonitorPkt(u_int32_t mon_node_index);
void AHSIM_MonitorPktChannelInfo(u_int32_t tx_channel, u_int32_t rx_channel);
void AHSIM_MonitorPktPERInfo(u_int8_t per, u_int8_t rv);

#define AHSIM_INFO_LOCK_INIT()    spin_lock_init(&AHSIM_sim_info.sim_info_lock)
#define AHSIM_INFO_LOCK_DESTROY() spin_lock_destroy(&AHSIM_sim_info.sim_info_lock)
#define AHSIM_INFO_LOCK()         spin_lock(&AHSIM_sim_info.sim_info_lock)
#define AHSIM_INFO_UNLOCK()       spin_unlock(&AHSIM_sim_info.sim_info_lock)

void AHSIM_DistributeTransmitPacket(struct ath_hal *ah);
void AHSIM_DistributeIntersimTransmitPacket();

#define AHSIM_OID_CONTROL_TYPE_DEBUG                0
#define AHSIM_OID_CONTROL_TYPE_PER                  1
#define AHSIM_OID_CONTROL_TYPE_AGG_PER              2
#define AHSIM_OID_CONTROL_TYPE_FILTER               3
#define AHSIM_OID_CONTROL_TYPE_CHANNEL              4
#define AHSIM_OID_CONTROL_TYPE_BOP                  5 /* break-on-packet */
#define AHSIM_OID_CONTROL_TYPE_READ_SIM             6
#define AHSIM_OID_CONTROL_TYPE_WRITE_SIM            7
#define AHSIM_OID_CONTROL_TYPE_GET_SIMID            8
#define AHSIM_OID_CONTROL_TYPE_INTERSIM_CTRL        9

#pragma pack(push)
#pragma pack(4)

struct ah_debug_control_t {
    u_int32_t enable_pkt;
    u_int32_t snippet_length;
    u_int32_t enable_key;
};

struct ah_per_control_t { /* src & dest are indices into sim_devices - they identify individual simualted devices */
    int32_t src; /* a value of -1 indicates all 'src'es */
    int32_t dest; /* a value of -1 indicates all 'dest's */
    u_int32_t per; /* must be 0 to 100 - inclusive */
};

struct ah_bop_control_t {
    u_int32_t enable_bop;
    u_int32_t type;
    u_int32_t subtype;
};

struct ah_intersim_data_t {
    u_int32_t num_bytes;
    u_int32_t overflow_indicator;
    u_int32_t data_base;
};

struct ah_sim_id_t {
    u_int32_t machine_id;
    u_int32_t engine_id;
};

struct ah_intersim_control_t {
    u_int32_t stand_alone;
    u_int32_t promiscuous_mode;
};

struct ah_control_t {
    u_int32_t control_type;
    union {
        struct ah_debug_control_t debug_control;
        struct ah_per_control_t per_control;
        struct ah_bop_control_t bop_control;
        struct ah_intersim_data_t intersim_data;
        struct ah_intersim_control_t intersim_control;
        u_int32_t on_off_flag;
        struct ah_sim_id_t sim_id;
    };
};

#pragma pack(pop)

NDIS_STATUS AHSIM_DebugControl(struct ah_debug_control_t* control)
{
    AHSIM_sim_info.output_mon_pkts = control->enable_pkt;
    AHSIM_sim_info.output_pkt_snippet_length = control->snippet_length;
    AHSIM_sim_info.output_key = control->enable_key;

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AHSIM_PERControl(struct ah_per_control_t* control, bool is_agg_per)
{
    int32_t src = control->src;
    int32_t dest = control->dest;
    u_int32_t per = control->per;
    u_int32_t i;

    if (src < -1 || src > MAX_NUMBER_SIM_DEVICES ||
        dest < -1 || dest > MAX_NUMBER_SIM_DEVICES ||
        per > 100) {
        return NDIS_STATUS_FAILURE;
    }

    if (src != -1 && dest != -1) {
        if (is_agg_per) {
            AHSIM_sim_info.agg_per_src_dest[src][dest] = per;
        } else {
            AHSIM_sim_info.per_src_dest[src][dest] = per;
        }
    } else if (src == -1 && dest == -1) {
        if (is_agg_per) {
            OS_MEMSET(AHSIM_sim_info.agg_per_src_dest, per, MAX_NUMBER_SIM_DEVICES * MAX_NUMBER_SIM_DEVICES);
        } else {
            OS_MEMSET(AHSIM_sim_info.per_src_dest, per, MAX_NUMBER_SIM_DEVICES * MAX_NUMBER_SIM_DEVICES);
        }
    } else if (src == -1) {
        for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
            if (is_agg_per) {
                AHSIM_sim_info.agg_per_src_dest[i][dest] = per;
            } else {
                AHSIM_sim_info.per_src_dest[i][dest] = per;
            }
        }
    } else {
        for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
            if (is_agg_per) {
                AHSIM_sim_info.agg_per_src_dest[src][i] = per;
            } else {
                AHSIM_sim_info.per_src_dest[src][i] = per;
            }
        }
    }

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AHSIM_BOPControl(struct ah_bop_control_t* control)
{
    AHSIM_sim_info.break_on_packet = control->enable_bop;
    AHSIM_sim_info.bop_type = control->type;
    AHSIM_sim_info.bop_subtype = control->subtype;

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AHSIM_SendIntersimData(struct ah_intersim_data_t* control)
{
    if (control->num_bytes != AHSIM_INTER_SIM_BUFFER_SIZE) {
        AHSIM_ASSERT(0);
        return NDIS_STATUS_FAILURE;
    }

    AHSIM_OUTPUT_BUFFER_LOCK();
    OS_MEMCPY((u_int8_t*)&control->data_base, AHSIM_sim_info.output_buffer, AHSIM_sim_info.output_buffer_num_bytes);
    control->num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
    control->overflow_indicator = AHSIM_sim_info.output_buffer_overflow_indicator;
    AHSIM_sim_info.output_buffer_num_bytes = 0;
    AHSIM_sim_info.output_buffer_overflow_indicator = false;
    AHSIM_OUTPUT_BUFFER_UNLOCK();

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AHSIM_ReceiveIntersimData(struct ah_intersim_data_t* control)
{
    if (control->num_bytes > AHSIM_INTER_SIM_BUFFER_SIZE) {
        AHSIM_ASSERT(0);
        return NDIS_STATUS_FAILURE;
    }

    AHSIM_INPUT_BUFFER_LOCK();
    OS_MEMCPY(AHSIM_sim_info.input_buffer, (u_int8_t*)&control->data_base, control->num_bytes);
    AHSIM_sim_info.input_buffer_num_bytes = control->num_bytes;
    AHSIM_DistributeIntersimTransmitPacket();
    AHSIM_INPUT_BUFFER_UNLOCK();

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AHSIM_Control(void* InformationBuffer)
{
    struct ah_control_t* control = (struct ah_control_t*)InformationBuffer;

    switch (control->control_type) {
    case AHSIM_OID_CONTROL_TYPE_DEBUG:
        return AHSIM_DebugControl(&control->debug_control);
    case AHSIM_OID_CONTROL_TYPE_PER:
        return AHSIM_PERControl(&control->per_control, false);
    case AHSIM_OID_CONTROL_TYPE_AGG_PER:
        return AHSIM_PERControl(&control->per_control, true);
    case AHSIM_OID_CONTROL_TYPE_FILTER:
        AHSIM_sim_info.sim_filter = (bool)control->on_off_flag;
        return NDIS_STATUS_SUCCESS;
    case AHSIM_OID_CONTROL_TYPE_CHANNEL:
        AHSIM_sim_info.sim_channels = (bool)control->on_off_flag;
        return NDIS_STATUS_SUCCESS;
    case AHSIM_OID_CONTROL_TYPE_BOP:
        return AHSIM_BOPControl(&control->bop_control);
    case AHSIM_OID_CONTROL_TYPE_READ_SIM:
        return AHSIM_SendIntersimData(&control->intersim_data);
    case AHSIM_OID_CONTROL_TYPE_WRITE_SIM:
        return AHSIM_ReceiveIntersimData(&control->intersim_data);
    case AHSIM_OID_CONTROL_TYPE_GET_SIMID:
        control->sim_id.engine_id = AHSIM_sim_info.sim_eng_id;
        control->sim_id.machine_id = AHSIM_sim_info.sim_mach_id;
        return NDIS_STATUS_SUCCESS;
    case AHSIM_OID_CONTROL_TYPE_INTERSIM_CTRL:
        AHSIM_sim_info.sim_stand_alone_mode = (bool)control->intersim_control.stand_alone;
        AHSIM_sim_info.inter_sim_promiscuous_mode = (bool)control->intersim_control.promiscuous_mode;
        return NDIS_STATUS_SUCCESS;
    }

    return NDIS_STATUS_FAILURE;
}

static INLINE
OS_TIMER_FUNC(ahsim_tx_callback)
{
    struct ath_hal *ah;
    struct ath_hal_sim *hal_sim;

    OS_GET_TIMER_ARG(ah, struct ath_hal *);
    hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;

    AHSIM_TIMER_LOCK(hal_sim);
    hal_sim->tx_timer_active = false;
    AHSIM_TIMER_UNLOCK(hal_sim);

    AHSIM_DistributeTransmitPacket(ah);
}

bool AHSIM_register_sim_device_instance(u_int16_t devid, struct ath_hal *ah)
{
    u_int16_t i;
    struct ath_hal_sim *hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    RTL_QUERY_REGISTRY_TABLE regTable[5];
    u_int32_t var1, var2;

    // simulator structure initialization
    if (!sim_info_init) {
        sim_info_init = true;

        /* grab simulation manager ID and simulatino machine ID from registry */
        RtlZeroMemory(regTable, sizeof(regTable));
        regTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
        regTable[0].Name = sim_mgr_id;
        regTable[0].EntryContext = &AHSIM_sim_info.sim_eng_id;
        regTable[0].DefaultType = REG_NONE;

        regTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
        regTable[1].Name = sim_mach_id;
        regTable[1].EntryContext = &AHSIM_sim_info.sim_mach_id;
        regTable[1].DefaultType = REG_NONE;

        regTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
        regTable[2].Name = sim_mode_id;
        regTable[2].EntryContext = &var1;
        regTable[2].DefaultType = REG_NONE;

        regTable[3].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
        regTable[3].Name = sim_promiscuous_id;
        regTable[3].EntryContext = &var2;
        regTable[3].DefaultType = REG_NONE;

        if (RtlQueryRegistryValues(RTL_REGISTRY_SERVICES, sim_id_path, regTable, NULL, NULL) != STATUS_SUCCESS) {
            HDPRINTF(NULL, HAL_DBG_UNMASKABLE, "AHSIM_register_sim_device_instance() -- can not find registry settings for simulation manager and machine IDs or simulation mode!\n");
            return false;
        }
        AHSIM_sim_info.sim_stand_alone_mode = var1;
        AHSIM_sim_info.inter_sim_promiscuous_mode = var2;

        var1 = AHSIM_sim_info.sim_eng_id + 1;
        if (RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, sim_id_path, sim_mgr_id, REG_DWORD, (PVOID)&var1, sizeof(DWORD))
            != STATUS_SUCCESS) {
            HDPRINTF(NULL, HAL_DBG_UNMASKABLE, "AHSIM_register_sim_device_instance() -- can not write updated registry setting for simulation manager ID!\n");
            return false;
        }

        AHSIM_sim_info.num_of_sim_devices = 0;
        OS_MEMSET(AHSIM_sim_info.sim_devices, 0, sizeof(struct ath_hal*) * MAX_NUMBER_SIM_DEVICES);

        AHSIM_sim_info.output_mon_pkts = true;
        AHSIM_sim_info.output_pkt_snippet_length = 0;
        AHSIM_sim_info.output_key = false;
        OS_MEMSET(AHSIM_sim_info.per_src_dest, 0, MAX_NUMBER_SIM_DEVICES * MAX_NUMBER_SIM_DEVICES);
        OS_MEMSET(AHSIM_sim_info.agg_per_src_dest, 0, MAX_NUMBER_SIM_DEVICES * MAX_NUMBER_SIM_DEVICES);
        AHSIM_sim_info.sim_filter = true;
        AHSIM_sim_info.sim_channels = true;
        AHSIM_sim_info.break_on_packet = false;

        AHSIM_sim_info.output_buffer_num_bytes = 0;
        AHSIM_sim_info.output_buffer_overflow_indicator = false;
        AHSIM_sim_info.input_buffer_num_bytes = 0;

        AHSIM_sim_info.ibss_beacon_sta_index = 0;

        AHSIM_srand(-1); /* initialize to random number - time() */
    }
    if (AHSIM_sim_info.num_of_sim_devices == 0) {
        AHSIM_INFO_LOCK_INIT();
        AHSIM_OUTPUT_BUFFER_LOCK_INIT();
        AHSIM_INPUT_BUFFER_LOCK_INIT();
    }

    // initialization of simulation instance
    AHSIM_TIMER_LOCK_INIT(hal_sim);
    OS_INIT_TIMER(AH_PRIVATE(ah)->ah_osdev, &hal_sim->tx_timer, ahsim_tx_callback, ah, QDF_TIMER_TYPE_WAKE_APPS);
    hal_sim->tx_timer_active = false;

    // registration
    if (hal_sim->ahsim_access_sim_reg == NULL ||
        hal_sim->ahsim_get_active_tx_queue == NULL ||
        hal_sim->ahsim_distribute_transmit_packet == NULL ||
        hal_sim->ahsim_done_transmit == NULL ||
        hal_sim->ahsim_record_tx_c == NULL ||
        hal_sim->ahsim_rx_dummy_pkt == NULL ||
        hal_sim->ahsim_get_tx_buffer == NULL)
        {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "RegisterSimDeviceInsatnce() -- device not supported at this time, devid = %d\n", devid);
        return false;
    }

    if (AHSIM_sim_info.num_of_sim_devices == MAX_NUMBER_SIM_DEVICES) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "RegisterSimDeviceInsatnce() -- maximum number of simulation devices registered already -- %d\n", MAX_NUMBER_SIM_DEVICES);
        return false;
    }

    AHSIM_INFO_LOCK();

    for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
        if (AHSIM_sim_info.sim_devices[i] == NULL) {
            AHSIM_sim_info.sim_devices[i] = ah;
            hal_sim->sim_index = i;

            ++AHSIM_sim_info.num_of_sim_devices;

            break;
        }
    }
    AHSIM_INFO_UNLOCK();

    if (i == MAX_NUMBER_SIM_DEVICES) {
        return false;    // should never be true
    }

    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "RegisterSimDeviceInsatnce() -- registered 0x%x at position %d - now have %d registered simulated devices\n",
        ah, hal_sim->sim_index, AHSIM_sim_info.num_of_sim_devices);

    return true;
}

void AHSIM_unregister_sim_device_instance(struct ath_hal *ah)
{
    u_int16_t i;
    struct ath_hal_sim *hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;

    // de-initialization
    AHSIM_TIMER_LOCK_DESTROY(hal_sim); // XXX - do we need to do a LOCK before doing this?
    OS_FREE_TIMER(&hal_sim->tx_timer);

    // de-registration
    AHSIM_ASSERT(hal_sim->sim_index < MAX_NUMBER_SIM_DEVICES); /* Invalid sim_index - out of range! */

    AHSIM_ASSERT(AHSIM_sim_info.sim_devices[hal_sim->sim_index] == ah); /* Invalid stored ah - doesn't match one being unregistered! */

    AHSIM_INFO_LOCK();
    AHSIM_sim_info.sim_devices[hal_sim->sim_index] = NULL;

    --AHSIM_sim_info.num_of_sim_devices;
    AHSIM_INFO_UNLOCK();
    if (AHSIM_sim_info.num_of_sim_devices == 0) {
        AHSIM_INFO_LOCK_DESTROY();
        AHSIM_OUTPUT_BUFFER_LOCK_DESTROY();
        AHSIM_INPUT_BUFFER_LOCK_DESTROY();
    }

    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "UnregisterSimDeviceInstance() -- unregistered 0x%x from position %d - now have %d registered simulated devices\n",
        ah, hal_sim->sim_index, AHSIM_sim_info.num_of_sim_devices);
}

u_int32_t AHSIM_UpdateMonitorDeviceInfo()
{
    int32_t devices_tbd;
    u_int32_t i;
    u_int32_t mon_node_index = MAX_NUMBER_SIM_DEVICES;

    /* Find monitor device (if any) and set indications in ah structure */
    AHSIM_sim_info.do_mon_pkt = AHSIM_sim_info.output_mon_pkts;
    if (AHSIM_sim_info.do_mon_pkt) {
        AHSIM_sim_info.mon_pkt_index = 0;
        AHSIM_sim_info.mon_pkt_entry_index = 0;
        AHSIM_sim_info.mon_pkt_entry_size[0] = 0;

        devices_tbd = AHSIM_sim_info.num_of_sim_devices;
        for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
            if (AHSIM_sim_info.sim_devices[i] == NULL) {
                continue;
            }
            --devices_tbd;

            AH_PRIVATE(AHSIM_sim_info.sim_devices[i])->ah_hal_sim.is_monitor =
                AH_PRIVATE(AHSIM_sim_info.sim_devices[i])->ah_opmode == HAL_M_MONITOR;
            AH_PRIVATE(AHSIM_sim_info.sim_devices[i])->ah_hal_sim.output_pkt_snippet_length =
                AHSIM_sim_info.output_pkt_snippet_length;

            if (AH_PRIVATE(AHSIM_sim_info.sim_devices[i])->ah_hal_sim.is_monitor) {
                if (mon_node_index != MAX_NUMBER_SIM_DEVICES) {
                    HDPRINTF(NULL, HAL_DBG_UNMASKABLE, "AHSIM_DistributeTransmitPacket() -- Multiple devices (%d & %d) are monitors -- descriptor packets will only be output on the first one!\n",
                             mon_node_index, i);
                } else {
                    mon_node_index = i;
                }
            }

            if (!devices_tbd) {
                break;
            }
        }

        if (mon_node_index == MAX_NUMBER_SIM_DEVICES) {
            AHSIM_sim_info.do_mon_pkt = false;
        }
    }

    return mon_node_index;
}

void AHSIM_DistributeTransmitPacket(struct ath_hal *ah)
{
    struct ath_hal_sim *src_hal_sim;
    int32_t devices_tbd;
    u_int32_t i;
    u_int32_t q;
    u_int32_t mon_node_index;
    u_int32_t num_pkts = 0;
    u_int32_t var;
    u_int8_t per;
    u_int8_t rv;
    u_int64_t ba_mask;
    u_int64_t final_ba_mask = 0;
    bool is_unicast_for_anyone;
    bool unicast;
    bool intersim_buffer_full = false;

    src_hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    q = src_hal_sim->ahsim_get_active_tx_queue(ah);
    if (q == 255) {
        return;
    }

    AHSIM_INFO_LOCK(); /* also prevents another Tx from interfering until this is completely done */

    /* IBSS beacon handling - random decision on every beacon cycle (not the way hardware handles but gives some randomness/rotation) */
    if ((q == (HAL_NUM_TX_QUEUES - 1)) && (AH_PRIVATE(ah)->ah_opmode == HAL_M_IBSS)) {
        u_int32_t num_ibss = 0;
        int32_t lowest_ibss_index = -1;
        u_int32_t my_ibss_index = 0;

        devices_tbd = AHSIM_sim_info.num_of_sim_devices;
        for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
            if (AHSIM_sim_info.sim_devices[i] == NULL) {
                continue;
            }
            --devices_tbd;

            if (AH_PRIVATE(AHSIM_sim_info.sim_devices[i])->ah_opmode == HAL_M_IBSS) {
                if (AHSIM_sim_info.sim_devices[i] == ah) {
                    my_ibss_index = num_ibss;
                }
                ++num_ibss;
                if (lowest_ibss_index == -1) {
                    lowest_ibss_index = i;
                }
            }

            if (!devices_tbd) {
                break;
            }
        }

        if (num_ibss > 1) {
            if (src_hal_sim->sim_index == lowest_ibss_index) {
                AHSIM_sim_info.ibss_beacon_sta_index = AHSIM_rand(0, num_ibss - 1);
            }

            if (my_ibss_index != AHSIM_sim_info.ibss_beacon_sta_index) {
                src_hal_sim->ahsim_done_transmit(ah, q, 0, 0); /* not sure if this is needed - at least clear out queue */
                AHSIM_INFO_UNLOCK();
                return;
            }
        }
    }

    /* Find monitor device (if any) and set indications in ah structure */
    mon_node_index = AHSIM_UpdateMonitorDeviceInfo();

    if (AHSIM_sim_info.do_mon_pkt) {
        AHSIM_MonitorPktDataNodeIndex(src_hal_sim->sim_index, ah, NULL);
        src_hal_sim->ahsim_record_tx_c(ah, q, false);
    }

    devices_tbd = AHSIM_sim_info.num_of_sim_devices;
    is_unicast_for_anyone = false;
    for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
        struct ath_hal* dest_ath_hal = AHSIM_sim_info.sim_devices[i];

        if (devices_tbd == 0) {
            break;
        }

        if (dest_ath_hal == NULL) {
            continue;
        }

        --devices_tbd;

        if (dest_ath_hal == ah) {
            continue;
        }

        AHSIM_MonitorPktDataNodeIndex(AH_PRIVATE(dest_ath_hal)->ah_hal_sim.sim_index, dest_ath_hal, NULL);

        /* break-on-packet */
        if (AHSIM_sim_info.break_on_packet) {
            u_int32_t frag_length;
            u_int8_t* tx_buffer = src_hal_sim->ahsim_get_tx_buffer(ah, q, &frag_length);
            u_int8_t frame_type, frame_subtype;

            frame_type = (tx_buffer[0] & 0x0C) >> 2;
            frame_subtype = (tx_buffer[0] & 0xF0) >> 4;

            if ((frame_type == AHSIM_sim_info.bop_type) && (frame_subtype == AHSIM_sim_info.bop_subtype)) {
                AHSIM_BREAKPOINT
            }
        }

        /* are we on the same channel? */
        if (AHSIM_sim_info.sim_channels &&
            (AH_PRIVATE(dest_ath_hal)->ah_curchan->channel != AH_PRIVATE(ah)->ah_curchan->channel) &&
            !AH_PRIVATE(dest_ath_hal)->ah_hal_sim.is_monitor) {
            AHSIM_MonitorPktChannelInfo(AH_PRIVATE(ah)->ah_curchan->channel, AH_PRIVATE(dest_ath_hal)->ah_curchan->channel);
            continue;
        }

        /* PER simulation (not agg!) */
        per = AHSIM_sim_info.per_src_dest[src_hal_sim->sim_index][AH_PRIVATE(dest_ath_hal)->ah_hal_sim.sim_index];
        if (per && !AH_PRIVATE(dest_ath_hal)->ah_hal_sim.is_monitor) {
            rv = AHSIM_rand(0, 99);
            if (per > rv) {
                AHSIM_MonitorPktPERInfo(per, rv);
                continue;
            }
        }

        ba_mask = 0;
        unicast = false;
        var = src_hal_sim->ahsim_distribute_transmit_packet(ah, NULL, dest_ath_hal, q, &ba_mask, &unicast);
        if (!num_pkts) {
            num_pkts = var;
            final_ba_mask = ba_mask;
        } else {
            AHSIM_ASSERT(var == 0); /* not kosher to have multiple ACK'ers - something is fishy in simulator */
        }
        if (!is_unicast_for_anyone) {
            is_unicast_for_anyone = unicast;
        } else {
            AHSIM_ASSERT(!unicast); /* not kosher to be unicast to multiple nodes (essentially same check as the ACK check above) */
        }
    }

    if (!AHSIM_sim_info.sim_stand_alone_mode && (AHSIM_sim_info.inter_sim_promiscuous_mode || !is_unicast_for_anyone)) {
        intersim_buffer_full = !src_hal_sim->ahsim_record_tx_c(ah, q, true);
        if (intersim_buffer_full) {
            AHSIM_sim_info.output_buffer_overflow_indicator = true;
            AHSIM_MonitorPktStall(); /* i.e. we'll be putting a stall indicattor until the output buffer is cleared! */
        }
    }

    if (!AHSIM_sim_info.sim_stand_alone_mode && !num_pkts) {
        num_pkts = 1;
        final_ba_mask = ~final_ba_mask;
    }

    AHSIM_MonitorPktDataNodeIndex(src_hal_sim->sim_index, ah, NULL);
    src_hal_sim->ahsim_done_transmit(ah, q, num_pkts, final_ba_mask);

    if (AHSIM_sim_info.do_mon_pkt) {
        AHSIM_TxMonitorPkt(mon_node_index);
    }

    AHSIM_INFO_UNLOCK();

    AHSIM_TIMER_LOCK(src_hal_sim);
    if (src_hal_sim->ahsim_get_active_tx_queue(ah) != 255) {
        src_hal_sim->tx_timer_active = true;
        OS_SET_TIMER(&src_hal_sim->tx_timer, 0);
    }
    AHSIM_TIMER_UNLOCK(src_hal_sim);
}

void AHSIM_DistributeIntersimTransmitPacket()
{
    u_int32_t mon_node_index = 0;
    u_int32_t input_buffer_offset = 0;
    u_int32_t devices_tbd = 0;
    u_int32_t i = 0;
    u_int32_t tx_channel = 0;

    u_int64_t ba_mask = 0;
    bool unicast = false;
    bool is_unicast_for_anyone = false;
    u_int32_t num_pkts = 0;
    u_int32_t var = 0;

    AHSIM_INFO_LOCK();

    /* Find monitor device (if any) and set indications in ah structure */
    mon_node_index = AHSIM_UpdateMonitorDeviceInfo();

    /* unpack and process intersim data */
    while (input_buffer_offset < AHSIM_sim_info.input_buffer_num_bytes) {
        AHSIM_ASSERT((*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset] & DBG_ATHEROS_MASK) == DBG_ATHEROS_DESC_ID);

        if (AHSIM_sim_info.do_mon_pkt) {
            AHSIM_MonitorPktDataNodeIndex(0xFFFF, NULL, &AHSIM_sim_info.input_buffer[input_buffer_offset] + 4);
            AHSIM_monitor_pkt_data_descriptor(23 * 4, &AHSIM_sim_info.input_buffer[input_buffer_offset] + 10 + 12);
        }

        input_buffer_offset += 10;

        AHSIM_ASSERT(*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset] == DBG_ATHEROS_CHANNEL_ID);
        tx_channel = *(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset + 4];
        input_buffer_offset += 12;

        AHSIM_ASSERT((*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset] & DBG_ATHEROS_MASK) == (ATHEROS_VENDOR_ID << 16));
        AHSIM_ASSERT(*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset + 23 * 4] == DBG_ATHEROS_KEY_ID);
        AHSIM_ASSERT((*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset + 23 * 4 + 9 * 4] & DBG_ATHEROS_MASK) == DBG_ATHEROS_SNIPPET_ID);

        num_pkts = 0;
        is_unicast_for_anyone = false;
        devices_tbd = AHSIM_sim_info.num_of_sim_devices;
        for (i = 0; i < MAX_NUMBER_SIM_DEVICES; ++i) {
            struct ath_hal* dest_ath_hal = AHSIM_sim_info.sim_devices[i];

            if (devices_tbd == 0) {
                break;
            }

            if (dest_ath_hal == NULL) {
                continue;
            }

            --devices_tbd;

            AHSIM_MonitorPktDataNodeIndex(AH_PRIVATE(dest_ath_hal)->ah_hal_sim.sim_index, dest_ath_hal, NULL);

            /* break-on-packet */
            if (AHSIM_sim_info.break_on_packet) {
                u_int8_t* tx_buffer = &AHSIM_sim_info.input_buffer[input_buffer_offset + 23 * 4 + 9 * 4 + 4];
                u_int8_t frame_type, frame_subtype;

                frame_type = (tx_buffer[0] & 0x0C) >> 2;
                frame_subtype = (tx_buffer[0] & 0xF0) >> 4;

                if ((frame_type == AHSIM_sim_info.bop_type) && (frame_subtype == AHSIM_sim_info.bop_subtype)) {
                    AHSIM_BREAKPOINT
                }
            }

            /* are we on the same channel? */
            if (AHSIM_sim_info.sim_channels &&
                (AH_PRIVATE(dest_ath_hal)->ah_curchan->channel != tx_channel) &&
                !AH_PRIVATE(dest_ath_hal)->ah_hal_sim.is_monitor) {
                AHSIM_MonitorPktChannelInfo(tx_channel, AH_PRIVATE(dest_ath_hal)->ah_curchan->channel);
                continue;
            }

            ba_mask = 0;
            unicast = false;
            var = AH_PRIVATE(dest_ath_hal)->ah_hal_sim.ahsim_distribute_transmit_packet(NULL, AHSIM_sim_info.input_buffer + input_buffer_offset,
                                                                       dest_ath_hal, 0, &ba_mask, &unicast);
            if (!num_pkts) {
                num_pkts = var;
            } else {
                AHSIM_ASSERT(var == 0); /* not kosher to have multiple ACK'ers - something is fishy in simulator */
            }
            if (!is_unicast_for_anyone) {
                is_unicast_for_anyone = unicast;
            } else {
                AHSIM_ASSERT(!unicast); /* not kosher to be unicast to multiple nodes (essentially same check as the ACK check above) */
            }
        }

        /* move through the complete packet (i.e. all tx_c's until the next node ID) */
        while (1) {
            input_buffer_offset += 23 * 4; /* tx_c */
            input_buffer_offset += 9 * 4; /* key */
            input_buffer_offset += (*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset] & ~DBG_ATHEROS_MASK) + 4;

            if (input_buffer_offset >= AHSIM_sim_info.input_buffer_num_bytes) {
                break;
            }

            if ((*(u_int32_t*)&AHSIM_sim_info.input_buffer[input_buffer_offset] & DBG_ATHEROS_MASK) == DBG_ATHEROS_DESC_ID) {
                break;
            }
        }
    }

    if (AHSIM_sim_info.do_mon_pkt) {
        AHSIM_TxMonitorPkt(mon_node_index);
    }

    AHSIM_INFO_UNLOCK();
}

static bool AHSIM_UpdatePktIndex(u_int32_t num_bytes)
{
    if ((AHSIM_sim_info.mon_pkt_index + num_bytes) > AHSIM_MON_PKT_BUFFER_SIZE) {
        return false;
    }

    if ((AHSIM_sim_info.mon_pkt_index - AHSIM_sim_info.mon_pkt_entry_size[AHSIM_sim_info.mon_pkt_entry_index] + num_bytes) > AHSIM_MON_PKT_MAX_SIZE) {
        AHSIM_sim_info.mon_pkt_entry_size[++AHSIM_sim_info.mon_pkt_entry_index] = AHSIM_sim_info.mon_pkt_index;
    }

    return true;
}

void AHSIM_MonitorPktDataNodeIndex(u_int32_t node_index, struct ath_hal *ah, u_int8_t* alt_data)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(10)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_DESC_ID | node_index;
    if (alt_data) {
        OS_MEMCPY(AHSIM_sim_info.mon_pkt + AHSIM_sim_info.mon_pkt_index + 4, alt_data, 6);
    } else {
        ah->ah_get_mac_address(ah, AHSIM_sim_info.mon_pkt + AHSIM_sim_info.mon_pkt_index + 4);
    }
    AHSIM_sim_info.mon_pkt_index += 10;
}

void AHSIM_monitor_pkt_data_descriptor(u_int32_t num_bytes, u_int8_t* bytes)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(num_bytes)) {
        return;
    }

    OS_MEMCPY(AHSIM_sim_info.mon_pkt + AHSIM_sim_info.mon_pkt_index, bytes, num_bytes);
    AHSIM_sim_info.mon_pkt_index += num_bytes;
}

void AHSIM_monitor_pkt_snippet(u_int32_t num_bytes, u_int8_t* bytes)
{
    if (!AHSIM_sim_info.do_mon_pkt || !num_bytes) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(num_bytes + 4)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_SNIPPET_ID | num_bytes;
    AHSIM_sim_info.mon_pkt_index += 4;

    OS_MEMCPY(AHSIM_sim_info.mon_pkt + AHSIM_sim_info.mon_pkt_index, bytes, num_bytes);
    AHSIM_sim_info.mon_pkt_index += num_bytes;
}

void AHSIM_monitor_pkt_filter_info(u_int32_t rx_filter, u_int32_t failed_filter)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(12)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_FILTER_ID;
    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index + 4] = rx_filter;
    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index + 8] = failed_filter;
    AHSIM_sim_info.mon_pkt_index += 12;
}

void AHSIM_MonitorPktChannelInfo(u_int32_t tx_channel, u_int32_t rx_channel)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(12)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_CHANNEL_ID;
    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index + 4] = tx_channel;
    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index + 8] = rx_channel;
    AHSIM_sim_info.mon_pkt_index += 12;
}

void AHSIM_MonitorPktPERInfo(u_int8_t per, u_int8_t rv)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(4)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_PER_ID | (per << 8) | rv;
    AHSIM_sim_info.mon_pkt_index += 4;
}

void AHSIM_monitor_pkt_agg_per_info(u_int8_t agg_per, u_int8_t rv, u_int32_t agg_pkt_num)
{
    if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(8)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_AGG_PER_ID | (agg_per << 8) | rv;
    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index + 4] = agg_pkt_num;
    AHSIM_sim_info.mon_pkt_index += 8;
}

void AHSIM_MonitorPktKey(u_int8_t* key_buffer)
{
   if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(9 * 4)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_KEY_ID;
    OS_MEMCPY(AHSIM_sim_info.mon_pkt + AHSIM_sim_info.mon_pkt_index + 4, key_buffer, 8 * 4);
    AHSIM_sim_info.mon_pkt_index += 9 * 4;
}

void AHSIM_MonitorPktStall()
{
   if (!AHSIM_sim_info.do_mon_pkt) {
        return;
    }
    if (!AHSIM_UpdatePktIndex(4)) {
        return;
    }

    *(u_int32_t*)&AHSIM_sim_info.mon_pkt[AHSIM_sim_info.mon_pkt_index] = DBG_ATHEROS_SIM_STALL_ID;
    AHSIM_sim_info.mon_pkt_index += 4;
}

void AHSIM_TxMonitorPkt(u_int32_t mon_node_index)
{
    u_int32_t i;
    AHSIM_sim_info.mon_pkt_entry_size[++AHSIM_sim_info.mon_pkt_entry_index] = AHSIM_sim_info.mon_pkt_index;

    for (i = 0; i < AHSIM_sim_info.mon_pkt_entry_index; ++i) {
        AH_PRIVATE(AHSIM_sim_info.sim_devices[mon_node_index])->ah_hal_sim.ahsim_rx_dummy_pkt(AHSIM_sim_info.sim_devices[mon_node_index], AHSIM_sim_info.mon_pkt +
                                                                                               AHSIM_sim_info.mon_pkt_entry_size[i],
                                                                 AHSIM_sim_info.mon_pkt_entry_size[i + 1] - AHSIM_sim_info.mon_pkt_entry_size[i]);
    }
}

/* Basic random number generator */

#define RAND_MODULUS    2147483647 /* DON'T CHANGE THIS VALUE                  */
#define RAND_MULTIPLIER 48271      /* DON'T CHANGE THIS VALUE                  */
#define RAND_DEFAULT    123456789  /* initial seed, use 0 < DEFAULT < MODULUS  */

static u_int32_t rand_seed = RAND_DEFAULT;

/* Returns [min,max] - i.e. inclusive of both ends */
u_int32_t AHSIM_rand(u_int32_t min, u_int32_t max)
{
    const u_int32_t Q = RAND_MODULUS / RAND_MULTIPLIER;
    const u_int32_t R = RAND_MODULUS % RAND_MULTIPLIER;
    u_int32_t t;

    t = RAND_MULTIPLIER * (rand_seed % Q) - R * (rand_seed / Q);
    if (t > 0) {
        rand_seed = t;
    } else {
        rand_seed = t + RAND_MODULUS;
    }

    return min + rand_seed % (max - min + 1);
}

/* initialize random number generator seed */
void AHSIM_srand(u_int32_t x)
{
    if (x > 0) {
        rand_seed = x % RAND_MODULUS;
    } else {
        rand_seed = OS_GET_TIMESTAMP() % RAND_MODULUS;
    }
}


#endif // ATH_DRIVER_SIM

