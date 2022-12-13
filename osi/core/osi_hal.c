/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
#include "vlan_filter.h"
#include "core_common.h"
#include "eqos_core.h"
#include "mgbe_core.h"
#include "frp.h"
#ifdef OSI_DEBUG
#include "debug.h"
#endif /* OSI_DEBUG */

/**
 * @brief g_ops - Static core operations array.
 */

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_core: OSI local core data structure.
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
static inline nve32_t validate_args(struct osi_core_priv_data *const osi_core,
				    struct core_local *const l_core)
{
	nve32_t ret = 0;

	if ((osi_core == OSI_NULL) || (osi_core->base == OSI_NULL) ||
	    (l_core->init_done == OSI_DISABLE) ||
	    (l_core->magic_num != (nveu64_t)osi_core)) {
		ret = -1;
	}

	return ret;
}

/**
 * @brief Function to validate function pointers.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] ops_p: OSI Core operations structure.
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
static nve32_t validate_func_ptrs(struct osi_core_priv_data *const osi_core,
				  struct core_ops *ops_p)
{
	nveu32_t i = 0;
	nve32_t ret = 0;
	void *temp_ops = (void *)ops_p;
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

	for (i = 0; i < (sizeof(*ops_p) / (nveu64_t)__SIZEOF_POINTER__); i++) {
		if (*l_ops == 0U) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "core: fn ptr validation failed at\n",
				     (nveu64_t)i);
			ret = -1;
			goto fail;
		}

		l_ops++;
	}
fail:
	return ret;
}

/**
 * @brief osi_hal_write_phy_reg - HW API to Write to a PHY register through MAC
 * over MDIO bus.
 *
 * @note
 * Algorithm:
 * - Before proceeding for reading for PHY register check whether any MII
 *   operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 * - Program data into MAC MDIO data register.
 * - Populate required parameters like phy address, phy register etc,,
 *   in MAC MDIO Address register. write and GMII busy bits needs to be set
 *   in this operation.
 * - Write into MAC MDIO address register poll for GMII busy for MDIO
 *   operation to complete.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: TODO
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_hal_write_phy_reg(struct osi_core_priv_data *const osi_core,
				     const nveu32_t phyaddr, const nveu32_t phyreg,
				     const nveu16_t phydata)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	return l_core->ops_p->write_phy_reg(osi_core, phyaddr, phyreg, phydata);
}

/**
 * @brief osi_hal_read_phy_reg - HW API to Read from a PHY register through MAC
 * over MDIO bus.
 *
 * @note
 * Algorithm:
 *  - Before proceeding for reading for PHY register check whether any MII
 *    operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 *  - Populate required parameters like phy address, phy register etc,,
 *    in program it in MAC MDIO Address register. Read and GMII busy bits
 *    needs to be set in this operation.
 *  - Write into MAC MDIO address register poll for GMII busy for MDIO
 *    operation to complete. After this data will be available at MAC MDIO
 *    data register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: TODO
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
static nve32_t osi_hal_read_phy_reg(struct osi_core_priv_data *const osi_core,
				    const nveu32_t phyaddr, const nveu32_t phyreg)

{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	return l_core->ops_p->read_phy_reg(osi_core, phyaddr, phyreg);
}

static nve32_t osi_hal_init_core_ops(struct osi_core_priv_data *const osi_core)
{
	nve32_t ret = -1;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	typedef void (*init_core_ops_arr)(struct core_ops *local_ops);
	static struct core_ops g_ops[MAX_MAC_IP_TYPES];
	init_core_ops_arr i_ops[MAX_MAC_IP_TYPES][MAX_MAC_IP_TYPES] = {
		{ eqos_init_core_ops, OSI_NULL },
		{ mgbe_init_core_ops, OSI_NULL }
	};

	if (osi_core == OSI_NULL) {
		goto exit;
	}

	if ((l_core->magic_num != (nveu64_t)osi_core) ||
	    (l_core->init_done == OSI_ENABLE)) {
		goto exit;
	}

	if ((osi_core->osd_ops.ops_log == OSI_NULL) ||
	    (osi_core->osd_ops.udelay == OSI_NULL) ||
	    (osi_core->osd_ops.msleep == OSI_NULL) ||
#ifdef OSI_DEBUG
	    (osi_core->osd_ops.printf == OSI_NULL) ||
#endif /* OSI_DEBUG */
	    (osi_core->osd_ops.usleep_range == OSI_NULL)) {
		goto exit;
	}

	if (osi_core->mac > OSI_MAC_HW_MGBE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid MAC HW type\n", 0ULL);
		goto exit;
	}

	if (osi_core->use_virtualization > OSI_ENABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid use_virtualization value\n", 0ULL);
		goto exit;
	}

	if (i_ops[osi_core->mac][osi_core->use_virtualization] != OSI_NULL) {
		i_ops[osi_core->mac][osi_core->use_virtualization](&g_ops[osi_core->mac]);
	}

	if (validate_func_ptrs(osi_core, &g_ops[osi_core->mac]) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "core: function ptrs validation failed\n", 0ULL);
		goto exit;
	}

	l_core->ops_p = &g_ops[osi_core->mac];
	l_core->init_done = OSI_ENABLE;

	ret = 0;
exit:
	return ret;
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief init_vlan_filters - Helper function to init all VLAN SW information.
 *
 * Algorithm: Initialize VLAN filtering information.
 *
 * @param[in] osi_core: OSI Core private data structure.
 */
static inline void init_vlan_filters(struct osi_core_priv_data *const osi_core)
{
	nveu32_t i = 0U;

	for (i = 0; i < VLAN_NUM_VID; i++) {
		osi_core->vid[i] = VLAN_ID_INVALID;
	}

	osi_core->vf_bitmap = 0U;
	osi_core->vlan_filter_cnt = 0U;
}
#endif

/**
 * @brief osi_hal_hw_core_deinit - HW API for MAC deinitialization.
 *
 * @note
 * Algorithm:
 *  - Stops MAC transmission and reception.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: TODO
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: No
 *  - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_hal_hw_core_deinit(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	/* Stop the MAC */
	hw_stop_mac(osi_core);

	/* Disable MAC interrupts */
	osi_writela(osi_core, 0U, ((nveu8_t *)osi_core->base + HW_MAC_IER));

	if (l_core->l_mac_ver != MAC_CORE_VER_TYPE_EQOS) {
		osi_writela(osi_core, 0U,
			    ((nveu8_t *)osi_core->base + WRAP_COMMON_INTR_ENABLE));
	}

	/* Handle the common interrupt if any status bits set */
	l_core->ops_p->handle_common_intr(osi_core);

	l_core->hw_init_successful = OSI_DISABLE;

	if (l_core->state != OSI_SUSPENDED) {
		/* Reset restore operation flags on interface down */
		l_core->cfg.flags = OSI_DISABLE;
	}

	l_core->state = OSI_DISABLE;

	return 0;
}

/**
 * @brief div_u64 - Calls a function which returns quotient
 *
 * @param[in] dividend: Dividend
 * @param[in] divisor: Divisor
 *
 * @pre MAC IP should be out of reset and need to be initialized as the
 *      requirements.
 *
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 * @returns Quotient
 */
static inline nveu64_t div_u64(nveu64_t dividend,
			       nveu64_t divisor)
{
	nveu64_t remain;

	return div_u64_rem(dividend, divisor, &remain);
}

/**
 * @brief osi_ptp_configuration - Configure PTP
 *
 * @note
 * Algorithm:
 *  - Configure the PTP registers that are required for PTP.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] enable: Enable or disable Time Stamping. 0: Disable 1: Enable
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - osi->ptp_config.ptp_filter need to be filled accordingly to the
 *    filter that need to be set for PTP packets. Please check osi_ptp_config
 *    structure declaration on the bit fields that need to be filled.
 *  - osi->ptp_config.ptp_clock need to be filled with the ptp system clk.
 *    Currently it is set to 62500000Hz.
 *  - osi->ptp_config.ptp_ref_clk_rate need to be filled with the ptp
 *    reference clock that platform supports.
 *  - osi->ptp_config.sec need to be filled with current time of seconds
 *  - osi->ptp_config.nsec need to be filled with current time of nseconds
 *  - osi->base need to be filled with the ioremapped base address
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_021
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_ptp_configuration(struct osi_core_priv_data *const osi_core,
				     OSI_UNUSED const nveu32_t enable)
{
#ifndef OSI_STRIPPED_LIB
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
#endif /* !OSI_STRIPPED_LIB */
	nve32_t ret = 0;
	nveu64_t temp = 0, temp1 = 0, temp2 = 0;
	nveu64_t ssinc = 0;

#ifndef OSI_STRIPPED_LIB
	if (enable == OSI_DISABLE) {
		/* disable hw time stamping */
		/* Program MAC_Timestamp_Control Register */
		hw_config_tscr(osi_core, OSI_DISABLE);
		/* Disable PTP RX Queue routing */
		ret = l_core->ops_p->config_ptp_rxq(osi_core,
					    osi_core->ptp_config.ptp_rx_queue,
					    OSI_DISABLE);
	} else {
#endif /* !OSI_STRIPPED_LIB */
		/* Program MAC_Timestamp_Control Register */
		hw_config_tscr(osi_core, osi_core->ptp_config.ptp_filter);

		/* Program Sub Second Increment Register */
		hw_config_ssir(osi_core);

		/* formula for calculating addend value is
		 * TSAR = (2^32 * 1000) / (ptp_ref_clk_rate in MHz * SSINC)
		 * 2^x * y == (y << x), hence
		 * 2^32 * 1000 == (1000 << 32)
		 * so addend = (2^32 * 1000)/(ptp_ref_clk_rate in MHZ * SSINC);
		 */
		ssinc = OSI_PTP_SSINC_4;
		if (osi_core->mac_ver == OSI_EQOS_MAC_5_30) {
			ssinc = OSI_PTP_SSINC_6;
		}

		temp = ((nveu64_t)1000 << 32);
		temp = (nveu64_t)temp * 1000000U;

		temp1 = div_u64(temp,
			(nveu64_t)osi_core->ptp_config.ptp_ref_clk_rate);

		temp2 = div_u64(temp1, (nveu64_t)ssinc);

		if (temp2 < UINT_MAX) {
			osi_core->default_addend = (nveu32_t)temp2;
		} else {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "core: temp2 >= UINT_MAX\n", 0ULL);
			ret = -1;
			goto fail;
		}

		/* Program addend value */
		ret = hw_config_addend(osi_core, osi_core->default_addend);

		/* Set current time */
		if (ret == 0) {
			ret = hw_set_systime_to_mac(osi_core,
						    osi_core->ptp_config.sec,
						    osi_core->ptp_config.nsec);
#ifndef OSI_STRIPPED_LIB
			if (ret == 0) {
				/* Enable PTP RX Queue routing */
				ret = l_core->ops_p->config_ptp_rxq(osi_core,
					osi_core->ptp_config.ptp_rx_queue,
					OSI_ENABLE);
			}
#endif /* !OSI_STRIPPED_LIB */
		}
