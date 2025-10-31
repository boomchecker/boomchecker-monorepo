
#include <stdint.h>

typedef struct {
  int *data;
  int size;
  int head;
} rb_struct;

void rb_init(rb_struct *rb, int samples);
void rb_free(rb_struct *rb);
void rb_push(rb_struct *rb, int value);
void rb_copy_tail(const rb_struct *rb, int *out_arr, int offset, int count);
