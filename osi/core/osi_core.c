/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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

#include <osi_core.h>
#include <osd.h>
#include <local_common.h>

int osi_write_phy_reg(struct osi_core_priv_data *const osi_core,
		      const unsigned int phyaddr, const unsigned int phyreg,
		      const unsigned short phydata)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->write_phy_reg != OSI_NULL)) {
		return osi_core->ops->write_phy_reg(osi_core,
						    phyaddr, phyreg, phydata);
	}

	return -1;
}

int osi_read_phy_reg(struct osi_core_priv_data *const osi_core,
		     const unsigned int phyaddr, const unsigned int phyreg)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->read_phy_reg != OSI_NULL)) {
		return osi_core->ops->read_phy_reg(osi_core, phyaddr, phyreg);
	}

	return -1;
}

int osi_init_core_ops(struct osi_core_priv_data *const osi_core)
{
	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		/* Get EQOS HW ops */
		osi_core->ops = eqos_get_hw_core_ops();
		/* Explicitly set osi_core->safety_config = OSI_NULL if
		 * a particular MAC version does not need SW safety mechanisms
		 * like periodic read-verify.
		 */
		osi_core->safety_config = (void *)eqos_get_core_safety_config();
		return 0;
	}

	return -1;
}

int osi_poll_for_mac_reset_complete(
			struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->poll_for_swr != OSI_NULL)) {
		return osi_core->ops->poll_for_swr(osi_core->base,
						   osi_core->pre_si);
	}
	return -1;
}

int osi_set_mdc_clk_rate(struct osi_core_priv_data *const osi_core,
			 const unsigned long csr_clk_rate)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_mdc_clk_rate != OSI_NULL)) {
		osi_core->ops->set_mdc_clk_rate(osi_core, csr_clk_rate);
		return 0;
	}

	return -1;
}

int osi_hw_core_init(struct osi_core_priv_data *const osi_core,
		     unsigned int tx_fifo_size,
		     unsigned int rx_fifo_size)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->core_init != OSI_NULL)) {
		return osi_core->ops->core_init(osi_core, tx_fifo_size,
						rx_fifo_size);
	}

	return -1;
}

int osi_hw_core_deinit(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->core_deinit != OSI_NULL)) {
		osi_core->ops->core_deinit(osi_core);
		return 0;
	}

	return -1;
}

int osi_start_mac(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->start_mac != OSI_NULL)) {
		osi_core->ops->start_mac(osi_core->base);
		return 0;
	}

	return -1;
}

int osi_stop_mac(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->stop_mac != OSI_NULL)) {
		osi_core->ops->stop_mac(osi_core->base);
		return 0;
	}

	return -1;
}

int osi_common_isr(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->handle_common_intr != OSI_NULL)) {
		osi_core->ops->handle_common_intr(osi_core);
		return 0;
	}

	return -1;
}

int osi_set_mode(struct osi_core_priv_data *const osi_core, const int mode)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_mode != OSI_NULL)) {
		osi_core->ops->set_mode(osi_core->base, mode);
		return 0;
	}

	return -1;
}

int osi_set_speed(struct osi_core_priv_data *const osi_core,
		  const int speed)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_speed != OSI_NULL)) {
		osi_core->ops->set_speed(osi_core->base, speed);
		return 0;
	}

	return -1;
}

int osi_pad_calibrate(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->pad_calibrate != OSI_NULL)) {
		return osi_core->ops->pad_calibrate(osi_core->base);
	}

	return -1;
}

int osi_config_mac_loopback(struct osi_core_priv_data *const osi_core,
			    const unsigned int lb_mode)
{
	/* Configure MAC loopback */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_mac_loopback != OSI_NULL)) {
		return osi_core->ops->config_mac_loopback(osi_core->base,
							  lb_mode);
	}

	return -1;
}

int osi_config_fw_err_pkts(struct osi_core_priv_data *const osi_core,
			   const unsigned int qinx, const unsigned int fw_err)
{
	/* Configure Forwarding of Error packets */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_fw_err_pkts != OSI_NULL)) {
		return osi_core->ops->config_fw_err_pkts(osi_core->base,
							 qinx, fw_err);
	}

	return -1;
}