#ifndef OSI_STRIPPED_LIB
	}
#endif /* !OSI_STRIPPED_LIB */
fail:
	return ret;
}

static nve32_t osi_get_mac_version(struct osi_core_priv_data *const osi_core, nveu32_t *mac_ver)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret = 0;

	*mac_ver = osi_readla(osi_core, ((nveu8_t *)osi_core->base + (nve32_t)MAC_VERSION)) &
			      MAC_VERSION_SNVER_MASK;

	if (validate_mac_ver_update_chans(*mac_ver, &l_core->num_max_chans,
					  &l_core->l_mac_ver) == 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid MAC version\n", (nveu64_t)*mac_ver)
		ret = -1;
	}

	return ret;
}

static nve32_t osi_hal_hw_core_init(struct osi_core_priv_data *const osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const nveu32_t ptp_ref_clk_rate[3] = {EQOS_X_PTP_CLK_SPEED, EQOS_PTP_CLK_SPEED,
					      MGBE_PTP_CLK_SPEED};
	nve32_t ret;

	ret = osi_get_mac_version(osi_core, &osi_core->mac_ver);
	if (ret < 0) {
		goto fail;
	}

	/* Bring MAC out of reset */
	ret = hw_poll_for_swr(osi_core);
	if (ret < 0) {
		goto fail;
	}

#ifndef OSI_STRIPPED_LIB
	init_vlan_filters(osi_core);

	/* Init FRP */
	init_frp(osi_core);
#endif /* !OSI_STRIPPED_LIB */

	ret = l_core->ops_p->core_init(osi_core);
	if (ret < 0) {
		goto fail;
	}

	/* By default set MAC to Full duplex mode.
	 * Since this is a local function it will always return sucess,
	 * so no need to check for return value
	 */
	(void)hw_set_mode(osi_core, OSI_FULL_DUPLEX);

	/* By default enable rxcsum */
	ret = hw_config_rxcsum_offload(osi_core, OSI_ENABLE);
	if (ret == 0) {
		l_core->cfg.rxcsum = OSI_ENABLE;
		l_core->cfg.flags |= DYNAMIC_CFG_RXCSUM;
	}

	/* Set default PTP settings */
	osi_core->ptp_config.ptp_rx_queue = 3U;
	osi_core->ptp_config.ptp_ref_clk_rate = ptp_ref_clk_rate[l_core->l_mac_ver];
	osi_core->ptp_config.ptp_filter = OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT |
					  OSI_MAC_TCR_TSCTRLSSR | OSI_MAC_TCR_TSVER2ENA |
					  OSI_MAC_TCR_TSIPENA | OSI_MAC_TCR_TSIPV6ENA |
					  OSI_MAC_TCR_TSIPV4ENA | OSI_MAC_TCR_SNAPTYPSEL_1;
	osi_core->ptp_config.sec = 0;
	osi_core->ptp_config.nsec = 0;
	osi_core->ptp_config.one_nsec_accuracy = OSI_ENABLE;
	ret = osi_ptp_configuration(osi_core, OSI_ENABLE);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Fail to configure PTP\n", 0ULL);
		goto fail;
	}

	/* Start the MAC */
	hw_start_mac(osi_core);

	l_core->lane_status = OSI_ENABLE;
	l_core->hw_init_successful = OSI_ENABLE;

fail:
	return ret;
}

#ifndef OSI_STRIPPED_LIB
static nve32_t conf_ptp_offload(struct osi_core_priv_data *const osi_core,
				struct osi_pto_config *const pto_config)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret = -1;

	/* Validate input arguments */
	if (pto_config == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "pto_config is NULL\n", 0ULL);
		return ret;
	}

	if ((pto_config->mc_uc != OSI_ENABLE) &&
	    (pto_config->mc_uc != OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid mc_uc flag value\n",
			     (nveul64_t)pto_config->mc_uc);
		return ret;
	}

	if ((pto_config->en_dis != OSI_ENABLE) &&
	    (pto_config->en_dis != OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid enable flag value\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	if ((pto_config->snap_type != OSI_PTP_SNAP_ORDINARY) &&
	    (pto_config->snap_type != OSI_PTP_SNAP_TRANSPORT) &&
	    (pto_config->snap_type != OSI_PTP_SNAP_P2P)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid SNAP type value\n",
			     (nveul64_t)pto_config->snap_type);
		return ret;
	}

	if ((pto_config->master != OSI_ENABLE) &&
	    (pto_config->master != OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid master flag value\n",
			     (nveul64_t)pto_config->master);
		return ret;
	}

	if (pto_config->domain_num >= OSI_PTP_MAX_DOMAIN) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid ptp domain\n",
			     (nveul64_t)pto_config->domain_num);
		return ret;
	}

	if (pto_config->portid >= OSI_PTP_MAX_PORTID) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid ptp port ID\n",
			     (nveul64_t)pto_config->portid);
		return ret;
	}

	ret = l_core->ops_p->config_ptp_offload(osi_core, pto_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Fail to configure PTO\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	/* Configure PTP */
	ret = osi_ptp_configuration(osi_core, pto_config->en_dis);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Fail to configure PTP\n",
			     (nveul64_t)pto_config->en_dis);
		return ret;
	}

	return ret;
}
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief osi_l2_filter - configure L2 mac filter.
 *
 * @note
 * Algorithm:
 *  - This sequence is used to configure MAC in different packet
 *    processing modes like promiscuous, multicast, unicast,
 *    hash unicast/multicast and perfect/inverse matching for L2 DA
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] filter: OSI filter structure.
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_018
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_l2_filter(struct osi_core_priv_data *const osi_core,
			     const struct osi_filter *filter)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret = 0;

	ret = hw_config_mac_pkt_filter_reg(osi_core, filter);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "failed to configure MAC packet filter register\n",
			     0ULL);
		goto fail;
	}

	if (((filter->oper_mode & OSI_OPER_ADDR_UPDATE) != OSI_NONE) ||
	    ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE)) {
		ret = -1;

		if ((filter->dma_routing == OSI_ENABLE) &&
		    (osi_core->dcs_en != OSI_ENABLE)) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "DCS requested. Conflicts with DT config\n",
				     0ULL);
			goto fail;
		}

		ret = l_core->ops_p->update_mac_addr_low_high_reg(osi_core,
								  filter);
	}

fail:
	return ret;
}

/**
 * @brief l3l4_find_match - function to find filter match
 *
 * @note
 * Algorithm:
 * - Search through filter list l_core->cfg.l3_l4[] and find for a
 *   match with l3_l4 input data.
 * - Filter data matches, store the filter index into filter_no.
 * - Store first found filter index into free_filter_no.
 * - Return 0 on match.
 * - Return -1 on failure.
 *
 * @param[in] l_core: OSI local core data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 * @param[out] filter_no: pointer to filter index
 * @param[out] free_filter_no: pointer to free filter index
 * @param[in] max_filter_no: maximum allowed filter number
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t l3l4_find_match(const struct core_local *const l_core,
			       const struct osi_l3_l4_filter *const l3_l4,
			       nveu32_t *filter_no,
			       nveu32_t *free_filter_no,
			       nveu32_t max_filter_no)
{
	nveu32_t i;
	nve32_t ret = -1;
	nveu32_t found_free_index = 0;
	nve32_t filter_size = (nve32_t)sizeof(l3_l4->data);

	/* init free index value to invalid value */
	*free_filter_no = UINT_MAX;

	for (i = 0; i < max_filter_no; i++) {
		if (l_core->cfg.l3_l4[i].filter_enb_dis == OSI_FALSE) {
			/* filter not enabled, save free index */
			if (found_free_index == 0U) {
				*free_filter_no = i;
				found_free_index = 1;
			}
			continue;
		}

		if (osi_memcmp(&(l_core->cfg.l3_l4[i].data), &(l3_l4->data),
			      filter_size) != 0) {
			/* data do not match */
			continue;
		}

		/* found a match */
		ret = 0;
		*filter_no = i;
		break;
	}

	return ret;
}

/**
 * @brief configure_l3l4_filter_valid_params - parameter validation function for l3l4 configuration
 *
 * @note
 * Algorithm:
 * - Validate all the l3_l4 structure parameter.
 * - Verify routing dma channel id value.
 * - Vefify each enable/disable parameters is <= OSI_TRUE.
 * - Return -1 if parameter validation fails.
 * - Return 0 on success.
 *
 * @param[inout] l_core: OSI local core data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t configure_l3l4_filter_valid_params(const struct osi_core_priv_data *const osi_core,
						  const struct osi_l3_l4_filter *const l3_l4)
{
	const nveu32_t max_dma_chan[2] = {
		OSI_EQOS_MAX_NUM_CHANS,
		OSI_MGBE_MAX_NUM_CHANS
	};
	nve32_t ret = -1;

	/* validate dma channel */
	if (l3_l4->dma_chan > max_dma_chan[osi_core->mac]) {
		OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
			("L3L4: Wrong DMA channel: "), (l3_l4->dma_chan));
		goto exit_func;
	}

	/* valate enb parameters */
	if ((l3_l4->filter_enb_dis
#ifndef OSI_STRIPPED_LIB
	     | l3_l4->dma_routing_enable |
	     l3_l4->data.is_udp |
	     l3_l4->data.is_ipv6 |
	     l3_l4->data.src.port_match |
	     l3_l4->data.src.addr_match |
	     l3_l4->data.dst.port_match |
	     l3_l4->data.dst.addr_match |
	     l3_l4->data.src.port_match_inv |
	     l3_l4->data.src.addr_match_inv |
	     l3_l4->data.dst.port_match_inv |
	     l3_l4->data.dst.addr_match_inv
#endif /* !OSI_STRIPPED_LIB */
	     ) > OSI_TRUE) {
		OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
			("L3L4: one of the enb param > OSI_TRUE: "), 0);
		goto exit_func;
	}

