/*
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/immap_85xx.h>
#include <asm/io.h>
#include <miiphy.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fsl_mdio.h>
#include <tsec.h>
#include <mmc.h>
#include <netdev.h>
#include <pci.h>
#include <asm/fsl_serdes.h>
#include <asm/fsl_ifc.h>
#include <asm/fsl_pci.h>
#include <hwconfig.h>
#include <i2c.h>

DECLARE_GLOBAL_DATA_PTR;

#define GPIO4_PCIE_RESET_SET		0x08000000
#define MUX_CPLD_CAN_UART		0x00
#define MUX_CPLD_TDM			0x01
#define MUX_CPLD_SPICS0_FLASH		0x00
#define MUX_CPLD_SPICS0_SLIC		0x02
#define PMUXCR1_IFC_MASK	0x00ffff00
#define PMUXCR1_SDHC_MASK	0x00fff000
#define PMUXCR1_SDHC_ENABLE	0x00555000
#define PMUXCR1_ULPI_MASK	0x00ffffc0
#define PMUXCR1_ULPI_ENABLE	0x00aaaa80

enum {
	MUX_TYPE_IFC,
	MUX_TYPE_SDHC,
	MUX_TYPE_ULPI,
};

uint pin_mux;

#ifndef CONFIG_SDCARD
struct cpld_data {
	u8 cpld_ver; /* cpld revision */
	u8 pcba_ver; /* pcb revision number */
	u8 twindie_ddr3;
	u8 res1[6];
	u8 bank_sel; /* NOR Flash bank */
	u8 res2[5];
	u8 usb2_sel;
	u8 res3[1];
	u8 porsw_sel;
	u8 tdm_can_sel;
	u8 spi_cs0_sel; /* SPI CS0 SLIC/SPI Flash */
	u8 por0; /* POR Options */
	u8 por1; /* POR Options */
	u8 por2; /* POR Options */
	u8 por3; /* POR Options */
};

void cpld_show(void)
{
	struct cpld_data *cpld_data = (void *)(CONFIG_SYS_CPLD_BASE);

	printf("CPLD: V%x.%x PCBA: V%x.0\n",
		in_8(&cpld_data->cpld_ver) & 0xF0,
		in_8(&cpld_data->cpld_ver) & 0x0F,
		in_8(&cpld_data->pcba_ver) & 0x0F);

#ifdef CONFIG_DEBUG
	printf("twindie_ddr =%x\n",
		in_8(&cpld_data->twindie_ddr3));
	printf("bank_sel =%x\n",
		in_8(&cpld_data->bank_sel));
	printf("usb2_sel =%x\n",
		in_8(&cpld_data->usb2_sel));
	printf("porsw_sel =%x\n",
		in_8(&cpld_data->porsw_sel));
	printf("tdm_can_sel =%x\n",
		in_8(&cpld_data->tdm_can_sel));
	printf("tdm_can_sel =%x\n",
		in_8(&cpld_data->tdm_can_sel));
	printf("spi_cs0_sel =%x\n",
		in_8(&cpld_data->spi_cs0_sel));
	printf("bcsr0 =%x\n",
		in_8(&cpld_data->bcsr0));
	printf("bcsr1 =%x\n",
		in_8(&cpld_data->bcsr1));
	printf("bcsr2 =%x\n",
		in_8(&cpld_data->bcsr2));
	printf("bcsr3 =%x\n",
		in_8(&cpld_data->bcsr3));
#endif
}
#endif

int board_early_init_f(void)
{
	ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
#ifndef CONFIG_SDCARD
	struct fsl_ifc *ifc = (void *)CONFIG_SYS_IFC_ADDR;

	/* Clock configuration to access CPLD using IFC(GPCM) */
	setbits_be32(&ifc->ifc_gcr, 1 << IFC_GCR_TBCTL_TRN_TIME_SHIFT);
#endif
	/*
	* Reset PCIe slots via GPIO4
	*/
	setbits_be32(&pgpio->gpdir, GPIO4_PCIE_RESET_SET);
	setbits_be32(&pgpio->gpdat, GPIO4_PCIE_RESET_SET);

	return 0;
}

int board_early_init_r(void)
{
#ifndef CONFIG_SDCARD
	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = find_tlb_idx((void *)flashbase, 1);

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	/* invalidate existing TLB entry for flash */
	disable_tlb(flash_esel);

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
			0, flash_esel, BOOKE_PAGESZ_16M, 1);

	set_tlb(1, flashbase + 0x1000000,
			CONFIG_SYS_FLASH_BASE_PHYS + 0x1000000,
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
			0, flash_esel+1, BOOKE_PAGESZ_16M, 1);
