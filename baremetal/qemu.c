//
// The desktop's machine under QEMU: qemu.h says what this is for.
//

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "../vm_constants.h"
#include "../test/deskcheck.h"
#include "qemu.h"

#define W       DISP_W
#define H       DISP_H
#define TURN    150             // the thousandths of a second a turn is given

// The keys of a keyboard as QEMU names them, where the name is not the
// character; and the characters that are a key with Shift held.
static const struct { char c; const char* key; } NAMED[] = {
    {' ', "spc"}, {'\r', "ret"}, {'.', "dot"}, {',', "comma"}, {'/', "slash"}, {'-', "minus"}, {'=', "equal"},
    {';', "semicolon"}, {'\'', "apostrophe"}, {'[', "bracket_left"}, {']', "bracket_right"},
    {'\\', "backslash"}, {'`', "grave_accent"}, {0}};
static const char SHIFTED[] = "!@#$%^&*()_+:\"<>?{}|~";
static const char UNSHIFTED[] = "1234567890-=;',./[]\\`";

int qemu_scale = 1, qemu_shots, qemu_inexact;

static pid_t qemu;              // QEMU,
static int serial;              // what its console says,
static int qmp;                 // and where it is told things
static char tmp[64];            // a place for its socket and for what it dumps
static const char* disk;
static uint8_t* screen;

