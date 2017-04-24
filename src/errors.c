#include <tsdp.h>
#include <string.h>
#include <errno.h>

#define ERROR_BASE 513
static const char * ERRORS[] = {
	"Invalid TSDP MSG version",
	"Invalid TSDP MSG opcode",
	"Invalid TSDP MSG flag",
	"Invalid TSDP MSG payload",
	"Invalid frame count for TSDP MSG",
	"Invalid frame type in TSDP MSG",
	NULL,
};

const char *
tsdp_strerror(int num)
{
	static int max = 0;
	static char buf[256];

	if (num < 0) num *= -1;
	for (; ERRORS[max]; max++);
	if (num < ERROR_BASE)            snprintf(buf, 256, "%s (system error %d)", strerror(num), num);
	else if (num - ERROR_BASE < max) snprintf(buf, 256, "%s (bolo error %d)", ERRORS[num - ERROR_BASE], num);
	else                             snprintf(buf, 256, "Unrecognized failure (error %d)", num);
	return buf;
}

void
tsdp_perror(const char *prefix)
{
	const char *e = tsdp_strerror(errno);
	if (prefix) fprintf(stderr, "%s: %s\n", prefix, e);
	else        fprintf(stderr, "%s\n", e);
}