#ifndef OSI_STRIPPED_LIB
	/* validate port/addr enb bits */
	if (l3_l4->filter_enb_dis == OSI_TRUE) {
		if ((l3_l4->data.src.port_match | l3_l4->data.src.addr_match |
		     l3_l4->data.dst.port_match | l3_l4->data.dst.addr_match)
			== OSI_FALSE) {
			OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
				("L3L4: None of the enb bits are not set: "), 0);
			goto exit_func;
		}
		if ((l3_l4->data.is_ipv6 & l3_l4->data.src.addr_match &
			l3_l4->data.dst.addr_match) != OSI_FALSE) {
			OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
				("L3L4: Both ip6 addr match bits are set\n"), 0);
			goto exit_func;
		}
	}
#endif /* !OSI_STRIPPED_LIB */

	/* success */
	ret = 0;

exit_func:

	return ret;
}

/**
 * @brief configure_l3l4_filter_helper - helper function for l3l4 configuration
 *
 * @note
 * Algorithm:
 * - Confifure l3l4 filter using l_core->ops_p->config_l3l4_filters().
 *   Return -1 if config_l3l4_filters() fails.
 * - Store the filter into l_core->cfg.l3_l4[] and enable
 *   l3l4 filter if any of the filter index enabled currently.
 *
 * @param[inout] l_core: OSI local core data structure.
 * @param[in] filter_no: pointer to filter number
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t configure_l3l4_filter_helper(struct osi_core_priv_data *const osi_core,
					    nveu32_t filter_no,
					    const struct osi_l3_l4_filter *const l3_l4)
{
	struct osi_l3_l4_filter *cfg_l3_l4;
	struct core_local *const l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret;

	ret = l_core->ops_p->config_l3l4_filters(osi_core, filter_no, l3_l4);
	if (ret < 0) {
		OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
			("Failed to config L3L4 filters: "), (filter_no));
		goto exit_func;
	}

	cfg_l3_l4 = &(l_core->cfg.l3_l4[filter_no]);
	if (l3_l4->filter_enb_dis == OSI_TRUE) {
		/* Store the filter.
		 * osi_memcpy is an internal function and it cannot fail, hence
		 * ignoring return value.
		 */
		(void)osi_memcpy(cfg_l3_l4, l3_l4, sizeof(struct osi_l3_l4_filter));
		OSI_CORE_INFO((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
			("L3L4: ADD: "), (filter_no));

		/* update filter mask bit */
		osi_core->l3l4_filter_bitmask |= ((nveu32_t)1U << (filter_no & 0x1FU));
	} else {
		/* Clear the filter data.
		 * osi_memset is an internal function and it cannot fail, hence
		 * ignoring return value.
		 */
		(void)osi_memset(cfg_l3_l4, 0, sizeof(struct osi_l3_l4_filter));
		OSI_CORE_INFO((osi_core->osd), (OSI_LOG_ARG_OUTOFBOUND),
			("L3L4: DELETE: "), (filter_no));

		/* update filter mask bit */
		osi_core->l3l4_filter_bitmask &= ~((nveu32_t)1U << (filter_no & 0x1FU));
	}

	if (osi_core->l3l4_filter_bitmask != 0U) {
		/* enable l3l4 filter */
		ret = hw_config_l3_l4_filter_enable(osi_core, OSI_ENABLE);
	} else {
		/* disable l3l4 filter */
		ret = hw_config_l3_l4_filter_enable(osi_core, OSI_DISABLE);
	}

exit_func:

	return ret;
}

#ifndef OSI_STRIPPED_LIB
#ifdef L3L4_WILDCARD_FILTER
/**
 * @brief l3l4_add_wildcard_filter - function to configure wildcard filter.
 *
 * @note
 * Algorithm:
 *  - Configure wildcard filter with all 4 tuple as 0s using configure_l3l4_filter_helper().
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] max_filter_no: maximum allowed filter number
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 */
static void l3l4_add_wildcard_filter(struct osi_core_priv_data *const osi_core,
				     nveu32_t max_filter_no)
{
	nve32_t err;
	struct osi_l3_l4_filter *l3l4_filter;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	/* use max filter index to confiture wildcard filter */
	if (l_core->l3l4_wildcard_filter_configured != OSI_ENABLE) {
		/* configure INV filter for IPV4/UDP with DA(0) + SP (0) +
		 * DP (0) with routing not enabled
		 */
		l3l4_filter = &(l_core->cfg.l3_l4[max_filter_no]);
		osi_memset(l3l4_filter, 0, sizeof(struct osi_l3_l4_filter));
		l3l4_filter->filter_enb_dis = OSI_TRUE;
		l3l4_filter->data.is_udp = OSI_TRUE;
		l3l4_filter->data.src.port_match = OSI_TRUE;
		l3l4_filter->data.src.port_match_inv = OSI_TRUE;
		l3l4_filter->data.src.addr_match = OSI_TRUE;
		l3l4_filter->data.src.addr_match_inv = OSI_TRUE;
		l3l4_filter->data.dst.port_match = OSI_TRUE;
		l3l4_filter->data.dst.port_match_inv = OSI_TRUE;
		l3l4_filter->data.dst.addr_match = OSI_TRUE;
		l3l4_filter->data.dst.addr_match_inv = OSI_TRUE;

		/* configure wildcard at last filter index */
		err = configure_l3l4_filter_helper(osi_core, max_filter_no, l3l4_filter);
		if (err < 0) {
			/* wildcard config failed */
			OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_INVALID),
				("L3L4: Wildcard config failed: "), (max_filter_no));
		} else {
			/* wildcard config success */
			l_core->l3l4_wildcard_filter_configured = OSI_ENABLE;
			OSI_CORE_INFO((osi_core->osd), (OSI_LOG_ARG_INVALID),
				("L3L4: Wildcard config success"), (max_filter_no));
		}
	}
}
#endif /* L3L4_WILDCARD_FILTER */
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief configure_l3l4_filter - function to configure l3l4 filter.
 *
 * @note
 * Algorithm:
 *  - Validate all the l3_l4 structure parameter using configure_l3l4_filter_valid_params().
 *    Return -1 if parameter validation fails.
 *  - For filter enable case,
 *    -> If filter already enabled, return -1 to report error.
 *    -> Otherwise find free index and configure filter using configure_l3l4_filter_helper().
 *  - For filter disable case,
 *    -> If filter match not found, return 0 to report caller that filter already removed.
 *    -> Otherwise disable filter using configure_l3l4_filter_helper().
 *  - Return -1 if configure_l3l4_filter_helper() fails.
 *  - Return 0 on success.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 *
 * @pre
 *  - MAC should be initialized and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t configure_l3l4_filter(struct osi_core_priv_data *const osi_core,
				     const struct osi_l3_l4_filter *const l3_l4)
{
	nve32_t err;
	nveu32_t filter_no = 0;
	nveu32_t free_filter_no = UINT_MAX;
	const struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const nveu32_t max_filter_no[2] = {
		/* max usable filter number is less by 1 for accommodating
		 * wildcard filter at last index.
		 */
		EQOS_MAX_L3_L4_FILTER - 1U,
		OSI_MGBE_MAX_L3_L4_FILTER - 1U,
	};
	nve32_t ret = -1;

	if (configure_l3l4_filter_valid_params(osi_core, l3_l4) < 0) {
		/* parameter validation failed */
		goto exit_func;
	}

	/* search for a duplicate filter request or find for free index */
	err = l3l4_find_match(l_core, l3_l4, &filter_no, &free_filter_no,
				  max_filter_no[osi_core->mac]);

	if (l3_l4->filter_enb_dis == OSI_TRUE) {
		if (err == 0) {
			/* duplicate filter request */
			OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
				("L3L4: Failed: duplicate filter: "), (filter_no));
			goto exit_func;
		}

		/* check free index */
		if (free_filter_no >= max_filter_no[osi_core->mac]) {
			/* no free entry found */
			OSI_CORE_INFO((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
				("L3L4: Failed: no free filter: "), (free_filter_no));
			goto exit_func;
		}
		filter_no = free_filter_no;
	} else {
		if (err < 0) {
			/* no match found */
			OSI_CORE_INFO((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
				("L3L4: delete: no filter match: "), (filter_no));
			/* filter already deleted, return success */
			ret = 0;
			goto exit_func;
		}
	}

#ifndef OSI_STRIPPED_LIB
#ifdef L3L4_WILDCARD_FILTER
	/* setup l3l4 wildcard filter for l3l4 */
	l3l4_add_wildcard_filter(osi_core, max_filter_no[osi_core->mac]);
	if (l_core->l3l4_wildcard_filter_configured != OSI_ENABLE) {
		OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
			("L3L4: Rejected: wildcard is not enabled: "), (filter_no));
		goto exit_func;
	}
#endif /* L3L4_WILDCARD_FILTER */
#endif /* !OSI_STRIPPED_LIB */

	/* configure l3l4 filter */
	err = configure_l3l4_filter_helper(osi_core, filter_no, l3_l4);
	if (err < 0) {
		/* filter config failed */
		OSI_CORE_ERR((osi_core->osd), (OSI_LOG_ARG_HW_FAIL),
			("L3L4: configure_l3l4_filter_helper() failed"), (filter_no));
		goto exit_func;
	}

	/* success */
	ret = 0;

exit_func:

	return ret;
}

