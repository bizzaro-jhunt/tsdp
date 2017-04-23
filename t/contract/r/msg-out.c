#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <tsdp.h>

#define OK(x) do {\
	if ((x) != 0) { \
		fprintf(stderr, "FAILED: %s returned non-zero\n", #x); \
		exit(1); \
	} \
} while (0)

#define NOTOK(x) do {\
	if ((x) == 0) { \
		fprintf(stderr, "FAILED: %s returned zero\n", #x); \
		exit(1); \
	} \
} while (0)

struct tsdp_msg *
repack(struct tsdp_msg *m)
{
	size_t len, left;
	void *buf;
	struct tsdp_msg *copy;

	len = tsdp_msg_pack(NULL, 0, m);
	if (len == 0) exit(5);
	buf = malloc(len);
	if (!buf) exit(4);

	tsdp_msg_pack(buf, len, m);
	copy = tsdp_msg_unpack(buf, len, &left);
	if (!copy) exit(4);

	free(buf);
	return copy;
}

void dump(struct tsdp_msg *m)
{
	size_t i, len;
	void *buf;
	struct tsdp_msg *m2;

	m2 = repack(m);
	len = tsdp_msg_pack(NULL, 0, m2);
	if (len == 0) exit(5);
	buf = malloc(len);
	if (!buf) exit(4);
	tsdp_msg_pack(buf, len, m2);

	printf("---\n");
	for (i = 0; i < len; i++) {
		printf(" %02x", ((const unsigned char *)buf)[i]);
		if (((i+1) % 16) == 0) printf("\n");
	}
	printf("\n\n");

	free(buf);
	tsdp_msg_free(m2);
}

int main(int argc, char **argv)
{
	struct tsdp_msg *m;
	void *buf;
	size_t len;
	int rc;

	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	float f32, f64;

	m = tsdp_msg_new(
			TSDP_PROTOCOL_V1,
			TSDP_OPCODE_HEARTBEAT,
			0x55,
			TSDP_PAYLOAD_SAMPLE|TSDP_PAYLOAD_TALLY);
	if (!m) exit(3);
	OK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 0));
	OK(tsdp_msg_extend(m, TSDP_FRAME_STRING, "test", 4));
	dump(m);
	tsdp_msg_free(m);

	m = tsdp_msg_new(
			TSDP_PROTOCOL_V1,
			TSDP_OPCODE_SUBSCRIBE,
			0x14, 0);
	if (!m) exit(3);
	u16 = 0x1234;               OK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u16, 2));
	u32 = 0xdeadbeef;           OK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u32, 4));
	u64 = 0xdecafbadabad1deaLU; OK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 8));
	f32 = 1.2345;               OK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f32, 4));
	f64 = 456789.1234567890123; OK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 8));
	                            OK(tsdp_msg_extend(m, TSDP_FRAME_STRING, "", 0));
	                            OK(tsdp_msg_extend(m, TSDP_FRAME_STRING, "hiya, world", 4)); /* deliberately short */
	                            OK(tsdp_msg_extend(m, TSDP_FRAME_STRING, "hello, world", 12));
	u64 = 0x1122334455667788LU; OK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 8));
	                            OK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 0));
	                            OK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 0));
	dump(m);
	tsdp_msg_free(m);

	m = tsdp_msg_new(
			TSDP_PROTOCOL_V1,
			TSDP_OPCODE_FORGET,
			0x42, TSDP_PAYLOAD_STATE|TSDP_PAYLOAD_EVENT);
	if (!m) exit(3);
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u16, 0));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u16, 1));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 3));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 5));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 6));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 7));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 9));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_UINT, &u64, 100));

	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 0));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 1));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 2));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 3));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 5));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 6));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 7));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 9));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_FLOAT, &f64, 100));

	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 1));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 2));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 4));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 8));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 10));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 100));

	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 0));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 1));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 2));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 3));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 4));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 5));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 6));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 7));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 9));
	NOTOK(tsdp_msg_extend(m, TSDP_FRAME_TSTAMP, &u64, 100));

	OK(tsdp_msg_extend(m, TSDP_FRAME_NIL, NULL, 0));
	dump(m);
	tsdp_msg_free(m);

	return 0;
}
