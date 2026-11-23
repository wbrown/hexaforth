//
// The reference VM as a machine the desktop's checks can run on.
// deskvm.h describes the interface.
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

// The Linux key codes of the keys that have no character, under the names
// deskcheck.c uses for them.
static const struct { const char* name; int code; } KEYS[] = {
    {"ret", 28}, {"backspace", 14}, {"up", 103}, {"down", 108}, {"left", 105}, {"right", 106},
    {"pgup", 104}, {"pgdn", 109}, {"ctrl", 29}, {"shift", 42}, {NULL}};

long deskvm_turns;

static int key_of[UCHAR_MAX + 1];       // the key code that gives each character
static bool shifted[UCHAR_MAX + 1];     // true if Shift is held for that character
static const char* image_path;  // the image the machine is running
static const char* disk_path;   // the copy of the disk
static const char* picture;
static const char* out_dir;     // the directory where the screens are saved
static bool started;            // true once the machine has left its prompt for the desktop
static uint8_t* screen;         // the screen as it was at the last desk_screen()
static long sent;               // the number of items sent that the machine has not read yet

// Called when the machine stops somewhere unexpected.  Prints the check
// and the action that were running and where the machine stopped
// (vm_stopped.h), and exits.
static void stopped_wrong(void) {
    printf("FAIL the machine stopped at 0x%04x (expected 0x%04x)%s%s%s%s%s\n"
           "  inspect with: build/cmake/disasm_hex %s 0x%04x 0x%04x\n"
           "  screens in:   %s/\n"
           "  console said: %s\n",
           (unsigned)machine.EIP, (unsigned)stopping(),
           desk_checking ? " during check \"" : "",
           desk_checking ? desk_checking : "",
           desk_checking ? "\"" : "",
           desk_doing && desk_doing[0] ? " while doing: " : "",
           desk_doing && desk_doing[0] ? desk_doing : "",
           image_path ? image_path : "IMAGE.hex",
           (unsigned)machine.EIP, (unsigned)machine.EIP,
           out_dir ? out_dir : ".", said);
    static const char file[] = "/stopped.memory";
    char* memory = malloc(strlen(out_dir ? out_dir : ".") + sizeof(file));
    if (memory) sprintf(memory, "%s%s", out_dir ? out_dir : ".", file);
    post_mortem(stdout, &machine, image_path, memory);
    exit(EXIT_FAILURE);
}

// Runs the machine until it has read everything sent and has stopped in
// its event loop.
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

// The machine runs for the first time here, from its prompt.  If the
// lines typed at the prompt threw an error, the machine never reached its
// desktop, and anything sent for the desktop would be read as a line that
// never ends.  So the only thing sent with the line that starts the
// desktop is the request to stop, and the desktop is confirmed to have
// started before anything else is sent.
const char* desk_boot(void) {
    static const char stops[] = "%d execute";
    sent += snprintf(NULL, 0, stops, stopping()) + 1;   // the desktop reads this line at one character per turn
    line(stops, stopping());
    taken();
    if (!strstr(said, "desktop") || strstr(said, "error")) {
        printf("FAIL the machine did not start its desktop.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
    char* console = malloc(strlen(said) + 1);           // keep a copy: `said` is replaced the next time the machine runs
    if (console == NULL) {
        fprintf(stderr, "there is no room for what the console said\n");
        exit(EXIT_FAILURE);
    }
    strcpy(console, said);
    desk_turns(read_tiff(picture).h + 80);      // the viewer draws one row of the picture per turn
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

// The pointer's position is sent as a tablet reports it: on a scale of
// 32,768 across and 32,768 down.
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
    *bytes = MEM_BYTES;
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

// Copies the disk and gives the machine the copy.  The whole disk is read
// before the copy is opened for writing, so that copying a disk onto
// itself does not empty it.  A file smaller than one block is rejected: it
// is not a disk.
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

// Opens the second disk, which has the picture.  It is used in place,
// without a copy, because nothing writes to it.
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

    // A machine started from its ROM alone has nothing else in memory.  It
    // reads the rest of its image from the disk, which must be one it can
    // start from (mkdisk --boot-desk), and compiles the files the disk's
    // block 1 lists before it reaches its prompt (doc/boot.md).
    nucleus_is(image);
    start();
    if (rom_only) memset((uint8_t*)machine.memory + KROM_BYTES, 0, MEM_BYTES - KROM_BYTES);
    // Besides a halt, two things stop the machine (vm_stopped.h): running
    // an address that was never written, and running past the budget.  A
    // boot that compiles the system reads no key for about a hundred
    // million instructions.  The budget is twenty times that.
    if (!STOPS.budget) STOPS.budget = 2000000000ull;
    stops_watch_code(&machine);
    stopped_short = stopped_wrong;
    disk_is(disk, copy);
    pictures_are(pictures);
    if (rom_only) {             // run the boot until the machine is at its prompt, and fail if it reported an error in a block
        type();
        if (strstr(said, "error")) {
            printf("FAIL the machine's boot from the disk said an error:\n%s\n", said);
            exit(EXIT_FAILURE);
        }
        if (getenv("BOOTED")) {         // save its memory as the boot left it, before anything is typed at its prompt
            FILE* booted = fopen(getenv("BOOTED"), "wb");
            if (booted == NULL || fwrite(machine.memory, 1, MEM_BYTES, booted) != MEM_BYTES) {
                perror(getenv("BOOTED"));
                exit(EXIT_FAILURE);
            }
            fclose(booted);
        }
    }
    going_on();
    if (rom_only) {             // `windows` is the last word of the last file the boot compiles (desk.fs)
        line("' windows");
        type_more();
        if (machine.SP != 1) {
            printf("FAIL the machine did not compile its desktop from the disk.  It said:\n%s\n", said);
            exit(EXIT_FAILURE);
        }
    }

    // Find the key for each character: the lowest key code that gives it,
    // without Shift if possible.  The machine's key map has one character
    // for each key code, followed by the character for each key code with
    // Shift held.
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

    // Define the keys and the pointer: the word that reads one item from
    // the console on each turn of the event loop.  k is a key, m is the
    // pointer's position, b is its button, s is a request to stop, and
    // any other item does nothing.  A device reports each event and then
    // reports that it has no more (an event of type 0).  These words are
    // the newest in the dictionary, and tp is the newest of them.  The two
    // zeros at the start of the last line leave two unused cells at the
    // bottom of the data stack, under the desktop, because resuming the
    // machine overwrites those two cells (nucleus.h).
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
