/* i8080_mnem.c
 * Github: https:\\github.com\tommojphillips
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "i8080.h"
#include "i8080_mnem.h"

#define SP mnem->cpu->sp
#define PC mnem->pc
#define COUNT(x) mnem->count = x
#define SSS (mnem->opcode & 0b111)
#define DDD ((mnem->opcode >> 3) & 0b111)

#define H  mnem->cpu->registers[REG_H]
#define L  mnem->cpu->registers[REG_L]
#define HL	((H << 8) | L)

#define SF mnem->cpu->status_flags->s
#define CF mnem->cpu->status_flags->c
#define ZF mnem->cpu->status_flags->z
#define AF mnem->cpu->status_flags->h
#define PF mnem->cpu->status_flags->p

#define READ_BYTE(address) mnem->cpu->read_byte(address)
#define READ_WORD(address) ((READ_BYTE((address) + 1) << 8) | READ_BYTE(address))
#define READ_ADDRESS READ_WORD(PC+1);
#define READ_STACK_ADDRESS READ_WORD(SP)

#define MNEM(x, ...) snprintf(mnem->str, sizeof(mnem->str), x, ##__VA_ARGS__)

static const char* reg_mnem[REG_COUNT] = {
	"B", "C", "D", "E", "H", "L", "M", "A"
};
static const char* rp_mnem[RP_COUNT] = {
	"BC", "DE", "HL", "PSW"
};

/* OPCODES */