/**
 * @brief osi_adjust_freq - Adjust frequency
 *
 * @note
 * Algorithm:
 *  - Adjust a drift of +/- comp nanoseconds per second.
 *    "Compensation" is the difference in frequency between
 *    the master and slave clocks in Parts Per Billion.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] ppb: Parts per Billion
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_023
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_adjust_freq(struct osi_core_priv_data *const osi_core, nve32_t ppb)
{
	nveu64_t adj;
	nveu64_t temp;
	nveu32_t diff = 0;
	nveu32_t addend;
	nveu32_t neg_adj = 0;
	nve32_t ret = -1;
	nve32_t ppb1 = ppb;

	addend = osi_core->default_addend;
	if (ppb1 < 0) {
		neg_adj = 1U;
		ppb1 = -ppb1;
		adj = (nveu64_t)addend * (nveu32_t)ppb1;
	} else {
		adj = (nveu64_t)addend * (nveu32_t)ppb1;
	}

	/*
	 * div_u64 will divide the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	temp = div_u64(adj, OSI_NSEC_PER_SEC);
	if (temp < UINT_MAX) {
		diff = (nveu32_t)temp;
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID, "temp > UINT_MAX\n",
			     0ULL);
		goto fail;
	}

	if (neg_adj == 0U) {
		if (addend <= (UINT_MAX - diff)) {
			addend = (addend + diff);
		} else {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "addend > UINT_MAX\n", 0ULL);
			goto fail;
		}
	} else {
		if (addend > diff) {
			addend = addend - diff;
		} else if (addend < diff) {
			addend = diff - addend;
		} else {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "addend = diff\n", 0ULL);
		}
	}

	ret = hw_config_addend(osi_core, addend);

fail:
	return ret;
}

static nve32_t osi_adjust_time(struct osi_core_priv_data *const osi_core,
			       nvel64_t nsec_delta)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nveu32_t neg_adj = 0;
	nveu32_t sec = 0, nsec = 0;
	nveu64_t quotient;
	nveu64_t reminder = 0;
	nveu64_t udelta = 0;
	nve32_t ret = -1;
	nvel64_t nsec_delta1 = nsec_delta;

	if (nsec_delta1 < 0) {
		neg_adj = 1;
		nsec_delta1 = -nsec_delta1;
		udelta = (nveul64_t)nsec_delta1;
	} else {
		udelta = (nveul64_t)nsec_delta1;
	}

	quotient = div_u64_rem(udelta, OSI_NSEC_PER_SEC, &reminder);
	if (quotient <= UINT_MAX) {
		sec = (nveu32_t)quotient;
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "quotient > UINT_MAX\n", 0ULL);
		goto fail;
	}

	if (reminder <= UINT_MAX) {
		nsec = (nveu32_t)reminder;
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "reminder > UINT_MAX\n", 0ULL);
		goto fail;
	}

	ret = l_core->ops_p->adjust_mactime(osi_core, sec, nsec, neg_adj,
					    osi_core->ptp_config.one_nsec_accuracy);
fail:
	return ret;
}



#ifndef OSI_STRIPPED_LIB
/**
 * @brief rxq_route_config - Enable PTP RX packets routing
 *
 * Algorithm: Program PTP RX queue index
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] rx_route: Pointer to the osi_rxq_route structure, which
 * contains RXQ routing parameters.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t rxq_route_config(struct osi_core_priv_data *const osi_core,
				const struct osi_rxq_route *rxq_route)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (rxq_route->route_type != OSI_RXQ_ROUTE_PTP) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid route_type\n",
			     rxq_route->route_type);
		return -1;
	}

	return l_core->ops_p->config_ptp_rxq(osi_core,
				     rxq_route->idx,
				     rxq_route->enable);
}

/**
 * @brief vlan_id_update - invoke osi call to update VLAN ID
 *
 * @note
 * Algorithm:
 *  - return 16 bit VLAN ID
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] vid: VLAN ID
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t vlan_id_update(struct osi_core_priv_data *const osi_core,
			      const nveu32_t vid)
{
	struct core_local *const l_core = (struct core_local *)(void *)osi_core;
	nveu32_t action = vid & VLAN_ACTION_MASK;
	nveu16_t vlan_id = (nveu16_t)(vid & VLAN_VID_MASK);

	if ((osi_core->mac_ver == OSI_EQOS_MAC_4_10) ||
	    (osi_core->mac_ver == OSI_EQOS_MAC_5_00)) {
		/* No VLAN ID filtering */
		return 0;
	}

	if (((action != OSI_VLAN_ACTION_ADD) &&
	    (action != OSI_VLAN_ACTION_DEL)) ||
	    (vlan_id >= VLAN_NUM_VID)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "CORE: Invalid action/vlan_id\n", 0ULL);
		/* Unsupported action */
		return -1;
	}

	return update_vlan_id(osi_core, l_core->ops_p, vid);
}

/**
 * @brief conf_eee - Configure EEE LPI in MAC.
 *
 * @note
 * Algorithm:
 *  - This routine invokes configuration of EEE LPI in the MAC.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] tx_lpi_enabled: Enable (1)/disable (0) tx lpi
 * @param[in] tx_lpi_timer: Tx LPI entry timer in usecs upto
 *            OSI_MAX_TX_LPI_TIMER (in steps of 8usec)
 *
 * @pre
 *  - MAC and PHY should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t conf_eee(struct osi_core_priv_data *const osi_core,
			nveu32_t tx_lpi_enabled, nveu32_t tx_lpi_timer)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if ((tx_lpi_timer >= OSI_MAX_TX_LPI_TIMER) ||
	    (tx_lpi_timer <= OSI_MIN_TX_LPI_TIMER) ||
	    ((tx_lpi_timer % OSI_MIN_TX_LPI_TIMER) != OSI_NONE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid Tx LPI timer value\n",
			     (nveul64_t)tx_lpi_timer);
		return -1;
	}

	l_core->ops_p->configure_eee(osi_core, tx_lpi_enabled, tx_lpi_timer);

	return 0;
}

/**
 * @brief config_arp_offload - Configure ARP offload in MAC.
 *
 * @note
 * Algorithm:
 *  - Invokes EQOS config ARP offload routine.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] flags: Enable/disable flag.
 * @param[in] ip_addr: Char array representation of IP address
 *
 * @pre
 *  - MAC should be init and started. see osi_start_mac()
 *  - Valid 4 byte IP address as argument ip_addr
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t conf_arp_offload(struct osi_core_priv_data *const osi_core,
				const nveu32_t flags,
				const nveu8_t *ip_addr)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if (ip_addr == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "CORE: ip_addr is NULL\n", 0ULL);
		return -1;
	}

	if ((flags != OSI_ENABLE) && (flags != OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid ARP offload enable/disable flag\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->config_arp_offload(osi_core, flags, ip_addr);
}

/**
 * @brief conf_mac_loopback - Configure MAC loopback
 *
 * @note
 * Algorithm:
 *  - Configure the MAC to support the loopback.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lb_mode: Enable or disable MAC loopback
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t conf_mac_loopback(struct osi_core_priv_data *const osi_core,
				 const nveu32_t lb_mode)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	/* don't allow only if loopback mode is other than 0 or 1 */
	if ((lb_mode != OSI_ENABLE) && (lb_mode != OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid loopback mode\n", 0ULL);
		return -1;
	}

	return l_core->ops_p->config_mac_loopback(osi_core, lb_mode);
}
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief configure_frp - Configure the FRP offload entry in the
 * Instruction Table.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] cmd: FRP command data structure.
 *
 * @pre
 *  - MAC and PHY should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t configure_frp(struct osi_core_priv_data *const osi_core,
			     struct osi_core_frp_cmd *const cmd)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nve32_t ret;

	if (cmd == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "FRP command invalid\n", 0ULL);
		ret = -1;
		goto done;
	}

	/* Check for supported MAC version */
	if ((osi_core->mac == OSI_MAC_HW_EQOS) &&
	    (osi_core->mac_ver < OSI_EQOS_MAC_5_30)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "MAC doesn't support FRP\n", OSI_NONE);
		ret = -1;
		goto done;
	}

	ret = setup_frp(osi_core, l_core->ops_p, cmd);
done:
	return ret;
}

/**
 * @brief config_est - Read Setting for GCL from input and update
 * registers.
 *
 * Algorithm:
 * - Write  TER, LLR and EST control register
 * - Update GCL to sw own GCL (MTL_EST_Status bit SWOL will tell which is
 *   owned by SW) and store which GCL is in use currently in sw.
 * - TODO set DBGB and DBGM for debugging
 * - EST_data and GCRR to 1, update entry sno in ADDR and put data at
 *   est_gcl_data enable GCL MTL_EST_SSWL and wait for self clear or use
 *   SWLC in MTL_EST_Status. Please note new GCL will be pushed for each entry.
 * - Configure btr. Update btr based on current time (current time
 *   should be updated based on PTP by this time)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration osi_est_config structure.
 *
 * @pre
 * - MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t config_est(struct osi_core_priv_data *osi_core,
			  struct osi_est_config *est)
{
	nve32_t ret;

	if (est == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "EST data is NULL", 0ULL);
		ret = -1;
		goto done;
	}

	if ((osi_core->flow_ctrl & OSI_FLOW_CTRL_TX) ==
	     OSI_FLOW_CTRL_TX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "TX Flow control enabled, please disable it",
			      0ULL);
		ret = -1;
		goto done;
	}

	ret = hw_config_est(osi_core, est);

done:
	return ret;
}

/**
 * @brief config_fep - Read Setting for preemption and express for TC
 * and update registers.
 *
 * Algorithm:
 * - Check for TC enable and TC has masked for setting to preemptable.
 * - Update FPE control status register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] fpe: Configuration osi_fpe_config structure.
 *
 * @pre
 * - MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @note
 * Classification:
 * - Interrupt: No
 * - Signal handler: No
 * - Thread safe: No
 * - Required Privileges: None
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t config_fpe(struct osi_core_priv_data *osi_core,
			  struct osi_fpe_config *fpe)
{
	nve32_t ret;

	if (fpe == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "FPE data is NULL", 0ULL);
		ret = -1;
		goto done;
	}

	ret = hw_config_fpe(osi_core, fpe);

done:
	return ret;
}

/**
 * @brief Free stale timestamps for channel
 *
 * Algorithm:
 * - Check for if any timestamp for input channel id
 * - if yes, reset node to 0x0 for reuse.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] chan: 1 for DMA channel 0, 2 for dma channel 1,...
 *		    0 is used for onestep.
 */