#endif
	return 0;
}

#ifdef CONFIG_PCI
void pci_init_board(void)
{
	fsl_pcie_init_board(0);
}
#endif /* ifdef CONFIG_PCI */

int checkboard(void)
{
	struct cpu_type *cpu;

	cpu = gd->cpu;
	printf("Board: %sRDB\n", cpu->name);

	return 0;
}

#ifdef CONFIG_TSEC_ENET
int board_eth_init(bd_t *bis)
{
	struct fsl_pq_mdio_info mdio_info;
	struct tsec_info_struct tsec_info[4];
	struct cpu_type *cpu;
	int num = 0;

	cpu = gd->cpu;

#ifdef CONFIG_TSEC1
	SET_STD_TSEC_INFO(tsec_info[num], 1);
	num++;
#endif
#ifdef CONFIG_TSEC2
	SET_STD_TSEC_INFO(tsec_info[num], 2);
	num++;
#endif
#ifdef CONFIG_TSEC3
	/* P1014 and it's derivatives do not support eTSEC3 */
	if (cpu->soc_ver != SVR_P1014) {
		SET_STD_TSEC_INFO(tsec_info[num], 3);
		num++;
	}
#endif
	if (!num) {
		printf("No TSECs initialized\n");
		return 0;
	}

	mdio_info.regs = (struct tsec_mii_mng *)CONFIG_SYS_MDIO_BASE_ADDR;
	mdio_info.name = DEFAULT_MII_NAME;

	fsl_pq_mdio_init(bis, &mdio_info);

	tsec_eth_init(bis, tsec_info, num);

	return pci_eth_init(bis);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
void fdt_del_flexcan(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
				"fsl,p1010-flexcan")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_del_spi_flash(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
				"spansion,s25sl12801")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_del_spi_slic(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
				"zarlink,le88266")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_del_tdm(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
				"fsl,starlite-tdm")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_del_sdhc(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
			"fsl,esdhc")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_del_ifc(void *blob)
{
	int nodeoff = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(blob, 0,
			"fsl,ifc")) >= 0) {
		fdt_del_node(blob, nodeoff);
	}
}

void fdt_disable_uart1(void *blob)
{
	int nodeoff;

	nodeoff = fdt_node_offset_by_compat_reg(blob, "fsl,ns16550",
					CONFIG_SYS_NS16550_COM2);

	if (nodeoff > 0) {
		fdt_status_disabled(blob, nodeoff);
	} else {
		printf("WARNING unable to set status for fsl,ns16550 "
			"uart1: %s\n", fdt_strerror(nodeoff));
	}
}

void ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;
	struct cpu_type *cpu;

	cpu = gd->cpu;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

#if defined(CONFIG_PCI)
	FT_FSL_PCI_SETUP;
#endif

	fdt_fixup_memory(blob, (u64)base, (u64)size);

#if defined(CONFIG_HAS_FSL_DR_USB)
	fdt_fixup_dr_usb(blob, bd);
#endif

       /* P1014 and it's derivatives don't support CAN and eTSEC3 */
	if (cpu->soc_ver == SVR_P1014) {
		fdt_del_flexcan(blob);
		fdt_del_node_and_alias(blob, "ethernet2");
	}

	/* Delete IFC node as IFC pins are multiplexing with SDHC */
	if (pin_mux != MUX_TYPE_IFC)
		fdt_del_ifc(blob);
	else
		fdt_del_sdhc(blob);

	if (hwconfig_subarg_cmp("fsl_p1010mux", "tdm_can", "can")) {
		fdt_del_tdm(blob);
		fdt_del_spi_slic(blob);
	} else if (hwconfig_subarg_cmp("fsl_p1010mux", "tdm_can", "tdm")) {
		fdt_del_flexcan(blob);
		fdt_del_spi_flash(blob);
		fdt_disable_uart1(blob);
	} else {
		/*
		 * If we don't set fsl_p1010mux:tdm_can to "can" or "tdm"
		 * explicitly, defaultly spi_cs_sel to spi-flash instead of
		 * to tdm/slic.
		 */
		fdt_del_tdm(blob);
		fdt_del_flexcan(blob);
		fdt_disable_uart1(blob);
	}
}
#endif

