#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int counter;

typedef struct ConditionCodes {
  uint8_t z:1;
  uint8_t s:1;
  uint8_t p:1;
  uint8_t cy:1;
  uint8_t ac:1;
  uint8_t pad:3;
} ConditionCodes;

typedef struct State8080 {
  uint8_t a;
  uint8_t b;
  uint8_t c;
  uint8_t d;
  uint8_t e;
  uint8_t h;
  uint8_t l;
  uint16_t  sp;
  uint16_t  pc;
  uint8_t *memory;
  struct ConditionCodes cc;
  uint8_t int_enable;
} State8080;

int Disassemble8080p(unsigned char *buffer, int pc);

void UnimplementedInstruction(State8080 *state);
void ReadFile(State8080 *state, char *filename);

int Parity(int x, int size);

int Emulate8080p(State8080 *state);

State8080 *Init8080(void);

int main(int argc, char* argv[])
{

  counter = 0;

  State8080 *state = Init8080();

  ReadFile(state, argv[1]);
#if TEST
  state->memory[0] = 0xc3;
  state->memory[1] = 0;
  state->memory[2] = 0x01;

  state->memory[368] = 0x7;

  state->memory[0x59c] = 0xc3;
  state->memory[0x59d] = 0xc2;
  state->memory[0x59e] = 0x05;
#endif
#if 0
  while(pc < fsize) {
    pc += Disassemble8080p(buffer, pc);
  }
#endif
  int done = 0;
  while(done == 0)
  {
    done = Emulate8080p(state);
    counter++;
  }

  return 0;
}

void ReadFile(State8080 *state, char *filename)
{
  FILE *pfile = fopen(filename, "rb");
  int fsize = 0;
  if(pfile == NULL)
  {
    printf("Error: couldn't open %s\n", filename);
    exit(1);
  }
  fseek(pfile, 0L, SEEK_END);
  fsize = ftell(pfile);
  fseek(pfile, 0L, SEEK_SET);

#if TEST
  uint8_t *buffer = &state->memory[0x100];
#else
  uint8_t *buffer = &state->memory[0];
#endif
  fread(buffer, fsize, 1, pfile);
  fclose(pfile);
}

State8080 *Init8080(void)
{
  State8080 *state = calloc(1, sizeof(State8080));
  state->memory = malloc(0x10000);
  return state;
}

void UnimplementedInstruction(State8080 *state)
{
  printf("Error: Unimplemented instruction\n");

  state->pc--;
  Disassemble8080p(state->memory, state->pc);
  printf("\nOPcode: %02x", state->memory[state->pc]);
  printf("\n");
  printf("%d\n", counter);
  exit(1);
}

int Parity(int x, int size)
{
  int i;
  int parity = 0;
  x = (x & ((1 << size)-1));
  for(i = 0; i < size; i++) {
    if(x & 0x1)
      parity++;
    x = x >> 1; 
  }
  return ((parity & 0x1) == 0);
}

