#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <poll.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <regex.h>
#include "whitebox.h"
#include "whiteboxd.h"

#define PORT 11287
#define FRAME_SIZE 512

#define NUM_WEAVER_TAPS 109
int16_t weaver_fir_taps[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, -1, 0, 1, 0, -1, 0, 2, 0, -2, 0, 2, 0, -3, 0, 3, 0, -4, 0, 4, 0, -5, 0, 6, 0, -6, 0, 8, 0, -9, 0, 11, 0, -13, 0, 16, 0, -21, 0, 31, 0, -52, 0, 157, 247, 157, 0, -52, 0, 31, 0, -21, 0, 16, 0, -13, 0, 11, 0, -9, 0, 8, 0, -6, 0, 6, 0, -5, 0, 4, 0, -4, 0, 3, 0, -3, 0, 2, 0, -2, 0, 2, 0, -1, 0, 1, 0, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int done;

float diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1e9+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return (temp.tv_sec * 1e9 + temp.tv_nsec) / 1e9 * 1e3;
}

char *whitebox_mode_to_string(enum whitebox_mode mode)
{
    if (mode & WBM_IQ_FD)
        return "iq";
    else if (mode & WBM_IQ_TONE)
        return "cw";
    else if (mode & WBM_IQ_TEST)
        return "test";
    else if (mode & WBM_AUDIO_TONE)
        return "2tone";
    else if (mode & WBM_AUDIO_FD)
        return "audio";
    else
        return "idle";
}

struct whitebox_modulator {
    char name[256];
    int (*modulate)(struct whitebox_source *iq_source,
            struct whitebox_source *audio_source);
};

struct whitebox_modulator modulators[] = {
/*    { "iq", modulate_iq },
    { "ssb", modulate_ssb },
    { "am", modulate_am },
    { "fm", modulate_fm },*/
    { 0, 0 },
};

struct whitebox_source_entry {
    char name[256];
    int (*alloc)(struct whitebox_source **source);
};

struct whitebox_source_entry audio_sources[] = {
/*    { "constant", whitebox_const },
    { "synth", whitebox_synth },
    { "socket", whitebox_socket },*/
    { 0, 0 },
};

struct whitebox_source_entry iq_sources[] = {
    { "lsb", whitebox_qsynth_lsb_alloc },
    { "usb", whitebox_qsynth_usb_alloc },
    { "constant", whitebox_qconst_alloc },
//    { "socket", whitebox_qsocket },
    { 0, 0 },
};

void whitebox_config_init(struct whitebox_config *config)
{
    config->mode = WBM_IDLE;
    config->debug = 0;

    config->verbose_flag = 0;
    config->sample_rate = 48e3;
    config->carrier_freq = 145.00e6;
    config->port = PORT;
    config->ctl_port = PORT + 10;
    config->audio_port = PORT + 20;
    config->duration = 0.00;

    config->z_filename = NULL;
    config->tone1 = 700;

    config->tone2 = 1900;
    config->tone2_offset = 0;
    
    config->dat_enable = 1;
    config->audio_enable = 1;
    config->ctl_enable = 1;
    config->httpd_enable = 1;

    strncpy(config->modulation, "iq", 255);
    strncpy(config->audio_source, "constant", 255);
    strncpy(config->iq_source, "constant", 255);
}

void whitebox_runtime_init(struct whitebox_runtime *rt)
{
    rt->fd = -1;
    rt->log_file = stderr;
    rt->i = 0;
    rt->ctl_fd = -1;
    rt->ctl_needs_poll = 0;
    rt->ctl_listening_fd = -1;
    rt->ctl_slen = sizeof(rt->ctl_sock_other);
    rt->dat_fd = -1;
    rt->dat_needs_poll = 0;
    rt->dat_listening_fd = -1;
    rt->dat_slen = sizeof(rt->dat_sock_other);
    rt->audio_fd = -1;
    rt->audio_needs_poll = 0;
    rt->audio_listening_fd = -1;
    rt->audio_slen = sizeof(rt->audio_sock_other);
    rt->tone1_fcw = 0;
    rt->tone1_phase = 0;
    rt->tone2_fcw = 0;
    rt->tone2_phase = 0;
    rt->global_re = -1;
    rt->global_im = -1;
    rt->ptt = 0;
    rt->ptl = 0;
    rt->rx_cal = 0;
    whitebox_qsynth_lsb_alloc(&rt->iq_source);
    whitebox_source_tune(rt->iq_source, 12.000e3);
}

