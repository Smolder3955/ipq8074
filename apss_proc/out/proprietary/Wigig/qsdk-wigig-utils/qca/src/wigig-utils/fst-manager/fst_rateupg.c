/*
 * FST Manager: Rate Upgrade
 *
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/list.h"
#include "fst_rateupg.h"

#define FST_MGR_COMPONENT "RATEUPG"
#include "fst_manager.h"
#include "common/ieee802_11_defs.h"

struct rate_upgrade_mac {
	u8             addr[ETH_ALEN];
	struct dl_list lentry;
};

struct rate_upgrade_group {
	char                  *groupname;
	char                  *master;
	char                  *acl_fname;
	struct fst_iface_info *slaves;
	int                    slave_cnt;
	struct dl_list         acl_macs;
	struct dl_list         lentry;
};

struct rate_upgrade_manager {
	struct fst_ini_config *iniconf;
	struct dl_list        groups;
};

static struct rate_upgrade_manager g_rateupg_mgr;
static int            g_rateupg_mgr_initialized = 0;

static struct rate_upgrade_mac *find_rate_upgrade_mac(
	struct rate_upgrade_group *g, const u8 *addr)
{
	struct rate_upgrade_mac *p;
	dl_list_for_each(p, &g->acl_macs, struct rate_upgrade_mac, lentry)
		if (!os_memcmp(p->addr, addr, ETH_ALEN))
			return p;

	return NULL;
}

static struct rate_upgrade_mac *add_rate_upgrade_mac(
	struct rate_upgrade_group *g, const u8 *addr)
{
	struct rate_upgrade_mac *p = os_malloc(sizeof(*p));
	if (p) {
		os_memcpy(p->addr, addr, ETH_ALEN);
		dl_list_add_tail(&g->acl_macs, &p->lentry);
	}
	return p;
}

static void del_rate_upgrade_mac(struct rate_upgrade_mac *p)
{
	dl_list_del(&p->lentry);
	os_free(p);
}

static int update_acl_file(struct rate_upgrade_group *g)
{
	struct rate_upgrade_mac *p;
	int res = -1;
	FILE *f;

	if (!g->acl_fname)
		return 0;

	f = fopen(g->acl_fname, "w");
	if (!f) {
		fst_mgr_printf(MSG_ERROR, "group %s: cannot open acl file: %s",
			g->groupname, g->acl_fname);
		goto error_file;
	}

	dl_list_for_each(p, &g->acl_macs, struct rate_upgrade_mac, lentry)
		if (fprintf(f, MACSTR "\n", MAC2STR(p->addr)) <= 0) {
			fst_mgr_printf(MSG_ERROR,
				"group %s: cannot fill acl file: %s",
				g->groupname, g->acl_fname);
			goto error_fprintf;
		}

	res = 0;

error_fprintf:
	fclose(f);
error_file:
	return res;
}

static struct rate_upgrade_group *find_rate_upgrade_group(const char *name)
{
	struct rate_upgrade_group *g;
	dl_list_for_each(g, &g_rateupg_mgr.groups, struct rate_upgrade_group, lentry) {
		if(!strncmp(g->groupname, name, strlen(name)))
			return g;
	}
	return NULL;
}

static void deinit_rate_upgrade_group(struct rate_upgrade_group *g)
{
	free(g->slaves);
	free(g->master);
	free(g->groupname);
	free(g->acl_fname);
	dl_list_del(&g->lentry);
	free(g);
}

int fst_rate_upgrade_init(struct fst_ini_config *h)
{
	os_memset(&g_rateupg_mgr, 0, sizeof(g_rateupg_mgr));
	dl_list_init(&g_rateupg_mgr.groups);
	g_rateupg_mgr_initialized = 1;
	g_rateupg_mgr.iniconf = h;
	return 0;
}

void fst_rate_upgrade_deinit()
{
	if (g_rateupg_mgr_initialized) {
		while (!dl_list_empty(&g_rateupg_mgr.groups)) {
			struct rate_upgrade_group *g = dl_list_first(&g_rateupg_mgr.groups,
					struct rate_upgrade_group, lentry);
			deinit_rate_upgrade_group(g);
		}
		g_rateupg_mgr_initialized = 0;
	}
}

int fst_rate_upgrade_add_group(const struct fst_group_info *group)
{
	struct rate_upgrade_group *g;
	struct fst_iface_info *ifaces;
	int i;
	char *master = NULL;
	char *acl_fname = NULL;
	char ctrl_iface_dir[256];
	int ctrl_iface_dir_len;

	if (find_rate_upgrade_group(group->id)) {
		fst_mgr_printf(MSG_WARNING, "Group %s already added", group->id);
		return 0;
	}

	master = fst_ini_config_get_rate_upgrade_master(
		g_rateupg_mgr.iniconf, group->id);
	if (!master)
		return 0;

	if (!fst_is_supplicant()) {
		acl_fname = fst_ini_config_get_rate_upgrade_acl_fname(
			g_rateupg_mgr.iniconf, group->id);
		if (acl_fname != NULL)
			fst_mgr_printf(MSG_INFO, "Using ACL file %s", acl_fname);
	}

	g = malloc(sizeof(struct rate_upgrade_group));
	if (!g) {
		fst_mgr_printf(MSG_ERROR, "Cannot alloc group %s",
			group->id);
		goto error_group;
	}

	memset(g, 0, sizeof(struct rate_upgrade_group));
	g->master = master;
	g->groupname = strdup(group->id);
	if (g->groupname == NULL) {
		fst_mgr_printf(MSG_ERROR, "Cannot alloc groupname %s",
			group->id);
		goto error_groupname;
	}
	g->slave_cnt = fst_ini_config_get_group_slave_ifaces(
		g_rateupg_mgr.iniconf, group, master, &ifaces);
	if (g->slave_cnt < 0) {
		fst_mgr_printf(MSG_ERROR, "Cannot add group %s", group->id);
		goto error_get_slave;
	}
	else if (g->slave_cnt == 0) {
		fst_mgr_printf(MSG_ERROR,
			"No slave ifaces found in group %s", group->id);
		goto error_get_slave;
	}
	g->slaves = ifaces;
	dl_list_init(&g->acl_macs);
	g->acl_fname = acl_fname;

	if (update_acl_file(g)) {
		fst_mgr_printf(MSG_ERROR, "Cannot update ACL file");
		goto error_acl_file;
	}

	ctrl_iface_dir_len = fst_ini_config_get_slave_ctrl_interface(
		g_rateupg_mgr.iniconf, ctrl_iface_dir, sizeof(ctrl_iface_dir));
	for (i = 0; i < g->slave_cnt; i++) {
		if (fst_add_iface(master, &ifaces[i], g->acl_fname,
				(ctrl_iface_dir_len > 0) ? ctrl_iface_dir : NULL)) {
			fst_mgr_printf(MSG_ERROR,
				"Cannot add slave interface %s", ifaces[i].name);
			goto error_add;
		}
	}

	dl_list_add_tail(&g_rateupg_mgr.groups, &g->lentry);

	return 0;

error_add:
	while(i-- > 0)
		fst_del_iface(&ifaces[i]);
error_acl_file:
	free(ifaces);
error_get_slave:
	free(g->groupname);
error_groupname:
	free(g);
error_group:
	free(acl_fname);
	free(master);
	return -1;
}

int fst_rate_upgrade_del_group(const struct fst_group_info *group)
{
	struct rate_upgrade_group *g;
	int i;

	g = find_rate_upgrade_group(group->id);
	if (g == NULL) {
		fst_mgr_printf(MSG_ERROR, "No group exists %s", group->id);
		return -1;
	}

	for (i = 0; i < g->slave_cnt; i++) {
		if (fst_del_iface(&g->slaves[i])) {
			fst_mgr_printf(MSG_ERROR, "Cannot delete iface %s",
				g->slaves[i].name);
		}
	}

	deinit_rate_upgrade_group(g);
	return 0;
}
static int fst_dup_connection_sta(const struct rate_upgrade_group *g,
				  const char *iface, const u8* addr)
{
	int i;
	char *str_mbies = NULL;
	int str_mbies_size;
	int mbies_size;
	u8 *mbies = NULL, *mbies_iter;

	str_mbies_size = fst_get_peer_mbies(iface, addr, &str_mbies);
	if (str_mbies_size < 2 || str_mbies_size & 1) {
		fst_mgr_printf(MSG_INFO, "invalid mbies size %d",
			       str_mbies_size);
		for (i = 0; i < g->slave_cnt; i++) {
			if (fst_dup_connection(&g->slaves[i], g->master, addr,
					       g->acl_fname)) {
				fst_mgr_printf(MSG_ERROR, "Cannot connect iface %s",
					       g->slaves[i].name);
				goto error_connect;
			}
		}
		os_free(str_mbies);
		return 0;
	}

	mbies_size = str_mbies_size / 2;
	mbies = os_malloc(mbies_size);
	if (!mbies) {
		fst_mgr_printf(MSG_ERROR, "mbies allocation failed");
		goto error_mbie;
	}
	if (hexstr2bin(str_mbies, mbies, mbies_size)) {
		fst_mgr_printf(MSG_ERROR, "failed converting hex mbie to bin");
		goto error_mbie;
	}

	/* for each slave duplicate the addresses from all bands */
	for (i = 0; i < g->slave_cnt; i++) {
		mbies_iter = mbies;
		while (mbies_size >= 2) {
			struct multi_band_ie *mbie = (struct multi_band_ie *) mbies_iter;
			const u8 *addr_on_other_band;

			if (mbie->eid != WLAN_EID_MULTI_BAND ||
			    (size_t) 2 + mbie->len < sizeof(*mbie))
				break;

			addr_on_other_band = fst_mgr_get_addr_from_mbie(mbie);
			if (!addr_on_other_band)
				continue;

			if (fst_dup_connection(&g->slaves[i], g->master,
					       addr_on_other_band,
					       g->acl_fname))
				goto error_connect;

			mbies_iter += mbie->len + 2;
			mbies_size -= mbie->len + 2;
		}
	}
	os_free(str_mbies);
	os_free(mbies);
	return 0;

