#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http.h"
#include "whitebox.h"
#include "whiteboxd.h"

int http_parse_status(struct http_request *r, char *line)
{
    char *method, *url, *version;
    method = strtok(line, " \t\r\n");
    url = strtok(NULL, " \t\r\n");
    version = strtok(NULL, " \t\r\n");

    strncpy(r->method, method, HTTP_METHOD_LEN);
    r->method[HTTP_METHOD_LEN] = '\0';
    strncpy(r->url, url, HTTP_URL_LEN);
    r->url[HTTP_URL_LEN] = '\0';
    strncpy(r->version, version, HTTP_VERSION_LEN);
    r->url[HTTP_VERSION_LEN] = '\0';

    //printf("status: %s %s %s\n", r->method, r->url, r->version);
    return 0;
}

int http_parse_header(struct http_request *r, char *line)
{
    char *var, *val;
    var = strtok(line, ": \t");
    val = strtok(NULL, "\r\n");
    //printf("header: %s %s\n", var, val);
    return 0;
}

int http_parse_body(struct http_request *r, char *body)
{
    char *var, *val;
    int i = 0;
    var = strtok(body, "=&\r\n");
    val = strtok(NULL, "=&\r\n");
    while (var && val && i < HTTP_PARAMS_MAX - 1) {
        strncpy(r->params[i].name, var, HTTP_PARAM_NAME_LEN);
        r->params[i].name[HTTP_PARAM_NAME_LEN] = '\0';
        strncpy(r->params[i].value, val, HTTP_PARAM_VALUE_LEN);
        r->params[i].value[HTTP_PARAM_VALUE_LEN] = '\0';
        var = strtok(NULL, "=&\r\n");
        val = strtok(NULL, "=&\r\n");
        i++;
    }
    r->params[i].name[0] = '\0';
    r->params[i].value[0] = '\0';
    return 0;
}

int http_parse(int fd, struct http_request *r)
{
    int recvd;
    static char command[HTTP_BUFFER_LEN + 1];
    char *i;
    int newline_machine = 0;
    char *linestart;
    char *bodystart;
    static char linebuf[HTTP_LINE_LEN + 1];
    int linenum;

    if ((recvd = read(fd, (void*)command, HTTP_BUFFER_LEN)) < 0) {
        if (errno == EAGAIN) {
            printf("Eagain\n");
            return recvd;
        }
        perror("recvfrom");
        exit(1);
    }

    if ((recvd == 1 && *((char*)command) == '\0') || (recvd == 0)) {
        return 0;
    }

    command[HTTP_BUFFER_LEN] = '\0';

    linenum = 0;
    linestart = command;
    for (i = command; *i; ++i) {
        switch (newline_machine) {
            case 0: case 2: {
                if (*i == '\r') newline_machine++;
                else newline_machine = 0;
            } break;
            case 1: {
                if (*i == '\n') newline_machine++;
                else newline_machine = 0;
            } break;
            case 3: {
                if (*i == '\n') newline_machine++;
                else newline_machine = 0;
            } break;
            default: {
                newline_machine = 0;
            }
        }
        if (newline_machine == 2) {
            int linelength = i - linestart;
            linelength = linelength < HTTP_LINE_LEN ? linelength : HTTP_LINE_LEN;
            strncpy(linebuf, linestart, i - linestart);
            linebuf[HTTP_LINE_LEN] = '\0';
            if (linenum == 0) {
                if (http_parse_status(r, linebuf) < 0) {
                    printf("Estatus\n");
                    return -1;
                }
            } else {
                if (http_parse_header(r, linebuf) < 0) {
                    printf("Eheader\n");
                    return -1;
                }
            }

            linestart = i + 1;
            linenum++;
        }
        if (newline_machine == 4) {
            bodystart = i + 1;
            if (http_parse_body(r, bodystart) < 0) {
                printf("Ebody\n");
                return -1;
            }
            linenum++;
            break;
        }
    }

    return 0;
}

int http_send_header(int fd, struct http_request *r)
{
    static char header[HTTP_LINE_LEN + 1];
    int header_len;

    header_len = snprintf(header, HTTP_LINE_LEN, 
        "%s %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        r->version,
        r->status_code,
        r->status_msg,
        r->response_type,
        r->response_len);
    return send(fd, header, header_len, 0);
}