int reconfigure_runtime(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    static ptt = 0;
    static ptl = 0;
    static enum whitebox_mode mode = WBM_IDLE;

    if (rt->ptt != ptt) {
        ptt = rt->ptt;
        if (ptt) {
            whitebox_tx(wb, config->carrier_freq);
        } else {
            fsync(wb->fd);
            whitebox_tx_standby(wb);
        }
        if (config->mode == WBM_IDLE) {
            change_mode(wb, config, rt, WBM_IQ_TONE);  // Web browser causes CW on PTT
        }
    }

    if (rt->ptl != ptl) {
        ptl = rt->ptl;
        if (ptl) {
            whitebox_rx(wb, config->carrier_freq);
        } else {
            fsync(wb->fd);
            whitebox_rx_standby(wb);
        }
    }

    if (ptt || ptl) {
        while (!whitebox_plls_locked(wb)) {
            if (done)
                exit(1);
            whitebox_log(WBL_WARN, "U");
        }
    }

    //whitebox_debug_to_file(wb, stdout);
    
    if (mode != config->mode) {
        mode = config->mode;
        if (config->verbose_flag)
            whitebox_log(WBL_INFO, "Changed mode to %d\n", mode);
        if (config->mode & WBM_IQ_MIXED) {
            whitebox_log(WBL_INFO, "fir on\n");
            whitebox_tx_flags_enable(wb, WS_FIREN);
        } else {
            whitebox_tx_flags_disable(wb, WS_FIREN);
        }

        if (config->mode & WBM_IQ_SOCKET) {
            //rt->ptt = 1;
        }
        if (config->mode & WBM_IQ_FILE) {
            if ((rt->dat_fd = open(config->z_filename, O_RDONLY | O_NONBLOCK)) < 0) {
                perror("open");
                return -1;
            }
        }
        if (config->mode & WBM_IQ_TONE) {
            //rt->tone1_fcw = freq_to_fcw(config->tone1, config->sample_rate);
            config_change(wb, config, rt, "tone1", "440");
            //rt->iq_source = &rt->bfo.source;
        }
        if (config->mode & WBM_AUDIO_TONE) {
            //rt->tone2_fcw = freq_to_fcw(config->tone2, config->sample_rate);
            config_change(wb, config, rt, "tone1", "12000");
            config_change(wb, config, rt, "tone2", "440");
        }
        if (config->mode & WBM_AUDIO_SOCKET) {
            config_change(wb, config, rt, "tone1", "12000");
            //rt->ptt = 1;
        }
    }

    if (config->verbose_flag > 2) {
        whitebox_log(WBL_INFO, "WBMODE %08x\n", config->mode);
        //whitebox_debug_to_file(wb, stdout);
    }
}

int change_mode(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, enum whitebox_mode mode)
{
    if (config->mode != mode) {
        config->mode = mode;
        whitebox_log(WBL_INFO, "Changing mode to %s\n", whitebox_mode_to_string(config->mode));
    }
    return 0;
}

int whitebox_start(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    struct timespec res;
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    static int16_t coeffs[WF_COEFF_COUNT];
    int i, n;
    const rlim_t stack_size = 4 * 1024 * 1024;
    struct rlimit rl;
    int result;

    dsp_init();
    whitebox_init(wb);
    if ((rt->fd = whitebox_open(wb, "/dev/whitebox", O_RDWR | O_NONBLOCK, config->sample_rate)) < 0) {
        perror("open");
        return -1;
    }
    if (whitebox_mmap(wb) < 0) {
        perror("mmap");
        return -1;
    }

    whitebox_fir_load_coeffs(wb, 0, NUM_WEAVER_TAPS, weaver_fir_taps);

    clock_getres(CLOCK_MONOTONIC, &res);
    whitebox_log(WBL_INFO, "Clock resolution: %f ms\n", (res.tv_sec * 1e9 + res.tv_nsec) / 1e6);
    result = getrlimit(RLIMIT_STACK, &rl);
    whitebox_log(WBL_INFO, "stack size:%d MB\n", rl.rlim_cur/1024/1024);

    whitebox_log(WBL_INFO, "sincos LUT size: %d kB, %d bits\n", DDS_RAM_SIZE_BITS >> 13, DDS_RAM_SIZE_BITS);
    n = whitebox_fir_get_coeffs(wb, 0, WF_COEFF_COUNT, &coeffs);
    whitebox_log(WBL_INFO, "FIR coefficients: [");
    for (i = 0; i < n-1; ++i)
        whitebox_log(WBL_INFO, "%d, ", coeffs[i]);
    whitebox_log(WBL_INFO, "%d]\n", coeffs[n-1]);
    //whitebox_debug_to_file(wb, stdout);

    if (config->ctl_enable) {
        /* Listen for incomming control packets */
        if ((rt->ctl_listening_fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==-1) {
            perror("socket");
            return -1;
        }
        setsockopt(rt->ctl_listening_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, optlen);
        memset((char *) &rt->ctl_sock_me, 0, sizeof(rt->ctl_sock_me));
        rt->ctl_sock_me.sin_family = AF_INET;
        rt->ctl_sock_me.sin_port = htons(config->ctl_port);
        rt->ctl_sock_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(rt->ctl_listening_fd, (struct sockaddr*)&rt->ctl_sock_me, sizeof(rt->ctl_sock_me))==-1) {
            perror("bind");
            return -1;
        }
        if (listen(rt->ctl_listening_fd, 0) == -1) {
            perror("listen");
            return -1;
        }
    }

    if (config->dat_enable) {
        if ((rt->dat_listening_fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==-1) {
            perror("socket");
            return -1;
        }
        setsockopt(rt->dat_listening_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, optlen);
        memset((char *) &rt->dat_sock_me, 0, sizeof(rt->dat_sock_me));
        rt->dat_sock_me.sin_family = AF_INET;
        rt->dat_sock_me.sin_port = htons(config->port);
        rt->dat_sock_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(rt->dat_listening_fd, (struct sockaddr*)&rt->dat_sock_me, sizeof(rt->dat_sock_me))==-1) {
            perror("bind");
            return -1;
        }
        if (listen(rt->dat_listening_fd, 0) == -1) {
            perror("listen");
            return -1;
        }
    }

    if (config->audio_enable) {
        if ((rt->audio_listening_fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==-1) {
            perror("socket");
            return -1;
        }
        setsockopt(rt->audio_listening_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&optval, optlen);
        memset((char *) &rt->audio_sock_me, 0, sizeof(rt->audio_sock_me));
        rt->audio_sock_me.sin_family = AF_INET;
        rt->audio_sock_me.sin_port = htons(config->audio_port);
        rt->audio_sock_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(rt->audio_listening_fd, (struct sockaddr*)&rt->audio_sock_me, sizeof(rt->audio_sock_me))==-1) {
            perror("bind");
            return -1;
        }
        if (listen(rt->audio_listening_fd, 0) == -1) {
            perror("listen");
            return -1;
        }
    }

    change_mode(wb, config, rt, config->mode);

    rt->latency_ms = whitebox_tx_get_latency(wb);

    return 0;
}

