#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tsdp.h>

char* chomp(char *s)
{
	char *nl = strrchr(s, '\n');
	if (nl) {
		*nl = '\0';
	}
	return s;
}

int main(int argc, char **argv)
{
	char buf[8192];
	while ( (fgets(buf, 8192, stdin)) != NULL ) {
		printf("%s\n", tsdp_qname_string(tsdp_qname_parse(chomp(buf))));
	}
	return 0;
}
