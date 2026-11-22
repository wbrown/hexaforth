//
// An image as cross.fs writes it: a line for every four bytes of it, eight
// hex digits, the first of the four bytes the lowest.
//

#ifndef HEXAFORTH_IMAGE_H
#define HEXAFORTH_IMAGE_H

#include <stddef.h>
#include <stdint.h>

// Read an image's bytes, and return how many there are.  The memory is the
// caller's to free, and has a cell of zeros after the last byte.  The
// program is then of the image's machine (machine_from).
size_t load_image(const char* path, uint8_t** bytes);

// The machine an image is for.  cross.fs says it beside the image, in the
// file named as the image is and ending .machine: a line of the sizes
// basewords.fs gave it, the bits of a cell, the bits of an address, and the
// display's width, height, first cell, whether its leftmost pixel is a
// cell's lowest bit, whether a set bit is black, its stride, its banks and
// the cells from a bank to the next.  That is made the program's machine
// (THIS_MACHINE, vm_constants.h), and whether there was such a file is
// returned: a program is of one machine, so this is before it has used a
// size, and an image that says another machine than an image before it
// said stops the program.  A machine that vm_constants.h would not have had
// compiled is not had here either, and stops the program.
int machine_of(const char* image_path);

// The same, for a program that is to be of the image's machine.  An image
// that does not say its machine is of the machine the program has been said
// already, by another image or to the program (machine_said); if none has
// been said, the program is stopped: it would be of a machine nothing said.
void machine_from(const char* image_path);

// The machine, said as util/machines says one: the bits of a cell, the
// bits of an address, and its display's width, height and first cell.
// What is less than nothing is not said, and is what vm_constants.h gives
// a cell of that size.  For what makes a thing for a machine and is given
// no image of it.
void machine_said(int cell_bits, int addr_bits, int disp_w, int disp_h, long long disp_base);

#endif //HEXAFORTH_IMAGE_H