int x_read(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, int out_available, int16_t *x_dest)
{
    int in_available = 0;
    int16_t *x_dest_offset = x_dest;
    out_available >>= 1; // audio is 16-bit while IQ samples are 32-bit

    if (config->mode & WBM_AUDIO_SOCKET && rt->audio_needs_poll) {
        int recvd;
        while (out_available > 0) {
            if ((recvd = recvfrom(rt->audio_fd, (void*)x_dest_offset, out_available, 0, (struct sockaddr*)&rt->audio_sock_other, &rt->audio_slen)) < 0) {
                if (errno == EAGAIN) {
                    break;
                }
                perror("recvfrom");
                exit(1);
            }
            if ((recvd == 1 && *((char*)x_dest_offset) == '\0') || (recvd == 0))
                break;
            if (recvd % 2 != 0)
                whitebox_log(WBL_WARN, "not word aligned\n");
            //rt->ptt = 1;
            //printf("x"); fflush(stdout);
            in_available += recvd << 1;
            out_available -= recvd;
            x_dest_offset += recvd >> 1;
        }
        if ((recvd == 1 && *((char*)x_dest_offset) == '\0') || (recvd == 0)) {
            rt->audio_needs_poll = 0;
            //rt->ptt = 0;
            close(rt->audio_fd);
            rt->audio_fd = -1;
            change_mode(wb, config, rt, WBM_IDLE);
            whitebox_log(WBL_INFO, "Connection closed\n");
            return 0;
        }
    }
    return in_available;
}

int z_read(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, int out_available, uint32_t *z_dest)
{
    uint32_t *z_dest_offset;
    int in_available = 0;

    if (config->mode & WBM_IQ_SOCKET && rt->dat_needs_poll) {
        int recvd;
        z_dest_offset = z_dest;
        while (out_available > 0) {
            if ((recvd = recvfrom(rt->dat_fd, (void*)z_dest_offset, out_available, 0, (struct sockaddr*)&rt->dat_sock_other, &rt->dat_slen)) < 0) {
                if (errno == EAGAIN) {
                    break;
                }
                perror("recvfrom");
                exit(1);
            }
            if ((recvd == 1 && *((char*)z_dest_offset) == '\0') || (recvd == 0))
                break;
            if (recvd % 4 != 0)
                whitebox_log(WBL_WARN, "not word aligned\n");
            //rt->ptt = 1;
            //printf("z"); fflush(stdout);
            in_available += recvd;
            out_available -= recvd;
            z_dest_offset += recvd >> 2;
        }
        if ((recvd == 1 && *((char*)z_dest_offset) == '\0') || (recvd == 0)) {
            rt->dat_needs_poll = 0;
            //rt->ptt = 0;
            close(rt->dat_fd);
            rt->dat_fd = -1;
            change_mode(wb, config, rt, WBM_IDLE);
            whitebox_log(WBL_INFO, "Connection closed\n");
            return 0;
        }
    } else if (config->mode & WBM_IQ_FILE) {
        int recvd;
        if (!rt->ptt)
            return 0;
        z_dest_offset = z_dest;
        while (out_available > 0) {
            if ((recvd = read(rt->dat_fd, (void*)z_dest_offset, out_available)) < 0) {
                if (errno == EAGAIN) {
                    whitebox_log(WBL_INFO, "w");
                    break;
                }
                perror("read");
                exit(1);
            }
            if (recvd % 4 != 0)
                whitebox_log(WBL_ERROR, "not word aligned\n");
            if (recvd == 0)
                break;
            in_available += recvd;
            out_available -= recvd;
            z_dest_offset += recvd >> 2;
        }
        if (recvd == 0)
            return 0;
    } else if (config->mode & WBM_IQ_TONE || config->mode & WBM_IQ_MIXED) {
        if (!rt->ptt)
            return 0;
        in_available = whitebox_source_work(rt->iq_source, 0, 0, (unsigned long)z_dest, out_available);
    }
    return in_available;
}

