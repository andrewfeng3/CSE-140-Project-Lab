#include <stdio.h>    
#include <stdint.h>   
#include <string.h>   

/*
 * Converts a 32-bit binary string into a 32-bit unsigned integer.
 * Each character ('0' or '1') is shifted into the integer.
 */
uint32_t binToUint(char *bin) {
    uint32_t value = 0;           // Initialize result value to 0
    for (int i = 0; i < 32; i++) { // Loop through all 32 bits
        value = (value << 1) | (bin[i] - '0'); // Shift left and add current bit
    }
    return value; // Return the final integer representation
}

/*
 * Sign-extends a value that originally has 'bits' width into 32-bit signed int.
 * Needed for immediates that may be negative.
 */
int signExtend(int value, int bits) {
    int shift = 32 - bits;        // Amount to shift to propagate sign bit
    return (value << shift) >> shift; // Left then right shift preserves sign
}

/*
 * Decode R-type instructions using funct3 and funct7 fields.
 * Returns the mnemonic string.
 */
const char* decodeR(uint32_t funct3, uint32_t funct7) {
    if (funct3 == 0 && funct7 == 0) return "add";   // add instruction
    if (funct3 == 0 && funct7 == 32) return "sub";  // sub instruction
    if (funct3 == 7) return "and";                  // bitwise and
    if (funct3 == 6) return "or";                   // bitwise or
    if (funct3 == 4) return "xor";                  // bitwise xor
    if (funct3 == 1) return "sll";                  // shift left logical
    if (funct3 == 2) return "slt";                  // set less than (signed)
    if (funct3 == 3) return "sltu";                 // set less than (unsigned)
    if (funct3 == 5 && funct7 == 0) return "srl";   // shift right logical
    if (funct3 == 5 && funct7 == 32) return "sra";  // shift right arithmetic
    return "unknown";                               // Fallback (should not occur)
}

/*
 * Decode I-type instructions.
 * opcode distinguishes arithmetic, load, and jalr instructions.
 */
const char* decodeI(uint32_t opcode, uint32_t funct3, uint32_t funct7) {
    if (opcode == 0x13) {               // Immediate arithmetic instructions
        if (funct3 == 0) return "addi";
        if (funct3 == 7) return "andi";
        if (funct3 == 6) return "ori";
        if (funct3 == 4) return "xori";
        if (funct3 == 2) return "slti";
        if (funct3 == 3) return "sltiu";
        if (funct3 == 1) return "slli";
        if (funct3 == 5 && funct7 == 0) return "srli";
        if (funct3 == 5 && funct7 == 32) return "srai";
    }

    if (opcode == 0x03) {               // Load instructions
        if (funct3 == 0) return "lb";
        if (funct3 == 1) return "lh";
        if (funct3 == 2) return "lw";
    }

    if (opcode == 0x67) return "jalr";  // Jump and link register

    return "unknown";
}

/*
 * Decode S-type store instructions based on funct3 field.
 */
const char* decodeS(uint32_t funct3) {
    if (funct3 == 0) return "sb";  // store byte
    if (funct3 == 1) return "sh";  // store halfword
    if (funct3 == 2) return "sw";  // store word
    return "unknown";
}

/*
 * Decode SB-type branch instructions based on funct3 field.
 */
const char* decodeSB(uint32_t funct3) {
    if (funct3 == 0) return "beq"; // branch if equal
    if (funct3 == 1) return "bne"; // branch if not equal
    if (funct3 == 4) return "blt"; // branch if less than
    if (funct3 == 5) return "bge"; // branch if greater or equal
    return "unknown";
}

