#pragma once


typedef unsigned char OpCode;
#define OPCODE(code) (OpCode(code))

// Packet:
// <OPCODE_STOP> [EOF]
#define OPCODE_STOP   OPCODE(0)

// Packet:
// <OPCODE_STRING> <String> [EOF]
#define OPCODE_STRING OPCODE(1)

// Packet:
// <OPCODE_RESIZE> <Int32 Rows> <Int32 Cols> [EOF]
#define OPCODE_RESIZE OPCODE(2)