int osi_l2_filter(struct osi_core_priv_data *const osi_core,
		  const struct osi_filter *filter)
{
	struct osi_core_ops *op;
	int ret = -1;

	if (osi_core == OSI_NULL || osi_core->ops == OSI_NULL ||
	    filter == OSI_NULL) {
		return ret;
	}

	op = osi_core->ops;

	if ((op->config_mac_pkt_filter_reg != OSI_NULL)) {
		ret = op->config_mac_pkt_filter_reg(osi_core, filter);
	} else {
		OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			 "op->config_mac_pkt_filter_reg is null\n", 0ULL);
		return ret;
	}

	if (((filter->oper_mode & OSI_OPER_ADDR_UPDATE) != OSI_NONE) ||
	    ((filter->oper_mode & OSI_OPER_ADDR_DEL) != OSI_NONE)) {
		ret = -1;

		if ((filter->dma_routing == OSI_ENABLE) &&
		    (osi_core->dcs_en != OSI_ENABLE)) {
			OSI_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				"DCS requested. Conflicts with DT config\n",
				0ULL);
			return ret;
		}

		if ((op->update_mac_addr_low_high_reg != OSI_NULL)) {
			ret = op->update_mac_addr_low_high_reg(osi_core,
								filter);
		} else {
			OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
				 "op->update_mac_addr_low_high_reg is null\n",
				 0ULL);
		}
	}

	return ret;
}

/**
 * @brief helper_l4_filter helper function for l4 filtering
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_filter: filter structure
 * @param[in] type: filter type l3 or l4
 * @param[in] dma_routing_enable: dma routing enable (1) or disable (0)
 * @param[in] dma_chan: dma channel
 *
 * @pre MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline int helper_l4_filter(
				   struct osi_core_priv_data *const osi_core,
				   struct osi_l3_l4_filter l_filter,
				   unsigned int type,
				   unsigned int dma_routing_enable,
				   unsigned int dma_chan)
{
	struct osi_core_ops *op = osi_core->ops;

	if (op->config_l4_filters != OSI_NULL) {
		if (op->config_l4_filters(osi_core,
					  l_filter.filter_no,
					  l_filter.filter_enb_dis,
					  type,
					  l_filter.src_dst_addr_match,
					  l_filter.perfect_inverse_match,
					  dma_routing_enable,
					  dma_chan) < 0) {
			return -1;
		}
	} else {
		OSI_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"op->config_l4_filters is NULL\n", 0ULL);
		return -1;
	}

	if (op->update_l4_port_no != OSI_NULL) {
		return op->update_l4_port_no(osi_core,
					     l_filter.filter_no,
					     l_filter.port_no,
					     l_filter.src_dst_addr_match);
	} else {
		OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			 "op->update_l4_port_no is NULL\n", 0ULL);
		return -1;
	}

}

/**
 * @brief helper_l3_filter helper function for l3 filtering
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] l_filter: filter structure
 * @param[in] type: filter type l3 or l4
 * @param[in] dma_routing_enable: dma routing enable (1) or disable (0)
 * @param[in] dma_chan: dma channel
 *
 * @pre MAC needs to be out of reset and proper clock configured.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline int helper_l3_filter(
				   struct osi_core_priv_data *const osi_core,
				   struct osi_l3_l4_filter l_filter,
				   unsigned int type,
				   unsigned int dma_routing_enable,
				   unsigned int dma_chan)
{
	struct osi_core_ops *op = osi_core->ops;

	if ((op->config_l3_filters != OSI_NULL)) {
		if (op->config_l3_filters(osi_core,
					  l_filter.filter_no,
					  l_filter.filter_enb_dis,
					  type,
					  l_filter.src_dst_addr_match,
					  l_filter.perfect_inverse_match,
					  dma_routing_enable,
					  dma_chan) < 0) {
			return -1;
		}
	} else {
		OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			 "op->config_l3_filters is NULL\n", 0ULL);
		return -1;
	}

	if (type == OSI_IP6_FILTER) {
		if (op->update_ip6_addr != OSI_NULL) {
			return op->update_ip6_addr(osi_core, l_filter.filter_no,
					  l_filter.ip6_addr);
		} else {
			OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
				 "op->update_ip6_addr is NULL\n", 0ULL);
			return -1;
		}
	} else if (type == OSI_IP4_FILTER) {
		if (op->update_ip4_addr != OSI_NULL) {
			return op->update_ip4_addr(osi_core,
						   l_filter.filter_no,
						   l_filter.ip4_addr,
						   l_filter.src_dst_addr_match);
		} else {
			OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
				 "op->update_ip4_addr is NULL\n",
				 0ULL);
			return -1;
		}
	} else {
		return -1;
	}
}

int osi_l3l4_filter(struct osi_core_priv_data *const osi_core,
	const struct osi_l3_l4_filter l_filter, const unsigned int type,
	const unsigned int dma_routing_enable, const unsigned int dma_chan,
	const unsigned int is_l4_filter)
{
	int ret = -1;

	if ((osi_core == OSI_NULL) || (osi_core->ops == OSI_NULL)) {
		return ret;
	}

	if ((dma_routing_enable == OSI_ENABLE) &&
	    (osi_core->dcs_en != OSI_ENABLE)) {
		OSI_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			"dma routing enabled but dcs disabled in DT\n",
			0ULL);
		return ret;
	}

	if (is_l4_filter == OSI_ENABLE) {
		ret = helper_l4_filter(osi_core, l_filter, type,
				       dma_routing_enable, dma_chan);
	} else {
		ret = helper_l3_filter(osi_core, l_filter, type,
				       dma_routing_enable, dma_chan);
	}

	if (ret < 0) {
		OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			 "L3/L4 helper function failed\n", 0ULL);
		return ret;
	}

	if (osi_core->ops->config_l3_l4_filter_enable != OSI_NULL) {
		if (osi_core->l3l4_filter_bitmask != OSI_DISABLE) {
			ret = osi_core->ops->config_l3_l4_filter_enable(
								osi_core->base,
								OSI_ENABLE);
		} else {
			ret = osi_core->ops->config_l3_l4_filter_enable(
								osi_core->base,
								OSI_DISABLE);
		}

	} else {
		OSI_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
			 "op->config_l3_l4_filter_enable is NULL\n", 0ULL);
		ret = -1;
	}

	return ret;
}

int osi_config_rxcsum_offload(
			      struct osi_core_priv_data *const osi_core,
			      const unsigned int enable)
{
	if (osi_core != OSI_NULL && osi_core->ops != OSI_NULL &&
	    osi_core->ops->config_rxcsum_offload != OSI_NULL) {
		return osi_core->ops->config_rxcsum_offload(osi_core->base,
							    enable);
	}

	return -1;
}

int osi_set_systime_to_mac(struct osi_core_priv_data *const osi_core,
			   const unsigned int sec, const unsigned int nsec)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_systime_to_mac != OSI_NULL)) {
		return osi_core->ops->set_systime_to_mac(osi_core->base,
							 sec,
							 nsec);
	}

	return -1;
}

/**
 *@brief div_u64_rem - updates remainder and returns Quotient
 *
 * @note
 * Algorithm:
 *  - Dividend will be divided by divisor and stores the
 *    remainder value and returns quotient
 *
 * @param[in] dividend: Dividend value
 * @param[in] divisor: Divisor value
 * @param[out] remain: Remainder
 *
 * @pre MAC IP should be out of reset and need to be initialized as the
 *      requirements
 *
 * @returns Quotient
 */
