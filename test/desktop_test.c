//
// desktop_test IMAGE.hex DISK.img PICTURE.tiff FORTH OUT [names]
//
// The desktop on the reference VM, and what is on its screen.
//
// The checks are those of deskcheck.c, and this is the machine they are
// run on: the nucleus on vm.c (nucleus.h), which is built into the test
// for a machine of 64 bits, 32 or 16.
//
//   the disk       the VM's own, which vmdisk.fs is the driver for: a copy
//                  of DISK.img, in OUT, for the editor and HexaPaint to
//                  write to.  mkdisk made it as it makes QEMU's: block 1
//                  for the editor, the picture as a TIFF, which
//                  PICTURE.tiff is too, and the tables of names if the
//                  last argument says the build made any.
//   keys, pointer  the VM has none, so the test is them: a word in 'poll,
//                  typed at the prompt, which the event loop runs each time
//                  round.  It takes one thing from the console: a key going
//                  down or up, by its Linux code, as a virtio keyboard
//                  gives them; the pointer's place; its button; nothing;
//                  or to stop.  It hands events to on-event, as any driver
//                  does.
//   the screen     the display in the VM's memory.  When the test wants to
//                  look, the last thing it sends is to stop: the machine
//                  stops in its event loop when it has taken everything
//                  sent before, and is gone on with from there.
//
// The desktop takes exactly one turn of its loop for each thing sent, so
// the test is the same every time it is run.  Which key is which character
// is asked of the machine, which has the map.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../bench/tiff.h"
#include "deskcheck.h"
#include "nucleus.h"
#include "tables.h"

#define W   DISP_W
#define H   DISP_H

#ifdef COUNTING_A_TEST
void counts_written(void);
#endif

// The keys that are not a character, by the names deskcheck.c has for
// them: their Linux codes.
static const struct { const char* name; int code; } KEYS[] = {
    {"ret", 28}, {"backspace", 14}, {"up", 103}, {"down", 108}, {"left", 105}, {"right", 106},
    {"pgup", 104}, {"pgdn", 109}, {"ctrl", 29}, {"shift", 42}, {NULL}};

static int key_of[256];         // the key that gives each character,
static bool shifted[256];       // and whether Shift is held for it
static const char* disk_path;   // the copy of the disk
static const char* picture;
static bool started;            // the machine has left its prompt for the desktop
static uint8_t* screen;         // its screen, when it was last asked for

// The machine takes what has been sent and then stops, in its event loop.
static void taken(void) {
    keys("s", 1);
    if (started) go_on(); else set_off();
    started = true;
    if (machine.EIP != stopping()) {
        printf("FAIL the machine stopped where it was not sent.  It said:\n%s\n", said);
        exit(EXIT_FAILURE);
    }
}

static void event(char what, int a, int b) {
    char bytes[3] = { what, (char)a, (char)b };
    keys(bytes, 3);
}

static int code_of(const char* name) {
    for(int n=0; KEYS[n].name; n++) {
        if (!strcmp(KEYS[n].name, name)) return(KEYS[n].code);
    }
    int code = name[1] ? 0 : key_of[(unsigned char)name[0]];
    if (!code) {
        fprintf(stderr, "desktop_test: the machine has no key for %s\n", name);
        exit(EXIT_FAILURE);
    }
    return(code);
}

const char* desk_boot(void) {
    static char console[sizeof(said)];
    desk_turns(read_tiff(picture).h + 80);      // the picture comes down a row a turn
    taken();
    memcpy(console, said, sizeof(said));
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
}

void desk_button(bool down) {
    keys(down ? "b1" : "b0", 2);
}

void desk_turns(int turns) {
    for(int n=0; n < turns; n++) keys(".", 1);
}

const uint8_t* desk_screen(void) {
    taken();
    for(int y=0; y < H; y++) {
        for(int x=0; x < W; x++) {
            uint64_t cell = fetch(DISP_ROW(y) + x / CELL_BITS);
            int bit = x % CELL_BITS;
            bool set = cell >> (DISP_LEFT_LOW ? bit : CELL_BITS - 1 - bit) & 1;
            screen[(size_t)y * W + x] = DISP_INK_BLACK ? set : !set;
        }
    }
    return(screen);
}

void desk_block(long n, int count, uint8_t* bytes) {
    taken();
    fflush(machine.DISK);
    FILE* in = fopen(disk_path, "rb");
    memset(bytes, 0, (size_t)count * DISK_BLOCK);
    if (in == NULL || fseek(in, n * DISK_BLOCK, SEEK_SET) || !fread(bytes, 1, (size_t)count * DISK_BLOCK, in)) {
        perror(disk_path);
        exit(EXIT_FAILURE);
    }
    fclose(in);
}

// A copy of the disk, which the machine is given.
static void disk_is(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    FILE* out = fopen(to, "wb");
    if (in == NULL || out == NULL) {
        perror(in == NULL ? from : to);
        exit(EXIT_FAILURE);
    }
    char bytes[1 << 16];
    size_t n, len = 0;
    while ((n = fread(bytes, 1, sizeof(bytes), in)) > 0) len += fwrite(bytes, 1, n, out);
    fclose(in);
    fclose(out);
    machine.DISK = fopen(to, "r+b");
    machine.DISKBLOCKS = (int64_t)(len / DISK_BLOCK);
    disk_path = to;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s IMAGE.hex DISK.img PICTURE.tiff FORTH OUT [names]\n", argv[0]);
        return(1);
    }
    alarm(1800);                // a machine that does not stop is stopped
    static char copy[512];
    snprintf(copy, sizeof(copy), "%s/disk.img", argv[5]);
    picture = argv[3];
    screen = malloc((size_t)W * H);

    nucleus_is(argv[1]);
    tables_are(argv[4]);
    start();
    disk_is(argv[2], copy);
    going_on();

    // Which key is which character: the lowest code that gives it, with
    // no Shift if it can be.  The map is a character for each code, and
    // after those the character for each with Shift held.
    line(": km dup 64 + swap do keymap i 3 lshift char@ loop ;");
    for(int from=0; from < 256; from += 64) {
        line("%d km", from);
        type_more();
        if (machine.SP != 64) {
            printf("FAIL the machine did not say what its keys are.  It said:\n%s\n", said);
            return(1);
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
    line("0 0 display-init vmdisk-init ' tp 'poll ! windows");

    desk on = { {"tp", "t2"}, " key t1 t2 drop", argc > 6, picture, argv[5] };
    int failed = desk_checks(&on);

    // For whoever wants what the desktop left, or what it ran: the
    // machine's memory, written to the file the environment's MEMORY
    // names; and, if the VM in this is the one that counts
    // (bench/counting.c), its counts.
    if (getenv("MEMORY")) {
        FILE* out = fopen(getenv("MEMORY"), "wb");
        if (out == NULL) {
            perror(getenv("MEMORY"));
            return(1);
        }
        fwrite(machine.memory, 1, sizeof(machine.memory), out);
        fclose(out);
    }
#ifdef COUNTING_A_TEST
    counts_written();
#endif
    printf("the desktop on the reference VM, cells of %d bits, %d by %d: %d checks failed; screens in %s/\n",
           CELL_BITS, W, H, failed, argv[5]);
    return(failed != 0);
}
