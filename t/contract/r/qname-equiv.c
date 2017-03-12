#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct tsdp_qname *a, *b;
	char mode;
	int rc;

	if (argc != 4) {
		fprintf(stderr, "incorrect usage.  try %s a=b '~' c=d\n", argv[1]);
		return 2;
	}

	a = strcmp(argv[1], "<nil>") == 0 ? INVALID_QNAME : tsdp_qname_parse(argv[1]);
	mode = argv[2][0];
	b = strcmp(argv[3], "<nil>") == 0 ? INVALID_QNAME : tsdp_qname_parse(argv[3]);
	rc = tsdp_qname_equal(a,b);
	tsdp_qname_free(a);
	tsdp_qname_free(b);

	switch (mode) {
	case '~': return (rc ? 0 : 1);
	case '!': return (rc ? 1 : 0);
	}
	fprintf(stderr, "incorrect usage.  comparison must be '~' or '!' (not '%c')\n", argv[2][0]);
	return 2;
}
