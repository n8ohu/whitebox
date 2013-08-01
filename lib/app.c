/*
 * This is a user-space application that interacts with the radio
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gpio.h"
#include "adf4351.h"
#include "cmx991.h"

void radio_wr_byte(uint8_t byte) {
    int i, j;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(RADIO_CDATA, ((byte << i) & 0x80) ? 1 : 0);
        GPIO_set_output(RADIO_SCLK, 0);
        GPIO_set_output(RADIO_SCLK, 1);
    }
}

void radio_wr1(uint8_t address, uint8_t data) {
    GPIO_set_output(RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    radio_wr_byte(data);
    GPIO_set_output(RADIO_CSN, 1);
    GPIO_set_output(RADIO_SCLK, 0);
}

uint8_t radio_rd_byte() {
    uint8_t i;
    uint8_t byte = 0;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(RADIO_SCLK, 0);
        byte <<= 1;
        GPIO_set_output(RADIO_SCLK, 1);
        if (GPIO_get_input(RADIO_RDATA))
            byte |= 0x01;
    }
    return byte;
}

uint8_t radio_rd1(uint8_t address) {
    uint8_t value;
    GPIO_set_output(RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    value = radio_rd_byte();
    GPIO_set_output(RADIO_CSN, 1);
    GPIO_set_output(RADIO_SCLK, 0);
    return value;
}

void radio_init() {
    puts("radio_init");
    GPIO_config(RADIO_CSN, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_SCLK, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_CDATA, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_RDATA, GPIO_INPUT_MODE);
    GPIO_config(RADIO_RESETN, GPIO_OUTPUT_MODE);
    radio_wr1(0x10, 0x00);
}

void radio_power_down() {
    puts("radio_power_down");
    GPIO_set_output(RADIO_RESETN, 0);
}

void radio_power_up() {
    puts("radio_power_up");
    GPIO_set_output(RADIO_RESETN, 1);
    radio_wr1(0x10, 0x00);
}

void vco_init() {
    puts("vco_init");
    GPIO_config(VCO_LE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_PDB, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CLK, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_DATA, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_LD, GPIO_INPUT_MODE);
}

void vco_power_down() {
    puts("vco_power_down");
    GPIO_set_output(VCO_CE, 0);
    GPIO_set_output(VCO_PDB, 0);
}

void vco_power_up() {
    puts("vco_power_up");
    GPIO_set_output(VCO_CE, 1);
    GPIO_set_output(VCO_PDB, 1);
}

int rfpll_locked() {
    return GPIO_get_input(VCO_LD);
}


void vco_dial(uint32_t data) {
    int i;
    puts("vco_dial");
    // Setup
    GPIO_set_output(VCO_LE, 1);
    GPIO_set_output(VCO_CLK, 0);

    // Bring LE low to start writing
    GPIO_set_output(VCO_LE, 0);

    for (i = 0; i < 32; ++i) {
        // Write Data
        GPIO_set_output(VCO_DATA, ((data << i) & 0x80000000) ? 1: 0);
        // Bring clock high
        GPIO_set_output(VCO_CLK, 1);
        // Bring clock low
        GPIO_set_output(VCO_CLK, 0);
    }

    // Bring LE high to write register
    GPIO_set_output(VCO_LE, 1);

    for (i = 0; i < 10000; ++i) {}
}

void dac_init() {
    puts("dac_init");
    GPIO_config(DAC_CS, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_PD, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_EN, GPIO_OUTPUT_MODE);
}

void dac_power_down() {
    puts("dac_power_down");
    GPIO_set_output(DAC_PD, 1);
    GPIO_set_output(DAC_CS, 1);
}

void dac_power_up() {
    puts("dac_power_up");
    GPIO_set_output(DAC_EN, 0);
    GPIO_set_output(DAC_PD, 0);
    GPIO_set_output(DAC_CS, 1);
}

void dac_tx() {
    puts("dac_tx");
    GPIO_set_output(DAC_EN, 1);
    GPIO_set_output(DAC_CS, 0);
    sleep(10);
    GPIO_set_output(DAC_CS, 1);
    GPIO_set_output(DAC_EN, 0);
}

void adc_init() {
    puts("adc_init");
    GPIO_config(ADC_S1, GPIO_OUTPUT_MODE);
    GPIO_config(ADC_S2, GPIO_OUTPUT_MODE);
    GPIO_config(ADC_DFS, GPIO_OUTPUT_MODE);
}

void adc_power_down() {
    puts("adc_power_down");
    GPIO_set_output(ADC_S1, 0);
    GPIO_set_output(ADC_S2, 0);
}

void adc_power_up() {
    puts("adc_power_up");
    GPIO_set_output(ADC_S1, 1);
    GPIO_set_output(ADC_S2, 0);
    GPIO_set_output(ADC_DFS, 1); // TWO's COMPLEMENT
}

int main(int argc, char **argv) {
	char * app_name = argv[0];
	char * dev_name = "/dev/whitebox";
	int ret = -1;
	int c, x;

    cmx991_t cmx991;
    cmx991_init(&cmx991);

    if (argc == 1) {
        fprintf(stderr, "You must submit a command!\n");
        ret = 1;
        goto Done;
    }

    if(strcmp(argv[1], "init") == 0) {
        vco_init();
        radio_init();
        dac_init();
        adc_init();
    }

    if(strcmp(argv[1], "power_down") == 0) {
        fprintf(stdout, "Powering down everything...\n");
        vco_power_down();
        radio_power_down();
        dac_power_down();
        adc_power_down();
    }

    if(strcmp(argv[1], "power_up") == 0) {
        fprintf(stdout, "Powering up everything...\n");
        vco_power_up();
        dac_power_up();
        radio_power_up();
        //adc_power_up();
    }

    if(strcmp(argv[1], "dial") == 0) {
        fprintf(stdout, "Dialing VCO...\n");

        adf4351_t adf4351;
        adf4351_init(&adf4351);
        adf4351.charge_pump_current = CHARGE_PUMP_CURRENT_2_50MA;
        adf4351.muxout = MUXOUT_DLD;
        adf4351.rf_output_enable = RF_OUTPUT_ENABLE_ENABLED;
        adf4351.aux_output_power = AUX_OUTPUT_POWER_5DBM;
        adf4351.aux_output_enable = AUX_OUTPUT_ENABLE_ENABLED;
        adf4351.aux_output_select = AUX_OUTPUT_SELECT_DIVIDED;
        adf4351_tune(&adf4351, 198.000e6);
        adf4351_print_to_file(&adf4351, stdout);
        vco_dial(adf4351_pack(&adf4351, 5));
        vco_dial(adf4351_pack(&adf4351, 4));
        vco_dial(adf4351_pack(&adf4351, 3));
        vco_dial(adf4351_pack(&adf4351, 2));
        vco_dial(adf4351_pack(&adf4351, 1));
        vco_dial(adf4351_pack(&adf4351, 0));
        printf("Acutal frequency: %f", adf4351_actual_frequency(&adf4351));
    }

    if(strcmp(argv[1], "tx_tune") == 0) {
        fprintf(stdout, "Resuming...\n");
        cmx991_resume(&cmx991);
        fprintf(stdout, "PLL on...\n");
        if (cmx991_pll_enable_m_n(&cmx991, 19.2e6, 192, 1800) < 0) {
            fprintf(stderr, "Error setting the pll\n");
        }
        fprintf(stdout, "TX Tune...\n");
        cmx991_tx_tune(&cmx991, 198.00e6, IF_FILTER_BW_120MHZ, HI_LO_HIGHER,
            TX_RF_DIV_BY_2, TX_IF_DIV_BY_4, GAIN_P6DB);
        cmx991.iq_out = IQ_OUT_IFOUT;
        cmx991_print_to_file(&cmx991, stdout);

        radio_wr1(0x11, cmx991_pack(&cmx991, 0x11));
        fprintf(stdout, "read 0x11 %02x\n", radio_rd1(0xe1));
        radio_wr1(0x12, cmx991_pack(&cmx991, 0x12));
        fprintf(stdout, "read 0x12 %02x\n", radio_rd1(0xe2));
        radio_wr1(0x13, cmx991_pack(&cmx991, 0x13));
        fprintf(stdout, "read 0x13 %02x\n", radio_rd1(0xe3));
        radio_wr1(0x14, cmx991_pack(&cmx991, 0x14));
        fprintf(stdout, "read 0x14 %02x\n", radio_rd1(0xe4));
        radio_wr1(0x15, cmx991_pack(&cmx991, 0x15));
        fprintf(stdout, "read 0x15 %02x\n", radio_rd1(0xe5));
        radio_wr1(0x16, cmx991_pack(&cmx991, 0x16));
        fprintf(stdout, "read 0x16 %02x\n", radio_rd1(0xe6));
        radio_wr1(0x20, cmx991_pack(&cmx991, 0x20));
        fprintf(stdout, "read 0x20 %02x\n", radio_rd1(0xd0));
        radio_wr1(0x21, cmx991_pack(&cmx991, 0x21));
        fprintf(stdout, "read 0x21 %02x\n", radio_rd1(0xd1));
        radio_wr1(0x22, cmx991_pack(&cmx991, 0x22));
        fprintf(stdout, "read 0x22 %02x\n", radio_rd1(0xd2));
        radio_wr1(0x23, cmx991_pack(&cmx991, 0x23));
        fprintf(stdout, "read 0x23 %02x\n", radio_rd1(0xd3));

        fprintf(stdout, "Acutal PLL Frequency %0.2f\n",
            cmx991_pll_actual_frequency(&cmx991, 19.2e6));
    }

    if(strcmp(argv[1], "tx") == 0) {
        cmx991_load(&cmx991, 0x21, radio_rd1(0xd1));
        if (!cmx991_pll_locked(&cmx991)) {
            fprintf(stdout, "IF not locked!\n");
        } else {
            fprintf(stdout, "IF locked, transmitting\n");
            dac_tx();
        }
    }

	/*
 	 * If we are here, we have been successful
 	 */
	ret = 0;

Done:
	return ret;
}
