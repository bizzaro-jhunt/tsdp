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
	struct tsdp_qname *n1, *n2;
	while ( (fgets(buf, 8192, stdin)) != NULL ) {
		n1 = tsdp_qname_parse(chomp(buf));
		n2 = tsdp_qname_dup(n1);
		tsdp_qname_free(n1);
		memset(buf, 0, 8192);
		printf("%s\n", tsdp_qname_string(n2));
	}
	return 0;
}
