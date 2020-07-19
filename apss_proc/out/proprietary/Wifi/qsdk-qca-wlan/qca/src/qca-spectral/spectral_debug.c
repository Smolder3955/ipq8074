#include "spectral_defs.h"

void debug_print_samp_msg (SPECTRAL_SAMP_MSG *samp)
{
    SPECTRAL_SAMP_DATA *data = &(samp->samp_data);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL,"SPECTRAL_SAMP_MSG ");
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL,"freq=%d \n", samp->freq);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL1,"comb_rssi=%d\n", data->spectral_combined_rssi);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL1,"max_scale=%d\n", data->spectral_max_scale);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL1,"maxindex=%d\n", data->spectral_max_index);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL1,"maxmag=%d\n", data->spectral_max_mag);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL1,"maxexp=%d\n", data->spectral_max_exp);
}

void print_current_prev_message_pools(void)
{
    PER_FREQ_SAMP_MSG_POOL *lower_pool = &lower;
    PER_FREQ_SAMP_MSG_POOL *upper_pool = &upper;

    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL,"%s current freq = %d, lower_pool=%p pool_freq=%d \n", __func__, g_current_freq, lower_pool->pool_ptr, lower_pool->freq);
    print_saved_message_pool(lower_pool->pool_ptr);

    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL,"%s prev freq = %d, upper_pool=%p pool_freq=%d\n", __func__, g_prev_freq, upper_pool->pool_ptr, upper_pool->freq);
    print_saved_message_pool(upper_pool->pool_ptr);
}

void print_saved_message_pool(SAVED_SAMP_MESSAGE *pool)
{
    int i=0;
    SPECTRAL_SAMP_MSG *msg_ptr;

    for (i=0; i< MAX_SAVED_SAMP_MSGS; i++) {
        msg_ptr=&pool[i].saved_msg;
        SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL, "[%d] <%p> msg_valid=%d msg_freq=%d\n", i, &pool[i], pool[i].msg_valid,pool[i].saved_msg.freq);
    }

}

void print_samp_msg (SPECTRAL_SAMP_MSG *samp)
{
    SPECTRAL_SAMP_DATA *data = &(samp->samp_data);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"SPECTRAL_SAMP_MSG ");
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"freq=%d \n", samp->freq);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"tstamp=%x\n", data->spectral_tstamp);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"comb_rssi=%d\n", data->spectral_combined_rssi);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"max_scale=%d\n", data->spectral_max_scale);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"maxindex=%d\n", data->spectral_max_index);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"maxmag=%d\n", data->spectral_max_mag);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"maxexp=%d\n", data->spectral_max_exp);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"nb_lower=%d\n", data->spectral_nb_lower);
    SPECTRAL_DPRINTF(ATH_DEBUG_SPECTRAL2,"nb_upper=%d\n", data->spectral_nb_upper);
}