int config_pin_mux(int if_type)
{
	ccsr_gur_t __iomem *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);
	u8 tmp;
	uint orig_bus = i2c_get_bus_num();

	i2c_set_bus_num(CONFIG_SYS_PCA9557_BUS_NUM);
	tmp = 0xf0;
	if (i2c_write(CONFIG_SYS_I2C_PCA9557_ADDR, 3, 1, &tmp, 1) < 0)
		goto wr_err;

	switch (if_type) {
	case MUX_TYPE_IFC:
		tmp = 0x01;
		if (i2c_write(CONFIG_SYS_I2C_PCA9557_ADDR, 1, 1, &tmp, 1) < 0)
			goto wr_err;
		pin_mux = MUX_TYPE_IFC;
		clrbits_be32(&gur->pmuxcr, PMUXCR1_IFC_MASK);
		debug("pin mux is configured for IFC, SDHC disabled\n");
		break;
	case MUX_TYPE_SDHC:
		tmp = 0x05;
		if (i2c_write(CONFIG_SYS_I2C_PCA9557_ADDR, 1, 1, &tmp, 1) < 0)
			goto wr_err;
		pin_mux = MUX_TYPE_SDHC;
		clrsetbits_be32(&gur->pmuxcr, PMUXCR1_SDHC_MASK,
							PMUXCR1_SDHC_ENABLE);
		debug("pin mux is configured for SDHC, IFC disabled\n");
		break;
	case MUX_TYPE_ULPI:
		tmp = 0x06;
		if (i2c_write(CONFIG_SYS_I2C_PCA9557_ADDR, 1, 1, &tmp, 1) < 0)
			goto wr_err;
		pin_mux = MUX_TYPE_ULPI;
		clrsetbits_be32(&gur->pmuxcr, PMUXCR1_ULPI_MASK,
							PMUXCR1_ULPI_ENABLE);
		debug("pin mux is configured for ULPI, IFC & SDHC disabled\n");
		break;
	default:
		printf("wrong mux interface type\n");
		i2c_set_bus_num(orig_bus);
		return -1;
	}
	i2c_set_bus_num(orig_bus);
	return 0;
wr_err:
	printf("pca9557: i2c write failed\n");
	i2c_set_bus_num(orig_bus);
	return -1;
}

void board_reset(void)
{
	/* mux to IFC to enable CPLD for reset */
	if (pin_mux != MUX_TYPE_IFC)
		config_pin_mux(MUX_TYPE_IFC);
}

int misc_init_r(void)
{
#ifndef CONFIG_SDCARD
	struct cpld_data *cpld_data = (void *)(CONFIG_SYS_CPLD_BASE);
	ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);

	if (hwconfig_subarg_cmp("fsl_p1010mux", "tdm_can", "can")) {
		clrbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_CAN1_TDM |
				MPC85xx_PMUXCR_CAN1_UART |
				MPC85xx_PMUXCR_CAN2_TDM |
				MPC85xx_PMUXCR_CAN2_UART);
		out_8(&cpld_data->tdm_can_sel, MUX_CPLD_CAN_UART);
	} else if (hwconfig_subarg_cmp("fsl_p1010mux", "tdm_can", "tdm")) {
		clrbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_CAN2_UART |
				MPC85xx_PMUXCR_CAN1_UART);
		setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_CAN2_TDM |
				MPC85xx_PMUXCR_CAN1_TDM);
		clrbits_be32(&gur->pmuxcr2, MPC85xx_PMUXCR2_UART_GPIO);
		setbits_be32(&gur->pmuxcr2, MPC85xx_PMUXCR2_UART_TDM);
		out_8(&cpld_data->tdm_can_sel, MUX_CPLD_TDM);
		out_8(&cpld_data->spi_cs0_sel, MUX_CPLD_SPICS0_SLIC);
	} else {
		/* defaultly spi_cs_sel to flash */
		out_8(&cpld_data->spi_cs0_sel, MUX_CPLD_SPICS0_FLASH);
	}

	if (hwconfig("esdhc")) {
		pin_mux = MUX_TYPE_SDHC;
		config_pin_mux(MUX_TYPE_SDHC);
	}
#else
	pin_mux = MUX_TYPE_SDHC;
#endif
	return 0;
}

static int pin_mux_cmd(cmd_tbl_t *cmdtp, int flag, int argc,
					char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	if (strcmp(argv[1], "ifc") == 0)
		config_pin_mux(MUX_TYPE_IFC);
	else if (strcmp(argv[1], "sdhc") == 0)
		config_pin_mux(MUX_TYPE_SDHC);
	else if (strcmp(argv[1], "ulpi") == 0)
		config_pin_mux(MUX_TYPE_ULPI);
	else
		return CMD_RET_USAGE;
	return 0;
}

U_BOOT_CMD(
	mux, 2, 0, pin_mux_cmd,
	"configure multiplexing pin for IFC/SDHC/ULPI buses",
	"bus_type[ifc/sdhc/ulpi] (e.g. mux sdhc)"
);
