/* i8080.h
 * Github: https:\\github.com\tommojphillips
 */

#include <stdint.h>

#ifndef i8080_H
#define i8080_H

enum {
	REG_B = 0b000,
	REG_C = 0b001,
	REG_D = 0b010,
	REG_E = 0b011,
	REG_H = 0b100,
	REG_L = 0b101,
	REG_FLAGS = 0b110,
	REG_A = 0b111,
	REG_COUNT
};

enum {
	RP_BC = 0b00,
	RP_DE = 0b01,
	RP_HL = 0b10,
	RP_PSW = 0b11,
	RP_COUNT
};

typedef struct _STATUS_FLAGS {
	uint8_t c : 1;	
	uint8_t _one : 1;
	uint8_t p : 1;
	uint8_t _zero2 : 1;
	uint8_t h : 1;
	uint8_t _zero1 : 1;
	uint8_t z : 1;
	uint8_t s : 1;
} STATUS_FLAGS;

typedef struct {
	uint8_t interrupt : 1;
	uint8_t halt : 1;
} CPU_FLAGS;

typedef struct {
	uint16_t pc;
	uint16_t sp;
	uint8_t registers[REG_COUNT];
	STATUS_FLAGS* status_flags;
	CPU_FLAGS flags;
	uint32_t cycles;
	uint8_t opcode;

	uint8_t(*read_byte)(uint16_t);
	void(*write_byte)(uint16_t, uint8_t);

	uint8_t(*read_io)(uint8_t);
	void(*write_io)(uint8_t, uint8_t);

} I8080;

#ifdef __cplusplus
extern "C" {
#endif

void i8080_init(I8080* cpu);
void i8080_reset(I8080* cpu);
int i8080_execute(I8080* cpu);

#ifdef __cplusplus
};
#endif
#endif