static inline void free_tx_ts(struct osi_core_priv_data *osi_core,
			   nveu32_t chan)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	struct osi_core_tx_ts *head = &l_core->tx_ts_head;
	struct osi_core_tx_ts *temp = l_core->tx_ts_head.next;
	nveu32_t count = 0U;

	while ((temp != head) && (count < MAX_TX_TS_CNT)) {
		if (((temp->pkt_id >> CHAN_START_POSITION) & chan) == chan) {
			temp->next->prev = temp->prev;
			temp->prev->next = temp->next;
			/* reset in_use for temp node from the link */
			temp->in_use = OSI_DISABLE;
		}
		count++;
		temp = temp->next;
	}
}

/**
 * @brief Return absolute difference
 * Algorithm:
 * - calculate absolute positive difference
 *
 * @param[in] a - First input argument
 * @param[in] b - Second input argument
 *
 * @retval absolute difference
 */
static inline nveul64_t eth_abs(nveul64_t a, nveul64_t b)
{
	nveul64_t temp = 0ULL;

	if (a > b) {
		temp = (a - b);
	} else {
		temp = (b - a);
	}

	return temp;
}

/**
 * @brief Parses internal ts structure array and update time stamp if packet
 *   id matches.
 * Algorithm:
 * - Check for if any timestamp with packet ID in list and valid
 * - update sec and nsec in timestamp structure
 * - reset node to reuse for next call
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in,out] ts: osi core ts structure, pkt_id is input and time is output.
 *
 * @retval 0 on success
 * @retval -1 any other failure.
 */
static inline nve32_t get_tx_ts(struct osi_core_priv_data *osi_core,
				struct osi_core_tx_ts *ts)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	struct osi_core_tx_ts *temp = l_core->tx_ts_head.next;
	struct osi_core_tx_ts const *head = &l_core->tx_ts_head;
	nve32_t ret = -1;
	nveu32_t count = 0U;
	nveu32_t nsec, sec, temp_nsec;
	nveul64_t temp_val = 0ULL;
	nveul64_t ts_val = 0ULL;

	common_get_systime_from_mac(osi_core->base, osi_core->mac, &sec, &nsec);
	ts_val = (sec * OSI_NSEC_PER_SEC) + nsec;

	if (__sync_fetch_and_add(&l_core->ts_lock, 1) == 1U) {
		/* mask return as initial value is returned always */
		(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
#ifndef OSI_STRIPPED_LIB
		osi_core->stats.ts_lock_del_fail =
				osi_update_stats_counter(
				osi_core->stats.ts_lock_del_fail, 1U);
#endif
		goto done;
	}

	while ((temp != head) && (count < MAX_TX_TS_CNT)) {
		temp_nsec = temp->nsec & ETHER_NSEC_MASK;
		temp_val = (temp->sec * OSI_NSEC_PER_SEC) + temp_nsec;

		if ((eth_abs(ts_val, temp_val) > OSI_NSEC_PER_SEC) &&
		    (temp->in_use != OSI_NONE)) {
			/* remove old node from the link */
			temp->next->prev = temp->prev;
			temp->prev->next =  temp->next;
			/* Clear in_use fields */
			temp->in_use = OSI_DISABLE;
			OSI_CORE_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
				      "Removing stale TS from queue pkt_id\n",
				      (nveul64_t)temp->pkt_id);
			count++;
			temp = temp->next;
			continue;
		} else if ((temp->pkt_id == ts->pkt_id) &&
			   (temp->in_use != OSI_NONE)) {
			ts->sec = temp->sec;
			ts->nsec = temp->nsec;
			/* remove temp node from the link */
			temp->next->prev = temp->prev;
			temp->prev->next =  temp->next;
			/* Clear in_use fields */
			temp->in_use = OSI_DISABLE;
			ret = 0;
			break;
		} else {
			/* empty case */
		}

		count++;
		temp = temp->next;
	}

	/* mask return as initial value is returned always */
	(void)__sync_fetch_and_sub(&l_core->ts_lock, 1);
done:
	return ret;
}

/**
 * @brief calculate time drift between primary and secondary
 *  interface.
 * Algorithm:
 * - Get drift using last difference = 0 and
 *   current differance as  MGBE time - EQOS time
 *   drift  =  current differance with which EQOS should
 *   update.
 *
 * @param[in] sec: primary interface sec counter
 * @param[in] nsec: primary interface nsec counter
 * @param[in] secondary_sec: Secondary interface sec counter
 * @param[in] secondary_nsec: Secondary interface nsec counter
 *
 * @retval calculated drift value
 */
static inline nvel64_t dirft_calculation(nveu32_t sec, nveu32_t nsec,
					 nveu32_t secondary_sec,
					 nveu32_t secondary_nsec)
{
	nvel64_t val = 0LL;

	val = (nvel64_t)sec - (nvel64_t)secondary_sec;
	val = (nvel64_t)(val * 1000000000LL);
	val += (nvel64_t)nsec - (nvel64_t)secondary_nsec;

	return val;
}

/**
 * @brief calculate frequency adjustment between primary and secondary
 *  controller.
 * Algorithm:
 * - Convert Offset between primary and secondary interface to
 *   frequency adjustment value.
 *
 * @param[in] sec_osi_core: secondary interface osi core pointer
 * @param[in] offset: offset btween primary and secondary interface
 * @param[in] secondary_time: Secondary interface time in ns
 *
 * @retval calculated frequency adjustment value in ppb
 */
static inline nve32_t freq_offset_calculate(struct osi_core_priv_data *sec_osi_core,
					    nvel64_t offset,
					    nvel64_t secondary_time)
{
	struct core_ptp_servo *s;
	struct core_local *secondary_osi_lcore = (struct core_local *)(void *)sec_osi_core;
	nvel64_t ki_term, ppb = 0;
	nvel64_t cofficient;

	s = &secondary_osi_lcore->serv;
	ppb = s->last_ppb;

	/* if drift is too big in positive / negative  don't take any action,
	 * it should be corrected with adjust time
	 * threshold value 1 sec
	 */
	if ((offset >= 1000000000LL) || (offset <= -1000000000LL)) {
		s->count = SERVO_STATS_0; /* JUMP */
		goto fail;
	}

	switch (s->count) {
	case SERVO_STATS_0:
		s->offset[0] = offset;
		s->local[0] = secondary_time;
		s->count = SERVO_STATS_1;
		break;

	case SERVO_STATS_1:
		s->offset[1] = offset;
		s->local[1] = secondary_time;

		/* Make sure the first sample is older than the second. */
		if (s->local[0] >= s->local[1]) {
			s->offset[0] = s->offset[1];
			s->local[0] = s->local[1];
			s->count = SERVO_STATS_0;
			break;
		}

		/* Adjust drift by the measured frequency offset. */
		cofficient = (1000000000LL - s->drift) / (s->local[1] - s->local[0]);
		if ((cofficient == 0) ||
		 (((cofficient < 0) && (s->offset[1] < 0)) &&
		  ((OSI_LLONG_MAX / cofficient) < s->offset[1])) ||
		    ((cofficient < 0) && ((-OSI_LLONG_MAX / cofficient) > s->offset[1])) ||
		    ((s->offset[1] < 0) && ((-OSI_LLONG_MAX / cofficient) > s->offset[1]))) {
			/* do nothing */
		} else {

			if (((s->drift >= 0) && ((OSI_LLONG_MAX - s->drift) < (cofficient * s->offset[1]))) ||
			    ((s->drift < 0) && ((-OSI_LLONG_MAX - s->drift) > (cofficient * s->offset[1])))) {
				/* Do nothing */
			} else {
				s->drift += cofficient * s->offset[1];
			}
		}
		/* update this with constant */
		if (s->drift < -MAX_FREQ) {
			s->drift = -MAX_FREQ;
		} else if (s->drift > MAX_FREQ) {
			s->drift = MAX_FREQ;
		} else {
			/* Do Nothing */
		}

		ppb = s->drift;
		s->count = SERVO_STATS_2;
		s->offset[0] = s->offset[1];
		s->local[0] = s->local[1];
		break;

	case SERVO_STATS_2:
		s->offset[1] = offset;
		s->local[1] = secondary_time;
		if (s->local[0] >= s->local[1]) {
			s->offset[0] = s->offset[1];
			s->local[0] = s->local[1];
			s->count = SERVO_STATS_0;
			break;
		}

		cofficient = (1000000000LL) / (s->local[1] - s->local[0]);

		if ((cofficient != 0) && (offset < 0) &&
		    ((s->const_i > (-OSI_LLONG_MAX / (WEIGHT_BY_10 * cofficient * offset))) ||
		     (s->const_p > (-OSI_LLONG_MAX / (WEIGHT_BY_10 * cofficient * offset))))) {
			s->count = SERVO_STATS_0;
			break;
		}
		if ((cofficient != 0) && (offset > 0) &&
		    ((s->const_i > (OSI_LLONG_MAX / (WEIGHT_BY_10 * cofficient * offset))) ||
		     (s->const_p > (OSI_LLONG_MAX / (WEIGHT_BY_10 * cofficient * offset))))) {
			s->count = SERVO_STATS_0;
			break;
		}

		/* calculate ppb */
		ki_term = ((s->const_i * cofficient * offset) / WEIGHT_BY_10);
		ppb = (s->const_p * cofficient * offset / WEIGHT_BY_10) + s->drift +
		      ki_term;

		/* FIXME tune cofficients */
		if (ppb < -MAX_FREQ) {
			ppb = -MAX_FREQ;
		} else if (ppb > MAX_FREQ) {
			ppb = MAX_FREQ;
		} else {
			if (((s->drift >= 0) && ((OSI_LLONG_MAX - s->drift) < ki_term)) ||
			    ((s->drift < 0) && ((-OSI_LLONG_MAX - s->drift) > ki_term))) {
			} else {

				s->drift += ki_term;
			}
			s->offset[0] = s->offset[1];
			s->local[0] = s->local[1];
		}
		break;
	default:
		break;
	}

	s->last_ppb = ppb;

fail:
	if ((ppb > INT_MAX) || (ppb < -INT_MAX)) {
		ppb = 0LL;
	}

	return (nve32_t)ppb;
}

