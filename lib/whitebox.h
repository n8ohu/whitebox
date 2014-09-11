#ifndef __WHITEBOX_H__
#define __WHITEBOX_H__

#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>

#include "whitebox_ioctl.h"

#include "cmx991.h"
#include "adf4351.h"
#include "dsp.h"

typedef struct whitebox {
    int fd;
    cmx991_t cmx991;
    adf4351_t adf4351;

    int rate;
    int interp;
    float frequency;

    void *user_buffer;
    unsigned long user_buffer_size;
} whitebox_t;

int whitebox_parameter_set(const char *param, int value);
int whitebox_parameter_get(const char *param);

void whitebox_init(whitebox_t* wb);
whitebox_t* whitebox_alloc(void);
void whitebox_free(whitebox_t* wb);
int whitebox_open(whitebox_t* wb, const char* filn, int flags, int rate);
int whitebox_mmap(whitebox_t* wb);
int whitebox_munmap(whitebox_t* wb);
int whitebox_fd(whitebox_t* wb);
int whitebox_close(whitebox_t* wb);
void whitebox_debug_to_file(whitebox_t* wb, FILE* f);
int whitebox_reset(whitebox_t* wb);
unsigned int whitebox_status(whitebox_t* wb);
int whitebox_plls_locked(whitebox_t* wb);

int whitebox_tx_clear(whitebox_t* wb);
int whitebox_tx(whitebox_t* wb, float frequency);
int whitebox_tx_fine_tune(whitebox_t* wb, float frequency);
int whitebox_tx_standby(whitebox_t* wb);

int whitebox_tx_set_interp(whitebox_t* wb, uint32_t interp);
int whitebox_tx_set_buffer_threshold(whitebox_t* wb, uint16_t aeval, uint16_t afval);
void whitebox_tx_get_buffer_threshold(whitebox_t *wb, uint16_t *aeval, uint16_t *afval);
int whitebox_tx_get_buffer_runs(whitebox_t* wb, uint16_t* overruns, uint16_t* underruns);
int whitebox_tx_set_latency(whitebox_t *wb, int ms);
int whitebox_tx_get_latency(whitebox_t *wb);

void whitebox_tx_flags_enable(whitebox_t* wb, uint32_t flags);
void whitebox_tx_flags_disable(whitebox_t* wb, uint32_t flags);

void whitebox_tx_dds_enable(whitebox_t* wb, float fdes);

void whitebox_tx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q);
void whitebox_tx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q);

int whitebox_tx_set_gain(whitebox_t *wb, float gain_i, float gain_q);
int whitebox_tx_get_gain(whitebox_t *wb, float *gain_i, float *gain_q);


int whitebox_rx_clear(whitebox_t* wb);
int whitebox_rx(whitebox_t* wb, float frequency);
int whitebox_rx_fine_tune(whitebox_t* wb, float frequency);
int whitebox_rx_standby(whitebox_t* wb);
int whitebox_rx_cal_enable(whitebox_t *wb);
int whitebox_rx_cal_disable(whitebox_t *wb);

int whitebox_rx_set_decim(whitebox_t* wb, uint32_t decim);
int whitebox_rx_set_latency(whitebox_t *wb, int ms);
int whitebox_rx_get_latency(whitebox_t *wb);

void whitebox_rx_flags_enable(whitebox_t* wb, uint32_t flags);
void whitebox_rx_flags_disable(whitebox_t* wb, uint32_t flags);

void whitebox_rx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q);
void whitebox_rx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q);


/* CAT CONTROL */

// AG Command
void whitebox_set_audio_gain(struct whitebox *wb, uint8_t gain);
uint8_t whitebox_get_audio_gain(struct whitebox *wb);

// AN
void whitebox_set_antenna_connector(struct whitebox *wb, uint8_t ant);
uint8_t whitebox_get_antenna_connector(struct whitebox *wb);

// BD, BU
void whitebox_band_down(struct whitebox *wb);
void whitebox_band_up(struct whitebox *wb);

// BY - Returns 0 if not busy, 1 if busy
uint8_t whitebox_busy(struct whitebox *wb);

// CH; direction is 0 for down, 1 for up
void whitebox_change_channel(struct whitebox *wb, uint8_t direction);