void z_test(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, int in_available, uint32_t *z_dest)
{
    int n;
    int16_t re, im;
    for (n = 0; n < in_available >> 2; ++n) {
        QUAD_UNPACK(*(uint32_t*)(z_dest + n), re, im);
        if ((int16_t)(re - 1) != rt->global_re || (int16_t)(im - 1) != rt->global_im) {
            whitebox_log(WBL_ERROR, "missing samples %d, %d\n", rt->global_re, re);
            exit(1);
        }
        rt->global_re = re;
        rt->global_im = im;
    }
}

void mix_z_and_x(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, int in_available,
        int16_t *x_src, uint32_t *z_src,
        uint32_t *dest)
{
    if (config->mode & WBM_AUDIO_TONE) {
        int n;
        int16_t x, foo, re, im;
        for (n = 0; n < in_available >> 2; ++n) {
            QUAD_UNPACK(sincos16c(rt->tone2_fcw, &rt->tone2_phase), x, foo);
            QUAD_UNPACK(*(uint32_t*)(z_src + n), re, im);
            re = ((x * re) + (1 << 15)) >> 16;
            im = ((x * im) + (1 << 15)) >> 16;
            *((uint32_t*)(dest + n)) = QUAD_PACK(re, im);
        }
    } else if (config->mode & WBM_AUDIO_SOCKET) {
        int n;
        int16_t re, im;
        for (n = 0; n < in_available >> 2; ++n) {
            QUAD_UNPACK(*(uint32_t*)(z_src + n), re, im);
            re = ((x_src[n] * re) + (1 << 15)) >> 16;
            im = ((x_src[n] * im) + (1 << 15)) >> 16;
            *((uint32_t*)(dest + n)) = QUAD_PACK(re, im);
        }
    }
}

int z_write(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, int in_available, uint32_t *z_src)
{
    uint32_t *z_src_offset;
    int out_available = 0;

    int sent;
    z_src_offset = z_src;
    while (in_available > 0) {
        if ((sent = sendto(rt->dat_fd, (void*)z_src_offset, in_available, 0, (struct sockaddr*)&rt->dat_sock_other, rt->dat_slen)) < 0) {
            if (errno == EAGAIN) {
                break;
            }
            perror("sendto");
            exit(1);
        }
        out_available += sent;
        in_available -= sent;
        z_src_offset += sent >> 2;
    }

    return out_available;
}