static inline unsigned long div_u64_rem(unsigned long dividend,
					unsigned long divisor,
					unsigned long *remain)
{
	unsigned long ret = 0;

	if (divisor != 0U) {
		*remain = dividend % divisor;
		ret = dividend / divisor;
	} else {
		ret = 0;
	}
	return ret;
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
 * @returns Quotient
 */
static inline unsigned long div_u64(unsigned long dividend,
				    unsigned long divisor)
{
	unsigned long remain;

	return div_u64_rem(dividend, divisor, &remain);
}

int osi_adjust_freq(struct osi_core_priv_data *const osi_core, int ppb)
{
	unsigned long adj;
	unsigned long temp;
	unsigned int diff = 0;
	unsigned int addend;
	unsigned int neg_adj = 0;
	int ret = -1;

	if (ppb < 0) {
		neg_adj = 1U;
		ppb = -ppb;
	}

	if (osi_core == OSI_NULL) {
		return ret;
	}

	addend = osi_core->default_addend;
	adj = (unsigned long)addend * (unsigned int)ppb;

	/*
	 * div_u64 will divide the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	temp = div_u64(adj, OSI_NSEC_PER_SEC);
	if (temp < UINT_MAX) {
		diff = (unsigned int)temp;
	} else {
		/* do nothing here */
	}

	if (neg_adj == 0U) {
		if (addend <= UINT_MAX - diff) {
			addend = (addend + diff);
		} else {
			/* do nothing here */
		}
	} else {
		if (addend > diff) {
			addend = addend - diff;
		} else if (addend < diff) {
			addend = diff - addend;
		} else {
			/* do nothing here */
		}
	}

	if ((osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_addend != OSI_NULL)) {
		ret = osi_core->ops->config_addend(osi_core->base, addend);
	}

	return ret;
}

