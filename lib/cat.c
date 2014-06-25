#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http.h"
#include "whitebox.h"
#include "whiteboxd.h"
#include "cat.h"

#define CAT_COMMAND_LEN         2
#define CAT_PARAMS_LEN           255
#define CAT_ANSWER_LEN          1023

#define CAT_BUFFER_LEN          4095
#define CAT_LINE_LEN            2047

struct cat_request {
    int command_index;
    char command[CAT_COMMAND_LEN + 1];
    char params[CAT_PARAMS_LEN + 1];
    char answer[CAT_ANSWER_LEN + 1];
};

struct cat_command {
    char command[CAT_BUFFER_LEN + 1];
    int get_len;
    int (*get_handler)(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt);
    int set_len;
    int (*set_handler)(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt);
};

int cat_respond_string(const char *msg, ...)
{
    va_list arglist;

    va_start(arglist, msg);
    vfprintf(stdout, msg, arglist);
    va_end(arglist);
    fflush(stdout);
    return 0;
}

int get_af_gain(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("AG%01d%03d;", 0, 0);
}

int set_af_gain(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_af_gain(r, wb, config, rt);
}

int get_beat_canceller(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("BC%01d;", 0);
}

int set_beat_canceller(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_beat_canceller(r, wb, config, rt);
}

int get_vfoa(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("FA%011d;", (long)config->carrier_freq);
}

int set_vfoa(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_vfoa(r, wb, config, rt);
}

int get_agc(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("GT%03d;", 0);
}

int set_agc(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_agc(r, wb, config, rt);
}

int get_status_handler(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("IF%011d%05d%+05d%01d%01d%01d%02d%01d%01d%01d%01d%01d%01d%02d%01d;",
        (long)config->carrier_freq,
        0, // reserved
        0, // freq offset
        0, // rit
        0, // xit
        0, // memory channel bank #
        0, // memory channel #
        0, // rx/tx
        0, // mode
        0, // reserved
        0, // scan status
        0, // simplex/split
        0, // tone
        0, // tone freq
        0); // reserved
}

int get_mode(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("MD%01d;", 0);
}

int set_mode(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_mode(r, wb, config, rt);
}

int get_noise_blanker(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("NB%01d;", 0);
}

int set_noise_blanker(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_noise_blanker(r, wb, config, rt);
}

int get_noise_reduction(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("NR%01d;", 0);
}

int set_noise_reduction(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_noise_reduction(r, wb, config, rt);
}

int get_output_power(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("PC%03d;", 0);
}

int set_output_power(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_output_power(r, wb, config, rt);
}

int get_speech_processor(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("PR%01d;", 0);
}

int set_speech_processor(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_speech_processor(r, wb, config, rt);
}

int get_rf_gain(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("RG%03d;", 0);
}

int set_rf_gain(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_rf_gain(r, wb, config, rt);
}

int set_rx(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return cat_respond_string("RX%01d;", 0);
}

int get_squelch(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("SQ%01d%03d;", 0, 0);
}

int set_squelch(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_squelch(r, wb, config, rt);
}

int set_tx(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return cat_respond_string("TX%01d;", 0);
}

int get_vox(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    return cat_respond_string("VX%01d;", 0);
}

int set_vox(struct cat_request *r, struct whitebox *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    // TODO
    return get_vox(r, wb, config, rt);
}

struct cat_command cat_commands[] = {
    { "AG", 4, get_af_gain, 7, set_af_gain },
    { "BC", 3, get_beat_canceller, 4, set_beat_canceller },
    { "FA", 3, get_vfoa, 14, set_vfoa },
    { "GT", 3, get_agc, 6, set_agc },
    { "IF", 3, get_status_handler, 0, 0 },
    { "MD", 3, get_mode, 4, set_mode },
    { "NB", 3, get_noise_blanker, 4, set_noise_blanker },
    { "NR", 3, get_noise_reduction, 4, set_noise_reduction },
    { "PC", 3, get_output_power, 6, set_output_power },
    { "PR", 3, get_speech_processor, 4, set_speech_processor },
    { "RG", 3, get_rf_gain, 6, set_rf_gain },
    { "RX", 0, 0, 3, set_rx },
    { "SQ", 4, get_squelch, 7, set_squelch },
    { "TX", 0, 0, 3, set_tx },
    { "VX", 3, get_vox, 4, set_vox },
    { 0, 0, 0, 0, 0 },
};

int cat_parse(int fd, struct cat_request *r)
{
    int recvd;
    int i;

    if ((recvd = read(fd, (void*)(r->command + r->command_index), CAT_BUFFER_LEN - r->command_index)) < 0) {
        if (errno == EAGAIN) {
            printf("Eagain\n");
            return recvd;
        }
        perror("read");
        exit(1);
    }

    for (i = 0; i < recvd; ++i) {
        char c = *(r->command + r->command_index + i);
        if (c == ';') {
            *(r->command + r->command_index + i) = '\0';
            r->command_index = 0;
            return 1;
        }
    }
    r->command_index += recvd;

    return 0;
}

static struct cat_request cat_request;

int cat_init(struct whitebox *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    struct cat_request *r = &cat_request;
    r->command_index = 0;
    rt->cat_dat = (void*)r;
}

int cat_ctl(whitebox_t *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    struct cat_request *r = (struct cat_request *)rt->cat_dat;
    int result;
    int i;

    result = cat_parse(STDIN_FILENO, r);
    if (result <= 0) {
        return result;
    }

    whitebox_log(WBL_INFO, "cat_ctl request %s;\n", r->command);

    for (i = 0; cat_commands[i].command[0]; ++i) {
        if (strncasecmp(cat_commands[i].command, r->command, CAT_COMMAND_LEN))
            continue;
        if (strlen(r->command) == cat_commands[i].get_len - 1) {
            if (!cat_commands[i].get_handler)
                goto error;
            return cat_commands[i].get_handler(r, wb, config, rt);
        }
        if (strlen(r->command) == cat_commands[i].set_len - 1) {
            if (!cat_commands[i].set_handler)
                goto error;
            return cat_commands[i].set_handler(r, wb, config, rt);
        }
    }


error:
    whitebox_log(WBL_INFO, "error on request %s;\n", r->command);
    cat_respond_string("?;");
    r->command_index = 0;
    return 0;
}
