#include <stdio.h>
#include <stdlib.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	qname_string(qname_parse(argv[1]));
	if (malloc(1)) return 0;
	return 77;
}
