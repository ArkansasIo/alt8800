/* i8080_mnem.h
 * Github: https:\\github.com\tommojphillips
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	I8080* cpu;
	uint8_t opcode;
	uint8_t count;
	uint16_t pc;
	char str[32];
} I8080_MNEM;

void cpu_mnem_init(I8080_MNEM* mnem, I8080* cpu);
void cpu_mnem(I8080_MNEM* mnem, uint16_t pc);

#ifdef __cplusplus
};
#endif
