/* Original absolute-time codec. No host calendar, timezone or locale state. */
#ifndef TSFP_GUEST_TIME_H
#define TSFP_GUEST_TIME_H
#include <stddef.h>
/* TIME_FIELDS: eight little-endian 16-bit fields: year, month, day, hour,
 * minute, second, millisecond, weekday. Weekday input is ignored.
 * Absolute time: eight little-endian bytes, 100ns since 1601-01-01 UTC.
 * Invalid/null/short input or output preserves output. Unaligned and
 * overlapping buffers are supported. Decode discards sub-millisecond ticks.
 * Encode accepts valid Gregorian dates in 1601..30827; decode accepts the
 * full nonnegative signed64 range (including part of year30828). */
int tsfp_time_fields_to_ticks(const void *, size_t, void *, size_t);
int tsfp_time_ticks_to_fields(const void *, size_t, void *, size_t);
#endif