int osi_adjust_time(struct osi_core_priv_data *const osi_core,
		    long long nsec_delta)
{
	unsigned int neg_adj = 0;
	unsigned int sec = 0, nsec = 0;
	unsigned long quotient;
	unsigned long reminder = 0;
	unsigned long udelta = 0;
	int ret = -1;

	if (nsec_delta < 0) {
		neg_adj = 1;
		nsec_delta = -nsec_delta;
	}
	udelta = (unsigned long long) nsec_delta;
	quotient = div_u64_rem(udelta, OSI_NSEC_PER_SEC, &reminder);
	if (quotient <= UINT_MAX) {
		sec = (unsigned int)quotient;
	} else {
		/* do nothing */
	}
	if (reminder <= UINT_MAX) {
		nsec = (unsigned int)reminder;
	} else {
		/* do nothing here */
	}

	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->adjust_mactime != OSI_NULL)) {
		ret = osi_core->ops->adjust_mactime(osi_core->base, sec, nsec,
					neg_adj,
					osi_core->ptp_config.one_nsec_accuracy);
	}

	return ret;
}

int osi_ptp_configuration(struct osi_core_priv_data *const osi_core,
			  const unsigned int enable)
{
	int ret = 0;
	unsigned long temp = 0, temp1 = 0, temp2 = 0;
	unsigned long ssinc = 0;

	if ((osi_core == OSI_NULL) || (osi_core->ops == OSI_NULL) ||
	    (osi_core->ops->config_tscr == OSI_NULL) ||
	    (osi_core->ops->config_ssir == OSI_NULL) ||
	    (osi_core->ops->config_addend == OSI_NULL) ||
	    (osi_core->ops->set_systime_to_mac == OSI_NULL)) {
		return -1;
	}

	if (enable == OSI_DISABLE) {
		/* disable hw time stamping */
		/* Program MAC_Timestamp_Control Register */
		osi_core->ops->config_tscr(osi_core->base, OSI_DISABLE);
	} else {
		/* Program MAC_Timestamp_Control Register */
		osi_core->ops->config_tscr(osi_core->base,
					   osi_core->ptp_config.ptp_filter);

		/* Program Sub Second Increment Register */
		osi_core->ops->config_ssir(osi_core);

		/* formula for calculating addend value is
		 * TSAR = (2^32 * 1000) / (ptp_ref_clk_rate in MHz * SSINC)
		 * 2^x * y == (y << x), hence
		 * 2^32 * 1000 == (1000 << 32)
		 * so addend = (2^32 * 1000)/(ptp_ref_clk_rate in MHZ * SSINC);
		 */

		if (osi_core->mac_ver <= OSI_EQOS_MAC_4_10) {
			ssinc = OSI_PTP_SSINC_16;
		} else {
			ssinc = OSI_PTP_SSINC_4;
		}

		temp = ((unsigned long)1000 << 32);
		temp = (unsigned long)temp * 1000000U;

		temp1 = div_u64(temp,
			(unsigned long)osi_core->ptp_config.ptp_ref_clk_rate);

		temp2 = div_u64(temp1, (unsigned long)ssinc);

		if (temp2 < UINT_MAX) {
			osi_core->default_addend = (unsigned int)temp2;
		} else {
			/* do nothing here */
		}

		/* Program addend value */
		ret = osi_core->ops->config_addend(osi_core->base,
					osi_core->default_addend);

		/* Set current time */
		ret = osi_core->ops->set_systime_to_mac(osi_core->base,
						osi_core->ptp_config.sec,
						osi_core->ptp_config.nsec);
	}

	return ret;
}

int osi_read_mmc(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->read_mmc != OSI_NULL)) {
		osi_core->ops->read_mmc(osi_core);
		return 0;
	}

	return -1;
}

#ifndef OSI_STRIPPED_LIB
int osi_validate_core_regs(struct osi_core_priv_data *const osi_core)
{
	int ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->validate_regs != OSI_NULL) &&
	    (osi_core->safety_config != OSI_NULL)) {
		ret = osi_core->ops->validate_regs(osi_core);
	}

	return ret;
}

int osi_flush_mtl_tx_queue(struct osi_core_priv_data *const osi_core,
			   const unsigned int qinx)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->flush_mtl_tx_queue != OSI_NULL)) {
		return osi_core->ops->flush_mtl_tx_queue(osi_core->base, qinx);
	}

	return -1;
}