static void cfg_l3_l4_filter(struct core_local *l_core)
{
	nveu32_t i = 0U;

	for (i = 0U; i < OSI_MGBE_MAX_L3_L4_FILTER; i++) {
		if (l_core->cfg.l3_l4[i].filter_enb_dis == OSI_FALSE) {
			/* filter not enabled */
			continue;
		}

		(void)configure_l3l4_filter_helper(
			(struct osi_core_priv_data *)(void *)l_core,
			i, &l_core->cfg.l3_l4[i]);

#ifdef L3L4_WILDCARD_FILTER
#ifndef OSI_STRIPPED_LIB
		if (i == (OSI_MGBE_MAX_L3_L4_FILTER - 1U)) {
			/* last filter supposed to be wildcard filter */
			l_core->l3l4_wildcard_filter_configured = OSI_ENABLE;
		}
#endif /* L3L4_WILDCARD_FILTER */
#endif /* !OSI_STRIPPED_LIB */
	}
}

static void cfg_l2_filter(struct core_local *l_core)
{
	nveu32_t i;

	(void)osi_l2_filter((struct osi_core_priv_data *)(void *)l_core,
			    &l_core->cfg.l2_filter);

	for (i = 0U; i < EQOS_MAX_MAC_ADDRESS_FILTER; i++) {
		if (l_core->cfg.l2[i].used == OSI_DISABLE) {
			continue;
		}

		(void)osi_l2_filter((struct osi_core_priv_data *)(void *)l_core,
				    &l_core->cfg.l2[i].filter);
	}
}

static void cfg_rxcsum(struct core_local *l_core)
{
	(void)hw_config_rxcsum_offload((struct osi_core_priv_data *)(void *)l_core,
						    l_core->cfg.rxcsum);
}

#ifndef OSI_STRIPPED_LIB
static void cfg_vlan(struct core_local *l_core)
{
	nveu32_t i;

	for (i = 0U; i < VLAN_NUM_VID; i++) {
		if (l_core->cfg.vlan[i].used == OSI_DISABLE) {
			continue;
		}

		(void)vlan_id_update((struct osi_core_priv_data *)(void *)l_core,
				     (l_core->cfg.vlan[i].vid | OSI_VLAN_ACTION_ADD));
	}
}

static void cfg_fc(struct core_local *l_core)
{
	(void)l_core->ops_p->config_flow_control((struct osi_core_priv_data *)(void *)l_core,
						  l_core->cfg.flow_ctrl);
}

static void cfg_eee(struct core_local *l_core)
{
	(void)conf_eee((struct osi_core_priv_data *)(void *)l_core,
		       l_core->cfg.tx_lpi_enabled,
		       l_core->cfg.tx_lpi_timer);
}
#endif /* !OSI_STRIPPED_LIB */

static void cfg_avb(struct core_local *l_core)
{
	nveu32_t i;

	for (i = 0U; i < OSI_MGBE_MAX_NUM_QUEUES; i++) {
		if (l_core->cfg.avb[i].used == OSI_DISABLE) {
			continue;
		}

		(void)l_core->ops_p->set_avb_algorithm((struct osi_core_priv_data *)(void *)l_core,
						       &l_core->cfg.avb[i].avb_info);
	}
}

static void cfg_est(struct core_local *l_core)
{
	(void)config_est((struct osi_core_priv_data *)(void *)l_core,
			 &l_core->cfg.est);
}

static void cfg_fpe(struct core_local *l_core)
{
	(void)config_fpe((struct osi_core_priv_data *)(void *)l_core,
			 &l_core->cfg.fpe);
}

static void cfg_ptp(struct core_local *l_core)
{
	struct osi_core_priv_data *osi_core = (struct osi_core_priv_data *)(void *)l_core;
	struct osi_ioctl ioctl_data = {};

	ioctl_data.arg1_u32 = l_core->cfg.ptp;
	ioctl_data.cmd = OSI_CMD_CONFIG_PTP;

	(void)osi_handle_ioctl(osi_core, &ioctl_data);
}

static void apply_dynamic_cfg(struct osi_core_priv_data *osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	typedef void (*cfg_fn)(struct core_local *local_core);
	const cfg_fn fn[10] = {
		[DYNAMIC_CFG_L3_L4_IDX] = cfg_l3_l4_filter,
		[DYNAMIC_CFG_L2_IDX] = cfg_l2_filter,
		[DYNAMIC_CFG_RXCSUM_IDX] = cfg_rxcsum,
#ifndef OSI_STRIPPED_LIB
		[DYNAMIC_CFG_VLAN_IDX] = cfg_vlan,
		[DYNAMIC_CFG_FC_IDX] = cfg_fc,
		[DYNAMIC_CFG_EEE_IDX] = cfg_eee,
#endif /* !OSI_STRIPPED_LIB */
		[DYNAMIC_CFG_AVB_IDX] = cfg_avb,
		[DYNAMIC_CFG_EST_IDX] = cfg_est,
		[DYNAMIC_CFG_FPE_IDX] = cfg_fpe,
		[DYNAMIC_CFG_PTP_IDX] = cfg_ptp
	};
	nveu32_t flags = l_core->cfg.flags;
	nveu32_t i = 0U;

	while (flags > 0U) {
		if ((flags & OSI_ENABLE) == OSI_ENABLE) {
			fn[i](l_core);
		}

		flags = flags >> 1U;
		update_counter_u(&i, 1U);
	}
}

static void store_l2_filter(struct osi_core_priv_data *osi_core,
			    struct osi_filter *filter)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;

	if ((filter->oper_mode & OSI_OPER_ADDR_UPDATE) == OSI_OPER_ADDR_UPDATE) {
		(void)osi_memcpy(&l_core->cfg.l2[filter->index].filter, filter,
				 sizeof(struct osi_filter));
		l_core->cfg.l2[filter->index].used = OSI_ENABLE;
	} else if ((filter->oper_mode & OSI_OPER_ADDR_DEL) == OSI_OPER_ADDR_DEL) {
		l_core->cfg.l2[filter->index].used = OSI_DISABLE;
	} else {
		(void)osi_memcpy(&l_core->cfg.l2_filter, filter,
				 sizeof(struct osi_filter));
	}
}

