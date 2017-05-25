#include <stdio.h>
#include <string.h>
#include <tsdp.h>

int main(int argc, char **argv)
{
	struct qname *qn;
	char s[8192], *out;

	qn = qname_parse(NULL);
	if (qn != NULL) {
		fprintf(stderr, "oops.  qname_parse(NULL) != NULL (and really should have)\n");
		return 1;
	}

	memset(s, 'v', 8191);
	memcpy(s, "cpu k=", 6);
	s[8191] = '\0';
	if (strlen(s) != 8191) {
		fprintf(stderr, "oops.  our test setup is borked; strlen(s) != 8191\n");
		return 2;
	}
	qn = qname_parse(s);
	if (qn != NULL) {
		fprintf(stderr, "oops.  qname_parse() didn't fail with a qname that was 8191 octets long\n");
		return 3;
	}

	s[4096] = '\0';
	if (strlen(s) != 4096) {
		fprintf(stderr, "oops.  our test setup is borked; strlen(s) != 4096\n");
		return 4;
	}
	qn = qname_parse(s);
	if (qn != NULL) {
		fprintf(stderr, "oops.  qname_parse() didn't fail with a qname that was 4096octets long\n");
		return 5;
	}

	s[4095] = '\0';
	if (strlen(s) != 4095) {
		fprintf(stderr, "oops.  our test setup is borked; strlen(s) != 4095\n");
		return 6;
	}
	qn = qname_parse(s);
	if (qn == NULL) {
		fprintf(stderr, "oops.  qname_parse() failed with a qname that was 4095 octets long (at the max, but not over)\n");
		return 7;
	}

	out = qname_string(NULL);
	if (!out) {
		fprintf(stderr, "oops.  qname_string(NULL) return a NULL string.");
		return 8;
	}
	if (*out) {
		fprintf(stderr, "oops.  qname_string(NULL) return a non-empty string '%s'.", out);
		return 9;
	}

	return 0;
}
