/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include "../osi/common/common.h"
#include "core_common.h"
#include "mgbe_core.h"
#include "eqos_core.h"
#include "xpcs.h"

static inline nve32_t poll_check(struct osi_core_priv_data *const osi_core, nveu8_t *addr,
				 nveu32_t bit_check, nveu32_t *value)
{
	nveu32_t retry = RETRY_COUNT;
	nve32_t cond = COND_NOT_MET;
	nveu32_t count;
	nve32_t ret = 0;

	/* Poll Until Poll Condition */
	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
				     "poll_check: timeout\n", 0ULL);
			ret = -1;
			goto fail;
		}

		count++;

		*value = osi_readla(osi_core, addr);
		if ((*value & bit_check) == OSI_NONE) {
			cond = COND_MET;
		} else {
			osi_core->osd_ops.udelay(OSI_DELAY_1000US);
		}
	}
fail:
	return ret;
}


nve32_t hw_poll_for_swr(struct osi_core_priv_data *const osi_core)
{
	nveu32_t dma_mode_val = 0U;
	const nveu32_t dma_mode[2] = { EQOS_DMA_BMR, MGBE_DMA_MODE };
	void *addr = osi_core->base;

	return poll_check(osi_core, ((nveu8_t *)addr + dma_mode[osi_core->mac]),
			  DMA_MODE_SWR, &dma_mode_val);
}

void hw_start_mac(struct osi_core_priv_data *const osi_core)
{
	void *addr = osi_core->base;
	nveu32_t value;
	const nveu32_t mac_mcr_te_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };
	const nveu32_t mac_mcr_re_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_RMCR };
	const nveu32_t set_bit_te[2] = { EQOS_MCR_TE, MGBE_MAC_TMCR_TE };
	const nveu32_t set_bit_re[2] = { EQOS_MCR_RE, MGBE_MAC_RMCR_RE };

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
	value |= set_bit_te[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
	value |= set_bit_re[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

void hw_stop_mac(struct osi_core_priv_data *const osi_core)
{
	void *addr = osi_core->base;
	nveu32_t value;
	const nveu32_t mac_mcr_te_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };
	const nveu32_t mac_mcr_re_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_RMCR };
	const nveu32_t clear_bit_te[2] = { EQOS_MCR_TE, MGBE_MAC_TMCR_TE };
	const nveu32_t clear_bit_re[2] = { EQOS_MCR_RE, MGBE_MAC_RMCR_RE };

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
	value &= ~clear_bit_te[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
	value &= ~clear_bit_re[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

nve32_t hw_set_mode(struct osi_core_priv_data *const osi_core, const nve32_t mode)
{
	void *base = osi_core->base;
	nveu32_t mcr_val;
	nve32_t ret = 0;
	const nveu32_t set_bit[2] = { EQOS_MCR_DO, EQOS_MCR_DM };
	const nveu32_t clear_bit[2] = { EQOS_MCR_DM, EQOS_MCR_DO };

	/* don't allow only if loopback mode is other than 0 or 1 */
	if ((mode != OSI_FULL_DUPLEX) && (mode != OSI_HALF_DUPLEX)) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
				"Invalid duplex mode\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		mcr_val = osi_readla(osi_core, (nveu8_t *)base + EQOS_MAC_MCR);
		mcr_val |= set_bit[mode];
		mcr_val &= ~clear_bit[mode];
		osi_writela(osi_core, mcr_val, ((nveu8_t *)base + EQOS_MAC_MCR));
	}
fail:
	return ret;
}

nve32_t hw_set_speed(struct osi_core_priv_data *const osi_core, const nve32_t speed)
{
	nveu32_t  value;
	nve32_t  ret = 0;
	void *base = osi_core->base;
	const nveu32_t mac_mcr[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };

	if ((osi_core->mac == OSI_MAC_HW_EQOS && speed > OSI_SPEED_1000) ||
	    (osi_core->mac == OSI_MAC_HW_MGBE && (speed < OSI_SPEED_2500 ||
	    speed > OSI_SPEED_10000))) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "unsupported speed\n", (nveul64_t)speed);
		ret = -1;
		goto fail;
	}

	value = osi_readla(osi_core, ((nveu8_t *)base + mac_mcr[osi_core->mac]));
	switch (speed) {
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "unsupported speed\n",  (nveul64_t)speed);
		ret = -1;
		goto fail;
	case OSI_SPEED_10:
		value |= EQOS_MCR_PS;
		value &= ~EQOS_MCR_FES;
		break;
	case OSI_SPEED_100:
		value |= EQOS_MCR_PS;
		value |= EQOS_MCR_FES;
		break;
	case OSI_SPEED_1000:
		value &= ~EQOS_MCR_PS;
		value &= ~EQOS_MCR_FES;
		break;
	case OSI_SPEED_2500:
		value |= MGBE_MAC_TMCR_SS_2_5G;
		break;
	case OSI_SPEED_5000:
		value |= MGBE_MAC_TMCR_SS_5G;
		break;
	case OSI_SPEED_10000:
		value &= ~MGBE_MAC_TMCR_SS_10G;
		break;

	}
	osi_writela(osi_core, value, ((nveu8_t *)osi_core->base + mac_mcr[osi_core->mac]));

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		if (xpcs_init(osi_core) < 0) {
			OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_HW_FAIL,
					"xpcs_init failed\n", OSI_NONE);
			ret = -1;
			goto fail;
		}

		ret = xpcs_start(osi_core);
		if (ret < 0) {
			goto fail;
		}
	}
