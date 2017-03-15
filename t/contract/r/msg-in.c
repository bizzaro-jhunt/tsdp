#include <tsdp.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	char buf[8192];
	ssize_t n;
	n = read(0, buf, 8192);
	if (n > 0 && n != 8192) {
		size_t left;
		struct tsdp_msg *m;

		m = tsdp_msg_unpack(buf, n, &left);
		if (!m) {
			printf("malformed message\n");
			return 0;
		}
		if (left)               printf("~~ %d octets left ~~\n", left);
		if (!tsdp_msg_valid(m)) printf("~~ BOGON DETECTED ~~\n");
		tsdp_msg_fdump(stdout, m);
		return 0;
	}
	return 1;
}
