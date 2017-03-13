#include <tsdp.h>

int main(int argc, char **argv)
{
	return tsdp_qname_parse(argv[1]) == INVALID_QNAME ? 0 : 1;
}
