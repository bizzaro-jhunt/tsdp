#include <stdio.h>
#include <tsdp.h>

#define OK(x) do {\
	if ((x) != 0) { \
		fprintf(stderr, "FAILED: %s returned non-zero\n", #x); \
		exit(1); \
	} \
} while (0)

int main(int argc, char **argv)
{
	struct tsdp_msg *m;

	m = tsdp_msg_new(
			TSDP_PROTOCOL_V1,
			TSDP_OPCODE_HEARTBEAT,
			0x55,
			TSDP_PAYLOAD_SAMPLE|TSDP_PAYLOAD_TALLY);
	if (!m) return 1;
	OK(tsdp_msg_extend(m, TSDP_FRAME_STRING, "test", 4));

	if (tsdp_msg_version(m) != TSDP_PROTOCOL_V1)                         return  2;
	if (tsdp_msg_opcode(m)  != TSDP_OPCODE_HEARTBEAT)                    return  3;
	if (tsdp_msg_flags(m)   != 0x55)                                     return  4;
	if (tsdp_msg_payload(m) != (TSDP_PAYLOAD_SAMPLE|TSDP_PAYLOAD_TALLY)) return  5;
	if (tsdp_msg_nframes(m) != 1)                                        return  6;
	if (tsdp_frame_type(m->frames)   != TSDP_FRAME_STRING)               return  7;
	if (tsdp_frame_type(m->last)     != TSDP_FRAME_STRING)               return  8;
	if (tsdp_frame_length(m->frames) != 4)                               return  9;
	if (tsdp_frame_length(m->last)   != 4)                               return 10;
	return 0;
}
