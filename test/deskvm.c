//
// The desktop on the reference VM: deskvm.h says what this is for.
//

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../bench/tiff.h"
#include "deskvm.h"
#include "nucleus.h"
#include "../vm_stopped.h"

#define W   DISP_W
#define H   DISP_H

// The keys that are not a character, by the names deskcheck.c has for
// them: their Linux codes.
static const struct { const char* name; int code; } KEYS[] = {
    {"ret", 28}, {"backspace", 14}, {"up", 103}, {"down", 108}, {"left", 105}, {"right", 106},
    {"pgup", 104}, {"pgdn", 109}, {"ctrl", 29}, {"shift", 42}, {NULL}};

long deskvm_turns;

static int key_of[UCHAR_MAX + 1];       // the key that gives each character there can be,
static bool shifted[UCHAR_MAX + 1];     // and whether Shift is held for it
static const char* image_path;  // the image the machine is running
static const char* disk_path;   // the copy of the disk
static const char* picture;
static const char* out_dir;     // where the screens are kept
static bool started;            // the machine has left its prompt for the desktop
static uint8_t* screen;         // its screen, when it was last asked for
static long sent;               // the things sent that it has not taken yet

// The machine stopped where it was not sent: which check it was at, and
// what it was at (vm_stopped.h).
static void stopped_wrong(void) {
    printf("FAIL the machine stopped at 0x%04x (expected 0x%04x)%s%s%s%s%s\n"
           "  inspect with: build/cmake/disasm_hex-%d %s 0x%04x 0x%04x\n"
           "  screens in:   %s/\n"
           "  console said: %s\n",
           (unsigned)machine.EIP, (unsigned)stopping(),
           desk_checking ? " during check \"" : "",
           desk_checking ? desk_checking : "",
           desk_checking ? "\"" : "",
           desk_doing && desk_doing[0] ? " while doing: " : "",
           desk_doing && desk_doing[0] ? desk_doing : "",
           CELL_BITS, image_path ? image_path : "IMAGE.hex",
           (unsigned)machine.EIP, (unsigned)machine.EIP,
           out_dir ? out_dir : ".", said);
    static const char file[] = "/stopped.memory";
    char* memory = malloc(strlen(out_dir ? out_dir : ".") + sizeof(file));
    if (memory) sprintf(memory, "%s%s", out_dir ? out_dir : ".", file);
    post_mortem(stdout, &machine, image_path, memory, CELL_BITS);
    exit(EXIT_FAILURE);
}

// The machine takes what has been sent and then stops, in its event loop.
static void taken(void) {
    keys("s", 1);
    if (started) go_on(); else set_off();
    started = true;
    deskvm_turns += sent + 1;
    sent = 0;
    if (machine.EIP != stopping()) stopped_wrong();
}

static void event(char what, int a, int b) {
    char bytes[3] = { what, (char)a, (char)b };
    keys(bytes, 3);
    sent++;
}

static int code_of(const char* name) {
    for(int n=0; KEYS[n].name; n++) {
        if (!strcmp(KEYS[n].name, name)) return(KEYS[n].code);
    }
    int code = name[1] ? 0 : key_of[(unsigned char)name[0]];
    if (!code) {
        fprintf(stderr, "the machine has no key for %s\n", name);
        exit(EXIT_FAILURE);
    }
    return(code);
}

