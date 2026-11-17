//
// An image as cross.fs writes it: a line for every four bytes of it, eight
// hex digits, the first of the four bytes the lowest.
//

#ifndef HEXAFORTH_IMAGE_H
#define HEXAFORTH_IMAGE_H

#include <stddef.h>
#include <stdint.h>

// Read an image's bytes, and return how many there are.  The memory is the
// caller's to free, and has a cell of zeros after the last byte.
size_t load_image(const char* path, uint8_t** bytes);

#endif //HEXAFORTH_IMAGE_H
