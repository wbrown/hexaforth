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
#include "../util/room.c"
#include "qemu.h"

#define W       DISP_W
#define H       DISP_H
// How long things are waited for was chosen, and is not the machine's.
#define TURN    150             // the thousandths of a second a turn is given
#define STARTS  10000           // and QEMU to start,
#define BOOTS   120000          // and the desktop to come up
#define STEP    50              // how often it is asked

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
static char tmp[] = "/tmp/hexaforth-qemu-XXXXXX";       // a place for its socket and for what it dumps
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

// Text made as printf makes it, as long as it comes to: it is made here,
// and is whoever asked's to free.
static char* text_of(const char* format, va_list args) {
    va_list again;
    va_copy(again, args);
    int len = vsnprintf(NULL, 0, format, args);
    char* text = room_for((size_t)len + 2, 1);          // and room for a line's end after it
    vsnprintf(text, (size_t)len + 1, format, again);
    va_end(again);
    return(text);
}

static char* text(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char* is = text_of(format, args);
    va_end(args);
    return(is);
}

// A line of what QEMU answers, however long: it is there until the next.
static const char* answer(void) {
    static char* line;
    static long room;
    long n = 0;
    for(;;) {
        char c;
        if (read(qmp, &c, 1) != 1) fail("it stopped answering");
        if (c == '\n') break;
        ONE_MORE(line, n + 1, room);
        line[n++] = c;
    }
    ONE_MORE(line, n, room);
    line[n] = '\0';
    return(line);
}

// QEMU is told something, and has done it when it answers so.
static void command(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char* told = text_of(format, args);
    va_end(args);
    size_t len = strlen(told);
    told[len++] = '\n';
    if (write(qmp, told, len) != (ssize_t)len) fail("it cannot be told");
    free(told);
    for(const char* said = answer(); !strstr(said, "\"return\"") && !strstr(said, "\"error\""); said = answer());
}

// A copy of a disk, in a directory, which is what QEMU is given: the
// machine writes to its disk, and the disk that was made is left as it was
// made.
static const char* copy_of(const char* from, const char* in) {
    unsigned char* bytes;
    size_t len = file_bytes(from, &bytes);
    if (len < DISK_BLOCK) {
        fprintf(stderr, "QEMU: %s is not a disk: it is %zu bytes\n", from, len);
        exit(EXIT_FAILURE);
    }
    char* to = text("%s/disk.img", in);
    FILE* out = fopen(to, "wb");
    if (out == NULL || fwrite(bytes, 1, len, out) != len || fclose(out)) fail(to);
    free(bytes);
    return(to);
}

void qemu_start(const char* elf, const char* on, const char* out) {
    disk = copy_of(on, out);
    if (getenv("SCALE") && atoi(getenv("SCALE")) > 1) qemu_scale = atoi(getenv("SCALE"));
    int rate = getenv("DISK") ? (int)(atof(getenv("DISK")) * 1024) : 0;
    screen = malloc((size_t)W * H);

    if (!mkdtemp(tmp)) fail(tmp);
    char* sock = text("%s/qmp", tmp);
    char* qmp_is = text("unix:%s,server,nowait", sock);
    char* drive = rate          // QEMU holds the drive to it, as it does for the target `demo`
        ? text("file=%s,if=none,format=raw,id=hd0,throttling.bps-read=%d,throttling.bps-write=%d", disk, rate, rate)
        : text("file=%s,if=none,format=raw,id=hd0", disk);

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
        // (QEMU_AARCH64 is where QEMU is, which the build says.)
        execlp(QEMU_AARCH64, QEMU_AARCH64, "-M", "virt", "-cpu", "cortex-a53", "-m", "256",
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
    if (strlen(sock) >= sizeof(at.sun_path)) {          // which is as long as the system lets a socket's name be
        fprintf(stderr, "QEMU: %s is too long a name for a socket\n", sock);
        exit(EXIT_FAILURE);
    }
    strcpy(at.sun_path, sock);
    qmp = socket(AF_UNIX, SOCK_STREAM, 0);
    for(int waited=0; connect(qmp, (struct sockaddr*)&at, sizeof(at)); waited += STEP) {
        if (waited > STARTS || waitpid(qemu, NULL, WNOHANG) == qemu) fail("it did not start");
        wait_ms(STEP);
    }
    answer();                                           // its greeting
    command("{\"execute\": \"qmp_capabilities\"}");
    free(sock);
    free(qmp_is);
    free(drive);
}

const char* qemu_said(void) {
    static char* said;
    static long have, room;
    for(;;) {
        AS_MANY(said, have + BUFSIZ + 1, room);
        ssize_t got = read(serial, said + have, BUFSIZ);
        if (got <= 0) break;                            // there is no more of it yet
        have += got;
    }
    said[have] = '\0';
    return(said);
}

uint64_t qemu_cell(uint64_t address) {
    char* path = text("%s/cell", tmp);
    uint64_t cell = 0;
    command("{\"execute\": \"pmemsave\", \"arguments\": {\"val\": %llu, \"size\": %d, \"filename\": \"%s\"}}",
            (unsigned long long)address, (int)sizeof(cell), path);
    FILE* in = fopen(path, "rb");
    if (in == NULL || fread(&cell, sizeof(cell), 1, in) != 1) fail(path);
    fclose(in);
    free(path);
    return(cell);
}

static void event(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char* said = text_of(format, args);
    va_end(args);
    command("{\"execute\": \"input-send-event\", \"arguments\": {\"events\": [%s]}}", said);
    free(said);
    wait_ms(STEP);
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
    const char* said = qemu_said();
    for(int waited=0; !strstr(said, "desktop") && waited < BOOTS; waited += STEP) {
        wait_ms(STEP);
        said = qemu_said();
    }
    return(said);
}

// (The name of a key is kept: what key_of gives is there only until it is
// next asked.)
void desk_type(const char* typed) {
    for(; *typed; typed++) {
        bool shift;
        char* held = text("%s", key_of(*typed, &shift));
        if (shift) key("shift", true);
        key(held, true);
        key(held, false);
        if (shift) key("shift", false);
        free(held);
    }
}

void desk_key(const char* name, const char* with) {
    bool shift;
    char* held = text("%s", name[1] ? name : key_of(name[0], &shift));
    if (with) key(with, true);
    key(held, true);
    key(held, false);
    if (with) key(with, false);
    free(held);
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
    char* path = text("%s/screen.ppm", tmp);
    int scale = qemu_scale;
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
    free(path);
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
