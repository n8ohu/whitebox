#ifndef __WHITEBOX_GPIO__
#define __WHITEBOX_GPIO__

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign.
 */
#define FPGA_GPIO_BASE 32
#define ADC_S2       (FPGA_GPIO_BASE+3)
#define ADC_S1       (FPGA_GPIO_BASE+4)
#define ADC_DFS      (FPGA_GPIO_BASE+5)
#define DAC_EN       (FPGA_GPIO_BASE+6)
#define DAC_PD       (FPGA_GPIO_BASE+7)
#define DAC_CS       (FPGA_GPIO_BASE+8)
#define RADIO_RESETN (FPGA_GPIO_BASE+9)
#define RADIO_CDATA  (FPGA_GPIO_BASE+10)
#define RADIO_SCLK   (FPGA_GPIO_BASE+11)
#define RADIO_RDATA  (FPGA_GPIO_BASE+12)
#define RADIO_CSN    (FPGA_GPIO_BASE+13)
#define VCO_CLK      (FPGA_GPIO_BASE+14)
#define VCO_DATA     (FPGA_GPIO_BASE+15)
#define VCO_LE       (FPGA_GPIO_BASE+16)
#define VCO_CE       (FPGA_GPIO_BASE+17)
#define VCO_PDB      (FPGA_GPIO_BASE+18)
#define VCO_LD       (FPGA_GPIO_BASE+19)

#define GPIO_OUTPUT_MODE (0 << 0)
#define GPIO_INPUT_MODE  (1 << 0)

void GPIO_config(unsigned gpio, int inout);
void GPIO_set_output(unsigned gpio, unsigned value);
int GPIO_get_input(unsigned gpio);

#endif /* __WHITEBOX_GPIO__ */
