/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <local_common.h>
#include <ivc_core.h>
#include "core_local.h"
#include "../osi/common/common.h"

static struct core_local g_core[MAX_CORE_INSTANCES];

/**
 * @brief Function to validate function pointers.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t validate_if_func_ptrs(struct osi_core_priv_data *const osi_core,
				     struct if_core_ops *if_ops_p)
{
	nveu32_t i = 0;
	void *temp_ops = (void *)if_ops_p;
	nve32_t ret = 0;
#if __SIZEOF_POINTER__ == 8
	nveu64_t *l_ops = (nveu64_t *)temp_ops;
#elif __SIZEOF_POINTER__ == 4
	nveu32_t *l_ops = (nveu32_t *)temp_ops;
#else
	OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
		     "Undefined architecture\n", 0ULL);
	ret = -1;
	goto fail;
#endif

	for (i = 0; i < (sizeof(*if_ops_p) / (nveu64_t)__SIZEOF_POINTER__);
	     i++) {
		if (*l_ops == 0U) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "failed at index : ", i);
			ret = -1;
			goto fail;
		}

		l_ops++;
	}
fail:
	return ret;
}

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_if_args(struct osi_core_priv_data *const osi_core,
				       struct core_local *l_core)
{
	nve32_t ret = 0;

	if ((osi_core == OSI_NULL) || (l_core->if_init_done == OSI_DISABLE) ||
	    (l_core->magic_num != (nveu64_t)osi_core)) {
		ret = -1;
	}

	return ret;
}

struct osi_core_priv_data *osi_get_core(void)
{
	nveu32_t i;
	struct osi_core_priv_data *osi_core = OSI_NULL;

	for (i = 0U; i < MAX_CORE_INSTANCES; i++) {
		if (g_core[i].if_init_done == OSI_ENABLE) {
			continue;
		}

		break;
	}

	if (i == MAX_CORE_INSTANCES) {
		goto fail;
	}

	g_core[i].magic_num = (nveu64_t)&g_core[i].osi_core;

	g_core[i].tx_ts_head.prev = &g_core[i].tx_ts_head;
	g_core[i].tx_ts_head.next = &g_core[i].tx_ts_head;
	g_core[i].pps_freq = OSI_DISABLE;

	osi_core = &g_core[i].osi_core;
fail:
	return osi_core;
}

struct osi_core_priv_data *get_role_pointer(nveu32_t role)
{
	nveu32_t i;
	struct osi_core_priv_data *ret_ptr = OSI_NULL;

	if ((role != OSI_PTP_M2M_PRIMARY) &&
	    (role != OSI_PTP_M2M_SECONDARY)) {
		goto done;
	}

	/* Current approch to give pointer for 1st role */
	for (i = 0U; i < MAX_CORE_INSTANCES; i++) {
		if ((g_core[i].if_init_done == OSI_ENABLE) &&
		    (g_core[i].ether_m2m_role == role)) {
			ret_ptr = &g_core[i].osi_core;
			break;
		}
	}

done:
	return ret_ptr;
}

nve32_t osi_init_core_ops(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	static struct if_core_ops if_ops[MAX_INTERFACE_OPS];
	nve32_t ret = 0;

	if (osi_core == OSI_NULL) {
		ret = -1;
		goto fail;
	}

	if (osi_core->use_virtualization > OSI_ENABLE) {
		ret = -1;
		goto fail;
	}

	if ((l_core->magic_num != (nveu64_t)osi_core) ||
	    (l_core->if_init_done == OSI_ENABLE)) {
		ret = -1;
		goto fail;
	}

	l_core->if_ops_p = &if_ops[osi_core->use_virtualization];

	if (osi_core->use_virtualization == OSI_DISABLE) {
		hw_interface_init_core_ops(l_core->if_ops_p);
	} else {
		ivc_interface_init_core_ops(l_core->if_ops_p);
	}

	if (validate_if_func_ptrs(osi_core, l_core->if_ops_p) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Interface function validation failed\n", 0ULL);
		ret = -1;
		goto fail;
	}

	ret = l_core->if_ops_p->if_init_core_ops(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "if_init_core_ops failed\n", 0ULL);
		goto fail;
	}
	l_core->ts_lock = OSI_DISABLE;
	l_core->ether_m2m_role = osi_core->m2m_role;
	l_core->serv.count = SERVO_STATS_0;
	l_core->serv.drift = 0;
	l_core->serv.last_ppb = 0;
	osi_lock_init(&l_core->serv.m2m_lock);
#ifdef MACSEC_SUPPORT
	osi_lock_init(&osi_core->macsec_fpe_lock);
#endif /* MACSEC_SUPPORT */
	l_core->hw_init_successful = OSI_DISABLE;
	l_core->m2m_tsync = OSI_DISABLE;
	l_core->if_init_done = OSI_ENABLE;
	if ((osi_core->m2m_role == OSI_PTP_M2M_PRIMARY) ||
	    (osi_core->m2m_role == OSI_PTP_M2M_SECONDARY)) {
		l_core->m2m_tsync = OSI_ENABLE;
	} else {
		l_core->m2m_tsync = OSI_DISABLE;
	}

	if (osi_core->pps_frq <= OSI_ENABLE) {
		l_core->pps_freq = osi_core->pps_frq;
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid pps_frq\n", (nveu64_t)osi_core->pps_frq);
		ret = -1;
	}
fail:
	return ret;
}

nve32_t osi_write_phy_reg(struct osi_core_priv_data *const osi_core,
			  const nveu32_t phyaddr, const nveu32_t phyreg,
			  const nveu16_t phydata)
{
	nve32_t ret = -1;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (validate_if_args(osi_core, l_core) < 0) {
		goto fail;
	}

	ret = l_core->if_ops_p->if_write_phy_reg(osi_core, phyaddr, phyreg,
						 phydata);
fail:
	return ret;
}

nve32_t osi_read_phy_reg(struct osi_core_priv_data *const osi_core,
			 const nveu32_t phyaddr, const nveu32_t phyreg)
{
	nve32_t ret = -1;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (validate_if_args(osi_core, l_core) < 0) {
		goto fail;
	}

	ret = l_core->if_ops_p->if_read_phy_reg(osi_core, phyaddr, phyreg);
fail:
	return ret;
}

nve32_t osi_hw_core_init(struct osi_core_priv_data *const osi_core)
{
	nve32_t ret = -1;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (validate_if_args(osi_core, l_core) < 0) {
		goto fail;
	}

	ret = l_core->if_ops_p->if_core_init(osi_core);
fail:
	return ret;
}

nve32_t osi_hw_core_deinit(struct osi_core_priv_data *const osi_core)
{
	nve32_t ret = -1;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (validate_if_args(osi_core, l_core) < 0) {
		goto fail;
	}

	ret = l_core->if_ops_p->if_core_deinit(osi_core);
fail:
	return ret;
}

nve32_t osi_handle_ioctl(struct osi_core_priv_data *osi_core,
			 struct osi_ioctl *data)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret = -1;

	if (validate_if_args(osi_core, l_core) < 0) {
		goto fail;
	}

	if (data == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "CORE: Invalid argument\n", 0ULL);
		goto fail;
	}

	ret = l_core->if_ops_p->if_handle_ioctl(osi_core, data);
fail:
	return ret;
}