int config_change(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt,
        const char *var, const char *val)
{
    int i;
    if (strncmp(var, "offset_correct_i", strlen("offset_correct_i")) == 0) {
        int16_t correct_i, correct_q;
        whitebox_tx_get_correction(wb, &correct_i, &correct_q);
        correct_i = atoi(val);
        whitebox_log(WBL_INFO, "correct_i=%d\n", correct_i);
        whitebox_tx_set_correction(wb, correct_i, correct_q);
    } else if (strncmp(var, "offset_correct_q", strlen("offset_correct_q")) == 0) {
        int16_t correct_i, correct_q;
        whitebox_tx_get_correction(wb, &correct_i, &correct_q);
        correct_q = atoi(val);
        whitebox_log(WBL_INFO, "correct_q=%d\n", correct_q);
        whitebox_tx_set_correction(wb, correct_i, correct_q);

    } else if (strncmp(var, "gain_i", strlen("gain_i")) == 0) {
        float gain_i, gain_q;
        whitebox_tx_get_gain(wb, &gain_i, &gain_q);
        gain_i = atof(val);
        whitebox_log(WBL_INFO, "gain_i=%.2f\n", gain_i);
        whitebox_tx_set_gain(wb, gain_i, gain_q);
    } else if (strncmp(var, "gain_q", strlen("gain_q")) == 0) {
        float gain_i, gain_q;
        whitebox_tx_get_gain(wb, &gain_i, &gain_q);
        gain_q = atof(val);
        whitebox_log(WBL_INFO, "gain_q=%.2f\n", gain_q);
        whitebox_tx_set_gain(wb, gain_i, gain_q);
    } else if (strncmp(var, "tone1", strlen("tone1")) == 0) {
        config->tone1 = atof(val);
        whitebox_log(WBL_INFO, "tone1=%.3f kHz\n", config->tone1 / 1e3);
        rt->tone1_fcw = freq_to_fcw(config->tone1, config->sample_rate);
    } else if (strncmp(var, "tone2", strlen("tone2")) == 0) {
        config->tone2 = atof(val);
        whitebox_log(WBL_INFO, "tone2=%.3f kHz\n", config->tone2 / 1e3);
        rt->tone2_fcw = freq_to_fcw(config->tone2, config->sample_rate);
    } else if (strncmp(var, "freq", strlen("freq")) == 0) {
        config->carrier_freq = atof(val) * 1e6;
        whitebox_log(WBL_INFO, "carrier_freq=%.3f MHz\n", config->carrier_freq / 1e6);
        whitebox_tx_fine_tune(wb, config->carrier_freq);
    } else if (strncmp(var, "ptt", strlen("ptt")) == 0) {
        rt->ptt = atoi(val);
        whitebox_log(WBL_INFO, "ptt=%d\n", atoi(val));
    } else if (strncmp(var, "ptl", strlen("ptl")) == 0) {
        rt->ptl = atoi(val);
        whitebox_log(WBL_INFO, "ptl=%d\n", atoi(val));
    } else if (strncmp(var, "rx_cal", strlen("rx_cal")) == 0) {
        //whitebox_args_t w;
        rt->rx_cal = atoi(val);
        whitebox_log(WBL_INFO, "rx_cal=%d\n", atoi(val));
        if (rt->rx_cal) {
            whitebox_rx_cal_enable(wb);
        } else {
            whitebox_rx_cal_disable(wb);
        }
        /*while (!whitebox_plls_locked(wb)) {
            printf("L");
            fflush(stdout);
        }*/
    } else if (strncmp(var, "rx_offset_correct_i", strlen("rx_offset_correct_i")) == 0) {
        int16_t correct_i, correct_q;
        whitebox_rx_get_correction(wb, &correct_i, &correct_q);
        correct_i = atoi(val);
        whitebox_log(WBL_INFO, "rx_correct_i=%d\n", correct_i);
        whitebox_rx_set_correction(wb, correct_i, correct_q);
    } else if (strncmp(var, "rx_offset_correct_q", strlen("rx_offset_correct_q")) == 0) {
        int16_t correct_i, correct_q;
        whitebox_rx_get_correction(wb, &correct_i, &correct_q);
        correct_q = atoi(val);
        whitebox_log(WBL_INFO, "rx_correct_q=%d\n", correct_q);
        whitebox_rx_set_correction(wb, correct_i, correct_q);
    } else if (strncmp(var, "mode", strlen("mode")) == 0) {
        whitebox_log(WBL_INFO, "mode=%s\n", val);
        if (strcmp(val, "cw") == 0)
            change_mode(wb, config, rt, WBM_IQ_TONE);
        else if (strcmp(val, "iq") == 0)
            change_mode(wb, config, rt, WBM_IQ_SOCKET);
        else if (strcmp(val, "2tone") == 0)
            change_mode(wb, config, rt, WBM_AUDIO_TONE);
        else if (strcmp(val, "audio") == 0)
            change_mode(wb, config, rt, WBM_AUDIO_SOCKET);
    } else if (strncmp(var, "latency", strlen("latency")) == 0) {
        rt->latency_ms = atoi(val);
        whitebox_log(WBL_INFO, "latency_ms=%d\n", rt->latency_ms);
        whitebox_tx_set_latency(wb, rt->latency_ms);
    } else if (strncmp(var, "modulation", strlen("modulation")) == 0) {
        strncpy(config->modulation, val, 255);
        config->modulation[255] = '\0';
        whitebox_log(WBL_INFO, "modulation=%s\n", config->modulation);
    } else if (strncmp(var, "audio_source", strlen("audio_source")) == 0) {
        strncpy(config->audio_source, val, 255);
        config->audio_source[255] = '\0';
        whitebox_log(WBL_INFO, "audio_source=%s\n", config->audio_source);
    } else if (strncmp(var, "iq_source", strlen("iq_source")) == 0) {
        strncpy(config->iq_source, val, 255);
        config->iq_source[255] = '\0';
        whitebox_log(WBL_INFO, "iq_source=%s\n", config->iq_source);
        whitebox_source_free(rt->iq_source);
        for (i = 0; iq_sources[i].alloc; ++i) {
            if (strncmp(iq_sources[i].name, val, 255) == 0) {
                iq_sources[i].alloc(&rt->iq_source);
                return 0;
            }
        }
        return -1;
    } else {
        whitebox_log(WBL_WARN, "bad bad %s\n", var);
        return -1;
    }
    return 0;
}