int http_respond_file(int fd,
        struct http_request *r,
        char *response_type,
        char *filename)
{
    struct stat st;
    int file_fd = open(filename, O_RDONLY);

    if (file_fd < 0)
        return http_respond_error(fd, r, 404);

    if (fstat(file_fd, &st) < 0)
        return http_respond_error(fd, r, 404);

    r->response_len = st.st_size;
    r->status_code = 200;
    strcpy(r->status_msg, "OK");
    strcpy(r->response_type, response_type);

    if (http_send_header(fd, r) < 0) {
        close(file_fd);
        return -1;
    }

    sendfile(fd, file_fd, 0, r->response_len, 0);
    close(file_fd);
    return 0;
}

int http_respond_error(int fd, struct http_request *r, int code)
{
    r->response_len = 0;
    r->status_code = code;
    strcpy(r->status_msg, "NO");
    strcpy(r->response_type, "text/html");

    return http_send_header(fd, r);
}

int http_respond_string(int fd,
        struct http_request *r,
        char *response_type,
        char *response_fmt, ...)
{
    va_list arglist;

    va_start(arglist, response_fmt);
    vsnprintf(r->response, HTTP_RESPONSE_LEN, response_fmt, arglist);
    va_end(arglist);
    r->response_len = strlen(r->response);

    r->status_code = 200;
    strcpy(r->status_msg, "OK");
    strcpy(r->response_type, response_type);

    if (http_send_header(fd, r) < 0)
        return -1;
    
    return send(fd, r->response, r->response_len, 0);
}

char *html_path(struct whitebox_config *config)
{
    if (config->debug)
        return "/mnt/whitebox/lib/www/index.html";
    else
        return "/var/www/index.html";
}

int http_ctl(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    // This must be static!
    static struct http_request request;

    struct http_request *r = &request;
    int result;

    if (!config->httpd_enable) {
        result = http_respond_error(rt->ctl_fd, r, 404);
        goto done;
    }

    result = http_parse(rt->ctl_fd, r);
    if (result < 0) {
        printf("parse fail\n");
        goto done;
    }

    if (strcmp(r->method, "GET") == 0 && strcmp(r->url, "/") == 0) {
        result = http_respond_file(rt->ctl_fd, r,
                "text/html",
                html_path(config));
    } else if (strcmp(r->method, "GET") == 0 && strcmp(r->url, "/config") == 0) {
        int16_t correct_i, correct_q;
        float gain_i, gain_q;
        int16_t rx_correct_i, rx_correct_q;
        
        whitebox_tx_get_correction(wb, &correct_i, &correct_q);
        whitebox_tx_get_gain(wb, &gain_i, &gain_q);
        whitebox_rx_get_correction(wb, &rx_correct_i, &rx_correct_q);

        result = http_respond_string(rt->ctl_fd, r,
                "application/json",
                "{ \"offset_correct_i\": %d,"
                "\"offset_correct_q\": %d,"
                "\"gain_i\": %.2f,"
                "\"gain_q\": %.2f,"
                "\"tone1\": %.1f,"
                "\"tone2\": %.1f,"
                "\"freq\": %.3f,"
                "\"ptt\": %d,"
                "\"ptl\": %d,"
                "\"rx_cal\": %d,"
                "\"rx_offset_correct_i\": %d,"
                "\"rx_offset_correct_q\": %d,"
                "\"mode\": \"%s\","
                "\"latency\": \"%d\","
                "\"modulation\": \"%s\","
                "\"audio_source\": \"%s\","
                "\"iq_source\": \"%s\""
                "}",
                correct_i, correct_q,
                gain_i, gain_q,
                config->tone1, config->tone2,
                config->carrier_freq,
                rt->ptt, rt->ptl,
                rt->rx_cal,
                rx_correct_i, rx_correct_q,
                whitebox_mode_to_string(config->mode),
                rt->latency_ms,
                config->modulation,
                config->audio_source,
                config->iq_source
                );
    } else if (strcmp(r->method, "POST") == 0 && strcmp(r->url, "/") == 0) {
        int i = 0;
        while (i < HTTP_PARAMS_MAX && strlen(r->params[i].name) > 0) {
            if (config_change(wb, config, rt, r->params[i].name, r->params[i].value) < 0) {
                result = http_respond_error(rt->ctl_fd, r, 404);
                goto done;
            }
            i++;
        }
        result = http_respond_string(rt->ctl_fd, r,
                "text/plain",
                "OK");
    } else {
        result = http_respond_error(rt->ctl_fd, r, 404);
    }

done:
    close(rt->ctl_fd);
    rt->ctl_needs_poll = 0;
    rt->ctl_fd = -1;
    return result;
}

