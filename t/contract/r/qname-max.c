#include <tsdp.h>

int main(int argc, char **argv)
{
	return qname_parse(argv[1]) == NULL ? 0 : 1;
}
