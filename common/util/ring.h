/*
 * ring.h - single-producer / single-consumer byte ring (lock free).
 * One side (e.g. an interrupt) only moves head, the other only tail.
 * Size must be a power of two.
 */
#ifndef COMMON_RING_H
#define COMMON_RING_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint8_t *buf;
  uint32_t mask;              /* size - 1 */
  volatile uint32_t head;     /* write index (producer) */
  volatile uint32_t tail;     /* read index (consumer) */
} Ring;

static inline void Ring_Init(Ring *r, uint8_t *buf, uint32_t size)
{
  r->buf = buf;
  r->mask = size - 1u;
  r->head = 0;
  r->tail = 0;
}

static inline uint32_t Ring_Count(const Ring *r)  { return (r->head - r->tail) & r->mask; }
static inline uint32_t Ring_Free(const Ring *r)   { return r->mask - Ring_Count(r); }
static inline int Ring_Empty(const Ring *r)       { return r->head == r->tail; }

static inline int Ring_Put(Ring *r, uint8_t b)
{
  uint32_t h = r->head;
  uint32_t next = (h + 1u) & r->mask;
  if (next == r->tail) return 0;   /* full */
  r->buf[h] = b;
  r->head = next;
  return 1;
}

static inline int Ring_Get(Ring *r, uint8_t *b)
{
  uint32_t t = r->tail;
  if (t == r->head) return 0;      /* empty */
  *b = r->buf[t];
  r->tail = (t + 1u) & r->mask;
  return 1;
}

#endif