error_connect:
	while (i-- > 0)
		fst_dedup_connection(&g->slaves[i], g->acl_fname);
error_mbie:
	os_free(str_mbies);
	os_free(mbies);
	return -1;
}

static int fst_dup_connection_ap(const struct rate_upgrade_group *g,
				 const u8* addr)
{
	int i;

	for (i = 0; i < g->slave_cnt; i++) {
		if (fst_dup_connection(&g->slaves[i], g->master, addr,
				       g->acl_fname)) {
			fst_mgr_printf(MSG_ERROR, "Cannot connect iface %s",
				       g->slaves[i].name);
			goto error_connect;
		}
	}
	return 0;

error_connect:
	while (i-- > 0)
		fst_dedup_connection(&g->slaves[i], g->acl_fname);
	return -1;
}

int fst_rate_upgrade_on_connect(const struct fst_group_info *group,
	const char *iface, const u8* addr)
{
	int res;
	struct rate_upgrade_group *g;
	struct rate_upgrade_mac *p;

	g = find_rate_upgrade_group(group->id);
	if (!g || os_strcmp(iface, g->master))
		return 0;

	if (find_rate_upgrade_mac(g, addr)) {
		fst_mgr_printf(MSG_WARNING, "MAC " MACSTR
			"is already connected", MAC2STR(addr));
		return 0;
	}

	p = add_rate_upgrade_mac(g, addr);
	if (!p) {
		fst_mgr_printf(MSG_ERROR, "Cannot add MAC " MACSTR,
			MAC2STR(addr));
		return -1;
	}

	if (update_acl_file(g)) {
		fst_mgr_printf(MSG_ERROR, "Cannot update ACL file");
		goto error_acl_file;
	}

	if (fst_is_supplicant())
		res = fst_dup_connection_sta(g, iface, addr);
	else
		res = fst_dup_connection_ap(g, addr);

	if (!res)
		return res;

error_acl_file:
	del_rate_upgrade_mac(p);
	return -1;
}