static void CALL(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CALL 0x%04X", address);
	COUNT(3);
	PC = address;
}
static void CC(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CC 0x%04X", address);
	COUNT(3);
	if (CF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CNC(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CNC 0x%04X", address);
	COUNT(3);
	if (!CF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CZ(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CZ 0x%04X", address);
	COUNT(3);
	if (ZF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CNZ(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CNZ 0x%04X", address);
	COUNT(3);
	if (!ZF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CP(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CP 0x%04X", address);
	COUNT(3);
	if (!SF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CM(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CM 0x%04X", address);
	COUNT(3);
	if (SF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CPE(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CPE 0x%04X", address);
	COUNT(3);
	if (PF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void CPO(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("CPO 0x%04X", address);
	COUNT(3);
	if (!PF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}

static void RET(I8080_MNEM* mnem) {
	MNEM("RET");
	COUNT(1);
	PC = READ_STACK_ADDRESS;
}
static void RC(I8080_MNEM* mnem) {
	MNEM("RC");
	COUNT(1);
	if (CF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RNC(I8080_MNEM* mnem) {
	MNEM("RNC");
	COUNT(1);
	if (!CF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RZ(I8080_MNEM* mnem) {
	MNEM("RZ");
	COUNT(1);
	if (ZF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RNZ(I8080_MNEM* mnem) {
	MNEM("RNZ");
	COUNT(1);
	if (!ZF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RP(I8080_MNEM* mnem) {
	MNEM("RP");
	COUNT(1);
	if (!SF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RM(I8080_MNEM* mnem) {
	MNEM("RM");
	COUNT(1);
	if (SF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RPE(I8080_MNEM* mnem) {
	MNEM("RPE");
	COUNT(1);
	if (PF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}
static void RPO(I8080_MNEM* mnem) {
	MNEM("RPO");
	COUNT(1);
	if (!PF) {
		PC = READ_STACK_ADDRESS;
	}
	else {
		PC += 1;
	}
}

static void RST(I8080_MNEM* mnem) {
	uint8_t rst_address = (mnem->opcode & 0b00111000);
	uint8_t rst_num = ((rst_address >> 3) & 0b111);
	MNEM("RST %d", rst_num);
	COUNT(1);
	PC = rst_address;
}

static void IN(I8080_MNEM* mnem) {
	uint8_t port = READ_BYTE(PC + 1);
	MNEM("IN 0x%02X", port);
	COUNT(2);
	PC += 2;
}
static void OUT(I8080_MNEM* mnem) {
	uint8_t port = READ_BYTE(PC + 1);
	MNEM("OUT 0x%02X", port);
	COUNT(2);
	PC += 2;
}

static void LXI_BC(I8080_MNEM* mnem) {
	uint16_t imm  = READ_ADDRESS;
	MNEM("LXI %s, 0x%04X", rp_mnem[RP_BC], imm);
	COUNT(3);
	PC += 3;
}
static void LXI_DE(I8080_MNEM* mnem) {
	uint16_t imm = READ_ADDRESS;
	MNEM("LXI %s, 0x%04X", rp_mnem[RP_DE], imm);
	COUNT(3);
	PC += 3;
}
static void LXI_HL(I8080_MNEM* mnem) {
	uint16_t imm = READ_ADDRESS;
	MNEM("LXI %s, 0x%04X", rp_mnem[RP_HL], imm);
	COUNT(3);
	PC += 3;
}
static void LXI_SP(I8080_MNEM* mnem) {
	uint16_t imm = READ_ADDRESS;
	MNEM("LXI SP, 0x%04X", imm);
	COUNT(3);
	PC += 3;
}

static void PUSH_BC(I8080_MNEM* mnem) {
	MNEM("PUSH %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void PUSH_DE(I8080_MNEM* mnem) {
	MNEM("PUSH %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void PUSH_HL(I8080_MNEM* mnem) {
	MNEM("PUSH %s", rp_mnem[RP_HL]);
	COUNT(1);
	PC += 1;
}
static void PUSH_PSW(I8080_MNEM* mnem) {
	MNEM("PUSH %s", rp_mnem[RP_PSW]);
	COUNT(1);
	PC += 1;
}

static void POP_BC(I8080_MNEM* mnem) {
	MNEM("POP %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void POP_DE(I8080_MNEM* mnem) {
	MNEM("POP %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void POP_HL(I8080_MNEM* mnem) {
	MNEM("POP %s", rp_mnem[RP_HL]);
	COUNT(1);
	PC += 1;
}
static void POP_PSW(I8080_MNEM* mnem) {
	MNEM("POP %s", rp_mnem[RP_PSW]);
	COUNT(1);
	PC += 1;
}

static void STA(I8080_MNEM* mnem) {
	/* Store A to [address] */
	uint16_t address = READ_ADDRESS;
	MNEM("STA 0x%04X", address);
	COUNT(3);
	PC += 3;
}
static void LDA(I8080_MNEM* mnem) {
	/* Load A from [address] */
	uint16_t address = READ_ADDRESS;
	MNEM("LDA 0x%04X", address);
	COUNT(3);
	PC += 3;
}

static void XCHG(I8080_MNEM* mnem) {
	MNEM("XCHG");
	COUNT(1);
	PC += 1;
}
static void XTHL(I8080_MNEM* mnem) {
	MNEM("XTHL");
	COUNT(1);
	PC += 1;
}

static void SPHL(I8080_MNEM* mnem) {
	MNEM("SPHL");
	COUNT(1);
	PC += 1;
}
static void PCHL(I8080_MNEM* mnem) {
	MNEM("PCHL");
	COUNT(1);
	PC = HL;
}

static void DAD_BC(I8080_MNEM* mnem) {
	MNEM("DAD %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void DAD_DE(I8080_MNEM* mnem) {
	MNEM("DAD %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void DAD_HL(I8080_MNEM* mnem) {
	MNEM("DAD %s", rp_mnem[RP_HL]);
	COUNT(1);
	PC += 1;
}
static void DAD_SP(I8080_MNEM* mnem) {
	MNEM("DAD SP");
	COUNT(1);
	PC += 1;
}

static void STAX_BC(I8080_MNEM* mnem) {
	MNEM("STAX %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void STAX_DE(I8080_MNEM* mnem) {
	MNEM("STAX %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}

static void LDAX_BC(I8080_MNEM* mnem) {
	MNEM("LDAX %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void LDAX_DE(I8080_MNEM* mnem) {
	MNEM("LDAX %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void HLT(I8080_MNEM* mnem) {
	MNEM("HLT");
	COUNT(1);
	PC += 1;
}

static void MOV(I8080_MNEM* mnem) {
	MNEM("MOV %s, %s", reg_mnem[DDD], reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void MOV_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("MVI %s, 0x%02X", reg_mnem[DDD], imm);
	COUNT(2);
	PC += 2;
}

static void INR(I8080_MNEM* mnem) {
	MNEM("INR %s", reg_mnem[DDD]);
	COUNT(1);
	PC += 1;
}
static void DCR(I8080_MNEM* mnem) {
	MNEM("DCR %s", reg_mnem[DDD]);
	COUNT(1);
	PC += 1;
}

static void ADD(I8080_MNEM* mnem) {
	MNEM("ADD %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void ADC(I8080_MNEM* mnem) {
	MNEM("ADC %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void SUB(I8080_MNEM* mnem) {
	MNEM("SUB %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void SBB(I8080_MNEM* mnem) {
	MNEM("SBB %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}

static void ANA(I8080_MNEM* mnem) {
	MNEM("ANA %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void XRA(I8080_MNEM* mnem) {	
	MNEM("XRA %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void ORA(I8080_MNEM* mnem) {
	MNEM("ORA %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}
static void CMP(I8080_MNEM* mnem) {
	MNEM("CMP %s", reg_mnem[SSS]);
	COUNT(1);
	PC += 1;
}

static void ADD_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("ADI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void ADC_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("ACI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void SUB_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("SUI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void SBB_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("SBI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}

static void ANA_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("ANI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void XRA_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("XRI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void ORA_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("ORI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}
static void CMP_I(I8080_MNEM* mnem) {
	uint8_t imm = READ_BYTE(PC + 1);
	MNEM("CPI 0x%02X", imm);
	COUNT(2);
	PC += 2;
}

static void RLC(I8080_MNEM* mnem) {
	MNEM("RLC");
	COUNT(1);
	PC += 1;
}
static void RRC(I8080_MNEM* mnem) {
	MNEM("RRC");
	COUNT(1);
	PC += 1;
}
static void RAL(I8080_MNEM* mnem) {
	MNEM("RAL");
	COUNT(1);
	PC += 1;
}
static void RAR(I8080_MNEM* mnem) {
	MNEM("RAR");
	COUNT(1);
	PC += 1;
}

static void JMP(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JMP 0x%04X", address);
	COUNT(3);
	PC = address;
}
static void JC(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JC 0x%04X", address);
	COUNT(3);
	if (CF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JNC(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JNC 0x%04X", address);
	COUNT(3);
	if (!CF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JZ(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JZ 0x%04X", address);
	COUNT(3);
	if (ZF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JNZ(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JNZ 0x%04X", address);
	COUNT(3);
	if (!ZF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JP(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JP 0x%04X", address);
	COUNT(3);
	if (!SF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JM(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JM 0x%04X", address);
	COUNT(3);
	if (SF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JPE(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JPE 0x%04X", address);
	COUNT(3);
	if (PF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}
static void JPO(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("JPO 0x%04X", address);
	COUNT(3);
	if (!PF) {
		PC = address;
	}
	else {
		PC += 3;
	}
}

static void INX_BC(I8080_MNEM* mnem) {
	MNEM("INX %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void INX_DE(I8080_MNEM* mnem) {
	MNEM("INX %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void INX_HL(I8080_MNEM* mnem) {
	MNEM("INX %s", rp_mnem[RP_HL]);
	COUNT(1);
	PC += 1;
}
static void INX_SP(I8080_MNEM* mnem) {
	MNEM("INX SP");
	COUNT(1);
	PC += 1;
}

static void DCX_BC(I8080_MNEM* mnem) {
	MNEM("DCX %s", rp_mnem[RP_BC]);
	COUNT(1);
	PC += 1;
}
static void DCX_DE(I8080_MNEM* mnem) {
	MNEM("DCX %s", rp_mnem[RP_DE]);
	COUNT(1);
	PC += 1;
}
static void DCX_HL(I8080_MNEM* mnem) {
	MNEM("DCX %s", rp_mnem[RP_HL]);
	COUNT(1);
	PC += 1;
}
static void DCX_SP(I8080_MNEM* mnem) {
	MNEM("DCX SP");
	COUNT(1);
	PC += 1;
}

static void CMA(I8080_MNEM* mnem) {
	MNEM("CMA");
	COUNT(1);
	PC += 1;
}
static void STC(I8080_MNEM* mnem) {
	MNEM("STC");
	COUNT(1);
	PC += 1;
}
static void CMC(I8080_MNEM* mnem) {
	MNEM("CMC");
	COUNT(1);
	PC += 1;
}
static void DAA(I8080_MNEM* mnem) {
	MNEM("DAA");
	COUNT(1);
	PC += 1;
}

static void SHLD(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("SHLD 0x%04X", address);
	COUNT(3);
	PC += 3;
}
static void LHLD(I8080_MNEM* mnem) {
	uint16_t address = READ_ADDRESS;
	MNEM("SHLD 0x%04X", address);
	COUNT(3);
	PC += 3;
}

static void EI(I8080_MNEM* mnem) {
	MNEM("EI");
	COUNT(1);
	PC += 1;
}
static void DI(I8080_MNEM* mnem) {
	MNEM("DI");
	COUNT(1);
	PC += 1;
}

static void NOP(I8080_MNEM* mnem) {
	MNEM("NOP");
	COUNT(1);
	PC += 1;
}

void cpu_mnem_init(I8080_MNEM* mnem, I8080* cpu) {
	mnem->cpu = cpu;
	mnem->pc = 0;
	mnem->opcode = 0;
	mnem->str[0] = '\0';
}

void cpu_mnem(I8080_MNEM* mnem, uint16_t pc) {
	
	mnem->pc = pc;
	mnem->opcode = READ_BYTE(mnem->pc);

	switch (mnem->opcode) {
		
		case 0x00:
		case 0x08: /* ALT */
		case 0x10: /* ALT */
		case 0x18: /* ALT */
		case 0x20: /* ALT */
		case 0x28: /* ALT */
		case 0x30: /* ALT */
		case 0x38: /* ALT */
			NOP(mnem);
			break;

		case 0x01:
			LXI_BC(mnem);
			break;
		case 0x11:
			LXI_DE(mnem);
			break;
		case 0x21:
			LXI_HL(mnem);
			break;
		case 0x31:
			LXI_SP(mnem);
			break;

		case 0x02:
			STAX_BC(mnem);
			break;
		case 0x12:
			STAX_DE(mnem);
			break;

		case 0x22:
			SHLD(mnem);
			break;
		case 0x32:
			STA(mnem);
			break;

		case 0x03:
			INX_BC(mnem);
			break;
		case 0x13:
			INX_DE(mnem);
			break;
		case 0x23:
			INX_HL(mnem);
			break;
		case 0x33:
			INX_SP(mnem);
			break;

		case 0x04:
		case 0x14:
		case 0x24:
		case 0x0C:
		case 0x1C:
		case 0x2C:
		case 0x3C:
			INR(mnem);
			break;

		case 0x34:
			INR(mnem);
			break;

		case 0x05:
		case 0x15:
		case 0x25:
		case 0x0D:
		case 0x1D:
		case 0x2D:
		case 0x3D:
			DCR(mnem);
			break;

		case 0x35:
			DCR(mnem);
			break;

		case 0x06:
		case 0x16:
		case 0x26:
		case 0x0E:
		case 0x1E:
		case 0x2E:
		case 0x3E:
			MOV_I(mnem);
			break;

		case 0x36:
			MOV_I(mnem);
			break;

		case 0x07:
			RLC(mnem);
			break;
		case 0x17:
			RAL(mnem);
			break;
		case 0x27:
			DAA(mnem);
			break;
		case 0x37:
			STC(mnem);
			break;

		case 0x09:
			DAD_BC(mnem);
			break;
		case 0x19:
			DAD_DE(mnem);
			break;
		case 0x29:
			DAD_HL(mnem);
			break;
		case 0x39:
			DAD_SP(mnem);
			break;

		case 0x0A:
			LDAX_BC(mnem);
			break;
		case 0x1A:
			LDAX_DE(mnem);
			break;
		case 0x2A:
			LHLD(mnem);
			break;
		case 0x3A:
			LDA(mnem);
			break;

		case 0x0B:
			DCX_BC(mnem);
			break;
		case 0x1B:
			DCX_DE(mnem);
			break;
		case 0x2B:
			DCX_HL(mnem);
			break;
		case 0x3B:
			DCX_SP(mnem);
			break;

		case 0x0F:
			RRC(mnem);
			break;
		case 0x1F:
			RAR(mnem);
			break;
		case 0x2F:
			CMA(mnem);
			break;
		case 0x3F:
			CMC(mnem);
			break;

		case 0x40: // MOV r, r
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4F:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5F:
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6F:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7F:
			MOV(mnem);
			break;

		case 0x70: // MOV M, r
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x77:
			MOV(mnem);
			break;

		case 0x46: // MOV r, M
		case 0x4E:
		case 0x56:
		case 0x5E:
		case 0x66:
		case 0x6E:
		case 0x7E:
			MOV(mnem);
			break;

		case 0x76:
			HLT(mnem);
			break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x87:
			ADD(mnem);
			break;

		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8F:
			ADC(mnem);
			break;

		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x97:
			SUB(mnem);
			break;

		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9F:
			SBB(mnem);
			break;

		case 0xA0:
		case 0xA1:
		case 0xA2:
		case 0xA3:
		case 0xA4:
		case 0xA5:
		case 0xA7:
			ANA(mnem);
			break;

		case 0xA8:
		case 0xA9:
		case 0xAA:
		case 0xAB:
		case 0xAC:
		case 0xAD:
		case 0xAF:
			XRA(mnem);
			break;

		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
		case 0xB4:
		case 0xB5:
		case 0xB7:
			ORA(mnem);
			break;

		case 0xB8:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBF:
			CMP(mnem);
			break;

		case 0x86:
			ADD(mnem);
			break;
		case 0x8E:
			ADC(mnem);
			break;
		case 0x96:
			SUB(mnem);
			break;
		case 0x9E:
			SBB(mnem);
			break;
		case 0xA6:
			ANA(mnem);
			break;
		case 0xAE:
			XRA(mnem);
			break;
		case 0xB6:
			ORA(mnem);
			break;
		case 0xBE:
			CMP(mnem);
			break;

		case 0xC6:
			ADD_I(mnem);
			break;
		case 0xCE:
			ADC_I(mnem);
			break;
		case 0xD6:
			SUB_I(mnem);
			break;
		case 0xDE:
			SBB_I(mnem);
			break;
		case 0xE6:
			ANA_I(mnem);
			break;
		case 0xEE:
			XRA_I(mnem);
			break;
		case 0xF6:
			ORA_I(mnem);
			break;
		case 0xFE:
			CMP_I(mnem);
			break;

		case 0xC0:
			RNZ(mnem);
			break;
		case 0xD0:
			RNC(mnem);
			break;
		case 0xE0:
			RPO(mnem);
			break;
		case 0xF0:
			RP(mnem);
			break;

		case 0xC1:
			POP_BC(mnem);
			break;
		case 0xD1:
			POP_DE(mnem);
			break;
		case 0xE1:
			POP_HL(mnem);
			break;
		case 0xF1:
			POP_PSW(mnem);
			break;

		case 0xC2:
			JNZ(mnem);
			break;
		case 0xD2:
			JNC(mnem);
			break;
		case 0xE2:
			JPO(mnem);
			break;
		case 0xF2:
			JP(mnem);
			break;

		case 0xC3:
			JMP(mnem);
			break;

		case 0xE3:
			XTHL(mnem);
			break;

		case 0xF3:
			DI(mnem);
			break;

		case 0xC4:
			CNZ(mnem);
			break;
		case 0xD4:
			CNC(mnem);
			break;
		case 0xE4:
			CPO(mnem);
			break;
		case 0xF4:
			CP(mnem);
			break;

		case 0xC5:
			PUSH_BC(mnem);
			break;
		case 0xD5:
			PUSH_DE(mnem);
			break;
		case 0xE5:
			PUSH_HL(mnem);
			break;
		case 0xF5:
			PUSH_PSW(mnem);
			break;

		case 0xC8:
			RZ(mnem);
			break;
		case 0xD8:
			RC(mnem);
			break;
		case 0xE8:
			RPE(mnem);
			break;
		case 0xF8:
			RM(mnem);
			break;

		case 0xC9:
		case 0xD9: /* ALT */
			RET(mnem);
			break;

		case 0xE9:
			PCHL(mnem);
			break;
		case 0xF9:
			SPHL(mnem);
			break;

		case 0xCA:
			JZ(mnem);
			break;
		case 0xDA:
			JC(mnem);
			break;
		case 0xEA:
			JPE(mnem);
			break;
		case 0xFA:
			JM(mnem);
			break;

		case 0xCB:
			JMP(mnem);
			break;

		case 0xD3:
			OUT(mnem);
			break;
		case 0xDB:
			IN(mnem);
			break;

		case 0xFB:
			EI(mnem);
			break;

		case 0xEB:
			XCHG(mnem);
			break;

		case 0xCC:
			CZ(mnem);
			break;
		case 0xDC:
			CC(mnem);
			break;
		case 0xEC:
			CPE(mnem);
			break;
		case 0xFC:
			CM(mnem);
			break;

		case 0xCD:
		case 0xDD: /* ALT */
		case 0xED: /* ALT */
		case 0xFD: /* ALT */
			CALL(mnem);
			break;

		case 0xC7:
		case 0xCF:
		case 0xD7:
		case 0xDF:
		case 0xE7:
		case 0xEF:
		case 0xF7:
		case 0xFF:
			RST(mnem);
			break;
	}
}