// CT, CN
void whitebox_set_ctts(struct whitebox *wb, uint8_t enabled);
uint8_t whitebox_get_ctts(struct whitebox *wb);

void whitebox_set_ctts_tone(struct whitebox *wb, uint8_t number);
uint8_t whitebox_get_ctts_tone(struct whitebox *wb);

// DN, UP
void whitebox_mic_down(struct whitebox *wb, uint8_t count);
void whitebox_mic_up(struct whitebox *wb, uint8_t count);

// FA, FB
void whitebox_set_vfoa(struct whitebox *wb, uint32_t freq);
uint32_t whitebox_set_vfoa(struct whitebox *wb);
void whitebox_set_vfob(struct whitebox *wb, uint32_t freq);
uint32_t whitebox_set_vfob(struct whitebox *wb);

// FR, 0=vfoa, 1=vfob, 2=m.ch
void whitebox_set_receiver_vfo(struct whitebox *wb, uint8_t vfo);
uint8_t whitebox_set_receiver_vfo(struct whitebox *wb);

// FT, 0=vfoa, 1=vfob, 2=m.ch
void whitebox_set_transmitter_vfo(struct whitebox *wb, uint8_t vfo);
uint8_t whitebox_set_transmitter_vfo(struct whitebox *wb);

// FS
void whitebox_set_fine_tuning(struct whitebox *wb, uint8_t enable);
uint8_t whitebox_set_fine_tuning(struct whitebox *wb);

// GT
void whitebox_set_agc(struct whitebox *wb, uint16_t agc);
uint16_t whitebox_get_agc(struct whitebox *wb);

// ID
uint16_t whitebox_get_id(struct whitebox *wb);

// IS
void whitebox_set_if_shift(struct whitebox *wb, int16_t shift);
int16_t whitebox_get_if_shift(struct whitebox *wb);

// KS
void whitebox_set_electric_keyer_speed(struct whitebox *wb, uint8_t wpm);
uint8_t whitebox_set_electric_keyer_speed(struct whitebox *wb);

// KY
uint8_t whitebox_set_morse_buffer(struct whitebox *wb, char *message);
uint8_t whitebox_get_morse_buffer_available(struct whitebox *wb);

// LK
void whitebox_set_key_lock(struct whitebox *wb, uint8_t frequency_lock, uint8_t tuning_control_lock);
voi whitebox_get_key_lock(struct whitebox *wb, uint8_t *frequency_lock, uint8_t *tuning_control_lock);

// MD, modes - 0-IQ, 1-LSB, 2-USB, 3-CW, 4-FM, 5-AM, 6-FSK, 7-CWR, 8-Tune, 9-FSR
void whitebox_set_mode(struct whitebox *wb, uint8_t mode);
uint8_t whitebox_get_mode(struct whitebox *wb);

// MG
void whitebox_set_mic_gain(struct whitebox *wb, uint8_t gain);
uint8_t whitebox_get_mic_gain(struct whitebox *wb);

// ML - tx monitor; loopback transmit audio back to the receive audio
void whitebox_set_tx_monitor_level(struct whitebox *wb, uint8_t level);
uint8_t whitebox_get_tx_monitor_level(struct whitebox *wb);

// PA
void whitbox_set_power_amplifier_status(struct whitebox *wb, uint8_t status);
uint8_t whitebox_get_power_amplifier_status(struct whitebox *wb);

// PC
void whitebox_set_output_power(struct whitebox *wb, uint8_t power);
uint8_t whitebox_get_output_power(struct whitebox *wb);

// PS
void whitebox_set_power_status(struct whitebox *wb, uint8_t status);
uint8_t whitebox_set_power_status(struct whitebox *wb);

// RA
void whitebox_set_attenuator(struct whitebox *wb, uint8_t attenuation);
uint8_t whitebox_set_attenuator(struct whitebox *wb);

// RG
void whitebox_set_rf_gain(struct whitebox *wb, uint8_t gain);
uint8_t whitebox_get_rf_gain(struct whitebox *wb);

// RM
void whitebox_set_meter_function(struct whitebox *wb, uint8_t function);
uint8_t whitebox_get_meter_function(struct whitebox *wb);

#endif /* __WHITEBOX_H__ */
