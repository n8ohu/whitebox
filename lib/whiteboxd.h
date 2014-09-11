#ifndef __WHITEBOXD_H__
#define __WHITEBOXD_H__

#include "whitebox.h"
#include "http.h"
#include <poll.h>
#include <netinet/in.h>

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

struct whitebox_source_operations;

struct whitebox_source {
    struct whitebox_source_operations *ops;    
};

struct whitebox_source_operations {
    int (*poll)(struct whitebox_source *source,
            struct pollfd* fds);
    int (*work)(struct whitebox_source *source,
            unsigned long src, size_t src_count,
            unsigned long dest, size_t dest_count);
};

int whitebox_source_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count);
void whitebox_source_free(struct whitebox_source *source);

struct whitebox_synth_source {
    struct whitebox_source source;
    float freq;
    uint32_t fcw;
    uint32_t phase;
};

struct whitebox_const_source {
    struct whitebox_source source;
    int16_t re;
    int16_t im;
};

int whitebox_source_tune(struct whitebox_source *source, float fdes);

int whitebox_const(struct whitebox_source *source);
int whitebox_synth(struct whitebox_source *source);
int whitebox_socket(struct whitebox_source *source);

int whitebox_qsynth_lsb_alloc(struct whitebox_source **source);
int whitebox_qsynth_usb_alloc(struct whitebox_source **source);
int whitebox_qconst_alloc(struct whitebox_source **source);
int whitebox_qsocket_alloc(struct whitebox_source **source);

struct whitebox_file_source {
    struct whitebox_source source;
    int fd;
};

int (*modulate)(struct whitebox_source *iq_source,
        struct whitebox_source *audio_source);

int modulate_idle(struct whitebox_source *iq_source,
        struct whitebox_source *audio_source);

enum whitebox_mode {
    WBM_IDLE         = 0x00000000,
    WBM_IQ_SOCKET    = 0x00000001,
    WBM_IQ_FILE      = 0x00000002,
    WBM_IQ_TONE      = 0x00000004,
    WBM_IQ_TEST      = 0x00000010,
    WBM_AUDIO_TONE   = 0x00000100,
    WBM_AUDIO_SOCKET = 0x00000200,
};

#define WBM_DEFAULT   WBM_IQ_SOCKET
#define WBM_IQ_FD (WBM_IQ_SOCKET | WBM_IQ_FILE)
#define WBM_IQ_MIXED  (WBM_AUDIO_TONE | WBM_AUDIO_SOCKET)
#define WBM_AUDIO_FD (WBM_AUDIO_SOCKET)
#define WBM_FROM_SOCKET (WBM_IQ_SOCKET)

struct whitebox_config {
    enum whitebox_mode mode;

    int verbose_flag;
    int sample_rate;
    float carrier_freq;
    unsigned short ctl_port;
    unsigned short audio_port;
    unsigned short port;
    float duration;
    int debug;
    
    // complex variable z
    char *z_filename;
    float tone1;

    // real variable x
    float tone2;
    int16_t tone2_offset;

    int ctl_enable;
    int dat_enable;
    int httpd_enable;
    int audio_enable;
    int cat_enable;

    char modulation[256];
    char audio_source[256];
    char iq_source[256];
};

struct whitebox_runtime {
    int fd;
    FILE *log_file;
    int i;
    int latency_ms;

    int ctl_listening_fd;
    struct sockaddr_in ctl_sock_me, ctl_sock_other;
    size_t ctl_slen;
    int ctl_fd;
    int ctl_needs_poll;
    
    int dat_listening_fd;
    struct sockaddr_in dat_sock_me, dat_sock_other;
    size_t dat_slen;
    int dat_fd;
    int dat_needs_poll;

    int audio_listening_fd;
    struct sockaddr_in audio_sock_me, audio_sock_other;
    size_t audio_slen;
    int audio_fd;
    int audio_needs_poll;

    int cat_fd;
    int cat_needs_poll;
    void *cat_dat;

    uint32_t tone1_fcw;
    uint32_t tone1_phase;

    uint32_t tone2_fcw;
    uint32_t tone2_phase;

    int16_t global_re;
    int16_t global_im;

    struct whitebox_source *iq_source;
    struct whitebox_source *audio_source;

    int ptt;
    int ptl;

    int rx_cal;
};

#endif /* __WHITEBOXD_H__ */
