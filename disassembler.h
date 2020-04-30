#ifndef H_DISASSEMBLER
#define H_DISASSEMBLER

#include <Windows.h>
#include <string>
#include <vector>

#define Fl_none					0x00000000
#define Fl_src_only				0x10000000
#define Fl_src_dest				0x20000000

#define Fl_src_r8				0x00000001
#define Fl_src_r16				0x00000002
#define Fl_src_r32   			0x00000004
#define Fl_src_imm8				0x00000008
#define Fl_src_imm16			0x00000010
#define Fl_src_imm32   			0x00000020
#define Fl_src_rm8				0x00000040
#define Fl_src_rm16				0x00000080
#define Fl_src_rm32				0x00000100
#define Fl_src_rxmm				0x00000200
#define Fl_src_disp8			0x00000400
#define Fl_src_disp16			0x00000800
#define Fl_src_disp32			0x00001000

#define Fl_dest_r8				0x00002000
#define Fl_dest_r16				0x00004000
#define Fl_dest_r32   			0x00008000
#define Fl_dest_imm8			0x00010000
#define Fl_dest_imm16			0x00020000
#define Fl_dest_imm32   		0x00040000
#define Fl_dest_rm8				0x00080000
#define Fl_dest_rm16			0x00100000
#define Fl_dest_rm32			0x00200000
#define Fl_dest_rxmm			0x00400000
#define Fl_dest_disp8			0x00800000
#define Fl_dest_disp16			0x40000000
#define Fl_dest_disp32			0x80000000

#define Fl_rel8					0x01000000
#define Fl_rel16				0x02000000
#define Fl_rel32				0x04000000

#define Fl_condition			0x08000000

#define PRE_BYTE_PTR			0x1
#define PRE_WORD_PTR			0x2
#define PRE_DWORD_PTR			0x4
#define PRE_QWORD_PTR			0x8

#define pre_repne   			0x1
#define pre_repe   				0x2
#define pre_66   				0x4
#define pre_67   				0x8
#define pre_lock 				0x10
#define pre_seg  				0x20
#define seg_cs					0x2E
#define seg_ss					0x36
#define seg_ds					0x3E
#define seg_es					0x26
#define seg_fs					0x64
#define seg_gs					0x65


#define addoff8(s,b,l,v)		char m[16]; v=*(uint8_t*)(b+l); (v<=0x7F)		? sprintf_s(m,"+%02X",v) : sprintf_s(m,"-%02X",(0xFF+1)-v);			strcat_s(s,m); l+=1
#define addoff16(s,b,l,v)		char m[16]; v=*(uint16_t*)(b+l);(v<=0x7FFF)		? sprintf_s(m,"+%04X",v) : sprintf_s(m,"-%04X",(0xFFFF+1)-v);		strcat_s(s,m); l+=2
#define addoff32(s,b,l,v)		char m[16]; v=*(uint32_t*)(b+l);(v<=0x7FFFFFFF)	? sprintf_s(m,"+%08X",v) : sprintf_s(m,"-%08X",(0xFFFFFFFF+1)-v);	strcat_s(s,m); l+=4
#define getr1(b,l)				(b[l] % 64 / 8)
#define getr2(b,l)				(b[l] % 64 % 8)
#define getmode20(b,l)			(b[l] / 0x20)
#define getmode40(b,l)			(b[l] / 0x40)

#define asm_out_none			0x0
#define asm_out_strings			0x1
#define asm_out_offs_1b			0x2
#define asm_out_offs_2b			0x4
#define asm_out_offs_4b			0x10

#define cvt_to_short(a,b)		short	(b << 8 | a)
#define cvt_to_int(a,b,c,d)		int		(a << 24 | b << 16 | c << 8 | d)

namespace disassembler {
	namespace convert {
		const char c_ref1[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
		const int  c_ref2[16] = {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15 };

		// translation
		BYTE to_hex(char* x) {
			if (lstrlenA(x) < 2) return 0;
			if (x[0] == '?' && x[1] == '?') return 0;
			BYTE b = 0;
			for (int i = 0; i < 16; i++) {
				if (x[0] == c_ref1[i]) b += c_ref2[i] * 16;
				if (x[1] == c_ref1[i]) b += i;
			}
			return b;
		}

		std::string to_str(BYTE b) {
			std::string x = "";
			x += c_ref1[b / 16];
			x += c_ref1[b % 16];
			return x;
		}

		std::string to_str(int address) {
			std::string str = "";
			char c[16];
			sprintf_s(c, "%08X", address);
			str += c;
			return str;
		}

		// converts a string representation of an address to a UINT/hex address
		int to_addr(char* addr) {
			if (lstrlenA(addr) < 8) return 0;

			char c1[2], c2[2], c3[2], c4[2];
			c1[0] = addr[0], c1[1] = addr[1];
			c2[0] = addr[2], c2[1] = addr[3];
			c3[0] = addr[4], c3[1] = addr[5];
			c4[0] = addr[6], c4[1] = addr[7];

			return static_cast<int>(cvt_to_int(to_hex(c4), to_hex(c3), to_hex(c2), to_hex(c1)));
		}

		std::string to_bytes(int addr) {
			BYTE* x = new BYTE[4];
			memcpy(x, &addr, 4);
			char y[16];
			sprintf_s(y, "%02X%02X%02X%02X", x[0], x[1], x[2], x[3]);
			std::string res(y);
			free(x);
			return res;
		}

		std::string to_bytes(const char* str) {
			std::string x = "";
			for (int i = 0; i < lstrlenA(str); i++) {
				BYTE c = static_cast<BYTE>(str[i]);
				if (i == lstrlenA(str) - 1)
					x += to_str(c);
				else {
					x += to_str(c);
					x += 0x20;
				}
			}
			return x;
		}
	}