// The machine is first run here, from its prompt.  If what was typed
// there threw, it never got to its desktop, and what was sent for the
// desktop would be read as a line with no end: so nothing is sent with
// the line that starts the desktop but what stops the machine, and it is
// seen to have started before it is sent more.
const char* desk_boot(void) {
    static const char stops[] = "%d execute";
    sent += snprintf(NULL, 0, stops, stopping()) + 1;   // which the desktop takes a turn a character
    line(stops, stopping());
    taken();
    if (!strstr(said, "desktop") || strstr(said, "error")) {
        printf("FAIL the machine did not start its desktop.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
    char* console = malloc(strlen(said) + 1);           // it is kept: what the machine says next is said elsewhere
    if (console == NULL) {
        fprintf(stderr, "there is no room for what the console said\n");
        exit(EXIT_FAILURE);
    }
    strcpy(console, said);
    desk_turns(read_tiff(picture).h + 80);      // the picture comes down a row a turn
    return(console);
}

void desk_type(const char* text) {
    for(; *text; text++) {
        char name[2] = { *text, '\0' };
        int code = code_of(name);
        bool shift = shifted[(unsigned char)*text];
        if (shift) event('k', code_of("shift"), '1');
        event('k', code, '1');
        event('k', code, '0');
        if (shift) event('k', code_of("shift"), '0');
    }
}

void desk_key(const char* name, const char* with) {
    if (with) event('k', code_of(with), '1');
    event('k', code_of(name), '1');
    event('k', code_of(name), '0');
    if (with) event('k', code_of(with), '0');
}

// The pointer's place is said as a tablet says it: of 32,768 across and down.
void desk_pointer(int x, int y) {
    int vx = x * 0x8000 / W + 1, vy = y * 0x8000 / H + 1;
    char bytes[5] = { 'm', (char)(vx >> 8), (char)vx, (char)(vy >> 8), (char)vy };
    keys(bytes, 5);
    sent++;
}

void desk_button(bool down) {
    keys(down ? "b1" : "b0", 2);
    sent++;
}

void desk_turns(int turns) {
    for(int n=0; n < turns; n++) keys(".", 1);
    sent += turns;
}

const uint8_t* desk_screen(void) {
    taken();
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) screen[(size_t)y * W + x] = black_at(x, y);
    }
    return(screen);
}

const uint8_t* desk_memory(size_t* bytes) {
    taken();
    *bytes = sizeof(machine.memory);
    return((const uint8_t*)machine.memory);
}