int fst_rate_upgrade_on_disconnect(const struct fst_group_info *group,
	const char *iface, const u8* addr)
{
	int i, res = 0;
	struct rate_upgrade_group *g;
	struct rate_upgrade_mac *p;

	g = find_rate_upgrade_group(group->id);
	if (!g || os_strcmp(iface, g->master))
		return 0;

	p = find_rate_upgrade_mac(g, addr);
	if (!p) {
		fst_mgr_printf(MSG_ERROR, "Cannot find master peer");
		return -1;
	}

	del_rate_upgrade_mac(p);

	if (update_acl_file(g)) {
		fst_mgr_printf(MSG_ERROR, "Cannot update ACL file");
		return -1;
	}

	for (i = 0; i < g->slave_cnt; i++) {
		if (fst_dedup_connection(&g->slaves[i], g->acl_fname)) {
			fst_mgr_printf(MSG_ERROR, "Cannot disconnect iface %s",
			g->slaves[i].name);
			res = -1;
		}
	}

	return res;
}

void fst_rate_upgrade_on_switch_completed(const struct fst_group_info *group,
	const char *old_iface, const char *new_iface, const u8* old_peer_addr)
{
	struct rate_upgrade_group *g;
	int ret;

	fst_mgr_printf(MSG_INFO, "%s=>%s, old peer address " MACSTR,
		old_iface, new_iface, MAC2STR(old_peer_addr));

	if (fst_is_supplicant())
		/* do nothing for STA mode */
		return;

	g = find_rate_upgrade_group(group->id);
	if (!g) {
		fst_mgr_printf(MSG_INFO, "%s is not a rate upgrade group", group->id);
		return;
	}

	if (os_strcmp(old_iface, g->master) == 0) {
		fst_mgr_printf(MSG_INFO, "switching from master, do nothing");
		return;
	}

	/* old_iface is not master, disconnect from peer */
	ret = fst_disconnect_peer(old_iface, old_peer_addr);
	if (ret)
		fst_mgr_printf(MSG_ERROR, "failed to disconnect peer");
}