	class operand {
	public:
		BYTE r8; // 8 bit register
		BYTE r16; // 16 bit register
		BYTE r32; // 32 bit register
		BYTE rxmm; // 64 bit register
		BYTE r_2; // second register in source operand (same bit type as others) (use discretion/no flag indicator yet)
		BYTE mul; // multiplier (0 by default, if there is a multiplier it wont be 0)
		BYTE imm8; // 8 bit offset value
		USHORT imm16; // 16 bit offset value
		UINT imm32; // 32 bit offset value
		BYTE disp8;
		USHORT disp16;
		UINT disp32; // a fixed value (not an 'offset' (+/-))
		BYTE pref; // prefix for this operand (byte/dword/qword ptr, etc.)
	};


	class inst {
	public:
		inst() {
			data[0] = '\0';
			opcode[0] = '\0';

			address = 0;
			len = 0; // defaults to 1 if it cant translate an instruction
			pref = 0;
			flags = 0;
			rel8 = 0;
			rel16 = 0;
			rel32 = 0;
			p_rep = 0;
			p_lock = 0;
			p_seg = 0;
			r_mod = 0;
			condition = 0;

			src.r8 = 0;
			src.r16 = 0;
			src.r32 = 0;
			src.rxmm = 0;
			src.r_2 = 0;
			src.mul = 0;
			src.imm8 = 0;
			src.imm16 = 0;
			src.imm32 = 0;
			src.disp8 = 0;
			src.disp16 = 0;
			src.disp32 = 0;
			src.pref = 0;

			dest.r8 = 0;
			dest.r16 = 0;
			dest.r32 = 0;
			dest.rxmm = 0;
			dest.r_2 = 0;
			dest.mul = 0;
			dest.imm8 = 0;
			dest.imm16 = 0;
			dest.imm32 = 0;
			dest.disp8 = 0;
			dest.disp16 = 0;
			dest.disp32 = 0;
			dest.pref = 0;
		};

		~inst() {};

		char data[128]; // readable output/translation of the instruction
		char opcode[16]; // just the instruction opcode

		void set_op(const char* x){
			strcpy_s(opcode, x);
			strcat_s(data, x);
			strcat_s(data, " ");
		}

		UINT address;

		BYTE len; // overall length of the instruction in bytes (defaults to 1)
		BYTE p_rep; // rep
		BYTE p_lock; // lock
		BYTE p_seg; // seg
		BYTE r_mod; // register mode

		UINT pref; // global instruction prefix (cs/ds/lock/repne/etc.)
		UINT flags; // global instruction flags (which also determine src/dest values)

		BYTE rel8; // 8 bit relative value (for jmp or call)
		USHORT rel16; // 16 bit relative value
		UINT rel32; // 32 bit relative value

		BYTE condition; // if Fl_condition flag is set, so is this (if its a ja/jne/jnl, this will be set to the condition enum a/ne/nl)

		operand src;
		operand dest;
	};


	const BYTE mults[] = { 0, 2, 4, 8 };

	enum reg_8 {
		al,
		cl,
		dl,
		bl,
		ah,
		ch,
		dh,
		bh
	};

	enum reg_16	{
		ax,
		bx,
		cx,
		dx,
		sp,
		bp,
		si,
		di
	};

	enum reg_32	{
		eax,
		ecx,
		edx,
		ebx,
		esp,
		ebp,
		esi,
		edi
	};

	enum reg_xmm {
		xmm0,
		xmm1,
		xmm2,
		xmm3,
		xmm4,
		xmm5,
		xmm6,
		xmm7
	};

	enum conds {
		o,
		no,
		b,
		nb,
		e,
		ne,
		na,
		a,
		s,
		ns,
		p,
		np,
		l,
		nl,
		le,
		g
	};
	
	// for visuals / text translation
	const char* c_reg_8[8] = {
		"al",
		"cl",
		"dl",
		"bl",
		"ah",
		"ch",
		"dh",
		"bh"
	};

	const char* c_reg_16[8] = {
		"ax",
		"bx",
		"cx",
		"dx",
		"sp",
		"bp",
		"si",
		"di"
	};

	const char* c_reg_32[8] = {
		"eax",
		"ecx",
		"edx",
		"ebx",
		"esp",
		"ebp",
		"esi",
		"edi"
	};

	const char*c_reg_xmm[8] = {
		"xmm0",
		"xmm1",
		"xmm2",
		"xmm3",
		"xmm4",
		"xmm5",
		"xmm6",
		"xmm7"
	};

