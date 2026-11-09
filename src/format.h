/* Printing milli-degrees without floating point: 31250 → "31.250", -1500 → "-1.500". */
#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>

#define MILLI_FMT "%s%u.%03u"
#define MILLI_ARGS(m) ((m) < 0 ? "-" : ""), milli_abs(m) / 1000U, milli_abs(m) % 1000U

static inline unsigned int milli_abs(int32_t m)
{
	return m < 0 ? 0U - (unsigned int)m : (unsigned int)m;
}

#endif
