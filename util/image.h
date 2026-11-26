//
// Reads an image as cross.fs writes it.  Each line of the file is four
// bytes of the image as eight hex digits, with the first of the four bytes
// in the lowest digits.
//

#ifndef HEXAFORTH_IMAGE_H
#define HEXAFORTH_IMAGE_H

#include <stddef.h>
#include <stdint.h>

// Reads an image's bytes and returns how many there are.  The caller frees
// the buffer, which has a cell of zeros after the last byte.  This also
// sets the program's machine to the image's (machine_from).
size_t load_image(const char* path, uint8_t** bytes);

// Sets the program's machine (THIS_MACHINE, vm_constants.h) to the machine
// an image was built for.  cross.fs writes that machine beside the image,
// in a file with the image's name and the extension .machine.  The file is
// one line of the sizes basewords.fs gave cross.fs: the cell size in bits,
// the address size in bits, and the display's width, height, first cell,
// whether its leftmost pixel is a cell's lowest bit, whether a set bit is
// black, its stride, its number of banks, and the cells from one bank to
// the next.  Returns 1 if the file exists and 0 if it does not.
//
// A program works for one machine, so call this before any size is used.
// The program exits if the image names a different machine from an image
// read earlier, or a machine that vm_constants.h would have refused to
// compile.
int machine_of(const char* image_path);

// The same, for a program that needs to know the machine.  If the image
// has no .machine file, the program keeps the machine it was already
// given, by another image or by machine_said().  If it was given none, the
// program exits, because it would otherwise assume a machine.
void machine_from(const char* image_path);

// Sets the program's machine from sizes, in the form util/machines lists
// them: the cell size in bits, the address size in bits, and the display's
// width, height and first cell.  A negative argument means "not given",
// and takes the default vm_constants.h has for that cell size.  This is
// for a tool that builds something for a machine without being given an
// image.
void machine_said(int cell_bits, int addr_bits, int disp_w, int disp_h, long long disp_base);

// Returns the path of the nucleus that CMake built for the machine with
// `bits`-bit cells ("16", "32" or "64"), or for the first machine in
// util/machines if `bits` is NULL.  The program exits with a message if
// that file is missing.  CMake gives util/image.c the build directory and
// the first machine (cmake/machines.cmake).
const char* built_nucleus(const char* bits);

#endif //HEXAFORTH_IMAGE_H
