#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

typedef struct {
  int16_t *data;
  int size;
  int head;
} rb_struct;

void rb_init(rb_struct *rb, int samples);
void rb_free(rb_struct *rb);
void rb_push(rb_struct *rb, int16_t value);
void rb_copy_tail(const rb_struct *rb, int16_t *out_arr, int offset, int count);

#endif
