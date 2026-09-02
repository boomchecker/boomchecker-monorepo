#ifndef __CMSIS_COMPILER_H
#define __CMSIS_COMPILER_H

#include <stdint.h>

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

#ifndef __STATIC_FORCEINLINE
#define __STATIC_FORCEINLINE static inline
#endif

#ifndef __ALIGNED
#define __ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#ifndef __PACKED
#define __PACKED __attribute__((packed, aligned(1)))
#endif

#ifndef __SSAT
static inline int32_t __SSAT(int32_t val, uint32_t sat) {
    if (sat >= 32) return val;
    int32_t max = (1 << (sat - 1)) - 1;
    int32_t min = -(1 << (sat - 1));
    if (val > max) return max;
    if (val < min) return min;
    return val;
}
#endif

#ifndef __CLZ
static inline uint32_t __CLZ(uint32_t data) {
    if (data == 0) return 32;
    return (uint32_t)__builtin_clz(data);
}
#endif

#ifndef __ROR
static inline uint32_t __ROR(uint32_t op1, uint32_t op2) {
    op2 %= 32;
    if (op2 == 0) return op1;
    return (op1 >> op2) | (op1 << (32 - op2));
}
#endif

#endif