	const char* c_conds[16] = {
		"o",
		"no",
		"b",
		"nb",
		"e",
		"ne",
		"na",
		"a",
		"s",
		"ns",
		"p",
		"np",
		"l",
		"nl",
		"le",
		"g"
	};

	
	// Returns the equivalent x86 instruction at the given address
	inst read(int address){
		DWORD _BASE = 0;
		BYTE c = 0;
		BYTE l = 0;
		BYTE* b = new BYTE[16];

		__asm {
			push eax
			mov eax, fs: [0x30];
			mov eax, [eax + 8];
			pop eax
		}

		inst p = inst();
		p.address = address;

		memcpy_s(b, 16, reinterpret_cast<void*>(p.address), 16);

		switch (*b) {
		case seg_cs: case seg_ss: case seg_ds: case seg_es: case seg_fs: case seg_gs:
			p.p_seg = b[l++];
			p.pref |= pre_seg;
			break;
		case 0x66:
			p.pref |= pre_66;
			l++;
			break;
		case 0x67:
			p.pref |= pre_67;
			l++;
			break;
		case 0xF0:
			p.p_lock = b[l++];
			p.pref |= pre_lock;
			strcat_s(p.data, "lock ");
			break;
		case 0xF2:
			p.p_lock = b[l++];
			p.pref |= pre_repne;
			strcat_s(p.data, "repne ");
			break;
		case 0xF3:
			p.p_lock = b[l++];
			p.pref |= pre_repe;
			strcat_s(p.data, "repe ");
			break;
		default: break;
		}

		if (b[l] == 0x0){
			p.set_op("add");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x1){
			p.set_op("add");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x2){
			p.set_op("add");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x3){
			p.set_op("add");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x4){
			p.set_op("add");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x5){
			p.set_op("add");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x8){
			p.set_op("or");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x9){
			p.set_op("or");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0xA){
			p.set_op("or");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0xB){
			p.set_op("or");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0xC){
			p.set_op("or");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0xD){
			p.set_op("or");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0xF) {
			l++;
			if (b[l] == 0x10){
				// check for changes all through-out with PRE_F2 and PRE_F3 prefixes
				if		(p.pref & pre_repne)	p.set_op("movsd");
				else if (p.pref & pre_repe)		p.set_op("movss");
				else							p.set_op("movups");
				p.dest.pref |= PRE_QWORD_PTR;
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x11){
				if		(p.pref & pre_repne)	p.set_op("movsd");
				else if (p.pref & pre_repe)		p.set_op("movss");
				else							p.set_op("movups");
				p.src.pref |= PRE_QWORD_PTR;
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm;  // swapped
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if (b[l] == 0x12){
				p.set_op("movhlps");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x13){
				p.set_op("movlps");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm;
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if (b[l] == 0x14){
				p.set_op("unpcklps");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x15){
				p.set_op("unpckhps");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x16){
				p.set_op("movhps");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x17){
				p.set_op("movhps");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm; // swapped
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if (b[l] == 0x18){
				// mode-based conditional prefetch instruction (can throw in later)
			} else if (b[l] == 0x1F){ // test this more
				l++;
				if (b[l] % 0x40 < 8){
					if (p.pref & pre_66)		p.src.pref |= PRE_WORD_PTR;
					else						p.src.pref |= PRE_WORD_PTR;
					p.set_op("nop");
					p.flags |= Fl_src_only | Fl_src_rm32;
					p.r_mod = getmode40(b,l);
					p.src.r32 = getr1(b,l);
				}
			} else if (b[l] == 0x28){
				p.set_op("movaps");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			}  else if (b[l] == 0x29){
				p.set_op("movaps");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm; // swapped
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if (b[l] == 0x2C){
				p.dest.pref |= PRE_QWORD_PTR;
				p.set_op("cvttsd2si");
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.r32 = getr1(b,l); // note to self...update all r32 src's for these
			} else if (b[l] == 0x2E){
				p.dest.pref |= PRE_QWORD_PTR;
				p.set_op("ucomisd");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x2F){
				//p.dest.pref |= PRE_QWORD_PTR;
				p.set_op("comiss");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if ((c=(b[l] & 0x40)) && b[l]<c+16) {
				p.condition = (b[l]-c);
				char opc[8];
				sprintf_s(opc, "cmov");
				strcat_s(opc, c_conds[p.condition]);
				p.set_op(opc);
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.r32 = getr1(b,l);
			} else if ((c=(b[l] & 0x50)) && b[l]<c+16) {
				const char* select_opcodes[] = { "movmskps", "sqrtps", "rsqrtps", "rcpps", "andps", "andnps", "orps", "xorps", "addps", "mulps", "cvtps2pd", "cvtdq2ps", "subps", "minps", "divps", "maxps" };
				p.dest.pref |= PRE_QWORD_PTR;
				p.set_op(select_opcodes[b[l]-c]);
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x6E){
				p.dest.pref |= PRE_DWORD_PTR;
				p.set_op("movd");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x7E){
				p.dest.pref |= PRE_QWORD_PTR;
				p.set_op("movq");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.rxmm = getr1(b,l);
			} else if (b[l] == 0x7F){
				p.src.pref |= PRE_QWORD_PTR;
				p.set_op("movdqu");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm;
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if ((c=(b[l] & 0x80)) && b[l]<c+16) {
				p.condition = (b[l]-c);
				char opc[8];
				sprintf_s(opc, "j");
				strcat_s(opc, c_conds[p.condition]);
				p.set_op(opc);
				p.flags |= Fl_src_only | Fl_rel32;
				l++;
			} else if ((c=(b[l] & 0x90)) && b[l]<c+16) {
				char opc[8];
				sprintf_s(opc, "set");
				strcat_s(opc, c_conds[p.condition]);
				p.set_op(opc);
				p.flags |= Fl_src_only | Fl_src_rm32;
				p.r_mod = getmode40(b,++l);
				p.src.r32 = getr1(b,l);
			} else if (b[l] == 0xB6) {
				p.dest.pref |= PRE_BYTE_PTR;
				p.set_op("movzx");
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b, ++l);
				p.src.r32 = getr1(b, l);
			} else if (b[l] == 0xB7) {
				p.dest.pref |= PRE_WORD_PTR;
				p.set_op("movzx");
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b, ++l);
				p.src.r32 = getr1(b, l);
			} else if (b[l] == 0xBE) {
				p.dest.pref |= PRE_BYTE_PTR;
				p.set_op("movsx");
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b, ++l);
				p.src.r32 = getr1(b, l);
			} else if (b[l] == 0xBF) {
				p.dest.pref |= PRE_WORD_PTR;
				p.set_op("movsx");
				p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
				p.r_mod = getmode40(b, ++l);
				p.src.r32 = getr1(b, l);
			} else if (b[l] == 0xD6){
				p.src.pref |= PRE_QWORD_PTR;
				p.set_op("movq");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_rxmm;
				p.r_mod = getmode40(b,++l);
				p.dest.rxmm = getr1(b,l);
			} else if (b[l] == 0xE6){
				// people say my code isn't clean. that's kind of inevitable.
				// it's a disassembler, I won't make this any easier for you.
				// It's my code, and I didn't /ask/ for you to look at it
				//
				// but since you're soo insistent,
				// let's break down what we're doing
				// 
				// The current instruction could be cvtdq2pd eax,xmm0.
				// it's a cvtdq2pd instruction. simple.
				// The bytes will be F3 0F E6 C0.
				// `F3` determines what form of cvtdq2pd to use; (32-bit/16-bit)
				// we'll skip this for now. (I dont even include too many
				// 16-bit instructions; just the most common ones)
				// `0F` implies it's one of these instructions in this set. ignore
				// `E6` implies it's the cvtdq2pd instruction.
				// `C0` implies everything else; it's the `mod` byte.
				// It determines both the src and dest operand, however,
				// if it were below C0 it `might` have an extending 
				// byte that determines more about the src or dest operand.
				// There is alot that goes into this; just know it's
				// the `mod` byte.
				p.dest.pref |= PRE_QWORD_PTR;
				// ^ Prefix flag for a src or dest operand;
				// This implies that, if the `mod` byte is over
				// 0xC0, rather than being		cvtdq2pd xmm0,eax
				// it will instead turn into	cvtdq2pd xmm0,xmm0
				// Many instructions have to have this recognized
				// It also implies that offsets should have a mark;
				// cvtdq2pd xmm0, [eax+04]		will instead show:
				// cvtdq2pd xmm0, QWORD PTR [eax+04].
				// So, this flag is important, but easy to recognize.
				p.set_op("cvtdq2pd");
				p.flags |= Fl_src_dest | Fl_src_rxmm | Fl_dest_rm32;
				// Normal flags; our instruction contains a src and dest
				// so we include Fl_src_dest.
				// First half of the opcode(src) is supposed to be a
				// a floating point register; xmm0-xmm7. Always.
				// so we use Fl_src_rxmm.
				// The second half of the opcode(dest) has to be a
				// 32-bit register(eax,ecx,edx, . . .), but it can also
				// contain a long offset; [eax+4*4], [ebx+ebx], etc.
				// so we would do Fl_dest_rm32 rather than Fl_dest_r32.
				p.r_mod = getmode40(b,++l);
				// move onto the next byte to interpret, while storing
				// the `mod` identifier byte into r_mod, for help in
				// calculating the rest of the instruction.
				// this is important, since we have to move onto the
				// next byte and allow the system below to finish up
				// computing the instruction.
				p.src.rxmm = getr1(b,l);
				// ^ I can't remember the exact reason for this line,
				// except that it'll guarantee src.rxmm has the correct value.
				// We typically set src.rxmm (or whatever we used for src),
				// IF the `dest` flag is an rm32.
				// And we would set dest.rxmm if the `src` flag is an rm32.
				// This is because the rm32 mode can cause the bytes
				// to be kind of shifted around for src operand
			}
		} else if (b[l] == 0x10){
			p.set_op("adc");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x11){
			p.set_op("adc");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x12){
			p.set_op("adc");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x13){
			p.set_op("adc");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x14){
			p.set_op("adc");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x15){
			p.set_op("adc");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x18){
			p.set_op("sbb");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x19){
			p.set_op("sbb");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x1A){
			p.set_op("sbb");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x1B){
			p.set_op("sbb");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x1C){
			p.set_op("sbb");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x1D){
			p.set_op("sbb");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x20){
			p.set_op("and");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x21){
			p.set_op("and");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x22){
			p.set_op("and");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x23){
			p.set_op("and");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x24){
			p.set_op("and");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x25){
			p.set_op("and");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x28){
			p.set_op("sub");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x29){
			p.set_op("sub");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x2A){
			p.set_op("sub");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x2B){
			p.set_op("sub");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x2C){
			p.set_op("sub");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x2D){
			p.set_op("sub");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x30){
			p.set_op("xor");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x31){
			p.set_op("xor");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x32){
			p.set_op("xor");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x33){
			p.set_op("xor");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x34){
			p.set_op("xor");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x35){
			p.set_op("xor");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if (b[l] == 0x38){
			p.set_op("cmp");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x39){
			p.set_op("cmp");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x3A){
			p.set_op("cmp");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x3B){
			p.set_op("cmp");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x3C){
			p.set_op("cmp");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = 0;
		} else if (b[l] == 0x3D){
			p.set_op("cmp");
			p.flags |= Fl_src_only | Fl_src_disp32;
			l++;
		} else if ((c=(b[l] & 0x40)) && b[l]<c+0x8){
			p.set_op("inc");
			p.src.r32 = b[l++] - c;
			p.flags |= Fl_src_only | Fl_src_r32;
		} else if ((c=(b[l] & 0x48)) && b[l]<c+0x8){
			p.set_op("dec");
			p.src.r32 = b[l++] - c;
			p.flags |= Fl_src_only | Fl_src_r32;
		} else if ((c=(b[l] & 0x50)) && b[l]<c+0x8){
			p.set_op("push");
			p.src.r32 = b[l++] - c;
			p.flags |= Fl_src_only | Fl_src_r32;
		} else if ((c=(b[l] & 0x58)) && b[l]<c+0x8){
			p.set_op("pop");
			p.src.r32 = b[l++] - c;
			p.flags |= Fl_src_only | Fl_src_r32;
		} else if (b[l] == 0x60)
			l++, p.set_op("pushad");
		else if (b[l] == 0x61)
			l++, p.set_op("popad");
		else if (b[l] == 0x68){
			l++, p.set_op("push");
			p.flags |= Fl_src_only | Fl_src_disp32;
		} else if (b[l] == 0x6A){
			l++, p.set_op("push");
			p.flags |= Fl_src_only | Fl_src_disp8;
		} else if ((c=(b[l] & 0x70)) && b[l]<c+16) {
			p.condition = (b[l]-c);
			if (p.condition >= 16) throw std::exception("Bad jmp condition index");
			char opc[16];
			sprintf_s(opc, "j");
			strcat_s(opc, c_conds[p.condition]);
			p.set_op(opc);
			p.flags |= Fl_src_only | Fl_rel8;
			l++;
		} else if (b[l] == 0x80 || b[l] == 0x82){
			p.src.pref |= PRE_BYTE_PTR; // unique identifier; dest is imm8 and not r8
			// we have it automatically use r8 if dest is r8 and its in 3rd mode but not in this case
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("add");
			else if (mode == 1) p.set_op("or");
			else if (mode == 2) p.set_op("adc");
			else if (mode == 3) p.set_op("sbb");
			else if (mode == 4) p.set_op("and");
			else if (mode == 5) p.set_op("sub");
			else if (mode == 6) p.set_op("xor");
			else if (mode == 7) p.set_op("cmp");

			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
			p.r_mod = getmode40(b,l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x81){
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("add");
			else if (mode == 1) p.set_op("or");
			else if (mode == 2) p.set_op("adc");
			else if (mode == 3) p.set_op("sbb");
			else if (mode == 4) p.set_op("and");
			else if (mode == 5) p.set_op("sub");
			else if (mode == 6) p.set_op("xor");
			else if (mode == 7) p.set_op("cmp");

			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp32;
			p.r_mod = getmode40(b,l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x83){
			p.src.pref |= PRE_DWORD_PTR;
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("add");
			else if (mode == 1) p.set_op("or");
			else if (mode == 2) p.set_op("adc");
			else if (mode == 3) p.set_op("sbb");
			else if (mode == 4) p.set_op("and");
			else if (mode == 5) p.set_op("sub");
			else if (mode == 6) p.set_op("xor");
			else if (mode == 7) p.set_op("cmp");

			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
			p.r_mod = getmode40(b,l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x84){
			p.src.pref |= PRE_BYTE_PTR;
			p.set_op("test");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x85){
			p.set_op("test");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x86){
			p.src.pref |= PRE_BYTE_PTR;
			p.set_op("xchg");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x87){
			p.set_op("xchg");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x88){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r8;
			p.r_mod = getmode40(b,++l);
			p.dest.r8 = getr1(b,l);
		} else if (b[l] == 0x89){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_r32;
			p.r_mod = getmode40(b,++l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0x8A){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r8 = getr1(b,l);
		} else if (b[l] == 0x8B){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x8D){
			p.set_op("lea");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_rm32;
			p.r_mod = getmode40(b,++l);
			p.src.r32 = getr1(b,l);
		} else if (b[l] == 0x90){
			p.set_op("nop");
			p.flags |= Fl_none;
			l++;
		} else if (b[l] == 0x9F){
			p.set_op("lahf");
			p.flags |= Fl_none;
			l++;
		} else if (b[l] == 0xA0){
			p.dest.pref |= PRE_DWORD_PTR;
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp32;
			p.src.r8 = 0;
			l++;
		} else if (b[l] == 0xA1){
			p.dest.pref |= PRE_DWORD_PTR;
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_disp32;
			p.src.r32 = 0;
			l++;
		} else if (b[l] == 0xA2){
			p.src.pref |= PRE_DWORD_PTR;
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_disp32 | Fl_dest_r8;
			p.dest.r8 = 0;
			l++;
		} else if (b[l] == 0xA3){
			p.src.pref |= PRE_DWORD_PTR;
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_disp32 | Fl_dest_r32;
			p.dest.r32 = 0;
			l++;
		} else if (b[l] == 0xA8){
			p.set_op("test");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.src.r8 = 0;
			l++;
		} else if (b[l] == 0xA9){
			p.set_op("test");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_disp32;
			p.src.r32 = 0;
			l++;
		} else if ((c=(b[l] & 0xB0)) && b[l]<c+0x8){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r8 | Fl_dest_disp8;
			p.src.r8 = b[l] - c;
			l++;
		} else if ((c=(b[l] & 0xB8)) && b[l]<c+0x8){
			p.set_op("mov");
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_disp32;
			p.src.r32 = b[l] - c;
			l++;
		} else if (b[l] == 0xC0){
			p.src.pref |= PRE_BYTE_PTR;
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("rol");
			else if (mode == 1) p.set_op("ror");
			else if (mode == 2) p.set_op("rcl");
			else if (mode == 3) p.set_op("rcr");
			else if (mode == 4) p.set_op("shl");
			else if (mode == 5) p.set_op("shr");
			else if (mode == 6) p.set_op("rol");
			else if (mode == 7) p.set_op("sar");

			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
			p.r_mod = getmode40(b,l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0xC1){
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("rol");
			else if (mode == 1) p.set_op("ror");
			else if (mode == 2) p.set_op("rcl");
			else if (mode == 3) p.set_op("rcr");
			else if (mode == 4) p.set_op("shl");
			else if (mode == 5) p.set_op("shr");
			else if (mode == 6) p.set_op("rol");
			else if (mode == 7) p.set_op("sar");

			p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
			p.r_mod = getmode40(b,l);
			p.dest.r32 = getr1(b,l);
		} else if (b[l] == 0xC2){
			p.set_op("ret");
			p.flags |= Fl_src_only | Fl_src_disp16;
			l++;
		} else if (b[l] == 0xC3){
			p.set_op("retn");
			p.flags |= Fl_none;
			l++;
		} else if (b[l] == 0xC6) {
			p.src.pref |= PRE_BYTE_PTR;
			int mode = getr1(b,++l);
			p.r_mod = getmode40(b,l);
			if (mode == 0) {
				p.set_op("mov");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
			}
		} else if (b[l] == 0xC7) {
			p.src.pref |= PRE_DWORD_PTR;
			int mode = getr1(b,++l);
			p.r_mod = getmode40(b,l);
			if (mode == 0) {
				p.set_op("mov");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp32;
			}
		} else if (b[l] == 0xCC) {
			p.set_op("align");
			p.flags |= Fl_none;
			l++;
		} else if (b[l] == 0xD3){ // *ADD TO EYESTEP C#
			p.src.pref |= PRE_DWORD_PTR;
			p.flags |= Fl_src_dest | Fl_src_r32 | Fl_dest_r8;

			int mode = getr1(b, ++l);
			if (mode == 0)		p.set_op("rol");
			else if (mode == 1) p.set_op("ror");
			else if (mode == 2) p.set_op("rcl");
			else if (mode == 3) p.set_op("rcr");
			else if (mode == 4) p.set_op("shl");
			else if (mode == 5) p.set_op("shr");
			else if (mode == 6) p.set_op("sal");
			else if (mode == 7) p.set_op("sar");

			p.r_mod = getmode40(b, l);
			p.dest.r8 = (getr2(b, l) + 1) % 8;
		} else if (b[l] == 0xE8) {
			p.set_op("call");
			p.flags |= Fl_src_only | Fl_rel32;
			l++; // start at relative value
		} else if (b[l] == 0xE9) {
			p.set_op("jmp");
			p.flags |= Fl_src_only | Fl_rel32;
			l++; // start at relative value
		} else if (b[l] == 0xEB) {
			p.set_op("jmp short");
			p.flags |= Fl_src_only | Fl_rel8;
			l++; // start at relative value
		} else if (b[l] == 0xF6){
			int mode = getr1(b,++l);
			if (mode == 0){ // only a test instruction
				p.src.pref |= PRE_BYTE_PTR;
				p.set_op("test");
				p.flags |= Fl_src_dest | Fl_src_rm32 | Fl_dest_disp8;
				p.r_mod = getmode40(b,l);
				p.dest.r8 = getr1(b,l);
			}
		} else if (b[l] == 0xFF){
			int mode = getr1(b,++l);
			if (mode == 0)		p.set_op("inc");
			else if (mode == 1) p.set_op("dec");
			else if (mode == 2) p.set_op("call");
			else if (mode == 3) p.set_op("call");
			else if (mode == 4) p.set_op("jmp");
			else if (mode == 5) p.set_op("jmp far");
			else if (mode == 6) p.set_op("push");

			p.src.pref |= PRE_DWORD_PTR;
			p.flags |= Fl_src_only | Fl_src_rm32;
			p.r_mod = getmode40(b,l);
		}








		// Append first operand (source)
		if (p.flags & Fl_src_imm8) { // not fullproof (havent used in src operand yet)
			addoff8(p.data, b, l, p.src.imm8); // 8 bit offset or signed value
		} else if (p.flags & Fl_src_imm16){
			addoff16(p.data, b, l, p.src.imm16); // 16 bit offset or signed value
		} else if (p.flags & Fl_src_imm32){
			addoff32(p.data, b, l, p.src.imm32); // 32 bit offset or signed value
		} else if (p.flags & Fl_src_disp8) { // constant unsigned byte value
			p.src.disp8 = *(BYTE*)(b+l);
			l += 1;
			char m[4];
			sprintf_s(m, "%02X", p.src.disp8);
			strcat_s(p.data, m);
		} else if (p.flags & Fl_src_disp16) { // constant unsigned short value
			p.src.disp16 = *(USHORT*)(b+l);
			l += 2;
			char m[8];
			sprintf_s(m, "%04X", p.src.disp16);
			strcat_s(p.data, m);
		} else if (p.flags & Fl_src_disp32) { // constant unsigned int value
			p.src.disp32 = *(UINT*)(b+l);
			l += 4;
			char m[16];
			sprintf_s(m, "%08X", p.src.disp32);
			strcat_s(p.data, m);
		} if (p.flags & Fl_rel8){
			p.rel8 = *(BYTE*)(b+l);
			char m[64];
			sprintf_s(m, "exe+%08X", ((address + l + 1) + p.rel8) - _BASE);
			l += 1;
			strcat_s(p.data, m);
		} else if (p.flags & Fl_rel16){
			p.rel16 = *(USHORT*)(b+l);
			char m[64];
			sprintf_s(m, "exe+%08X", ((address + l + 2) + p.rel16) - _BASE);
			l += 2;
			strcat_s(p.data, m);
		} else if (p.flags & Fl_rel32){
			p.rel32 = *(UINT*)(b+l);
			char m[64];
			sprintf_s(m, "exe+%08X", ((address + l + 4) + p.rel32) - _BASE);
			l += 4;
			strcat_s(p.data, m);
		} else if (p.flags & Fl_src_r8)
			strcat_s(p.data, c_reg_8[p.src.r8]);
		else if (p.flags & Fl_src_r16)
			strcat_s(p.data, c_reg_16[p.src.r16]);
		else if (p.flags & Fl_src_r32)
			strcat_s(p.data, c_reg_32[p.src.r32]);
		else if (p.flags & Fl_src_rxmm)
			strcat_s(p.data, c_reg_xmm[p.src.rxmm]);
		else if (p.flags & Fl_src_rm32) {
			if (p.r_mod != 3) {
				if (p.src.pref & PRE_BYTE_PTR)
					strcat_s(p.data, "byte ptr");
				else if (p.src.pref & PRE_WORD_PTR)
					strcat_s(p.data, "word ptr");
				else if (p.src.pref & PRE_DWORD_PTR)
					strcat_s(p.data, "dword ptr");
				else if (p.src.pref & PRE_QWORD_PTR)
					strcat_s(p.data, "qword ptr");
				else if (p.pref & pre_seg) {
					switch(p.p_seg){
						case seg_cs: strcat_s(p.data, "cs:"); break;
						case seg_ss: strcat_s(p.data, "ss:"); break;
						case seg_ds: strcat_s(p.data, "ds:"); break;
						case seg_es: strcat_s(p.data, "es:"); break;
						case seg_fs: strcat_s(p.data, "fs:"); break;
						case seg_gs: strcat_s(p.data, "gs:"); break;
						default: break;
					}
				}

				strcat_s(p.data, "[");
			}

			int mode = b[l++]%8;
			switch (p.r_mod) {
				case 0: // 0x00-0x3F
					if (mode == 5) {
						p.flags |= Fl_src_disp8;
						p.src.disp32 = *(UINT*)(b+l);
						char m[16];
						sprintf_s(m,"%08X",p.src.disp32);
						strcat_s(p.data, m);
						l += 4;
					} else if (mode != 4) {
						p.src.r32 = mode;
						strcat_s(p.data, c_reg_32[p.src.r32]);
					} else { // Extended mode with a possible 32-bit offset
						mode = getmode20(b,l);
						p.src.r32 = b[l]%8;
							
						// The order that this is done is extremely
						// precise to work for the weird-ass modes
						// intel has in place -- do not change
						if (!(b[l]-(32*mode) < 8 && (mode%2==1))){
							if (b[l]%8 != 5){
								strcat_s(p.data, c_reg_32[p.src.r32]);
								strcat_s(p.data, "+");
							}

							p.src.r_2 = getr1(b,l);
							strcat_s(p.data, c_reg_32[p.src.r_2]);

							p.src.mul = mults[getmode40(b,l)];
							if (p.src.mul != 0) {
								char m[4];
								sprintf_s(m, "*%i", p.src.mul);
								strcat_s(p.data, m);
							}

							if (b[l]%8 == 5){
								p.flags |= Fl_src_imm32;
								l++; addoff32(p.data, b, l, p.src.imm32); l--;
							}
						} else {
							if (b[l]-(32*mode) == 5){
								p.flags |= Fl_src_imm32;
								l++; addoff32(p.data, b, l, p.src.imm32); l--;
							} else
								strcat_s(p.data, c_reg_32[p.src.r32]);
						}
						l++;
					}
				break;
				case 1: case 2: // 0x40-0x7F, 0x80-0xBF
					if (p.r_mod == 1)
						p.flags |= Fl_src_imm8;
					else if (p.r_mod == 2)
						p.flags |= Fl_src_imm32;

					if (mode != 4) {
						p.src.r32 = mode;
						strcat_s(p.data, c_reg_32[p.src.r32]);

						if (p.r_mod == 1) {
							addoff8(p.data, b, l, p.src.imm8);
						} else if (p.r_mod == 2) {
							addoff32(p.data, b, l, p.src.imm32);
						}
					} else { // Extended mode with a 8-bit or 32-bit offset
						mode = getmode20(b,l);;
						p.src.r32 = b[l]%8;
						strcat_s(p.data, c_reg_32[p.src.r32]);

						if (!(b[l]-(32*mode) < 8 && (mode%2==1))){
							p.src.r_2 = getr1(b, l);
							strcat_s(p.data, "+");
							strcat_s(p.data, c_reg_32[p.src.r_2]);

							p.src.mul = mults[getmode40(b,l)];
							if (p.src.mul != 0) {
								char m[4];
								sprintf_s(m, "*%i", p.src.mul);
								strcat_s(p.data, m);
							}
						}

						l++;
						if (p.r_mod == 1) {
							addoff8(p.data, b, l, p.src.imm8);
						} else if (p.r_mod == 2) {
							addoff32(p.data, b, l, p.src.imm32);
						}
					}
				break;
				default: // 0xC0-0xFF
					if (p.flags & Fl_dest_r8 || p.src.pref & PRE_BYTE_PTR){
						p.src.r8 = mode;
						strcat_s(p.data, c_reg_8[p.src.r8]);
					} else if (p.flags & Fl_dest_r16 || p.src.pref & PRE_WORD_PTR){
						p.src.r16 = mode;
						strcat_s(p.data, c_reg_16[p.src.r16]);
					} else if (p.flags & Fl_dest_rxmm || p.src.pref & PRE_QWORD_PTR) {
						p.src.rxmm = mode;
						strcat_s(p.data, c_reg_xmm[p.src.rxmm]);
					} else {
						p.src.r32 = mode;
						strcat_s(p.data, c_reg_32[p.src.r32]);
					}
				break;
			}

			if (p.r_mod != 3) strcat_s(p.data, "]");
		}



		// Instruction uses both operands?
		if (p.flags & Fl_src_dest) {
			strcat_s(p.data, ",");

			// Append second operand (destination)
			// Some things are exclusive to the source operand
			// and not applied here
			if (p.flags & Fl_dest_imm8) {
				addoff8(p.data, b, l, p.dest.imm8);
			} else if (p.flags & Fl_dest_imm16){
				addoff16(p.data, b, l, p.dest.imm16);
			} else if (p.flags & Fl_dest_imm32){
				addoff32(p.data, b, l, p.dest.imm32);
			} else if (p.flags & Fl_dest_disp8) { // constant unsigned byte value
				p.dest.disp8 = *(BYTE*)(b+l);
				l += 1;
				char m[4];
				sprintf_s(m, "%02X", p.dest.disp8);
				strcat_s(p.data, m);
			} else if (p.flags & Fl_dest_disp16) { // constant unsigned short value
				p.dest.disp16 = *(USHORT*)(b+l);
				l += 2;
				char m[8];
				sprintf_s(m, "%04X", p.dest.disp16);
				strcat_s(p.data, m);
			} else if (p.flags & Fl_dest_disp32) { // constant unsigned int value
				p.dest.disp32 = *(UINT*)(b+l);
				l += 4;
				char m[16];
				sprintf_s(m, "%08X", p.dest.disp32);
				strcat_s(p.data, m);
			} else if (p.flags & Fl_dest_r8)
				strcat_s(p.data, c_reg_8[p.dest.r8]);
			else if (p.flags & Fl_dest_r16)
				strcat_s(p.data, c_reg_16[p.dest.r16]);
			else if (p.flags & Fl_dest_r32)
				strcat_s(p.data, c_reg_32[p.dest.r32]);
			else if (p.flags & Fl_dest_rxmm)
				strcat_s(p.data, c_reg_xmm[p.dest.rxmm]);
			else if (p.flags & Fl_dest_rm32) {
				if (p.r_mod != 3) {
					if (p.dest.pref & PRE_BYTE_PTR)
						strcat_s(p.data, "byte ptr");
					else if (p.dest.pref & PRE_WORD_PTR)
						strcat_s(p.data, "word ptr");
					else if (p.dest.pref & PRE_DWORD_PTR)
						strcat_s(p.data, "dword ptr");
					else if (p.dest.pref & PRE_QWORD_PTR)
						strcat_s(p.data, "qword ptr");
					else if (p.pref & pre_seg) {
						switch(p.p_seg){
							case seg_cs: strcat_s(p.data, "cs:"); break;
							case seg_ss: strcat_s(p.data, "ss:"); break;
							case seg_ds: strcat_s(p.data, "ds:"); break;
							case seg_es: strcat_s(p.data, "es:"); break;
							case seg_fs: strcat_s(p.data, "fs:"); break;
							case seg_gs: strcat_s(p.data, "gs:"); break;
							default: break;
						}
					}

					strcat_s(p.data, "[");
				}

				int mode = b[l++]%8;
				switch (p.r_mod) {
					case 0: // 0x00-0x3F
						if (mode == 5) {
							p.flags |= Fl_dest_disp32;
							p.dest.disp32 = *(UINT*)(b+l);
							char m[16];
							sprintf_s(m,"%08X",p.dest.disp32);
							strcat_s(p.data, m);
							l += 4;
						} else if (mode != 4) {
							p.dest.r32 = mode;
							strcat_s(p.data, c_reg_32[p.dest.r32]);
						} else { // Extended mode with a possible 32-bit offset
							mode = getmode20(b,l);
							p.dest.r32 = b[l]%8;
							
							// The order that this is done is extremely precise
							// and important for the computation
							if (!(b[l]-(32*mode) < 8 && (mode%2==1))){
								if (b[l]%8 != 5){
									strcat_s(p.data, c_reg_32[p.dest.r32]);
									strcat_s(p.data, "+");
								}

								p.dest.r_2 = getr1(b,l);
								strcat_s(p.data, c_reg_32[p.dest.r_2]);

								p.dest.mul = mults[getmode40(b,l)];
								if (p.dest.mul != 0) {
									char m[4];
									sprintf_s(m, "*%i", p.dest.mul);
									strcat_s(p.data, m);
								}

								if (b[l]%8 == 5){
									p.flags |= Fl_dest_imm32;
									l++; addoff32(p.data, b, l, p.dest.imm32); l--;
								}
							} else {
								if (b[l]-(32*mode) == 5){
									p.flags |= Fl_dest_imm32;
									l++; addoff32(p.data, b, l, p.dest.imm32); l--;
								} else
									strcat_s(p.data, c_reg_32[p.dest.r32]);
							}
							l++;
						}
					break;
					case 1: case 2: // 0x40-0x7F, 0x80-0xBF
						if (p.r_mod == 1)
							p.flags |= Fl_dest_imm8;
						else if (p.r_mod == 2)
							p.flags |= Fl_dest_imm32;

						if (mode != 4) {
							p.dest.r32 = mode;
							strcat_s(p.data, c_reg_32[p.dest.r32]);

							if (p.r_mod == 1) {
								addoff8(p.data, b, l, p.dest.imm8);
							} else if (p.r_mod == 2) {
								addoff32(p.data, b, l, p.dest.imm32);
							}
						} else { // Extended mode with a 8-bit or 32-bit offset
							mode = getmode20(b,l);
							p.dest.r32 = b[l]%8;
							strcat_s(p.data, c_reg_32[p.dest.r32]);

							if (!(b[l]-(32*mode) < 8 && (mode%2==1))){
								p.dest.r_2 = getr1(b,l);
								strcat_s(p.data, "+");
								strcat_s(p.data, c_reg_32[p.dest.r_2]);

								p.dest.mul = mults[getmode40(b,l)];
								if (p.dest.mul != 0) {
									char m[4];
									sprintf_s(m, "*%i", p.dest.mul);
									strcat_s(p.data, m);
								}
							}

							l++;
							if (p.r_mod == 1) {
								addoff8(p.data, b, l, p.dest.imm8);
							} else if (p.r_mod == 2) {
								addoff32(p.data, b, l, p.dest.imm32);
							}
						}
					break;
					default: // 0xC0-0xFF
						if (p.flags & Fl_src_r8 || p.dest.pref & PRE_BYTE_PTR){
							p.dest.r8 = mode;
							strcat_s(p.data, c_reg_8[p.dest.r8]);
						} else if (p.flags & Fl_dest_r16 || p.dest.pref & PRE_WORD_PTR){
							p.dest.r16 = mode;
							strcat_s(p.data, c_reg_16[p.dest.r16]);
						} else if (p.flags & Fl_dest_rxmm || p.dest.pref & PRE_QWORD_PTR) {
							p.dest.rxmm = mode;
							strcat_s(p.data, c_reg_xmm[p.dest.rxmm]);
						} else {
							p.dest.r32 = mode;
							strcat_s(p.data, c_reg_32[p.dest.r32]);
						}
					break;
				}

				if (p.r_mod != 3) strcat_s(p.data, "]");
			}
		}

		p.len = l;
		if (p.len == 0){
			p.len++; // skip if unidentified
			strcpy_s(p.data, "?");
		}
		
		free(b);
		return p;
	}
}

#endif
