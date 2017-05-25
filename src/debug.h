#ifndef TSDP_DEBUG_H
#define TSDP_DEBUG_H

#include <stdio.h>

#ifdef TSDP_DEBUG
#  define DEBUG(e) {e}
#  define debugf(s,...) fprintf(stderr, "D> %s:%d @%s(): " s, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#  define DEBUG(e)
#  define debugf(s,...)
#endif

#endif