/**
 * @brief osi_hal_handle_ioctl - HW function API to handle runtime command
 *
 * @note
 * Algorithm:
 *  - Handle runtime commands to OSI
 *  - OSI_CMD_MDC_CONFIG
 *	Derive MDC clock based on provided AXI_CBB clk
 *	arg1_u32 - CSR (AXI CBB) clock rate.
 *  - OSI_CMD_RESTORE_REGISTER
 *	Restore backup of MAC MMIO address space
 *  - OSI_CMD_POLL_FOR_MAC_RST
 *	Poll Software reset bit in MAC HW
 *  - OSI_CMD_COMMON_ISR
 *	Common ISR handler
 *  - OSI_CMD_PAD_CALIBRATION
 *	PAD calibration
 *  - OSI_CMD_READ_MMC
 *	invoke function to read actual registers and update
 *     structure variable mmc
 *  - OSI_CMD_GET_MAC_VER
 *	Reading MAC version
 *	arg1_u32 - holds mac version
 *  - OSI_CMD_VALIDATE_CORE_REG
 *	 Read-validate HW registers for func safety
 *  - OSI_CMD_RESET_MMC
 *	invoke function to reset MMC counter and data
 *        structure
 *  - OSI_CMD_SAVE_REGISTER
 *	 Take backup of MAC MMIO address space
 *  - OSI_CMD_MAC_LB
 *	Configure MAC loopback
 *  - OSI_CMD_FLOW_CTRL
 *	Configure flow control settings
 *	arg1_u32 - Enable or disable flow control settings
 *  - OSI_CMD_SET_MODE
 *	Set Full/Half Duplex mode.
 *	arg1_u32 - mode
 *  - OSI_CMD_SET_SPEED
 *	Set Operating speed
 *	arg1_u32 - Operating speed
 *  - OSI_CMD_L2_FILTER
 *	configure L2 mac filter
 *	l2_filter_struct - OSI filter structure
 *  - OSI_CMD_RXCSUM_OFFLOAD
 *	Configure RX checksum offload in MAC
 *	arg1_u32 - enable(1)/disable(0)
 *  - OSI_CMD_ADJ_FREQ
 *	Adjust frequency
 *	arg6_u32 - Parts per Billion
 *  - OSI_CMD_ADJ_TIME
 *	Adjust MAC time with system time
 *	arg1_u32 - Delta time in nano seconds
 *  - OSI_CMD_CONFIG_PTP
 *	Configure PTP
 *	arg1_u32 - Enable(1) or disable(0) Time Stamping
 *  - OSI_CMD_GET_AVB
 *	Get CBS algo and parameters
 *	avb_struct -  osi core avb data structure
 *  - OSI_CMD_SET_AVB
 *	Set CBS algo and parameters
 *	avb_struct -  osi core avb data structure
 *  - OSI_CMD_CONFIG_RX_CRC_CHECK
 *	Configure CRC Checking for Received Packets
 *	arg1_u32 - Enable or disable checking of CRC field in
 *	received pkts
 *  - OSI_CMD_UPDATE_VLAN_ID
 *	invoke osi call to update VLAN ID
 *	arg1_u32 - VLAN ID
 *  - OSI_CMD_CONFIG_TXSTATUS
 *	Configure Tx packet status reporting
 *	Enable(1) or disable(0) tx packet status reporting
 *  - OSI_CMD_GET_HW_FEAT
 *	Reading MAC HW features
 *	hw_feat_struct - holds the supported features of the hardware
 *  - OSI_CMD_CONFIG_FW_ERR
 *	Configure forwarding of error packets
 *	arg1_u32 - queue index, Max OSI_EQOS_MAX_NUM_QUEUES
 *	arg2_u32 - FWD error enable(1)/disable(0)
 *  - OSI_CMD_ARP_OFFLOAD
 *	Configure ARP offload in MAC
 *	arg1_u32 - Enable/disable flag
 *	arg7_u8_p - Char array representation of IP address
 *  - OSI_CMD_VLAN_FILTER
 *	OSI call for configuring VLAN filter
 *	vlan_filter - vlan filter structure
 *  - OSI_CMD_CONFIG_EEE
 *	Configure EEE LPI in MAC
 *	arg1_u32 - Enable (1)/disable (0) tx lpi
 *	arg2_u32 - Tx LPI entry timer in usecs upto
 *		   OSI_MAX_TX_LPI_TIMER (in steps of 8usec)
 *  - OSI_CMD_L3L4_FILTER
 *	invoke OSI call to add L3/L4
 *	l3l4_filter - l3_l4 filter structure
 *	arg1_u32 - L3 filter (ipv4(0) or ipv6(1))
 *            or L4 filter (tcp(0) or udp(1)
 *	arg2_u32 - filter based dma routing enable(1)
 *	arg3_u32 - dma channel for routing based on filter.
 *		   Max OSI_EQOS_MAX_NUM_CHANS.
 *	arg4_u32 - API call for L3 filter(0) or L4 filter(1)
 *  - OSI_CMD_SET_SYSTOHW_TIME
 *	set system to MAC hardware
 *	arg1_u32 - sec
 *	arg1_u32 - nsec
 *  - OSI_CMD_CONFIG_PTP_OFFLOAD
 *	enable/disable PTP offload feature
 *	pto_config - ptp offload structure
 *  - OSI_CMD_PTP_RXQ_ROUTE
 *	rxq routing to secific queue
 *	rxq_route - rxq routing information in structure
 *  - OSI_CMD_CONFIG_FRP
 *	Issue FRP command to HW
 *	frp_cmd - FRP command parameter
 *  - OSI_CMD_CONFIG_RSS
 *	Configure RSS
 *  - OSI_CMD_CONFIG_EST
 *	Configure EST registers and GCL to hw
 *	est - EST configuration structure
 *  - OSI_CMD_CONFIG_FPE
 *	Configuration FPE register and preemptable queue
 *	fpe - FPE configuration structure
 *
 *  - OSI_CMD_GET_TX_TS
 *	Command to get TX timestamp for PTP packet
 *	ts - OSI core timestamp structure
 *
 *  - OSI_CMD_FREE_TS
 *	Command to free old timestamp for PTP packet
 *	chan - DMA channel number +1. 0 will be used for onestep
 *
 *  - OSI_CMD_CAP_TSC_PTP
 *      Capture TSC and PTP time stamp
 *      ptp_tsc_data - output structure with time
 *
 *  - OSI_CMD_CONF_M2M_TS
 *	Enable/Disable MAC to MAC time sync for Secondary interface
 *	enable_disable - 1 - enable, 0- disable
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] data: void pointer pointing to osi_ioctl
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t osi_hal_handle_ioctl(struct osi_core_priv_data *osi_core,
				    struct osi_ioctl *data)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const struct core_ops *ops_p;
	nve32_t ret = -1;
	struct osi_core_priv_data *sec_osi_core;
	struct core_local *secondary_osi_lcore;
	nvel64_t drift_value = 0x0;
	nveu32_t sec = 0x0;
	nveu32_t nsec = 0x0;
	nveu32_t secondary_sec = 0x0;
	nveu32_t secondary_nsec = 0x0;
	nve32_t freq_adj_value = 0x0;
	nvel64_t secondary_time;

	ops_p = l_core->ops_p;

	switch (data->cmd) {
	case OSI_CMD_L3L4_FILTER:
		ret = configure_l3l4_filter(osi_core, &data->l3l4_filter);
		if (ret == 0) {
			l_core->cfg.flags |= DYNAMIC_CFG_L3_L4;
		}
		break;

#ifndef OSI_STRIPPED_LIB
	case OSI_CMD_MDC_CONFIG:
		ops_p->set_mdc_clk_rate(osi_core, data->arg5_u64);
		ret = 0;
		break;

	case OSI_CMD_RESET_MMC:
		ops_p->reset_mmc(osi_core);
		ret = 0;
		break;

	case OSI_CMD_MAC_LB:
		ret = conf_mac_loopback(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_FLOW_CTRL:
		ret = ops_p->config_flow_control(osi_core, data->arg1_u32);
		if (ret == 0) {
			l_core->cfg.flow_ctrl = data->arg1_u32;
			l_core->cfg.flags |= DYNAMIC_CFG_FC;
		}

		break;

	case OSI_CMD_CONFIG_RX_CRC_CHECK:
		ret = ops_p->config_rx_crc_check(osi_core, data->arg1_u32);
		break;

	case OSI_CMD_UPDATE_VLAN_ID:
		ret = vlan_id_update(osi_core, data->arg1_u32);
		if (ret == 0) {
			if ((data->arg1_u32 & VLAN_ACTION_MASK) == OSI_VLAN_ACTION_ADD) {
				l_core->cfg.vlan[data->arg1_u32 & VLAN_VID_MASK].vid =
					data->arg1_u32 & VLAN_VID_MASK;
				l_core->cfg.vlan[data->arg1_u32 & VLAN_VID_MASK].used = OSI_ENABLE;
			} else {
				l_core->cfg.vlan[data->arg1_u32 & VLAN_VID_MASK].used = OSI_DISABLE;
			}

			l_core->cfg.flags |= DYNAMIC_CFG_VLAN;
		}

		break;

	case OSI_CMD_CONFIG_TXSTATUS:
		ret = ops_p->config_tx_status(osi_core, data->arg1_u32);
		break;


	case OSI_CMD_ARP_OFFLOAD:
		ret = conf_arp_offload(osi_core, data->arg1_u32,
				       data->arg7_u8_p);
		break;

	case OSI_CMD_VLAN_FILTER:
		ret = ops_p->config_vlan_filtering(osi_core,
				data->vlan_filter.filter_enb_dis,
				data->vlan_filter.perfect_hash,
				data->vlan_filter.perfect_inverse_match);
		break;

	case OSI_CMD_CONFIG_EEE:
		ret = conf_eee(osi_core, data->arg1_u32, data->arg2_u32);
		if (ret == 0) {
			l_core->cfg.tx_lpi_enabled = data->arg1_u32;
			l_core->cfg.tx_lpi_timer = data->arg2_u32;
			l_core->cfg.flags |= DYNAMIC_CFG_EEE;
		}

		break;
	case OSI_CMD_CONFIG_FW_ERR:
		ret = hw_config_fw_err_pkts(osi_core, data->arg1_u32, data->arg2_u32);
		break;

	case OSI_CMD_POLL_FOR_MAC_RST:
		ret = hw_poll_for_swr(osi_core);
		break;

	case OSI_CMD_GET_MAC_VER:
		ret = osi_get_mac_version(osi_core, &data->arg1_u32);
		break;

	case OSI_CMD_SET_MODE:
		ret = hw_set_mode(osi_core, data->arg6_32);
		break;
#endif /* !OSI_STRIPPED_LIB */

	case OSI_CMD_GET_AVB:
		ret = ops_p->get_avb_algorithm(osi_core, &data->avb);
		break;

	case OSI_CMD_SET_AVB:
		if (data->avb.algo == OSI_MTL_TXQ_AVALG_CBS) {
			ret = hw_validate_avb_input(osi_core, &data->avb);
			if (ret != 0) {
				break;
			}
		}

		ret = ops_p->set_avb_algorithm(osi_core, &data->avb);
		if (ret == 0) {
			(void)osi_memcpy(&l_core->cfg.avb[data->avb.qindex].avb_info,
					 &data->avb, sizeof(struct osi_core_avb_algorithm));
			l_core->cfg.avb[data->avb.qindex].used = OSI_ENABLE;
			l_core->cfg.flags |= DYNAMIC_CFG_AVB;
		}
		break;

	case OSI_CMD_COMMON_ISR:
		ops_p->handle_common_intr(osi_core);
		ret = 0;
		break;

	case OSI_CMD_PAD_CALIBRATION:
		ret = ops_p->pad_calibrate(osi_core);
		break;

	case OSI_CMD_READ_MMC:
		ops_p->read_mmc(osi_core);
		ret = 0;
		break;

	case OSI_CMD_SET_SPEED:
		ret = hw_set_speed(osi_core, data->arg6_32);
		break;

	case OSI_CMD_L2_FILTER:
		ret = osi_l2_filter(osi_core, &data->l2_filter);
		if (ret == 0) {
			store_l2_filter(osi_core, &data->l2_filter);
			l_core->cfg.flags |= DYNAMIC_CFG_L2;
		}

		break;

	case OSI_CMD_RXCSUM_OFFLOAD:
		ret = hw_config_rxcsum_offload(osi_core, data->arg1_u32);
		if (ret == 0) {
			l_core->cfg.rxcsum = data->arg1_u32;
			l_core->cfg.flags |= DYNAMIC_CFG_RXCSUM;
		}

		break;

	case OSI_CMD_ADJ_FREQ:
		ret = osi_adjust_freq(osi_core, data->arg6_32);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: adjust freq failed\n", 0ULL);
			break;
		}

		if ((l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) &&
		    (l_core->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		sec_osi_core = get_role_pointer(OSI_PTP_M2M_SECONDARY);
		secondary_osi_lcore = (struct core_local *)(void *)sec_osi_core;
		if ((validate_args(sec_osi_core, secondary_osi_lcore) < 0) ||
		    (secondary_osi_lcore->hw_init_successful != OSI_ENABLE) ||
		    (secondary_osi_lcore->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		if (l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) {
			drift_value = 0x0;
			osi_lock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);
			common_get_systime_from_mac(sec_osi_core->base,
				    sec_osi_core->mac, &secondary_sec, &secondary_nsec);
			common_get_systime_from_mac(osi_core->base,
				    osi_core->mac, &sec, &nsec);
			osi_unlock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);

			drift_value = dirft_calculation(sec, nsec, secondary_sec, secondary_nsec);
			secondary_time = ((nvel64_t)secondary_sec * 1000000000LL) + (nvel64_t)secondary_nsec;
			secondary_osi_lcore->serv.const_i = I_COMPONENT_BY_10;
			secondary_osi_lcore->serv.const_p = P_COMPONENT_BY_10;
			freq_adj_value = freq_offset_calculate(sec_osi_core,
							       drift_value,
							       secondary_time);
			if (secondary_osi_lcore->serv.count == SERVO_STATS_0) {
				/* call adjust time as JUMP happened */
				ret = osi_adjust_time(sec_osi_core,
						      drift_value);
			} else {
				ret = osi_adjust_freq(sec_osi_core,
						      freq_adj_value);
			}
		}

		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: adjust_freq for sec_controller failed\n",
				     0ULL);
			ret = 0;
		}

		break;

	case OSI_CMD_ADJ_TIME:
		ret = osi_adjust_time(osi_core, data->arg8_64);

		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: adjust_time failed\n", 0ULL);
			break;
		}

		if ((l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) &&
		    (l_core->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		sec_osi_core = get_role_pointer(OSI_PTP_M2M_SECONDARY);
		secondary_osi_lcore = (struct core_local *)(void *)sec_osi_core;
		if ((validate_args(sec_osi_core, secondary_osi_lcore) < 0) ||
		    (secondary_osi_lcore->hw_init_successful != OSI_ENABLE) ||
		    (secondary_osi_lcore->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		if (l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) {
			drift_value = 0x0;
			osi_lock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);
			common_get_systime_from_mac(sec_osi_core->base,
				    sec_osi_core->mac, &secondary_sec, &secondary_nsec);
			common_get_systime_from_mac(osi_core->base,
				    osi_core->mac, &sec, &nsec);
			osi_unlock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);
			drift_value = dirft_calculation(sec, nsec,
							secondary_sec,
							secondary_nsec);
			ret = osi_adjust_time(sec_osi_core, drift_value);
			if (ret == 0) {
				secondary_osi_lcore->serv.count = SERVO_STATS_0;
				secondary_osi_lcore->serv.drift = 0;
				secondary_osi_lcore->serv.last_ppb = 0;
			}
		}

		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: adjust_time for sec_controller failed\n",
				     0ULL);
			ret = 0;
		}

		break;

	case OSI_CMD_CONFIG_PTP:
		ret = osi_ptp_configuration(osi_core, data->arg1_u32);
		if (ret == 0) {
			l_core->cfg.ptp = data->arg1_u32;
			l_core->cfg.flags |= DYNAMIC_CFG_PTP;
		}

		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: configure_ptp failed\n", 0ULL);
			break;
		}

		if ((l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) &&
		    (l_core->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		sec_osi_core = get_role_pointer(OSI_PTP_M2M_SECONDARY);
		secondary_osi_lcore = (struct core_local *)(void *)sec_osi_core;
		if ((validate_args(sec_osi_core, secondary_osi_lcore) < 0) ||
		    (secondary_osi_lcore->hw_init_successful != OSI_ENABLE) ||
		    (secondary_osi_lcore->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		if ((l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) &&
		    (data->arg1_u32 == OSI_ENABLE)) {
			secondary_osi_lcore->serv.count = SERVO_STATS_0;
			secondary_osi_lcore->serv.drift = 0;
			secondary_osi_lcore->serv.last_ppb = 0;
		}

		break;

	case OSI_CMD_GET_HW_FEAT:
		ret = ops_p->get_hw_features(osi_core, &data->hw_feat);
		break;

	case OSI_CMD_SET_SYSTOHW_TIME:
		ret = hw_set_systime_to_mac(osi_core, data->arg1_u32, data->arg2_u32);

		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: set systohw time failed\n", 0ULL);
			break;
		}

		if ((l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) &&
		    (l_core->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		sec_osi_core = get_role_pointer(OSI_PTP_M2M_SECONDARY);
		secondary_osi_lcore = (struct core_local *)(void *)sec_osi_core;
		if ((validate_args(sec_osi_core, secondary_osi_lcore) < 0) ||
		    (secondary_osi_lcore->hw_init_successful != OSI_ENABLE) ||
		    (secondary_osi_lcore->m2m_tsync != OSI_ENABLE)) {
			break;
		}

		if (l_core->ether_m2m_role == OSI_PTP_M2M_PRIMARY) {
			osi_lock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);
			common_get_systime_from_mac(osi_core->base,
				    osi_core->mac, &sec, &nsec);
			osi_unlock_irq_enabled(&secondary_osi_lcore->serv.m2m_lock);
			ret = hw_set_systime_to_mac(sec_osi_core, sec, nsec);
			if (ret == 0) {
				secondary_osi_lcore->serv.count = SERVO_STATS_0;
				secondary_osi_lcore->serv.drift = 0;
				secondary_osi_lcore->serv.last_ppb = 0;
			}
		}
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "CORE: set_time for sec_controller failed\n",
				     0ULL);
			ret = 0;
		}

		break;
#ifndef OSI_STRIPPED_LIB
	case OSI_CMD_CONFIG_PTP_OFFLOAD:
		ret = conf_ptp_offload(osi_core, &data->pto_config);
		break;

	case OSI_CMD_PTP_RXQ_ROUTE:
		ret = rxq_route_config(osi_core, &data->rxq_route);
		break;

	case OSI_CMD_CONFIG_RSS:
		ret = ops_p->config_rss(osi_core);
		break;

#endif /* !OSI_STRIPPED_LIB */
	case OSI_CMD_CONFIG_FRP:
		ret = configure_frp(osi_core, &data->frp_cmd);
		break;

	case OSI_CMD_CONFIG_EST:
		ret = config_est(osi_core, &data->est);
		if (ret == 0) {
			(void)osi_memcpy(&l_core->cfg.est, &data->est,
					 sizeof(struct osi_est_config));
			l_core->cfg.flags |= DYNAMIC_CFG_EST;
		}

		break;

	case OSI_CMD_CONFIG_FPE:
		ret = config_fpe(osi_core, &data->fpe);
		if (ret == 0) {
			(void)osi_memcpy(&l_core->cfg.fpe, &data->fpe,
					 sizeof(struct osi_fpe_config));
			l_core->cfg.flags |= DYNAMIC_CFG_FPE;
		}

		break;

	case OSI_CMD_READ_REG:
		ret = (nve32_t) ops_p->read_reg(osi_core, (nve32_t) data->arg1_u32);
		break;

	case OSI_CMD_WRITE_REG:
		ret = (nve32_t) ops_p->write_reg(osi_core, (nveu32_t) data->arg1_u32,
				       (nve32_t) data->arg2_u32);
		break;
#ifdef MACSEC_SUPPORT
	case OSI_CMD_READ_MACSEC_REG:
		ret = (nve32_t) ops_p->read_macsec_reg(osi_core, (nve32_t) data->arg1_u32);
		break;

	case OSI_CMD_WRITE_MACSEC_REG:
		ret = (nve32_t) ops_p->write_macsec_reg(osi_core, (nveu32_t) data->arg1_u32,
				       (nve32_t) data->arg2_u32);
		break;
#endif /*  MACSEC_SUPPORT */
	case OSI_CMD_GET_TX_TS:
		ret = get_tx_ts(osi_core, &data->tx_ts);
		break;

	case OSI_CMD_FREE_TS:
		free_tx_ts(osi_core, data->arg1_u32);
		ret = 0;
		break;

	case OSI_CMD_MAC_MTU:
		ret = 0;
#ifdef MACSEC_SUPPORT
		if ((osi_core->macsec_ops != OSI_NULL) &&
		    (osi_core->macsec_ops->update_mtu != OSI_NULL)) {
			ret = osi_core->macsec_ops->update_mtu(osi_core, data->arg1_u32);
		}
#endif /*  MACSEC_SUPPORT */
		break;

#ifdef OSI_DEBUG
	case OSI_CMD_REG_DUMP:
		core_reg_dump(osi_core);
		ret = 0;
		break;
	case OSI_CMD_STRUCTS_DUMP:
		core_structs_dump(osi_core);
		ret = 0;
		break;
#endif /* OSI_DEBUG */
	case OSI_CMD_CAP_TSC_PTP:
		ret = hw_ptp_tsc_capture(osi_core, &data->ptp_tsc);
		break;

	case OSI_CMD_CONF_M2M_TS:
		if (data->arg1_u32 <= OSI_ENABLE) {
			l_core->m2m_tsync = data->arg1_u32;
			ret = 0;
		}
		break;
#ifdef HSI_SUPPORT
	case OSI_CMD_HSI_CONFIGURE:
		ret = ops_p->core_hsi_configure(osi_core, data->arg1_u32);
		break;
	case OSI_CMD_HSI_INJECT_ERR:
		ret = ops_p->core_hsi_inject_err(osi_core, data->arg1_u32);
		break;
#endif

#ifdef OSI_DEBUG
	case OSI_CMD_DEBUG_INTR_CONFIG:
#ifdef DEBUG_MACSEC
		osi_core->macsec_ops->intr_config(osi_core, data->arg1_u32);
#endif
		ret = 0;
		break;
#endif
	case OSI_CMD_SUSPEND:
		l_core->state = OSI_SUSPENDED;
		ret = osi_hal_hw_core_deinit(osi_core);
		break;
	case OSI_CMD_RESUME:
		ret = osi_hal_hw_core_init(osi_core);
		if (ret < 0) {
			break;
		}

		apply_dynamic_cfg(osi_core);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "CORE: Incorrect command\n",
			     (nveul64_t)data->cmd);
		break;
	}

	return ret;
}

void hw_interface_init_core_ops(struct if_core_ops *if_ops_p)
{
	if_ops_p->if_core_init = osi_hal_hw_core_init;
	if_ops_p->if_core_deinit = osi_hal_hw_core_deinit;
	if_ops_p->if_write_phy_reg = osi_hal_write_phy_reg;
	if_ops_p->if_read_phy_reg = osi_hal_read_phy_reg;
	if_ops_p->if_init_core_ops = osi_hal_init_core_ops;
	if_ops_p->if_handle_ioctl = osi_hal_handle_ioctl;
}