static void wait_ms(int ms) {
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

static void stop(void) {
    if (qemu > 0) kill(qemu, SIGKILL);
}

static void fail(const char* what) {
    fprintf(stderr, "QEMU: %s: %s\n", what, strerror(errno));
    exit(EXIT_FAILURE);
}

// A line of what QEMU answers.
static void answer(char* text, size_t len) {
    size_t n = 0;
    while (n + 1 < len) {
        char c;
        if (read(qmp, &c, 1) != 1) fail("it stopped answering");
        if (c == '\n') break;
        text[n++] = c;
    }
    text[n] = '\0';
}

// QEMU is told something, and has done it when it answers so.
static void command(const char* format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(text, sizeof(text) - 1, format, args);
    va_end(args);
    text[len++] = '\n';
    if (write(qmp, text, len) != len) fail("it cannot be told");
    do answer(text, sizeof(text)); while (!strstr(text, "\"return\"") && !strstr(text, "\"error\""));
}

void qemu_start(const char* elf, const char* on) {
    disk = on;
    if (getenv("SCALE") && atoi(getenv("SCALE")) > 1) qemu_scale = atoi(getenv("SCALE"));
    int rate = getenv("DISK") ? (int)(atof(getenv("DISK")) * 1024) : 0;
    screen = malloc((size_t)W * H);

    snprintf(tmp, sizeof(tmp), "/tmp/hexaforth-qemu-XXXXXX");
    if (!mkdtemp(tmp)) fail(tmp);
    static char sock[128];
    char qmp_is[160], drive[512];
    snprintf(sock, sizeof(sock), "%s/qmp", tmp);
    snprintf(qmp_is, sizeof(qmp_is), "unix:%s,server,nowait", sock);
    snprintf(drive, sizeof(drive), "file=%s,if=none,format=raw,id=hd0", disk);
    if (rate) {         // QEMU holds the drive to it, as the Makefile's DISK does
        snprintf(drive + strlen(drive), sizeof(drive) - strlen(drive),
                 ",throttling.bps-read=%d,throttling.bps-write=%d", rate, rate);
    }

    int said[2];
    if (pipe(said)) fail("a pipe");
    qemu = fork();
    if (qemu < 0) fail("fork");
    if (!qemu) {
        dup2(said[1], 1);
        close(said[0]);
        int nothing = open("/dev/null", O_RDWR);
        dup2(nothing, 0);
        dup2(nothing, 2);
        execlp("qemu-system-aarch64", "qemu-system-aarch64", "-M", "virt", "-cpu", "cortex-a53", "-m", "256",
               "-display", "none", "-serial", "stdio", "-nic", "none", "-qmp", qmp_is, "-device", "ramfb",
               "-global", "virtio-mmio.force-legacy=false", "-drive", drive,
               "-device", "virtio-blk-device,drive=hd0", "-device", "virtio-keyboard-device",
               "-device", "virtio-tablet-device", "-kernel", elf, (char*)NULL);
        _exit(127);
    }
    atexit(stop);
    close(said[1]);
    serial = said[0];
    fcntl(serial, F_SETFL, fcntl(serial, F_GETFL) | O_NONBLOCK);

    struct sockaddr_un at = { .sun_family = AF_UNIX };
    snprintf(at.sun_path, sizeof(at.sun_path), "%s", sock);
    qmp = socket(AF_UNIX, SOCK_STREAM, 0);
    int tries = 0;
    while (connect(qmp, (struct sockaddr*)&at, sizeof(at))) {
        if (++tries > 200 || waitpid(qemu, NULL, WNOHANG) == qemu) fail("it did not start");
        wait_ms(50);
    }
    char hello[2048];
    answer(hello, sizeof(hello));
    command("{\"execute\": \"qmp_capabilities\"}");
}

void qemu_said(char* text, size_t room) {
    size_t n = strlen(text);
    ssize_t got = n + 1 < room ? read(serial, text + n, room - 1 - n) : 0;
    if (got > 0) text[n + got] = '\0';
}

uint64_t qemu_cell(uint64_t address) {
    char path[128];
    uint64_t cell = 0;
    snprintf(path, sizeof(path), "%s/cell", tmp);
    command("{\"execute\": \"pmemsave\", \"arguments\": {\"val\": %llu, \"size\": 8, \"filename\": \"%s\"}}",
            (unsigned long long)address, path);
    FILE* in = fopen(path, "rb");
    if (in == NULL || fread(&cell, 8, 1, in) != 1) fail(path);
    fclose(in);
    return(cell);
}

static void event(const char* format, ...) {
    char said[512];
    va_list args;
    va_start(args, format);
    vsnprintf(said, sizeof(said), format, args);
    va_end(args);
    command("{\"execute\": \"input-send-event\", \"arguments\": {\"events\": [%s]}}", said);
    wait_ms(50);
}

static void key(const char* name, bool down) {
    event("{\"type\": \"key\", \"data\": {\"down\": %s, \"key\": {\"type\": \"qcode\", \"data\": \"%s\"}}}",
          down ? "true" : "false", name);
}

// The key a character is on, as QEMU names it, and whether Shift is held.
static const char* key_of(char c, bool* shift) {
    static char name[2];
    const char* at = c ? strchr(SHIFTED, c) : NULL;
    *shift = at != NULL || (c >= 'A' && c <= 'Z');
    if (at) c = UNSHIFTED[at - SHIFTED];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    for(int n=0; NAMED[n].c; n++) {
        if (NAMED[n].c == c) return(NAMED[n].key);
    }
    name[0] = c;
    return(name);
}

// ---- what test/deskcheck.h says a machine does

const char* desk_boot(void) {
    static char said[1 << 16];
    for(int waited=0; !strstr(said, "desktop") && waited < 120000; waited += 20) {
        qemu_said(said, sizeof(said));
        wait_ms(20);
    }
    return(said);
}

void desk_type(const char* text) {
    for(; *text; text++) {
        bool shift;
        char held[32];
        snprintf(held, sizeof(held), "%s", key_of(*text, &shift));
        if (shift) key("shift", true);
        key(held, true);
        key(held, false);
        if (shift) key("shift", false);
    }
}

void desk_key(const char* name, const char* with) {
    bool shift;
    char held[32];
    snprintf(held, sizeof(held), "%s", name[1] ? name : key_of(name[0], &shift));
    if (with) key(with, true);
    key(held, true);
    key(held, false);
    if (with) key(with, false);
}

void desk_pointer(int x, int y) {
    event("{\"type\": \"abs\", \"data\": {\"axis\": \"x\", \"value\": %d}}, "
          "{\"type\": \"abs\", \"data\": {\"axis\": \"y\", \"value\": %d}}",
          x * 0x8000 / W + 1, y * 0x8000 / H + 1);
}

void desk_button(bool down) {
    event("{\"type\": \"btn\", \"data\": {\"down\": %s, \"button\": \"left\"}}", down ? "true" : "false");
}

void desk_turns(int turns) {
    wait_ms(turns * TURN);
}

static bool lit_at(const uint8_t* samples, int w, int x, int y) {      // has a pixel of the dump any light in it?
    const uint8_t* p = samples + ((size_t)y * w + x) * 3;
    return((299 * p[0] + 587 * p[1] + 114 * p[2]) / 1000 != 0);
}

// The screen, as QEMU dumps it: a picture of three samples to a pixel, of
// which a pixel is black if it has no light in it.  Of a block that is one
// pixel of the display it is the pixel in the middle that is taken.
const uint8_t* desk_screen(void) {
    char path[128];
    int scale = qemu_scale;
    snprintf(path, sizeof(path), "%s/screen.ppm", tmp);
    command("{\"execute\": \"screendump\", \"arguments\": {\"filename\": \"%s\"}}", path);
    wait_ms(300);
    FILE* in = fopen(path, "rb");
    int w = 0, h = 0, most = 0;
    if (in == NULL || fscanf(in, "P6 %d %d %d", &w, &h, &most) != 3 || w != W * scale || h != H * scale) {
        fprintf(stderr, "QEMU: the screen it dumped is %d by %d, and is to be %d by %d\n", w, h, W * scale, H * scale);
        exit(EXIT_FAILURE);
    }
    fgetc(in);
    uint8_t* samples = malloc((size_t)w * h * 3);
    if (fread(samples, 3, (size_t)w * h, in) != (size_t)w * h) fail(path);
    fclose(in);
    bool exact = true;
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) {
            bool lit = lit_at(samples, w, x * scale + scale / 2, y * scale + scale / 2);
            for(int by=0; by < scale; by++) {
                for(int bx=0; bx < scale; bx++) exact = exact && lit_at(samples, w, x * scale + bx, y * scale + by) == lit;
            }
            screen[(size_t)y * W + x] = !lit;
        }
    }
    free(samples);
    qemu_shots++;
    qemu_inexact += !exact;
    return(screen);
}

void desk_block(long n, int count, uint8_t* bytes) {
    wait_ms(500);
    FILE* in = fopen(disk, "rb");
    memset(bytes, 0, (size_t)count * DISK_BLOCK);
    if (in == NULL || fseek(in, n * DISK_BLOCK, SEEK_SET) || !fread(bytes, 1, (size_t)count * DISK_BLOCK, in)) fail(disk);
    fclose(in);
}
