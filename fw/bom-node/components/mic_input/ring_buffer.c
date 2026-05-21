
#include "ring_buffer.h"

#include <assert.h>
#include <stdlib.h>

void rb_init(rb_struct *rb, int samples) {
  rb->data = (int16_t *)calloc(samples, sizeof(int16_t));
  assert(rb->data != NULL);
  rb->size = samples;
  rb->head = 0;
}

void rb_free(rb_struct *rb) {
  if (rb->data) {
    free(rb->data);
    rb->data = NULL;
    rb->size = 0;
    rb->head = 0;
  }
}

void rb_push(rb_struct *rb, int16_t value) {
  rb->data[rb->head] = value;
  rb->head++;
  if (rb->head >= rb->size) {
    rb->head = 0;
  }
}

void rb_copy_tail(const rb_struct *rb, int16_t *out_arr, int offset,
                  int count) {
  int start = ((rb->head - offset - count) % rb->size + rb->size) % rb->size;
  for (int i = 0; i < count; i++) {
    out_arr[i] = rb->data[(start + i) % rb->size];
  }
}
