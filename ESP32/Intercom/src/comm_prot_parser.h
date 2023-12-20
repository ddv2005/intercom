#ifndef COMM_PROT_PARSER_H_
#define COMM_PROT_PARSER_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CP_RESULT_NEED_MORE		0
#define CP_RESULT_PACKET_READY	1
#define CP_RESULT_ERROR_GENERAL	-1
#define CP_RESULT_ERROR_CSUM	-2
#define CP_RESULT_ERROR_PSIZE	-3
#define	CP_RESULT_ERROR_SIZE	-4
#define CP_PREFIX_SIZE	2

typedef unsigned char byte_t;

#pragma pack(push)
#pragma pack(1)
typedef struct cp_prefix_t {
	cp_prefix_t(byte_t a, byte_t b) {
		data[0] = a;
		data[1] = b;
	}
	byte_t data[CP_PREFIX_SIZE];
}__packed cp_prefix_t;
#pragma pack(pop)

class comm_protocol_parser_c {
protected:
	cp_prefix_t m_prefix;
	byte_t m_max_cmd_size;
	byte_t* m_buffer;
	byte_t m_buffer_pos;
	byte_t m_state;
	byte_t m_payload_size;

public:
	comm_protocol_parser_c(cp_prefix_t prefix = cp_prefix_t(0xAA, 0xBB),
			uint8_t max_cmd_size = 64) :
		m_prefix(prefix), m_max_cmd_size(max_cmd_size) {
		reset();
		m_buffer = (byte_t*) malloc(m_max_cmd_size);
	}

	~comm_protocol_parser_c() {
		free(m_buffer);
	}

	byte_t get_expected_data_size() {
		if (m_state <= CP_PREFIX_SIZE)
			return 1;
		else
			return m_payload_size - m_buffer_pos;
	}

	void reset() {
		m_buffer_pos = 0;
		m_state = 0;
		m_payload_size = 0;
	}

	byte_t csum(byte_t*data, byte_t size, const byte_t extra = 0) {
		byte_t result = extra;
		while (size > 0) {
			result ^= *data;
			data++;
			size--;
		}
		return result;
	}

	byte_t *get_packet_buffer() {
		return m_buffer;
	}

	byte_t get_packet_size() {
		if (m_state == (CP_PREFIX_SIZE + 2))
			return m_payload_size - 1;
		else
			return 0;
	}

	byte_t get_packet(void *data, byte_t max_size) {
		byte_t size = get_packet_size();
		if ((max_size >= size) && (max_size > 0)) {
			memcpy(data, m_buffer, size);
			return size;
		} else
			return 0;
	}

	int8_t post_data(byte_t *data, const byte_t size) {
		int8_t result = CP_RESULT_NEED_MORE;
		if ((size > get_expected_data_size())
				|| (m_buffer_pos > m_max_cmd_size))
			result = CP_RESULT_ERROR_SIZE;
		{
			if (m_state < CP_PREFIX_SIZE) {
				if (*data == m_prefix.data[m_state]) {

					m_state++;
				} else
					result = CP_RESULT_ERROR_GENERAL;
			} else
				switch (m_state) {
				case CP_PREFIX_SIZE: {
					m_payload_size = *data;
					m_state++;
					if (m_payload_size == 0)
						result = CP_RESULT_ERROR_PSIZE;
					break;
				}

				case CP_PREFIX_SIZE + 1: {
					memcpy(&m_buffer[m_buffer_pos], data, size);
					m_buffer_pos += size;
					if (m_buffer_pos >= m_payload_size) {
						if (csum(m_buffer, m_payload_size - 1, m_payload_size)
								== m_buffer[m_payload_size - 1]) {
							m_state++;
							result = CP_RESULT_PACKET_READY;
						} else
							result = CP_RESULT_ERROR_CSUM;
					}
					break;
				}
				default: {
					result = CP_RESULT_ERROR_GENERAL;
					break;
				}
				}
		}
		if (result < 0)
			reset();
		return result;
	}

	byte_t get_packet_extra() {
		return CP_PREFIX_SIZE + 2;
	}

	byte_t create_packet(void *buffer, const byte_t max_size,
			const byte_t command, const void *data, const byte_t data_size) {
		if (max_size >= (data_size + 1 + get_packet_extra())) {
			byte_t *bbuffer = (byte_t*) buffer;
			memcpy(bbuffer, &m_prefix, CP_PREFIX_SIZE);
			bbuffer += CP_PREFIX_SIZE;
			byte_t *csum_buffer = bbuffer;
			*bbuffer = data_size + 2;
			bbuffer++;
			*bbuffer = command;
			bbuffer++;
			if (data_size > 0) {
				memcpy(bbuffer, data, data_size);
				bbuffer += data_size;
			}
			*bbuffer = csum(csum_buffer, data_size + 2);
			return data_size + get_packet_extra() + 1;
		} else
			return 0;
	}

	byte_t create_packet(void *buffer, const byte_t max_size, const void *data,
			const byte_t data_size) {
		if (max_size >= (data_size + get_packet_extra())) {
			byte_t *bbuffer = (byte_t*) buffer;
			memcpy(bbuffer, &m_prefix, CP_PREFIX_SIZE);
			bbuffer += CP_PREFIX_SIZE;
			byte_t *csum_buffer = bbuffer;
			*bbuffer = data_size + 1;
			bbuffer++;
			memcpy(bbuffer, data, data_size);
			bbuffer += data_size;
			*bbuffer = csum(csum_buffer, data_size + 1);
			return data_size + get_packet_extra();
		} else
			return 0;
	}
};

#endif /* COMM_PROT_PARSER_H_ */