#define WFD_WHITEBOX     0
#define WFD_AUDIO_SOCK   1
#define WFD_DAT_SOCK     2
#define WFD_AUDIO_LISTEN 3
#define WFD_DAT_LISTEN   4
#define WFD_CTL_LISTEN   5
#define WFD_CTL_SOCK     6
#define WFD_END          7

int whitebox_step(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt, long total_samples)
{
    static struct pollfd fds[WFD_END];
    static uint32_t z_buf[FRAME_SIZE];
    static int16_t x_buf[FRAME_SIZE];
    struct timespec tl_start, tl_poll, tl_sink, tl_source, tl_mix, tl_finish;
    long in_available = 0, out_available = 0;
    long rx_in_available = 0, rx_out_available = 0;
    int fcnt, f;
    uint32_t *dest;
    uint32_t *z_dest;
    uint32_t *src;

    clock_gettime(CLOCK_MONOTONIC, &tl_start);

    reconfigure_runtime(wb, config, rt);

    // step 1. poll
    fcnt = 0;
    fds[WFD_WHITEBOX].fd = rt->fd;
    fds[WFD_WHITEBOX].events = 0;
    if (rt->ptt)
        fds[WFD_WHITEBOX].events |= POLLOUT;
    if (rt->ptl)
        fds[WFD_WHITEBOX].events |= POLLIN;
    fcnt++;

    fds[WFD_AUDIO_SOCK].fd = rt->audio_fd;
    fds[WFD_AUDIO_SOCK].events = 0;
    if (rt->audio_needs_poll) {
        fds[fcnt].events |= POLLIN;
    }
    fcnt++;

    fds[WFD_DAT_SOCK].fd = rt->dat_fd;
    fds[WFD_DAT_SOCK].events = 0;
    if (rt->dat_needs_poll) {
        fds[fcnt].events |= POLLIN;
    }
    fcnt++;

    if (config->ctl_enable) {
        fds[fcnt].fd = rt->ctl_listening_fd;
        fds[fcnt].events = POLLIN;
        fcnt++;
    }
    if (config->dat_enable) {
        fds[fcnt].fd = rt->dat_listening_fd;
        fds[fcnt].events = POLLIN;
        fcnt++;
    }
    if (config->audio_enable) {
        fds[fcnt].fd = rt->audio_listening_fd;
        fds[fcnt].events = POLLIN;
        fcnt++;
    }
    if (rt->ctl_needs_poll) {
        fds[fcnt].fd = rt->ctl_fd;
        fds[fcnt].events = POLLIN;
        fcnt++;
    }

    if (poll(fds, fcnt, rt->latency_ms) < 0) {
        if (errno == EINTR)
            return -1;

        perror("poll");
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &tl_poll);

    // step 2. get the destination address and available space in whitebox sink
    if (fds[WFD_WHITEBOX].revents & POLLOUT) {
        out_available = ioctl(rt->fd, W_MMAP_WRITE, &dest);
        out_available = out_available < (FRAME_SIZE << 2) ? out_available : (FRAME_SIZE << 2);
        if (total_samples > 0 && (out_available >> 2) + rt->i > total_samples)
            out_available = (total_samples - rt->i) << 2;
    } else {
        out_available = 0;
    }

    if (fds[WFD_WHITEBOX].revents & POLLIN) {
        rx_in_available = ioctl(rt->fd, W_MMAP_READ, &src);
        if (rx_in_available < 0) {
            whitebox_log(WBL_ERROR, "rx error\n");
            return -1;
        }
        rx_in_available = rx_in_available < (FRAME_SIZE << 2) ? rx_in_available : (FRAME_SIZE << 2);
    } else {
        rx_in_available = 0;
    }

    if (rx_in_available > 0) {
        z_write(wb, config, rt, rx_in_available, src);
    }

    if (!(config->mode & WBM_IQ_MIXED))
        z_dest = dest;
    else
        z_dest = z_buf;

    clock_gettime(CLOCK_MONOTONIC, &tl_sink);

    // step 3. recv_from from the xiq/z source
    in_available = 0;
    if (out_available > 0 && fds[WFD_AUDIO_SOCK].revents & POLLIN) {
        //printf("no out\n");
        in_available = x_read(wb, config, rt, out_available, x_buf);
        z_read(wb, config, rt, in_available, z_dest);
    } else if (out_available > 0 && fds[WFD_DAT_SOCK].revents & POLLIN) {
        in_available = z_read(wb, config, rt, out_available, z_dest);
    } else if (!(rt->dat_needs_poll || rt->audio_needs_poll) && out_available > 0) {
        in_available = z_read(wb, config, rt, out_available, z_dest);
    }

    if (in_available < 0) {
        whitebox_log(WBL_ERROR, "z_read\n");
    }

    if (config->mode & WBM_IQ_TEST) {
        z_test(wb, config, rt, in_available, z_dest);
    }

    clock_gettime(CLOCK_MONOTONIC, &tl_source);

    // step 4. mix x and z
    if (in_available > 0 && config->mode & WBM_IQ_MIXED) {
        mix_z_and_x(wb, config, rt, in_available, x_buf, z_dest, dest);
    }

    clock_gettime(CLOCK_MONOTONIC, &tl_mix);

    // step 5. call write on the whitebox device with how much data was received
    if (in_available > 0) {
        if (write(rt->fd, 0, in_available) != in_available) {
            whitebox_log(WBL_ERROR, "Underrun\n");
            return -1;
        }
        else {
            //whitebox_log(WBL_INFO, ".");
        }
    } else {
        /*if (config->verbose_flag == 1) {
            if (out_available == 0) printf("-");
            else printf(">");
            fflush(stdout);
        }*/
    }

    rt->i += in_available >> 2;

    if (rx_in_available > 0) {
        if (read(rt->fd, 0, rx_in_available) != rx_in_available) {
            whitebox_log(WBL_ERROR, "Overrun\n");
            fflush(stdout);
            return -1;
        } else {
            /*if (config->verbose_flag == 1) {
                printf(".");
                fflush(stdout);
            }*/
        }
    } else {
        /*if (config->verbose_flag == 1) {
            if (out_available == 0) printf("-");
            else printf(">");
            fflush(stdout);
        }*/
    }

    for (f = 3; f < fcnt; ++f) {
        if ((fds[f].fd == rt->ctl_listening_fd)
                && (fds[f].revents & POLLIN)) {
            int fd;
            if ((fd = accept(rt->ctl_listening_fd, (struct sockaddr*)&rt->ctl_sock_other, &rt->ctl_slen)) == -1) {
                perror("accept");
                return -1;
            }
            // Only one ctl access at a time.
            if (rt->ctl_needs_poll) {
                close(fd);
                continue;
            }
            rt->ctl_fd = fd;
            fcntl(rt->ctl_fd, F_SETFL, fcntl(rt->ctl_fd, F_GETFL, 0) | O_NONBLOCK);
            if (config->verbose_flag > 2)
                whitebox_log(WBL_INFO, "Accepted client\n");
            rt->ctl_needs_poll = 1;
        }
        if ((fds[f].fd == rt->ctl_fd)
                && (fds[f].revents & POLLIN)) {
            http_ctl(wb, config, rt);
        }
        if ((fds[f].fd == rt->dat_listening_fd)
                && (fds[f].revents & POLLIN)) {
            if (rt->dat_fd > 0) {
                whitebox_log(WBL_WARN, "too many dat connections\n");
                return -1;
            }
            if ((rt->dat_fd = accept(rt->dat_listening_fd, (struct sockaddr*)&rt->dat_sock_other, &rt->dat_slen)) == -1) {
                perror("accept");
                return -1;
            }
            if (config->verbose_flag > 2)
                whitebox_log(WBL_INFO, "Accepted client\n");
            change_mode(wb, config, rt, WBM_IQ_SOCKET);
            rt->dat_needs_poll = 1;
        }
        if ((fds[f].fd == rt->audio_listening_fd)
                && (fds[f].revents & POLLIN)) {
            if (rt->audio_fd > 0) {
                whitebox_log(WBL_WARN, "too many audio connections\n");
                return -1;
            }
            if ((rt->audio_fd = accept(rt->audio_listening_fd, (struct sockaddr*)&rt->audio_sock_other, &rt->audio_slen)) == -1) {
                perror("accept");
                return -1;
            }
            if (config->verbose_flag > 2)
                whitebox_log(WBL_INFO, "Accepted client\n");
            change_mode(wb, config, rt, WBM_AUDIO_SOCKET);
            rt->audio_needs_poll = 1;
        }
    }

    /*if (config->verbose_flag >= 3) {
        int n;
        int16_t re, im;
        for (n = 0; n < in_available >> 2; ++n) {
            QUAD_UNPACK(*(uint32_t*)(z_dest + n), re, im);
            printf("%d, ", re);
        }
    }*/

    /*if (rt->i > 48e3*20) {
        int fd = open("/sys/power/state", O_WRONLY);
        write(fd, "standby\n", strlen("standby\n"));
        sleep(10);
    }*/
    clock_gettime(CLOCK_MONOTONIC, &tl_finish);
    if (config->verbose_flag >= 2) {
        if (diff(tl_start, tl_finish) > 1.0)
            whitebox_log(WBL_DEBUG, "poll=%f sink=%f source=%f mix=%f write=%f (total=%f) out_available=%d in_available=%d\n",
                    diff(tl_start, tl_poll),
                    diff(tl_poll, tl_sink),
                    diff(tl_sink, tl_source),
                    diff(tl_source, tl_mix),
                    diff(tl_mix, tl_finish),
                    diff(tl_start, tl_finish),
                    out_available >> 2,
                    in_available >> 2);
    }
    return 0;
}

