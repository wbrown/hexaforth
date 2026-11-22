#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "../vm_constants.h"

// What vm_constants.h holds the machine it is compiled for to, of the
// machine a program is told when it runs.
static const char* unfit(const machine_sizes* m) {
    if (m->cell_bits != 16 && m->cell_bits != 32 && m->cell_bits != 64) return("a cell is 16, 32 or 64 bits");
    if (m->addr_bits < 1 || m->addr_bits > m->cell_bits) return("an address is a cell");
    if (((uint64_t)(m->cell_bits / 8) << m->addr_bits) > MEM_MOST) return("it has more memory than a program has room for");
    if (m->disp_w == 0) return(NULL);
    if (m->disp_w > DISP_W_MOST || m->disp_h > DISP_H_MOST) return("its display is larger than a program has room for");
    if (m->disp_w % m->cell_bits) return("display: the width is not a whole number of cells");
    if (m->disp_stride * m->cell_bits < m->disp_w) return("display: a row is longer than the stride");
    if (m->disp_banks < 1) return("display: there must be at least one bank");
    if (m->disp_banks > 1 && m->disp_bank_step < (m->disp_h + m->disp_banks - 1) / m->disp_banks * m->disp_stride) {
        return("display: the banks overlap");
    }
    if (!DISP_ONE_LITERAL((unsigned long long)m->disp_base)) return("display: the base is not a single shifted literal");
    return(NULL);
}

static int is_said;                 // whether the machine has been said, by an image or to the program
static char* said_by;               // the image that said it, if one did

static int same_machine(const machine_sizes* a, const machine_sizes* b) {
    return(a->cell_bits == b->cell_bits && a->addr_bits == b->addr_bits && a->disp_w == b->disp_w &&
           a->disp_h == b->disp_h && a->disp_base == b->disp_base && a->disp_left_low == b->disp_left_low &&
           a->disp_ink_black == b->disp_ink_black && a->disp_stride == b->disp_stride &&
           a->disp_banks == b->disp_banks && a->disp_bank_step == b->disp_bank_step);
}

int machine_of(const char* path) {
    const char* dot = strrchr(path, '.');
    size_t stem = dot && !strchr(dot, '/') ? (size_t)(dot - path) : strlen(path);
    char* said = malloc(stem + sizeof(".machine"));
    memcpy(said, path, stem);
    strcpy(said + stem, ".machine");
    FILE* in = fopen(said, "r");
    if (in == NULL) {
        free(said);
        return(0);
    }
    machine_sizes m;
    int got = fscanf(in, "%d %d %d %d %lld %d %d %d %d %d", &m.cell_bits, &m.addr_bits, &m.disp_w, &m.disp_h, &m.disp_base,
                     &m.disp_left_low, &m.disp_ink_black, &m.disp_stride, &m.disp_banks, &m.disp_bank_step);
    fclose(in);
    const char* why = got == 10 ? unfit(&m) : "it does not say ten sizes";
    if (why) {
        fprintf(stderr, "%s is not a machine: %s\n", said, why);
        exit(EXIT_FAILURE);
    }
    free(said);
    if (said_by && !same_machine(&m, &THIS_MACHINE)) {
        fprintf(stderr, "%s is for another machine than %s: a program is of one machine\n", path, said_by);
        exit(EXIT_FAILURE);
    }
    if (!said_by) {
        said_by = malloc(strlen(path) + 1);
        strcpy(said_by, path);
    }
    THIS_MACHINE = m;
    is_said = 1;
    return(1);
}

void machine_said(int cell_bits, int addr_bits, int disp_w, int disp_h, long long disp_base) {
    machine_sizes m = THIS_MACHINE;
    m.cell_bits = cell_bits;
    m.addr_bits = addr_bits < 0 ? ADDR_BITS_OF(cell_bits) : addr_bits;
    m.disp_w = disp_w < 0 ? DISP_W_OF(cell_bits) : disp_w;
    m.disp_h = disp_h < 0 ? DISP_H_OF(cell_bits) : disp_h;
    if (disp_base >= 0) m.disp_base = disp_base;
    m.disp_stride = cell_bits ? m.disp_w / cell_bits : 0;
    const char* why = unfit(&m);
    if (why) {
        fprintf(stderr, "a machine of cells of %d bits and 2^%d of them, with a display of %d by %d at %lld, is not one: %s\n",
                m.cell_bits, m.addr_bits, m.disp_w, m.disp_h, m.disp_base, why);
        exit(EXIT_FAILURE);
    }
    THIS_MACHINE = m;
    is_said = 1;
}

void machine_from(const char* path) {
    if (machine_of(path) || is_said) return;
    fprintf(stderr, "%s does not say its machine, and none has been said: cross.fs writes the file ending .machine "
                    "beside an image, which is the sizes of the machine the image is for\n", path);
    exit(EXIT_FAILURE);
}

size_t load_image(const char* path, uint8_t** bytes) {
    FILE* hexfile = fopen(path, "r");
    if (hexfile == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    machine_from(path);

    char line[100];
    size_t len = 0;
    size_t room = 1 << 16;
    *bytes = malloc(room);
    while (fgets(line, sizeof(line), hexfile)) {
        if (!isxdigit(line[0])) continue;
        if (len + 4 + 8 > room) {
            room *= 2;
            *bytes = realloc(*bytes, room);
        }
        uint32_t four = (uint32_t)strtoul(line, NULL, 16);
        for(int idx=0; idx < 4; idx++) {
            (*bytes)[len++] = four >> (8 * idx);
        }
    }
    fclose(hexfile);
    memset(*bytes + len, 0, 8);
    return(len);
}
