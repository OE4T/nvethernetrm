###################################
#
# Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
###############################################################################


LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

include $(TEGRA_TOP)/kernel/nvethernetrm/include/config.tmk

GLOBAL_INCLUDES += \
	$(TEGRA_TOP)/kernel/nvethernetrm/include/ \
	$(TEGRA_TOP)/kernel/nvethernetrm/osi/common/include/ \
	$(TEGRA_TOP)/fsi-internal/fw/include \
	$(TEGRA_TOP)/fsi-internal/fw/app/tests/drivers/eqos_test \

MODULE_SRCS += \
        $(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/osi_dma.c \
        $(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/osi_dma_txrx.c \
        $(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/eqos_desc.c \
        $(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/mgbe_desc.c \
	$(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/mgbe_dma.c \
	$(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/eqos_dma.c \
	$(TEGRA_TOP)/kernel/nvethernetrm/osi/dma/debug.c

MODULE_COMPILEFLAGS += -Wno-format
MODULE_COMPILEFLAGS += -DFSI_EQOS_SUPPORT
MODULE_COMPILEFLAGS += -DOSI_DEBUG
MODULE_COMPILEFLAGS += -mgeneral-regs-only
#MODULE_COMPILEFLAGS += -DLOG_OSI
include make/module.mk