int osi_set_avb(struct osi_core_priv_data *const osi_core,
		const struct osi_core_avb_algorithm *avb)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->set_avb_algorithm != OSI_NULL)) {
		return osi_core->ops->set_avb_algorithm(osi_core, avb);
	}

	return -1;
}

int osi_get_avb(struct osi_core_priv_data *const osi_core,
		struct osi_core_avb_algorithm *avb)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->get_avb_algorithm != OSI_NULL)) {
		return osi_core->ops->get_avb_algorithm(osi_core, avb);
	}

	return -1;
}

int osi_configure_txstatus(struct osi_core_priv_data *const osi_core,
			   const unsigned int tx_status)
{
	/* Configure Drop Transmit Status */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_tx_status != OSI_NULL)) {
		return osi_core->ops->config_tx_status(osi_core->base,
						       tx_status);
	}

	return -1;
}

int osi_config_rx_crc_check(struct osi_core_priv_data *const osi_core,
			    const unsigned int crc_chk)
{
	/* Configure CRC Checking for Received Packets */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_rx_crc_check != OSI_NULL)) {
		return osi_core->ops->config_rx_crc_check(osi_core->base,
							  crc_chk);
	}

	return -1;
}

int osi_config_vlan_filtering(
			      struct osi_core_priv_data *const osi_core,
			      const unsigned int filter_enb_dis,
			      const unsigned int perfect_hash_filtering,
			      const unsigned int perfect_inverse_match)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_vlan_filtering != OSI_NULL)) {
		return osi_core->ops->config_vlan_filtering(
							osi_core,
							filter_enb_dis,
							perfect_hash_filtering,
							perfect_inverse_match);
	}

	return -1;
}

int  osi_update_vlan_id(struct osi_core_priv_data *const osi_core,
			const unsigned int vid)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->update_vlan_id != OSI_NULL)) {
		return osi_core->ops->update_vlan_id(osi_core->base,
						    vid);
	}

	return -1;
}

int osi_get_systime_from_mac(
			     struct osi_core_priv_data *const osi_core,
			     unsigned int *sec,
			     unsigned int *nsec)
{
	if ((osi_core != OSI_NULL) && (osi_core->base != OSI_NULL)) {
		common_get_systime_from_mac(osi_core->base, osi_core->mac, sec,
					    nsec);
	} else {
		return -1;
	}

	return 0;
}

int osi_reset_mmc(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->reset_mmc != OSI_NULL)) {
		osi_core->ops->reset_mmc(osi_core);
		return 0;
	}

	return -1;
}

int osi_configure_eee(struct osi_core_priv_data *const osi_core,
		       unsigned int tx_lpi_enabled, unsigned int tx_lpi_timer)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->configure_eee != OSI_NULL) &&
	    (tx_lpi_timer <= OSI_MAX_TX_LPI_TIMER) &&
	    (tx_lpi_timer >= OSI_MIN_TX_LPI_TIMER) &&
	    (tx_lpi_timer % OSI_MIN_TX_LPI_TIMER == OSI_NONE)) {
		osi_core->ops->configure_eee(osi_core, tx_lpi_enabled,
					     tx_lpi_timer);
		return 0;
	}

	return -1;
}

int osi_save_registers(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->save_registers != OSI_NULL)) {
		/* Call MAC save registers callback and return the value */
		return osi_core->ops->save_registers(osi_core);
	}

	return -1;
}

int osi_restore_registers(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->restore_registers != OSI_NULL)) {
		/* Call MAC restore registers callback and return the value */
		return osi_core->ops->restore_registers(osi_core);
	}

	return -1;
}

int osi_configure_flow_control(
			       struct osi_core_priv_data *const osi_core,
			       const unsigned int flw_ctrl)
{
	/* Configure Flow control settings */
	if ((osi_core != OSI_NULL) && (osi_core->ops != OSI_NULL) &&
	    (osi_core->ops->config_flow_control != OSI_NULL)) {
		return osi_core->ops->config_flow_control(osi_core->base,
							  flw_ctrl);
	}

	return -1;
}

int osi_config_arp_offload(struct osi_core_priv_data *const osi_core,
			   const unsigned int flags,
			   const unsigned char *ip_addr)
{
	if (osi_core != OSI_NULL && osi_core->ops != OSI_NULL &&
	    osi_core->ops->config_arp_offload != OSI_NULL) {
		return osi_core->ops->config_arp_offload(osi_core->mac_ver,
							 osi_core->base,
							 flags, ip_addr);
	}

	return -1;
}
#endif /* !OSI_STRIPPED_LIB */