int Emulate8080p(State8080 *state)
{
  unsigned char *opcode = &state->memory[state->pc];

  Disassemble8080p(state->memory, state->pc);

  state->pc+=1;

  switch(*opcode)
  {
    case 0x00:  // NOP
      break;
    case 0x01: // LXI B
      {
        state->c = opcode[1];
        state->b = opcode[2];
        state->pc += 2;
        break;
      }
    case 0x02:  // STAX B
      {
        uint16_t offset = ((state->b << 8) | state->c);
        state->memory[offset] = state->a;
        break;
      }
    case 0x03:  // INX B
      {
        state->c += 1;
        if(state->c == 0x00)
          state->b += 1;
        break;
      }
    case 0x04:  // INR B
      {
        state->b += 1;
        state->cc.p = Parity(state->b, 8);
        state->cc.z = (state->b == 0);
        state->cc.s = (state->b & 0x80 != 0);
        state->cc.cy = (state->b & 0xff);
        break;
      }
    case 0x05: // DCR B
      {
        state->b -= 1;
        if(!state->b)
          state->cc.z = 1;
        else
          state->cc.z = 0;
        if((state->b & 0x80) == 0x80)
          state->cc.s = 1;
        else
          state->cc.s = 0;
        state->cc.p = Parity(state->b, 8);
        break;
      }
    case 0x06:  // MVI B
      {
        state->b = opcode[1];
        state->pc += 1;
        break;
      }
    case 0x09:  // DAD B
      {
        uint32_t hl = ((state->h << 8) | state->l);
        uint32_t bc = ((state->b << 8) | state->c);
        uint32_t res = hl + bc;
        state->h = (res & 0xff00) >> 8;
        state->l = (res & 0xff);
        state->cc.cy = ((res & 0xffff0000) > 0);
        break;
      }
    case 0x0a:  // LDAX B
      {
        uint16_t offset = ((opcode[2] << 8) | opcode[1]);
        state->a = state->memory[offset];
        break;
      }
    case 0x0c:  // INR C
      {
        state->c += 1;
        state->cc.p = Parity(state->c, 8);
        state->cc.z = (state->c == 0);
        state->cc.s = (state->c & 0x80 != 0);
        break;
      }
    case 0x0d:  // DCR C
    {
      state->c -= 1;
      state->cc.p = Parity(state->c, 8);
      state->cc.z = (state->c == 0);
      state->cc.s = ((state->c & 0x80) != 0);
      break;
    }
    case 0x0e:  // MVI C
      {
        state->c = opcode[1];
        state->pc += 1;
        break;
      }
    case 0x0f:  // RRC
      {
        uint8_t t = state->a;
        state->a = ((t & 1) << 7) | (t >> 1);
        state->cc.cy = ((t & 0x1) == 1);
        break;
      }
    case 0x11:  // LXI D
      {
        state->d = opcode[2];
        state->e = opcode[1];
        state->pc += 2;
        break;
      }
    case 0x13:  // INX  D
      {
        state->e += 1;
        if(state->e == 0x00)
          state->d += 1;
        break;
      }
    case 0x14:  // INR D
      {
        state->d += 1;
        state->cc.p = Parity(state->d, 8);
        state->cc.z = (state->d == 0);
        state->cc.s = ((state->d & 0x80 != 0));
        break;
      }
    case 0x15:  // DCR D
      {
        state->d -= 1;
        state->cc.p = Parity(state->d, 8);
        state->cc.z = (state->d == 0);
        state->cc.s = ((state->d & 0x80) != 0);
        break;
      }
    case 0x16:  // MVI D
    {
      state->d = opcode[1];
      state->pc += 1;
      break; 
    }
    case 0x19:  // DAD D
      {
        uint32_t hl = ((state->h << 8) | state->l);
        uint32_t de = ((state->d << 8) | state->e);
        uint32_t res = hl + de;
        state->h = (res & 0xff00) >> 8;
        state->l = (res & 0xff);
        state->cc.cy = ((res & 0xffff0000) != 0);
        break;
      }
    case 0x1a:  // LDAX D
      {
        uint16_t offset = ((state->d << 8) | state->e);
        state->a = state->memory[offset];
        break;
      }
    case 0x1b:  // DCX B
      {
        state->d -= 1;
        state->e -= 1;
        break;
      }
    case 0x1c:  // INR E
      {
        state->e += 1;
        state->cc.p = Parity(state->e, 8);
        state->cc.z = (state->e == 0);
        state->cc.s = (state->e & 0x80 != 0);
        break;
      }
    case 0x1d:  // DCR E
    {
      state->d -= 1;
      state->cc.p = Parity(state->e, 8);
      state->cc.z = (state->e == 0);
      state->cc.s = ((state->e & 0x80) != 0);
      break;
    }
    case 0x1e:  // MVI E
    {
      state->e = opcode[1];
      state->pc += 1;
      break;
    }
    case 0x21:  // LXI H
      {
        state->h = opcode[2];
        state->l = opcode[1];
        state->pc += 2;
        break;
      }
    case 0x23:  // INX H
      {
        state->l += 1;
        if(state->l == 0x00)
          state->h += 1;
        break;
      }
    case 0x24:  // INR H
      {
        state->h += 1;
        state->cc.p = Parity(state->h, 8);
        state->cc.z = (state->h == 0);
        state->cc.s = ((state->h & 0x80) != 0);
      }
    case 0x25:  // DCR H
      {
        state->h -= 1;
        state->cc.p = Parity(state->h, 8);
        state->cc.z = (state->h == 0);
        state->cc.s = ((state->h & 0x80) != 0);
      }
    case 0x26:  // MVI H
      {
        state->h = opcode[1];
        state->pc += 1;
        break;
      }
    case 0x29:  // DAD H
      {
        uint32_t hl = ((state->h << 8) | state->l);
        hl <<= 1;
        state->h = (hl & 0xff00) >> 8;
        state->l = (hl & 0xff);
        state->cc.cy = ((hl & 0xffff0000) != 0);
        break;
      }
    case 0x2c:  // INR L
      {
        state->l += 1;
        state->cc.p = Parity(state->l, 8);
        state->cc.s = ((state->l & 0x80) != 0);
        state->cc.z = (state->l == 0);
      }
    case 0x2d:  // DCR L
    {
      state->l -= 1;
      state->cc.p = Parity(state->l, 8);
      state->cc.s = ((state->l & 0x80) != 0);
      state->cc.z = (state->l == 0);
      break;
    }
    case 0x2e:  // MVI L
    {
      state->l = opcode[1];
      state->pc += 1;
      break;
    }
    case 0x31:  // LXI SP
      {
        state->sp = (opcode[2] << 8 | opcode[1]);
        state->pc += 2;
        break;
      }
    case 0x36:  // MVI M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->memory[offset] = opcode[1];
        state->pc += 1;
        break;
      }
    case 0x32:  // STA
      {
        uint16_t offset = ((opcode[2] << 8) | opcode[1]);
        state->memory[offset] = state->a;
        state->pc += 2;
        break;
      }
    case 0x3a:  // LDA
      {
        uint16_t offset = ((opcode[2] << 8) | opcode[1]);
        state->a = state->memory[offset];
        state->pc += 2;
        break;
      }
    case 0x3c:  // INR A
      {
        state->a += 1;
        state->cc.z = (state->a == 0);
        state->cc.p = Parity(state->a, 8);
        state->cc.s = ((state->a & 0x80) != 0);
        break;
      }
    case 0x3d:  // DCR A
      {
        state->a -= 1;
        state->cc.z = (state->a == 0);
        state->cc.p = Parity(state->a, 8);
        state->cc.s = ((state->a & 0x80) != 0);
        break;
      }
    case 0x3e:  // MVI A
      {
        state->a = opcode[1];
        state->pc += 1;
        break;
      }
    case 0x40:  // MOV B,B
    {
      state->b = state->b;
      break;
    }
    case 0x41:  // MOV B,C
    {
      state->b = state->c;
      break;
    }
    case 0x42:  // MOV B,D
      {
        state->b = state->d;
        break;
      }
    case 0x43:  // MOV B,E
      {
        state->b = state->e;
        break;
      }
    case 0x44:  // MOV B,H
      {
        state->b = state->h;
        break;
      }
    case 0x45:  // MOV B,L
      {
        state->b = state->l;
        break;
      }
    case 0x47:  // MOV B,A
      {
        state->b = state->a;
        break;
      }
    case 0x48:  // MOV C,B
      {
        state->c = state->b;
        break;
      }
    case 0x49:  // MOV C,C
    {
      state->c = state->c;
      break;
    }
    case 0x4a:  // MOV C,D
    {
      state->c = state->d;
      break;
    }
    case 0x4b:  // MOV C,E
      {
        state->c = state->e;
        break;
      }
    case 0x4c:  // MOV C,H
    {
      state->c = state->h;
      break;
    }
    case 0x4d:  // MOV C,L
      {
        state->c = state->l;
        break;
      }
    case 0x4f:  // MOV C,A
      {
        state->c = state->a;
        break;
      }
    case 0x50:  // MOV D,B
      {
        state->d = state->b;
        break;
      }
    case 0x51:  // MOV D,C
      {
        state->d = state->c;
        break;
      }
    case 0x52:  // MOV D,D
    {
      state->d = state->d;
      break;
    }
    case 0x53:  // MOV D,E
    {
      state->d = state->e;
      break;
    }
    case 0x54:  // MOV D,H
    {
      state->d = state->h;
      break;
    }
    case 0x55:  // MOV D,L
      {
        state->d = state->l;
        break;
      }
    case 0x56:  // MOV D,M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->d = state->memory[offset];
        break;
      }
    case 0x57:  // MOV D,A
      {
        state->d = state->a;
        break;
      }
    case 0x58:  // MOV E,B
      {
        state->e = state->b;
        break;
      }
    case 0x59:  // MOV E,C
      {
        state->e = state->c;
        break;
      }
    case 0x5a:  // MOV E,D
      {
        state->e = state->d;
        break;
      }
    case 0x5b:  // MOV E,E
    {
      state->e = state->e;
      break;
    }
    case 0x5c:  // MOV E,H
    {
      state->e = state->h;
      break;
    }
    case 0x5d:  // MOV E,L
    {
      state->e = state->l;
      break;
    }
    case 0x5e:  // MOV E,M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->e = state->memory[offset];
        break;
      }
    case 0x5f:  // MOV E,A
      {
        state->e = state->a;
        break;
      }
    case 0x60:  // MOV H,B
      {
        state->h = state->b;
        break;
      }
    case 0x61:  // MOV H,C
      {
        state->h = state->c;
        break;
      }
    case 0x62:  // MOV H,D
      {
        state->h = state->d;
        break;
      }
    case 0x63:  // MOV H,E
      {
        state->h = state->e;
        break;
      }
    case 0x64:  // MOV H,H
    {
      state->h = state->h;
      break;
    }
    case 0x65:  // MOV H,L
    {
      state->h = state->l;
      break;
    }
    case 0x66:  // MOV H,M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->h = state->memory[offset];
        break;
      }
    case 0x67:  // MOV H,A
      {
        state->h = state->a;
        break;
      }
    case 0x68:  // MOV L,B
      {
        state->l = state->b;
        break;
      }
    case 0x69:  // MOV L,C
      {
        state->l = state->c;
        break;
      }
    case 0x6a:  // MOV L,D
      {
        state->l = state->d;
        break;
      }
    case 0x6b:  // MOV L,E
      {
        state->l = state->e;
        break;
      }
    case 0x6c:  // MOV L,H
      {
        state->l = state->h;
        break;
      }
    case 0x6d:  // MOV L,L
    {
      state->l = state->l;
      break;
    }
    case 0x6f:  // MOV L,A
      {
        state->l = state->a;
        break;
      }
    case 0x77:  // MOV M,A
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->memory[offset] = state->a;
        break;
      }
    case 0x78:  // MOV A,B
    {
      state->a = state->b;
      break;
    }
    case 0x79:  // MOV A,C
      {
        state->a = state->c;
        break;
      }
    case 0x7a:  // MOV A,D
      {
        state->a = state->d;
        break;
      }
    case 0x7b:  // MOV A,E
      {
        state->a = state->b;
        break;
      }
    case 0x7c:  // MOV A,H
      {
        state->a = state->h;
        break;
      }
    case 0x7d:  // MOV A,L
      {
        state->a = state->l;
        break;
      }
    case 0x7e:  // MOV A,M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        state->a = state->memory[offset];
        break;
      }
    case 0x7f:  // MOV A,A
    {
      state->a = state->a;
      break;
    }
    case 0x80:  // ADD B
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->b;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x81:  // ADD C
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->c;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x82:  // ADD D
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->d;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x83:  // ADD E
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->e;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x84:  // ADD H
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->h;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x85:  // ADD L
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->l;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x86:  // ADD M (HL)
      {
        uint16_t offset = (state->h<<8) | (state->l);
        uint16_t result = (uint16_t)state->a + state->memory[offset];
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x87:  // ADD A
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)state->a;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.cy = (result > 0xff);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0x88:  // ADC B
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->b + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x89:  // ADC C
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->c + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x8a:  // ADC D
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->d + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x8b:  // ADC E
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->e + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x8c:  // ADC H
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->h + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x8d:  // ADC L
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->l + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x8f:  // ADC A
    {
      uint16_t result = (uint16_t)state->a + (uint16_t)state->a + state->cc.cy;
      state->a = result & 0xff;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.cy = ((result & 0x100) != 0);
      state->cc.p = Parity(state->a, 8);
      break;
    }
    case 0x90:  // SUB B
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->b;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x91:  // SUB C
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->c;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x92:  // SUB D
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->d;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x93:  // SUB E
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->e;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x94:  // SUB H
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->h;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x95:  // SUB L
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->l;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x97:  // SUB A
    {
      uint16_t result = (uint16_t)state->a - (uint16_t)state->a;
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x98:  // SSB B
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->b + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x99:  // SSB C
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->c + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x9a:  // SSB D
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->d + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x9b:  // SSB E
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->e + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x9c:  // SSB H
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->h + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x9d:  // SSB L
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->l + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0x9f:  // SSB A
    {
      uint16_t result = (uint16_t)state->a - ((uint16_t)state->a + state->cc.cy);
      state->a = (uint8_t)result;
      state->cc.z = ((result & 0xff) == 0);
      state->cc.s = ((result & 0x80) != 0);
      state->cc.p = Parity(state->a, 8);
      state->cc.cy = ((result & 0x100) != 0);
      break;
    }
    case 0xa1:  // ANA C
      {
        state->a &= state->c;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xa2:  // ANA D
      {
        state->a &= state->d;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xa3:  // ANA E
      {
        state->a &= state->e;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xa4:  // ANA H
      {
        state->a &= state->h;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xa5:  // ANA L
      {
        state->a &= state->l;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xa7:  // ANA A
      {
        state->a &= state->a;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xaf:  // XRA A
      {
        state->a ^= state->a;
        state->cc.z = 1;
        state->cc.s = 0;
        state->cc.cy = 0;
        state->cc.ac = 0;
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb0:  // ORA B
      {
        state->a |= state->b;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb1:  // ORA C
      {
        state->a |= state->c;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb2:  // ORA D
      {
        state->a |= state->d;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb3:  // ORA E
      {
        state->a |= state->e;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb4:  // ORA H
      {
        state->a |= state->h;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb5:  // ORA L
      {
        state->a |= state->l;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
    case 0xb6:  // ORA M
      {
        uint16_t offset = ((state->h << 8) | state->l);
        uint16_t result = state->a | state->memory[offset];
        state->cc.cy = 0;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.p = Parity(result, 8);
        state->a = result & 0xff;
        break;
      }
    case 0xb7:  // ORA A
      {
        state->a |= state->a;
        state->cc.cy = 0;
        state->cc.z = ((state->a & 0xff) == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        break;
      }
#if 0
    case 0xbc:  // CMP H
      {
        uint16_t result = state->a - state->h;
        state->cc.z = (result == 0);
        state->cc.s = ((result & 0xff00) != 0);
        state->cc.p = Parity(result, 8);
        state->cc.cy = ((result & 0x100) != 1);
        break;
      }
#endif
    case 0xc0:  // RNZ
      {
        if(!state->cc.z) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xc1:  // POP B
      {
        state->c = state->memory[state->sp];
        state->b = state->memory[state->sp+1];
        state->sp += 2;
        break;
      }
    case 0xc2:  // JNZ
      {
        if(state->cc.z == 0)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xc3:  // JMP
      {
        state->pc = ((opcode[2] << 8) | opcode[1]);
        break;
      }
    case 0xc4:  // CNZ
      {
        if(!state->cc.z) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xc5:  // PUSH B
      {
        state->memory[state->sp-1] = state->b;
        state->memory[state->sp-2] = state->c;
        state->sp -= 2;
        break;
      }
    case 0xc6:  // ADI
      {
        uint16_t t = (uint16_t)state->a + (uint16_t)opcode[1];
        state->cc.z = ((t & 0xff) == 0);
        state->cc.s = ((t & 0x80) == 0x80);
        //state->cc.p = Parity((t & 0xff), 8);
        state->cc.p = Parity((uint8_t)t, 8);
        state->cc.cy = (t > 0xff);
        state->a = (uint8_t)t;
        state->pc += 1;
        break;
      }
    case 0xc8:  // RZ
      {
        if(state->cc.z) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xc9:  // RET
      {
        state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
        state->sp += 2;
        break;
      }
    case 0xca:  // JZ
      {
        if(state->cc.z)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xcc:  // CZ
      {
        if(state->cc.z) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xcd:  // CALL
#if TEST
      if(((opcode[2] << 8) | opcode[1]) == 5)
      {
        if(state->c == 9)
        {
          uint16_t offset = ((state->d << 8) | state->e);
          char *str = &state->memory[offset+3];
          while(*str != '$') {
            printf("%c", *str++);
          }
          printf("\n");
        }
        else if(state->c == 2)
        {
          printf("print char routine called\n");
        }
      }
      else if(((opcode[2] << 8) | opcode[1]) == 0)
      {
        exit(0);
      }
      else
#endif
      {
        uint16_t ret = state->pc+2;
        state->memory[state->sp-1] = (ret >> 8) & 0xff;
        state->memory[state->sp-2] = (ret & 0xff);
        state->sp = state->sp-2;
        state->pc = ((opcode[2] << 8) | opcode[1]);
        break;
      }
    case 0xce:  // ACI
      {
        uint16_t result = (uint16_t)state->a + (uint16_t)opcode[1] + state->cc.cy;
        state->cc.z = ((result & 0xff) == 0);
        state->cc.s = ((result & 0x80) == 0x80);
        state->cc.p = Parity(result, 8);
        state->cc.cy = (result > 0xff);
        state->a = (uint8_t)result;
        state->pc += 1;
        break;
      }
    case 0xd0:  // RNC
      {
        if(!state->cc.cy) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xd1:  // POP D
      {
        state->e = state->memory[state->sp];
        state->d = state->memory[state->sp+1];
        state->sp += 2;
        break;
      }
    case 0xd2:  // JNC
      {
        if(!state->cc.cy)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xd3:  // OUT
      {
        state->pc += 1;
        break;
      }
    case 0xd4:  // CNC
      {
        if(!state->cc.cy) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xd5:  // PUSH D
      {
        state->memory[state->sp-1] = state->d;
        state->memory[state->sp-2] = state->e;
        state->sp -= 2;
        break;
      }
    case 0xd6:  // SUI
      {
        uint16_t result = (uint16_t)state->a - (uint16_t)opcode[1];
        //state->cc.cy = 1;
        state->cc.cy = ((result & 0x100) != 0);
        state->cc.p = Parity((uint8_t)result, 8);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.z = (result == 0);
        state->a = (uint8_t)result;
        state->pc += 1;
        break;
      }
    case 0xd8:  // RC
      {
        if(state->cc.cy) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xda:  // JC
      {
        if(state->cc.cy)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xdc:  // CC
      {
        if(state->cc.cy) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xde:  // SBI
      {
        uint16_t result = (uint16_t)state->a - ((uint16_t)opcode[1] + state->cc.cy);
        state->cc.cy = 1;
        state->cc.p = Parity((uint8_t)result, 8);
        state->cc.s = ((result & 0x80) != 0);
        state->cc.z = (result == 0);
        state->a = (uint8_t)result;
        state->pc += 1;
        break;
      }
    case 0xe0:  // RPO
      {
        if(!state->cc.p) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xe1:  // POP H
      {
        state->l = state->memory[state->sp];
        state->h = state->memory[state->sp+1];
        state->sp += 2;
        break;
      }
    case 0xe2:  // JPO
      {
        if(!state->cc.p)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xe3:  // XTHL
      {
        uint8_t t1 = state->memory[state->sp];
        uint8_t t2 = state->memory[state->sp+1];
        state->memory[state->sp] = state->l;
        state->memory[state->sp] = state->h;
        state->l = t1;
        state->h = t2;
      }
    case 0xe4:  // CPO
      {
        if(!state->cc.p) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xe5:  // PUSH H
      {
        state->memory[state->sp-1] = state->h;
        state->memory[state->sp-2] = state->l;
        state->sp -= 2;
        break;
      }
    case 0xe6:  // ANI
      {
        state->a &= opcode[1];
        state->cc.cy = 0;
        state->cc.ac = 0;
        state->cc.s = ((state->a & 0x80) != 0);
        state->cc.p = Parity(state->a, 8);
        state->cc.z = (state->a == 0);
        state->pc += 1;
        break;
      }
    case 0xe8:  // RPE
      {
        if(state->cc.p) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xea:  // JPE
      {
        if(state->cc.p)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xeb:  // XCHG
      {
        uint16_t t1 = state->h;
        uint16_t t2 = state->l;
        state->h = state->d;
        state->l = state->e;
        state->d = t1;
        state->e = t2;
        break;
      }
    case 0xec:  // CPE
      {
        if(state->cc.p) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xee:  // XRI
      {
        state->a ^= opcode[1];
        state->cc.cy = 0;
        state->cc.p = Parity(state->a, 8);
        state->cc.z = (state->a == 0);
        state->cc.s = ((state->a & 0x80) != 0);
        state->pc += 1;
        break;
      }
    case 0xf0:  // RP
      {
        if(!state->cc.s) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xf1:  // POP PSW
      {
        state->a = state->memory[state->sp+1];
        uint8_t psw = state->memory[state->sp];
        state->cc.z = ((psw & 0x01) == 0x01);
        state->cc.s = ((psw & 0x02) == 0x02);
        state->cc.p = ((psw & 0x04) == 0x04);
        state->cc.cy = ((psw & 0x05) == 0x05);
        state->cc.ac = ((psw & 0x10) == 0x10);
        state->sp += 2;
        break;
      }
    case 0xf2:  // JP
      {
        if(!state->cc.s)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xf4:  // CP
      {
        if(!state->cc.s) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xf5:  // PUSH PSW
      {
        state->memory[state->sp-1] = state->a;
        uint8_t psw = (state->cc.z | state->cc.s << 1 | state->cc.p << 2 | state->cc.cy << 3 | state->cc.ac << 4);
        state->memory[state->sp-2] = psw;
        state->sp -= 2;
        break;
      }
    case 0xf6:  // ORI
      {
        state->a |= opcode[1];
        state->cc.cy = 0;
        state->cc.z = (state->a == 0);
        state->cc.p = Parity(state->a, 8);
        state->cc.s = ((state->a & 0x80) != 0);
        state->pc += 1;
        break;
      } 
    case 0xf8:  // RM
      {
        if(state->cc.s) {
          state->pc = ((state->memory[state->sp+1] << 8) | state->memory[state->sp]);
          state->sp += 2;
        }
        break;
      }
    case 0xfa:  // JM
      {
        if(state->cc.s)
          state->pc = ((opcode[2] << 8) | opcode[1]);
        else
          state->pc += 2;
        break;
      }
    case 0xfb:  //EI
      {
        state->int_enable = 1;
        break;
      }
    case 0xfc:  // CM
      {
        if(state->cc.s) {
          uint16_t ret = state->pc +2;
          state->memory[state->sp -1] = (ret >> 8) & 0xff;
          state->memory[state->sp -2] = (ret & 0xff);
          state->sp = state->sp -2;
          state->pc = ((opcode[2] << 8) | opcode[1]);
        }
        else
          state->pc += 2;
        break;
      }
    case 0xfe:  // CPI
      {
        uint8_t t = state->a - opcode[1];
        state->cc.z = (t == 0);
        state->cc.s = ((t & 0x80) == 0x80);
        state->cc.p = Parity(t, 8);
        state->cc.cy = (state->a < opcode[1]);
        state->pc += 1;
        break;
      }
    default:
      {
        UnimplementedInstruction(state);
        break;
      }
  }
#if DEBUG
  printf("\t");
  printf("%c", state->cc.z ? 'z' : '.');
  printf("%c", state->cc.s ? 's' : '.');
  printf("%c", state->cc.p ? 'p' : '.');
  printf("%c", state->cc.cy ? 'c' : '.');
  printf("%c  ", state->cc.ac ? 'a' : '.');
  printf("A $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP %04x\n", state->a, state->b, state->c,
      state->d, state->e, state->h, state->l, state->sp);
#endif
  return 0;
}

int Disassemble8080p(unsigned char *buffer, int pc)
{
  unsigned char* code = &buffer[pc];
  int opbytes = 1;
  printf("%04x ", pc);
  switch(*code)
  {
    case 0x00: printf("NOP"); break;
    case 0x01: printf("LXI    B,#$%02x%0x2", code[2], code[1]); opbytes = 3; break;
    case 0x02: printf("STAX   B\t"); break;
    case 0x03: printf("INX    B\t"); break;
    case 0x04: printf("INR    B\t"); break;
    case 0x05: printf("DCR    B\t"); break;
    case 0x06: printf("MVI    B,#$%02x", code[1]); opbytes = 2; break;
    case 0x07: printf("RLC    \t"); break;
    case 0x08: printf("NOP    \t"); break;
    case 0x09: printf("DAD    B\t"); break;
    case 0x0a: printf("LDAX   B\t"); break;
    case 0x0b: printf("DCX    B\t"); break;
    case 0x0c: printf("INR    C\t"); break;
    case 0x0d: printf("DCR    C\t"); break;
    case 0x0e: printf("MVI    C, #$%02x", code[1]); opbytes = 2; break;
    case 0x0f: printf("RRC    \t"); break;

    case 0x10: printf("NOP"); break;
    case 0x11: printf("LXI    D,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x12: printf("STAX   D\t"); break;
    case 0x13: printf("INX    D\t"); break;
    case 0x14: printf("INR    D\t"); break;
    case 0x15: printf("DCR    D\t"); break;
    case 0x16: printf("MVI    D,#$%02x", code[1]); opbytes = 2; break;
    case 0x17: printf("RAL    \t"); break;
    case 0x18: printf("NOP"); break;
    case 0x19: printf("DAD    D\t"); break;
    case 0x1a: printf("LDAX   D\t"); break;
    case 0x1b: printf("DXC    D\t"); break;
    case 0x1c: printf("INR    E\t"); break;
    case 0x1d: printf("DCR    E\t"); break;
    case 0x1e: printf("MVI    E,#$%02x", code[1]); opbytes = 2; break;
    case 0x1f: printf("RAR    \t"); break;

    case 0x20: printf("NOP"); break;
    case 0x21: printf("LXI    H,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x22: printf("SHLD   $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x23: printf("INX    H\t"); break;
    case 0x24: printf("INR    H\t"); break;
    case 0x25: printf("DCR    H\t"); break;
    case 0x26: printf("MVI    H,#$%02x", code[1]); opbytes = 2; break;
    case 0x27: printf("DAA    \t"); break;
    case 0x28: printf("NOP"); break;
    case 0x29: printf("DAD    H\t"); break;
    case 0x2a: printf("LHLD   $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x2b: printf("DCX    H\t"); break;
    case 0x2c: printf("INR    L\t"); break;
    case 0x2d: printf("DCR    L\t"); break;
    case 0x2e: printf("MVI    L,#$%02x", code[1]); opbytes = 2; break;
    case 0x2f: printf("CMA    \t"); break;

    case 0x30: printf("NOP"); break;
    case 0x31: printf("LXI    SP,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x32: printf("STA    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0x33: printf("INX    SP\t"); break;
    case 0x34: printf("INR    M\t"); break;
    case 0x35: printf("DCR    M\t"); break;
    case 0x36: printf("MVI    M,#$%02x", code[1]); opbytes = 2; break;
    case 0x37: printf("STC    \t"); break;
    case 0x38: printf("NOP"); break;
    case 0x39: printf("DAD    SP\t"); break;
    case 0x3a: printf("LDA    $%02x%0x2", code[2], code[1]); opbytes = 3; break;
    case 0x3b: printf("DCX    SP\t"); break;
    case 0x3c: printf("INR    A\t"); break;
    case 0x3d: printf("DCR    A\t"); break;
    case 0x3e: printf("MVI    A, #$%02x", code[1]); opbytes = 2; break;
    case 0x3f: printf("CMC    \t"); break;

    case 0x40: printf("MOV    B,B\t"); break;
    case 0x41: printf("MOV    B,C\t"); break;
    case 0x42: printf("MOV    B,D\t"); break;
    case 0x43: printf("MOV    B,E\t"); break;
    case 0x44: printf("MOV    B,H\t"); break;
    case 0x45: printf("MOV    B,L\t"); break;
    case 0x46: printf("MOV    B,M\t"); break;
    case 0x47: printf("MOV    B,A\t"); break;
    case 0x48: printf("MOV    C,B\t"); break;
    case 0x49: printf("MOV    C,C\t"); break;
    case 0x4a: printf("MOV    C,D\t"); break;
    case 0x4b: printf("MOV    C,E\t"); break;
    case 0x4c: printf("MOV    C,H\t"); break;
    case 0x4d: printf("MOV    C,L\t"); break;
    case 0x4e: printf("MOV    C,M\t"); break;
    case 0x4f: printf("MOV    C,A\t"); break;

    case 0x50: printf("MOV    D,B\t"); break;
    case 0x51: printf("MOV    D,C\t"); break;
    case 0x52: printf("MOV    D,D\t"); break;
    case 0x53: printf("MOV    D,E\t"); break;
    case 0x54: printf("MOV    D,H\t"); break;
    case 0x55: printf("MOV    D,L\t"); break;
    case 0x56: printf("MOV    D,M\t"); break;
    case 0x57: printf("MOV    D,A\t"); break;
    case 0x58: printf("MOV    E,B\t"); break;
    case 0x59: printf("MOV    E,C\t"); break;
    case 0x5a: printf("MOV    E,D\t"); break;
    case 0x5b: printf("MOV    E,E\t"); break;
    case 0x5c: printf("MOV    E,H\t"); break;
    case 0x5d: printf("MOV    E,L\t"); break;
    case 0x5e: printf("MOV    E,M\t"); break;
    case 0x5f: printf("MOV    E,A\t"); break;

    case 0x60: printf("MOV    H,B\t"); break;
    case 0x61: printf("MOV    H,C\t"); break;
    case 0x62: printf("MOV    H,D\t"); break;
    case 0x63: printf("MOV    H,E\t"); break;
    case 0x64: printf("MOV    H,H\t"); break;
    case 0x65: printf("MOV    H,L\t"); break;
    case 0x66: printf("MOV    H,M\t"); break;
    case 0x67: printf("MOV    H,A\t"); break;
    case 0x68: printf("MOV    L,B\t"); break;
    case 0x69: printf("MOV    L,C\t"); break;
    case 0x6a: printf("MOV    L,D\t"); break;
    case 0x6b: printf("MOV    L,E\t"); break;
    case 0x6c: printf("MOV    L,H\t"); break;
    case 0x6d: printf("MOV    L,L\t"); break;
    case 0x6e: printf("MOV    L,M\t"); break;
    case 0x6f: printf("MOV    L,A\t"); break;

    case 0x70: printf("MOV    M,B\t"); break;
    case 0x71: printf("MOV    M,C\t"); break;
    case 0x72: printf("MOV    M,D\t"); break;
    case 0x73: printf("MOV    M,E\t"); break;
    case 0x74: printf("MOV    M,H\t"); break;
    case 0x75: printf("MOV    M,L\t"); break;
    case 0x76: printf("HLT    "); break;
    case 0x77: printf("MOV    M,A\t"); break;
    case 0x78: printf("MOV    A,B\t"); break;
    case 0x79: printf("MOV    A,C\t"); break;
    case 0x7a: printf("MOV    A,D\t"); break;
    case 0x7b: printf("MOV    A,E\t"); break;
    case 0x7c: printf("MOV    A,H\t"); break;
    case 0x7d: printf("MOV    A,L\t"); break;
    case 0x7e: printf("MOV    A,M\t"); break;
    case 0x7f: printf("MOV    A,A\t"); break;

    case 0x80: printf("ADD    B\t"); break;
    case 0x81: printf("ADD    C\t"); break;
    case 0x82: printf("ADD    D\t"); break;
    case 0x83: printf("ADD    E\t"); break;
    case 0x84: printf("ADD    H\t"); break;
    case 0x85: printf("ADD    L\t"); break;
    case 0x86: printf("ADD    M\t"); break;
    case 0x87: printf("ADD    A\t"); break;
    case 0x88: printf("ADC    B\t"); break;
    case 0x89: printf("ADC    C\t"); break;
    case 0x8a: printf("ADC    D\t"); break;
    case 0x8b: printf("ADC    E\t"); break;
    case 0x8c: printf("ADC    H\t"); break;
    case 0x8d: printf("ADC    L\t"); break;
    case 0x8e: printf("ADC    M\t"); break;
    case 0x8f: printf("ADC    A\t"); break;

    case 0x90: printf("SUB    B\t"); break;
    case 0x91: printf("SUB    C\t"); break;
    case 0x92: printf("SUB    D\t"); break;
    case 0x93: printf("SUB    E\t"); break;
    case 0x94: printf("SUB    H\t"); break;
    case 0x95: printf("SUB    L\t"); break;
    case 0x96: printf("SUB    M\t"); break;
    case 0x97: printf("SUB    A\t"); break;
    case 0x98: printf("SSB    B\t"); break;
    case 0x99: printf("SSB    C\t"); break;
    case 0x9a: printf("SSB    D\t"); break;
    case 0x9b: printf("SSB    E\t"); break;
    case 0x9c: printf("SSB    H\t"); break;
    case 0x9d: printf("SSB    L\t"); break;
    case 0x9e: printf("SSB    M\t"); break;
    case 0x9f: printf("SSB    A\t"); break;

    case 0xa0: printf("ANA    B\t"); break;
    case 0xa1: printf("ANA    C\t"); break;
    case 0xa2: printf("ANA    D\t"); break;
    case 0xa3: printf("ANA    E\t"); break;
    case 0xa4: printf("ANA    H\t"); break;
    case 0xa5: printf("ANA    L\t"); break;
    case 0xa6: printf("ANA    M\t"); break;
    case 0xa7: printf("ANA    A\t"); break;
    case 0xa8: printf("XRA    B\t"); break;
    case 0xa9: printf("XRA    C\t"); break;
    case 0xaa: printf("XRA    D\t"); break;
    case 0xab: printf("XRA    E\t"); break;
    case 0xac: printf("XRA    H\t"); break;
    case 0xad: printf("XRA    L\t"); break;
    case 0xae: printf("XRA    M\t"); break;
    case 0xaf: printf("XRA    A\t"); break;

    case 0xb0: printf("ORA    B\t"); break;
    case 0xb1: printf("ORA    C\t"); break;
    case 0xb2: printf("ORA    D\t"); break;
    case 0xb3: printf("ORA    E\t"); break;
    case 0xb4: printf("ORA    H\t"); break;
    case 0xb5: printf("ORA    L\t"); break;
    case 0xb6: printf("ORA    M\t"); break;
    case 0xb7: printf("ORA    A\t"); break;
    case 0xb8: printf("CMP    B\t"); break;
    case 0xb9: printf("CMP    C\t"); break;
    case 0xba: printf("CMP    D\t"); break;
    case 0xbb: printf("CMP    E\t"); break;
    case 0xbc: printf("CMP    H\t"); break;
    case 0xbd: printf("CMP    L\t"); break;
    case 0xbe: printf("CMP    M\t"); break;
    case 0xbf: printf("CMP    A\t"); break;

    case 0xc0: printf("RNZ    \t"); break;
    case 0xc1: printf("POP    B\t"); break;
    case 0xc2: printf("JNZ    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xc3: printf("JMP    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xc4: printf("CNZ    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xc5: printf("PUSH   B\t"); break;
    case 0xc6: printf("ADI    #$%02x", code[1]); opbytes = 2; break;
    case 0xc7: printf("RST    0"); break;
    case 0xc8: printf("RZ     \t"); break;
    case 0xc9: printf("RET    \t"); break;
    case 0xca: printf("JZ     $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xcb: printf("JMP    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xcc: printf("CZ     $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xcd: printf("CALL   $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xce: printf("ACI    #$%02x", code[1]); opbytes = 2; break;
    case 0xcf: printf("RST    1\t"); break;

    case 0xd0: printf("RNC    \t"); break;
    case 0xd1: printf("POP    D\t"); break;
    case 0xd2: printf("JNC    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xd3: printf("OUT    #$%02x", code[1]); opbytes = 2; break;
    case 0xd4: printf("CNC    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xd5: printf("PUSH   D\t"); break;
    case 0xd6: printf("SUI    #$%02x", code[1]); opbytes = 2; break;
    case 0xd7: printf("RST    2\t"); break;
    case 0xd8: printf("RC     \t"); break;
    case 0xd9: printf("RET    \t"); break;
    case 0xda: printf("JC     $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xdb: printf("IN     #%02x", code[1]); opbytes = 2; break;
    case 0xdc: printf("CC     #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xdd: printf("CALL   #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xde: printf("SBI    #$%02x", code[1]); opbytes = 2; break;
    case 0xdf: printf("RST    5"); break;

    case 0xe0: printf("RPO    \t"); break;
    case 0xe1: printf("POP    H"); break;
    case 0xe2: printf("JPO    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xe3: printf("XTHL   \t"); break;
    case 0xe4: printf("CPO    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xe5: printf("PUSH   H\t"); break;
    case 0xe6: printf("ANI    #$%02x", code[1]); opbytes = 2; break;
    case 0xe7: printf("RST    4\t"); break;
    case 0xe8: printf("RPE    \t"); break;
    case 0xe9: printf("PCHL   \t"); break;
    case 0xea: printf("JPE    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xeb: printf("XCHG   \t"); break;
    case 0xec: printf("CPE    $%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xed: printf("CALL   #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xee: printf("XRI    #$%02x", code[1]); opbytes = 2; break;
    case 0xef: printf("RST    5\t"); break;

    case 0xf0: printf("RP      \t"); break;
    case 0xf1: printf("POP    PSW"); break;
    case 0xf2: printf("JP     #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xf3: printf("DI      \t"); break;
    case 0xf4: printf("CP     #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xf5: printf("PUSH   PSW"); break;
    case 0xf6: printf("ORI    #$%02x", code[1]); opbytes = 2; break;
    case 0xf7: printf("RST    6\t"); break;
    case 0xf8: printf("RM      \t"); break;
    case 0xf9: printf("SPHL   \t"); break;
    case 0xfa: printf("JM     #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xfb: printf("EI      \t"); break;
    case 0xfc: printf("CM     #$%02x%02x", code[2], code[1]); opbytes = 3; break;
    case 0xfd: printf("CALL   #$%02x%02x", code[2],code[1]); opbytes = 3; break;
    case 0xfe: printf("CPI    #$%02x", code[1]); opbytes = 2; break;
    case 0xff: printf("RST    7\t"); break;

    default:
               printf(": %02x", code[0]);
               break;
  }
  //printf("\n");

  return opbytes;
}
