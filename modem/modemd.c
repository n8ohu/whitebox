#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>

#define MAX_MATCH 10
#define BUF_LEN 1024

#define MODE_COMMAND 0
#define MODE_DATA 1

#define EMODEM_INVALID_ARGUMENT -1
#define EMODEM_INVALID_COMMAND -1

struct dispatch_entry_t {
    struct dispatch_entry_t* next;
    regex_t preg;
    int nmatch;
    int (*callback)(const char* command, const size_t nmatch, const regmatch_t pmatch[]);
};

struct dispatch_entry_t* dispatch_head = 0;

int mode;
int guarded;
int quiet;
char buf[BUF_LEN];

int dispatch_register(const char* regex, int nmatch, int (*callback)(const char*, const size_t, const regmatch_t[])) {
    struct dispatch_entry_t* entry;
    struct dispatch_entry_t* ptr = 0;
    int compilation_result;
    
    if (nmatch > MAX_MATCH) {
        return EMODEM_INVALID_ARGUMENT;
    }

    if (dispatch_head == 0) {
        dispatch_head = entry = malloc(sizeof(struct dispatch_entry_t));
        if (dispatch_head == 0)
            return errno;
    }

    else {
        entry = malloc(sizeof(struct dispatch_entry_t));
        if (entry == 0)
            return errno;

        ptr = dispatch_head;
        while (ptr->next)
            ptr = ptr->next;
    }

    entry->next = 0;
    entry->nmatch = nmatch;
    entry->callback = callback;
    compilation_result = regcomp(&(entry->preg), regex, REG_EXTENDED | REG_NEWLINE);
    if (compilation_result != 0) {
        return compilation_result;
    }

    if (ptr)
        ptr->next = entry;
    return 0;
}

int dispatch_command(const char* command) {
    struct dispatch_entry_t* ptr = dispatch_head;

    regmatch_t pmatch[MAX_MATCH];

    while (ptr) {
        if (regexec(&(ptr->preg), command, ptr->nmatch, pmatch, 0) == 0)
            break;
        ptr = ptr->next;
    }

    if (ptr)
        return ptr->callback(command, ptr->nmatch, pmatch);
    else {
        return EMODEM_INVALID_COMMAND;
    }
}

void mode_command() {
    mode = MODE_COMMAND;
    guarded = 0;
}

void mode_data() {
    mode = MODE_DATA;
    guarded = 0;
}

int sreg_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    printf("hi %d %d\n", (int)pmatch[1].rm_so, (int)pmatch[1].rm_eo);
    return 0;
}

int data_mode(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    mode_data();
    return 0;
}

void regs_init(void) {
    dispatch_register("^ATS([0-9]*)$", 2, sreg_callback);
    dispatch_register("^O$", 1, data_mode);
}

int echo_off_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    struct termios t;
    tcgetattr(fileno(stdin), &t);
    t.c_lflag &= ~(ECHO | ECHONL);
    tcsetattr(fileno(stdin), 0, &t);
    return 0;
}

int echo_on_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    struct termios t;
    tcgetattr(fileno(stdin), &t);
    t.c_lflag |= (ECHO | ECHONL);
    tcsetattr(fileno(stdin), 0, &t);
    return 0;
}

int quiet_off_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    quiet = 0;
    return 0;
}

int quiet_on_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    quiet = 1;
    return 0;
}

void term_init(void) {
    dispatch_register("^ATE$", 1, echo_off_callback);
    dispatch_register("^ATE0$", 1, echo_off_callback);
    dispatch_register("^ATE1$", 1, echo_on_callback);
    dispatch_register("^ATQ$", 1, quiet_off_callback);
    dispatch_register("^ATQ0$", 1, quiet_off_callback);
    dispatch_register("^ATQ1$", 1, quiet_on_callback);

    /*s_register(2, set_escape, get_escape, 43);
    s_register(3, set_cr, get_cr, 13);
    s_register(4, set_lf, get_lf, 10);
    s_register(5, set_backspace, get_backspace, 8);*/
}


int reset_callback(const char* command, const size_t nmatch, const regmatch_t pmatch[]) {
    printf("RESET\n");
    return 0;
}

void reset_init(void) {
    dispatch_register("^ATZ([0-9]*)$", 2, reset_callback);
}

void setup() {
    struct termios t;
    tcgetattr(fileno(stdin), &t);
    t.c_lflag &= ~(ICANON);
    t.c_cc[VMIN] = 3;
    t.c_cc[VTIME] = 10;
    tcsetattr(fileno(stdin), 0, &t);

    regs_init();
    term_init();
    reset_init();
    mode_command();
    quiet = 0;
}

void loop() {
    char* line = 0;
    size_t len = 0;
    ssize_t readd;
    fd_set read_fd_set;
    struct timeval interval;
    interval.tv_sec = 1;
    interval.tv_usec = 0;

    if (mode == MODE_COMMAND) {
        readd = getline(&line, &len, stdin);
        if (strcmp("\n", line) == 0)
            return;
        readd = dispatch_command(line);
        free(line);
        if (readd != 0) {
            if (!quiet)
                printf("ERROR\n");
        }
        else {
            if (!quiet)
                printf("OK\n");
        }
    }
    else { // MODE_DATA
        FD_ZERO(&read_fd_set);
        FD_SET(fileno(stdin), &read_fd_set);
        readd = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &interval);
        if (readd < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        else if (readd == 0) {
            if (guarded == 0) {
                guarded = 1;
            }
            else if (guarded == 2) {
                guarded = 0;
                mode_command();
            }
        }
        else {
            readd = read(fileno(stdin), &buf, BUF_LEN);
            if (readd > 0) {
                if (strcmp("+++", buf) == 0) {
                    guarded = 2;
                }
                else {
                    guarded = 0;
                    // This is where we call the currently active mode.
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    setup();
    while(1) {
        loop();
    }
    return 0;
}