int main() {
    char input[40]; // Buffer to store 32-bit binary string (plus null terminator)

    while (1) { // Loop to allow decoding multiple instructions
        printf("Enter an instruction:\n"); // Prompt user
        scanf("%s", input);                // Read 32-bit binary string

        // Convert binary string into 32-bit integer representation
        uint32_t instr = binToUint(input);

        // Extract common instruction fields using bit masking and shifting
        uint32_t opcode = instr & 0x7F;        // Bits [6:0]
        uint32_t rd = (instr >> 7) & 0x1F;     // Bits [11:7]
        uint32_t funct3 = (instr >> 12) & 0x7; // Bits [14:12]
        uint32_t rs1 = (instr >> 15) & 0x1F;   // Bits [19:15]
        uint32_t rs2 = (instr >> 20) & 0x1F;   // Bits [24:20]
        uint32_t funct7 = (instr >> 25) & 0x7F;// Bits [31:25]

        // ----------- R-Type Instructions -----------
        if (opcode == 0x33) { // R-type opcode
            printf("Instruction Type: R\n");
            printf("Operation: %s\n", decodeR(funct3, funct7)); // Decode mnemonic
            printf("Rs1: x%d\n", rs1); // Source register 1
            printf("Rs2: x%d\n", rs2); // Source register 2
            printf("Rd: x%d\n", rd);   // Destination register
            printf("Funct3: %d\n", funct3); // funct3 field
            printf("Funct7: %d\n", funct7); // funct7 field
        }

        // ----------- I-Type Instructions -----------
        else if (opcode == 0x13 || opcode == 0x03 || opcode == 0x67) {
            // Immediate is located in bits [31:20]
            int imm = signExtend((instr >> 20) & 0xFFF, 12);

            printf("Instruction Type : I\n");
            printf("Operation: %s\n", decodeI(opcode, funct3, funct7));
            printf("Rs1: x%d\n", rs1); // Source register
            printf("Rd: x%d\n", rd);   // Destination register
            printf("Immediate: %d (or 0x%X)\n", imm, imm & 0xFFF); // Signed and hex
        }

        // ----------- S-Type Instructions -----------
        else if (opcode == 0x23) { // Store instructions
            // Immediate split across bits [11:7] and [31:25]
            int imm = ((instr >> 7) & 0x1F) | (((instr >> 25) & 0x7F) << 5);
            imm = signExtend(imm, 12); // Sign extend 12-bit immediate

            printf("Instruction Type : S\n");
            printf("Operation: %s\n", decodeS(funct3));
            printf("Rs1: x%d\n", rs1); // Base register
            printf("Rs2: x%d\n", rs2); // Source data register
            printf("Immediate: %d (or 0x%X)\n", imm, imm & 0xFFF);
        }

        // ----------- SB-Type Instructions -----------
        else if (opcode == 0x63) { // Branch instructions
            // Immediate bits are scattered and must be reconstructed
            int imm = ((instr >> 7) & 0x1) << 11 |     // bit 11
                      ((instr >> 8) & 0xF) << 1 |      // bits [4:1]
                      ((instr >> 25) & 0x3F) << 5 |    // bits [10:5]
                      ((instr >> 31) & 0x1) << 12;     // bit 12 (sign)

            imm = signExtend(imm, 13); // Sign extend 13-bit branch immediate

            printf("Instruction Type : SB\n");
            printf("Operation: %s\n", decodeSB(funct3));
            printf("Rs1: x%d\n", rs1); // First comparison register
            printf("Rs2: x%d\n", rs2); // Second comparison register
            printf("Immediate: %d\n", imm); // Branch offset
        }

        // ----------- UJ-Type Instructions (jal) -----------
        else if (opcode == 0x6F) { // Jump and link
            // Immediate fields reconstructed from scattered bit positions
            int imm = ((instr >> 21) & 0x3FF) << 1 |   // bits [10:1]
                      ((instr >> 20) & 0x1) << 11 |    // bit 11
                      ((instr >> 12) & 0xFF) << 12 |   // bits [19:12]
                      ((instr >> 31) & 0x1) << 20;     // bit 20 (sign)

            imm = signExtend(imm, 21); // Sign extend 21-bit immediate

            printf("Instruction Type : UJ\n");
            printf("Operation: jal\n");
            printf("Rd: x%d\n", rd);   // Destination register for return address
            printf("Immediate: %d\n", imm); // Jump offset
        }
    }

    return 0; // End of program
}
