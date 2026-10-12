/**
 * @file app_mem.h
 * @brief Newlib heap watermark (pairs with sysmem.c _sbrk).
 */
#ifndef APP_MEM_H
#define APP_MEM_H

#include <stdint.h>

/**
 * @brief Bytes used vs total newlib heap (from _end to stack reserve).
 *        used is 0 until the first malloc/_sbrk.
 */
void AppMem_HeapStats(uint32_t *used_bytes, uint32_t *total_bytes);

#endif /* APP_MEM_H */
