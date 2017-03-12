#include <tsdp.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	struct tsdp_msg *m;
	char buf[8192];
	ssize_t n;
	size_t left;

	n = read(0, buf, 8192);
	if (n > 0 && n != 8192) {
		m = tsdp_msg_unpack(buf, n, &left);
		if (tsdp_msg_valid(m)) {
			tsdp_msg_fdump(stdout, m);
		}
	}
	return 0;
}