void desk_block(long n, int count, uint8_t* bytes) {
    taken();
    fflush(machine.DISKS[0]);
    FILE* in = fopen(disk_path, "rb");
    memset(bytes, 0, (size_t)count * DISK_BLOCK);
    if (in == NULL || fseek(in, n * DISK_BLOCK, SEEK_SET) || !fread(bytes, 1, (size_t)count * DISK_BLOCK, in)) {
        perror(disk_path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
}

// A copy of the disk, which the machine is given.  The disk is read whole
// before the copy is made, so that a copy that is to be where the disk is
// does not empty it; and a disk with nothing on it is not a disk.
static void disk_is(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    if (in == NULL) {
        perror(from);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    size_t len = ftell(in);
    fseek(in, 0, SEEK_SET);
    char* bytes = malloc(len + 1);
    if (len < DISK_BLOCK || fread(bytes, 1, len, in) != len) {
        fprintf(stderr, "%s is not a disk: it is %ld bytes\n", from, (long)len);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    FILE* out = fopen(to, "wb");
    if (out == NULL || fwrite(bytes, 1, len, out) != len) {
        perror(to);
        exit(EXIT_FAILURE);
    }
    fclose(out);
    free(bytes);
    machine.DISKS[0] = fopen(to, "r+b");
    machine.DISKBLOCKS[0] = (int64_t)(len / DISK_BLOCK);
    disk_path = to;
}

// The second disk, which has the picture: as it is, since nothing writes
// to it.
static void pictures_are(const char* path) {
    FILE* in = fopen(path, "rb");
    if (in == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fseek(in, 0, SEEK_END);
    machine.DISKS[1] = in;
    machine.DISKBLOCKS[1] = (int64_t)(ftell(in) / DISK_BLOCK);
}

desk deskvm_start(const char* image, const char* disk, const char* pictures, const char* is_picture, const char* out,
                  bool names, bool rom_only) {
    static const char file[] = "/disk.img";
    char* copy = malloc(strlen(out) + sizeof(file));
    if (copy == NULL) {
        fprintf(stderr, "there is no room for the name of the disk's copy\n");
        exit(EXIT_FAILURE);
    }
    sprintf(copy, "%s%s", out, file);
    image_path = image;
    out_dir = out;
    picture = is_picture;
    screen = malloc((size_t)W * H);

    // A machine started from its ROM alone has nothing else in memory: it
    // reads the rest of its image from the disk, which is then one it can
    // start from (mkdisk --boot-desk), and compiles what the disk's block 1
    // lists before it is at its prompt (doc/boot.md).
    nucleus_is(image);
    start();
    if (rom_only) memset((uint8_t*)machine.memory + KROM_BYTES, 0, sizeof(machine.memory) - KROM_BYTES);
    // What stops it besides its halt (vm_stopped.h): running where nothing
    // was put, and running on.  A boot that compiles the system takes no
    // key for some hundred million instructions; this is twenty times that.
    if (!STOPS.budget) STOPS.budget = 2000000000ull;
    stops_watch_code(&machine);
    stopped_short = stopped_wrong;
    disk_is(disk, copy);
    pictures_are(pictures);
    if (rom_only) {             // it boots, and is at its prompt: an error in a block of the disk is said
        type();
        if (strstr(said, "error")) {
            printf("FAIL the machine's boot from the disk said an error:\n%s\n", said);
            exit(EXIT_FAILURE);
        }
        if (getenv("BOOTED")) {         // its memory as its boot left it: nothing has been typed at its prompt
            FILE* booted = fopen(getenv("BOOTED"), "wb");
            if (booted == NULL || fwrite(machine.memory, 1, sizeof(machine.memory), booted) != sizeof(machine.memory)) {
                perror(getenv("BOOTED"));
                exit(EXIT_FAILURE);
            }
            fclose(booted);
        }
    }
    going_on();
    if (rom_only) {             // the last word of the last file its boot compiles (desk.fs)
        line("' windows");
        type_more();
        if (machine.SP != 1) {
            printf("FAIL the machine did not compile its desktop from the disk.  It said:\n%s\n", said);
            exit(EXIT_FAILURE);
        }
    }

    // Which key is which character: the lowest code that gives it, with
    // no Shift if it can be.  The map is a character for each code, and
    // after those the character for each with Shift held.
    line(": km dup 64 + swap do keymap i 3 lshift char@ loop ;");
    for(int from=0; from < 256; from += 64) {
        line("%d km", from);
        type_more();
        if (machine.SP != 64) {
            printf("FAIL the machine did not say what its keys are.  It said:\n%s\n", said);
            exit(EXIT_FAILURE);
        }
        for(int at=from; at < from + 64; at++) {
            int c = (int)left(from + 63 - at) & 255;
            if (c && (at & 127) && !key_of[c]) key_of[c] = at & 127, shifted[c] = at > 127;
        }
    }

    // The keys and the pointer: what a turn of the event loop takes from
    // the console.  k is a key, m the pointer's place, b its button, s is
    // to stop, and anything else is nothing.  A device says each thing
    // and then that it has said all there is (type 0).  These are the
    // newest words of the dictionary, tp the newest.  Under the desktop,
    // at the bottom of the data stack, are two cells that are not used:
    // what goes on with the machine does not keep them (nucleus.h).
    line(": k2 key 8 lshift key or ;");
    line(": ek 1 key key 48 - on-event ;");
    line(": em 3 0 k2 on-event 3 1 k2 on-event 0 0 0 on-event ;");
    line(": eb 1 272 key 48 - on-event 0 0 0 on-event ;");
    line(": t1 dup 107 = if ek then dup 109 = if em then ;");
    line(": t2 dup 98 = if eb then dup 115 = if %d execute then ;", stopping());
    line(": tp key t1 t2 drop ;");
    line("0 0 display-init ' tp 'poll ! windows");

    desk on = { names, picture, out };
    return(on);
}