fail:
	return ret;
}



/**
 * @brief hw_est_read - indirect read the GCL to Software own list
 * (SWOL)
 *
 * @param[in] base: MAC base IOVA address.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *	      1 for GCL memory.
 * @param[in] bunk: Memory bunk from which vlaues will be read. Possible
 *            value 0 or 1.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t hw_est_read(struct osi_core_priv_data *osi_core,
				  nveu32_t addr_val, nveu32_t *data,
				  nveu32_t gcla, nveu32_t bunk,
				  nveu32_t mac)
{
	nve32_t retry = 1000;
	nveu32_t val = 0U;
	nve32_t ret;
	const nveu32_t MTL_EST_GCL_CONTROL[MAX_MAC_IP_TYPES] = {
			EQOS_MTL_EST_GCL_CONTROL, MGBE_MTL_EST_GCL_CONTROL};
	const nveu32_t MTL_EST_DATA[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_DATA,
						   MGBE_MTL_EST_DATA};

	*data = 0U;
	val &= ~MTL_EST_ADDR_MASK;
	val |= (gcla == 1U) ? 0x0U : MTL_EST_GCRR;
	val |= MTL_EST_SRWO | MTL_EST_R1W0 | MTL_EST_DBGM | bunk | addr_val;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
		    MTL_EST_GCL_CONTROL[mac]);

	while (--retry > 0) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_EST_GCL_CONTROL[mac]);
		if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
			continue;
		}
		osi_core->osd_ops.udelay(OSI_DELAY_1US);

		break;
	}

	if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
	    (retry <= 0)) {
		ret = -1;
		goto err;
	}

	*data = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			   MTL_EST_DATA[mac]);
	ret = 0;
err:
	return ret;
}

/**
 * @brief eqos_gcl_validate - validate GCL from user
 *
 * Algorithm: validate GCL size and width of time interval value
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration input argument.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t gcl_validate(struct osi_core_priv_data *const osi_core,
		     struct osi_est_config *const est,
		     const nveu32_t *btr, nveu32_t mac)
{
	const struct core_local *l_core = (struct core_local *)osi_core;
	const nveu32_t PTP_CYCLE_8[MAX_MAC_IP_TYPES] = {EQOS_8PTP_CYCLE,
						  MGBE_8PTP_CYCLE};
	const nveu32_t MTL_EST_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL,
						MGBE_MTL_EST_CONTROL};
	const nveu32_t MTL_EST_STATUS[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_STATUS,
						MGBE_MTL_EST_STATUS};
	const nveu32_t MTL_EST_BTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_LOW,
						MGBE_MTL_EST_BTR_LOW};
	const nveu32_t MTL_EST_BTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_HIGH,
						MGBE_MTL_EST_BTR_HIGH};
	const nveu32_t MTL_EST_CTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_LOW,
						MGBE_MTL_EST_CTR_LOW};
	const nveu32_t MTL_EST_CTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_HIGH,
						MGBE_MTL_EST_CTR_HIGH};
	nveu32_t i;
	nveu64_t sum_ti = 0U;
	nveu64_t sum_tin = 0U;
	nveu64_t ctr = 0U;
	nveu64_t btr_new = 0U;
	nveu32_t btr_l, btr_h, ctr_l, ctr_h;
	nveu32_t bunk = 0U;
	nveu32_t est_status;
	nveu64_t old_btr, old_ctr;
	nve32_t ret;
	nveu32_t val = 0U;
	nveu64_t rem = 0U;
	const struct est_read hw_read_arr[4] = {
				    {&btr_l, MTL_EST_BTR_LOW[mac]},
				    {&btr_h, MTL_EST_BTR_HIGH[mac]},
				    {&ctr_l, MTL_EST_CTR_LOW[mac]},
				    {&ctr_h, MTL_EST_CTR_HIGH[mac]}};

	if (est->llr > l_core->gcl_dep) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument more than GCL depth\n",
			     (nveul64_t)est->llr);
		return -1;
	}

	ctr = ((nveu64_t)est->ctr[1] * OSI_NSEC_PER_SEC)  + est->ctr[0];
	btr_new = (((nveu64_t)btr[1] + est->btr_offset[1]) * OSI_NSEC_PER_SEC) +
		   (btr[0] + est->btr_offset[0]);
	for (i = 0U; i < est->llr; i++) {
		if (est->gcl[i] <= l_core->gcl_width_val) {
			sum_ti += ((nveu64_t)est->gcl[i] & l_core->ti_mask);
			if ((sum_ti > ctr) &&
			    ((ctr - sum_tin) >= PTP_CYCLE_8[mac])) {
				continue;
			} else if (((ctr - sum_ti) != 0U) &&
			    ((ctr - sum_ti) < PTP_CYCLE_8[mac])) {
				OSI_CORE_ERR(osi_core->osd,
					     OSI_LOG_ARG_INVALID,
					     "CTR issue due to trancate\n",
					     (nveul64_t)i);
				return -1;
			} else {
				//do nothing
			}
			sum_tin = sum_ti;
			continue;
		}

		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "validation of GCL entry failed\n",
			     (nveul64_t)i);
		return -1;
	}

	/* Check for BTR in case of new ETS while current GCL enabled */

	val = osi_readla(osi_core,
			 (nveu8_t *)osi_core->base +
			 MTL_EST_CONTROL[mac]);
	if ((val & MTL_EST_CONTROL_EEST) != MTL_EST_CONTROL_EEST) {
		return 0;
	}

	/* Read EST_STATUS for bunk */
	est_status = osi_readla(osi_core,
				(nveu8_t *)osi_core->base +
				MTL_EST_STATUS[mac]);
	if ((est_status & MTL_EST_STATUS_SWOL) == 0U) {
		bunk = MTL_EST_DBGB;
	}

	/* Read last BTR and CTR */
	for (i = 0U; i < (sizeof(hw_read_arr) / sizeof(hw_read_arr[0])); i++) {
		ret = hw_est_read(osi_core, hw_read_arr[i].addr,
				  hw_read_arr[i].var, OSI_DISABLE,
				  bunk, mac);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "Reading failed for index\n",
				     (nveul64_t)i);
			return ret;
		}
	}

	old_btr = btr_l + ((nveu64_t)btr_h * OSI_NSEC_PER_SEC);
	old_ctr = ctr_l + ((nveu64_t)ctr_h * OSI_NSEC_PER_SEC);
	if (old_btr > btr_new) {
		rem = (old_btr - btr_new) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			return -1;
		}
	} else if (btr_new > old_btr) {
		rem = (btr_new - old_btr) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			return -1;
		}
	} else {
		// Nothing to do
	}

	return 0;
}