int whitebox_run(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    int total_samples = 0;

    if (config->duration > 0.0)
        total_samples = config->duration * config->sample_rate;

    done = 0;

    while (((total_samples > 0) ? (rt->i < total_samples) : 1) && !done) {
        if (whitebox_step(wb, config, rt, total_samples) < 0) {
            if (errno == EINTR)
                break;
            whitebox_log(WBL_ERROR, "ERROR\n");
            break;
        }
    }
}

void whitebox_finish(whitebox_t *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    whitebox_log(WBL_INFO, "Gracefully exiting\n");
    fsync(rt->fd);
    whitebox_munmap(wb);
    whitebox_close(wb);

    if (config->dat_enable)
        close(rt->dat_listening_fd);
    if (config->audio_enable)
        close(rt->audio_listening_fd);
    if (config->ctl_enable)
        close(rt->ctl_listening_fd);

    if (config->z_filename)
        free(config->z_filename);

    whitebox_log(WBL_INFO, "Bytes written: %d\n", rt->i << 2);

    fclose(rt->log_file);
}

void whiteboxd_log_handler(int priority, const char *message)
{
    fprintf(stderr, message);
    fflush(stderr);
}

void exit_handler(int signo)
{
    done = 1;
}

int main(int argc, char **argv)
{
    int fd;
    int q, z, k;
    int16_t a, c, d, g, h;
    uint32_t b;
    struct timespec start, end, final;
    uint16_t entry;
    int opts;

    whitebox_t wb;
    struct whitebox_config current_config, previous_config;
    struct whitebox_config *config = &current_config;
    struct whitebox_runtime rt;

    whitebox_config_init(&current_config);
    whitebox_runtime_init(&rt);

    if (signal(SIGINT, exit_handler) == SIG_ERR) {
        perror("signal");
        return -1;
    }

    while (1) {
        int c;
        static struct option long_options[] = {
            { "verbose", no_argument, 0, 'v' },
            { "debug", no_argument, 0, 'D' },
            { "rate", required_argument, 0, 'r' },
            { "carrier_freq", required_argument, 0, 'c' },
            { "port", required_argument, 0, 'p' },
            { "file", required_argument, 0, 'f' },
            { "tone", required_argument, 0, 't' },
            { "audio_tone", required_argument, 0, 0 },
            { "am", required_argument, 0, 'A' },
            { "duration", required_argument, 0, 'd' },
            { "test", no_argument, 0, 0 },
            { "log", required_argument, 0, 'l' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "r:c:p:f:t:A:d:l:vD",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (strcmp("audio_tone", long_options[option_index].name) == 0) {
                    //config->mode = WBM_AUDIO_TONE | WBM_IQ_TONE;
                    //config->tone2 = atof(optarg);
                    break;
                }
                if (strcmp("test", long_options[option_index].name) == 0) {
                    //config->mode |= WBM_IQ_TEST;
                    break;
                }
                break;
            case 'r':
                config->sample_rate = atoi(optarg);
                break;

            case 'c':
                config->carrier_freq = atof(optarg);
                break;

            case 'v':
                config->verbose_flag += 1;
                break;

            case 'D':
                config->debug = 1;
                break;

            case 'p':
                config->port = atoi(optarg); 
                //config->mode = WBM_IQ_SOCKET;
                break;

            case 'f':
                config->z_filename = strdup(optarg); 
                rt.dat_needs_poll = 1;
                //config->mode = WBM_IQ_FILE;
                break;

            case 'l':
                rt.log_file = fopen(optarg, "w");
                if (!rt.log_file) {
                    perror("fopen");
                    exit(1);
                }
                break;

            case 't':
                config->tone1 = atof(optarg);
                //config->mode = WBM_IQ_TONE;
                break;

            case 'A':
                config->tone2_offset = atoi(optarg);
                break;

            case 'd':
                config->duration = atof(optarg);
                break;

            case '?':
                printf("usage\n");
                return 0;

            default:
                printf("Invalid argument\n");
                return -1;
        }
    }

    config->mode = WBM_IDLE;

    whitebox_log_init(rt.log_file, config->verbose_flag);

    if(whitebox_start(&wb, config, &rt) < 0)
        return -1;

    clock_gettime(CLOCK_MONOTONIC, &start);

    whitebox_run(&wb, config, &rt);

    whitebox_finish(&wb, config, &rt);

    clock_gettime(CLOCK_MONOTONIC, &end);

    whitebox_log(WBL_INFO, "Wall time: %f ms\n", diff(start, end));

    return 0;
}
