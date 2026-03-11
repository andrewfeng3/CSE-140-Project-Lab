#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdint>
using namespace std;

// ============== Global variables (single-cycle CPU) ==============
int32_t pc = 0;
int32_t next_pc = 4;
int32_t branch_target = 0;
int alu_zero = 0;
int32_t rf[32] = {0};   // register file
int32_t d_mem[32] = {0}; // data memory (each entry 4 bytes; addr 0x0..0x7C)
int total_clock_cycles = 0;

// Control signals (set by ControlUnit)
int RegWrite = 0, MemRead = 0, MemWrite = 0, Branch = 0;
int MemtoReg = 0, ALUSrc = 0;
int ALUOp = 0;  // 2-bit: 00, 01, 10 (used by ALU control)
int Jump = 0, Jalr = 0;  // for JAL / JALR (part 2)

// Values passed between stages
int32_t alu_result = 0;
int32_t mem_read_data = 0;
int32_t writeback_rd = 0;      // destination register index
int32_t writeback_data = 0;    // data to write to rd
int do_writeback = 0;          // use RegWrite
int32_t store_data = 0;        // data to store for SW (rs2 value)
// Previous cycle control (for PC update in next Fetch)
int prev_Branch = 0, prev_alu_zero = 0, prev_Jump = 0, prev_Jalr = 0;
int first_fetch = 1;

// Register names for output (ABI names for part 2)
const char* reg_name(int i) {
    static const char* names[] = {"zero","ra","sp","gp","tp","t0","t1","t2","s0","s1","a0","a1","a2","a3","a4","a5","a6","a7","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11","t3","t4","t5","t6"};
    return i < 32 ? names[i] : "?";
}

// Instruction fields
struct RISCV_Instruction
{
    string type, rs1, rs2, rd, opName; 
    int funct3, funct7; 
    int32_t immediate;
    int rd_num = 0, rs1_num = 0, rs2_num = 0;  // numeric indices for CPU  

    RISCV_Instruction(const string &binary)
    {
        // Initialize everything to 0/empty first
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;

        // Immediately trigger the parsing logic
        this->get_instruction_type(binary);
    }

     // Default constructor (used if no binary instruction is passed)
    RISCV_Instruction()
    {
        // Initialize everything to safe defaults
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
    }

    // Determines instruction type based on opcode
    void get_instruction_type(const string &binary)
    {
        // Extract opcode (bits 6-0) → located at string positions 25-31
        int opcode_decimal = stoi(binary.substr(25, 7), nullptr, 2);
        // Extract funct3 field (bits 14-12)
        int funct3 = stoi(binary.substr(17, 3), nullptr, 2);
        // Extract funct7 field (bits 31-25)
        int funct7 = stoi(binary.substr(0, 7), nullptr, 2);

        // Determine instruction type from opcode
        if (opcode_decimal == 51)
        {
            this->type = "R";
            this->assign_R_attributes(binary, funct3, funct7);
        }
        else if (opcode_decimal == 19 || opcode_decimal == 3 || opcode_decimal == 103)
        {   // opcodes for I-type instructions (arithmetic, loads, jalr)
            this->type = "I";
            this->assign_I_attributes(binary, funct7, opcode_decimal, funct3);
        }
        else if (opcode_decimal == 99) // branch instructions
        {
            this->type = "SB";
            this->assign_SB_attributes(binary, funct3);
        }
        else if (opcode_decimal == 111) // jal
        {
            this->type = "UJ";
            this->assign_UJ_attributes(binary, funct3);
        }
        else if (opcode_decimal == 55)  // lui
        {
            this->type = "U";
            this->assign_U_attributes(binary, "lui");
        }
        else if (opcode_decimal == 23)  // auipc
        {
            this->type = "U";
            this->assign_U_attributes(binary, "auipc");
        }
        else
        {
            // default case → S-type (store instructions)
            this->type = "S";
            this->assign_S_attributes(binary, funct3);
        }
    }

    void get_S_opName(const int funct3)
    {
        if (funct3 == 0)
            this->opName = "sb"; // store byte
        else if (funct3 == 1)    
            this->opName = "sh"; // store halfword
        else if (funct3 == 2)
            this->opName = "sw"; // store word
    }

    // Determines R-type operation name using funct3 and funct7
    void get_R_opName(const int funct7, const int funct3)
    {
        if (funct3 == 0 && funct7 == 0)
            this->opName = "add";
        else if (funct3 == 0 && funct7 == 32)
            this->opName = "sub";
        else if (funct3 == 7)
            this->opName = "and";
        else if (funct3 == 6)
            this->opName = "or";
        else if (funct3 == 4)
            this->opName = "xor";
        else if (funct3 == 1)
            this->opName = "sll";
        else if (funct3 == 2)
            this->opName = "slt";
        else if (funct3 == 3)
            this->opName = "sltu";
        else if (funct3 == 5 && funct7 == 0)
            this->opName = "srl";
        else if (funct3 == 5 && funct7 == 32)
            this->opName = "sra";
    }

    // Determines I-type operation name
    void get_I_opName(const int funct3, const int opcode_decimal, const int funct7)
    {
      

        // Shift operations require funct7 check
        if (funct3 == 5 && funct7 == 0)
            this->opName = "srli";
        else if (funct3 == 5 && funct7 == 32)
            this->opName = "srai";

        // Jump and link register (check before addi: jalr has funct3=0, opcode 103)
        else if (opcode_decimal == 103)
            this->opName = "jalr";

        // Load instructions
        else if (funct3 == 0 && opcode_decimal == 3)
            this->opName = "lb";
        else if (funct3 == 1 && opcode_decimal == 3)
            this->opName = "lh";
        else if (funct3 == 2 && opcode_decimal == 3)
            this->opName = "lw";
        
        // Immediate arithmetic instructions
        else if (funct3 == 0)
            this->opName = "addi";
        else if (funct3 == 7)
            this->opName = "andi";
        else if (funct3 == 6)
            this->opName = "ori";
        else if (funct3 == 4)
            this->opName = "xori";
        else if (funct3 == 2)
            this->opName = "slti";
        else if (funct3 == 3)
            this->opName = "sltiu";
        else if (funct3 == 1)
            this->opName = "slli";
    }

    // Determines branch instruction names
    void get_SB_opName(const int f3)
    {
        if (f3 == 0)
            this->opName = "beq";
        else if (f3 == 1)
            this->opName = "bne";
        else if (f3 == 4)
            this->opName = "blt";
        else if (f3 == 5)
            this->opName = "bge";
        else if (f3 == 6)
            this->opName = "bltu";
        else if (f3 == 7)
            this->opName = "bgeu";
    }

    // Extract and sign-extend S-type immediate
    int32_t get_sign_extension_S_type(const string &binary)
    {
        // get immediate field (split into two parts for S-type)
        // imm[11:5] from bits [31:25]
        int32_t imm_upper = stoi(binary.substr(0, 7), nullptr, 2);
        // imm[4:0] from bits [11:7]
        int32_t imm_lower = stoi(binary.substr(20, 5), nullptr, 2);
        // Combine: shift upper 7 bits left by 5, then OR with lower 5 bits
        int32_t imm = (imm_upper << 5) | imm_lower;
        // Sign-extend from 12 bits to 32-bit
        if (imm & (1 << 11))
        {                      // Check sign bit at position 11
            imm |= 0xFFFFF000; // Sign-extend to 32-bit
        }

        return imm;
    }

    int32_t get_sign_extention_I_type(const string &binary)
    {
        // Extract 12-bit immediate from bits [31:20]
        int32_t imm = stoi(binary.substr(0, 12), nullptr, 2);

        // Sign-extend from 12 bits to 32-bit
        if (imm & (1 << 11))
        {                      // Check sign bit at position 11
            imm |= 0xFFFFF000; // Sign-extend to 32-bit
        }

        return imm;
    }

private:
    void assign_I_attributes(const string &binary, const int f7, const int opcode_decimal, const int f3)
    {
        get_I_opName(f3, opcode_decimal, f7);

        // get immediate
        this->immediate = get_sign_extention_I_type(binary);

        // get rs1
        int rs1_decimal = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs1 = "x" + to_string(rs1_decimal);

        // get funct3
        this->funct3 = f3;
        // get rd
        int rd_decimal = stoi(binary.substr(20, 5), nullptr, 2);
        this->rd = "x" + to_string(rd_decimal);
        this->rd_num = rd_decimal;
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
    }

    void assign_S_attributes(const string &binary, const int f3)
    {
        // get operation name
        this->get_S_opName(f3);

        // funct3
        this->funct3 = f3;

        // get rs1
        int rs1_decimal = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs1 = "x" + to_string(rs1_decimal);

        // get rs2
        int rs2_decimal = stoi(binary.substr(7, 5), nullptr, 2);
        this->rs2 = "x" + to_string(rs2_decimal);

        // get immediate field
        this->immediate = get_sign_extension_S_type(binary);
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs2_num = stoi(binary.substr(7, 5), nullptr, 2);
    }
    void assign_R_attributes(const string &binary, const int f3, const int f7)
    {
        this->get_R_opName(f7, f3);
        // rs1
        int rs1_decimal = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs1 = "x" + to_string(rs1_decimal);

        // rs2
        int rs2_decimal = stoi(binary.substr(7, 5), nullptr, 2);
        this->rs2 = "x" + to_string(rs2_decimal);

        // rd
        int rd_decimal = stoi(binary.substr(20, 5), nullptr, 2);
        this->rd = "x" + to_string(rd_decimal);

        // funct3
        this->funct3 = f3;
        // funct7
        this->funct7 = f7;
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs2_num = stoi(binary.substr(7, 5), nullptr, 2);
        this->rd_num = stoi(binary.substr(20, 5), nullptr, 2);
    }

    void assign_U_attributes(const string &binary, const string &op)
    {
        this->opName = op;
        int rd_decimal = stoi(binary.substr(20, 5), nullptr, 2);
        this->rd = "x" + to_string(rd_decimal);
        // U-type: imm[31:12] in instruction → value is imm20 << 12
        uint32_t imm20 = (uint32_t)stoi(binary.substr(0, 20), nullptr, 2);
        this->immediate = (int32_t)(imm20 << 12);
    }

    void assign_SB_attributes(const string &binary, int f3)
    {
        // Determine the branch instruction name (beq, bne, blt, bge)
        get_SB_opName(f3);

        // Store funct3 field for the instruction
        this->funct3 = f3;

        // Extract rs1 register
        // rs1 is located at bits [19:15] in the instruction
        // In the 32-bit string representation this corresponds to index 12 with length 5
        int rs1_decimal = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs1 = "x" + to_string(rs1_decimal);

        // Extract rs2 register
        // rs2 is located at bits [24:20]
        // In the string representation this corresponds to index 7 with length 5
        int rs2_decimal = stoi(binary.substr(7, 5), nullptr, 2);
        this->rs2 = "x" + to_string(rs2_decimal);

        /*
            SB Immediate layout:

            imm[12]   = bit 31
            imm[10:5] = bits 30:25
            imm[4:1]  = bits 11:8
            imm[11]   = bit 7
            imm[0]    = 0 (always)

            Final order:
            imm[12|11|10:5|4:1|0]
        */

        // Construct the immediate by assembling pieces from the instruction
        string imm_bits =
            string(1, binary[0]) +           // bit 31 -> imm[12]
            string(1, binary[24]) +          // bit 7 -> imm[11]
            binary.substr(1, 6) +            // bits 30:25 -> imm[10:15]
            binary.substr(20, 4) +           // bits 11:8 -> imm [4:1]
            "0";                             // LSB = 0 (branch addresses aligned)

        // Convert the immediate bit string to a signed integer
        int32_t imm = stoi(imm_bits, nullptr, 2);

        // Sign extend 13-bit immediate to 32 bits
        // If the sign bit (bit 12) is 1, fill upper bits with 1s
        if (imm & (1 << 12))
            imm |= 0xFFFFE000;

        this->immediate = imm;
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs2_num = stoi(binary.substr(7, 5), nullptr, 2);
    }

    void assign_UJ_attributes(const string &binary, int f3)
    {
        // UJ type corresponds to the JAL instruction
        this->opName = "jal";

    // Extract destination register (rd)
    // rd is stored in bits [11:7]
    int rd_decimal = stoi(binary.substr(20, 5), nullptr, 2);
    this->rd = "x" + to_string(rd_decimal);

    /*
        UJ Immediate layout:

        imm[20]   = bit 31
        imm[10:1] = bits 30:21
        imm[11]   = bit 20
        imm[19:12]= bits 19:12
        imm[0]    = 0

        Final order:
        imm[20|19:12|11|10:1|0]
    */
    
    // Reconstruct the immediate value from its scattered bit locations
    string imm_bits =
        string(1, binary[0]) +           // bit 31
        binary.substr(12, 8) +           // bits 19:12
        string(1, binary[11]) +          // bit 20
        binary.substr(1, 10) +           // bits 30:21
        "0";

    // Convert bit string to integer
    int32_t imm = stoi(imm_bits, nullptr, 2);

    // Sign extend 21-bit immediate
    if (imm & (1 << 20))
        imm |= 0xFFE00000;

    this->immediate = imm;
    this->rd_num = rd_decimal;
    }
};

ostream &operator<<(ostream &os, const RISCV_Instruction &instr)
{
    os << "Instruction Type: " << instr.type << endl;
    os << "Operation: " << instr.opName << endl;
    
    // R-type: has rs1, rs2, rd, funct3, funct7
    if (instr.type == "R")
    {
        os << "Rs1: " << instr.rs1 << endl;
        os << "Rs2: " << instr.rs2 << endl;
        os << "Rd: " << instr.rd << endl;
        os << "Funct3: " << instr.funct3 << endl;
        os << "Funct7: " << instr.funct7 << endl;
    }
    // I-type: has rs1, rd, immediate
    else if (instr.type == "I")
    {
        os << "Rs1: " << instr.rs1 << endl;
        os << "Rd: " << instr.rd << endl;
        os << "Immediate: " << instr.immediate
           << " (or 0x" << uppercase << hex << instr.immediate
           << nouppercase << dec << ")" << endl;
    }
    // S-type: has rs1, rs2, immediate
    else if (instr.type == "S")
    {
        os << "Rs1: " << instr.rs1 << endl;
        os << "Rs2: " << instr.rs2 << endl;
        os << "Immediate: " << instr.immediate
           << " (or 0x" << uppercase << hex << instr.immediate
           << nouppercase << dec << ")" << endl;
    }
    // SB-type: has rs1, rs2, immediate
    else if (instr.type == "SB")
    {
        os << "Rs1: " << instr.rs1 << endl;
        os << "Rs2: " << instr.rs2 << endl;
        os << "Immediate: " << instr.immediate
           << " (or 0x" << uppercase << hex << instr.immediate
           << nouppercase << dec << ")" << endl;
    }
    // UJ-type: has rd, immediate
    else if (instr.type == "UJ")
    {
        os << "Rd: " << instr.rd << endl;
        os << "Immediate: " << instr.immediate
           << " (or 0x" << uppercase << hex << instr.immediate
           << nouppercase << dec << ")" << endl;
    }
    // U-type: has rd, immediate (lui, auipc)
    else if (instr.type == "U")
    {
        os << "Rd: " << instr.rd << endl;
        os << "Immediate: " << instr.immediate
           << " (or 0x" << uppercase << hex << instr.immediate
           << nouppercase << dec << ")" << endl;
    }

    return os;
}

// --------------- ALU Control: ALUOp + funct3/funct7 -> 4-bit alu_ctrl ---------------
// Page 14 Processor-2: 0000 AND, 0001 OR, 0010 ADD, 0110 SUB, ...
int get_alu_ctrl(int ALUOp, int funct3, int funct7, int opcode) {
    if (ALUOp == 0) return 0x2;   // 0010 ADD (lw, sw, addi, jalr)
    if (ALUOp == 1) {             // branch
        return 0x6;               // SUB to compare
    }
    if (ALUOp == 2) {             // R-type / I-type arith
        if (funct3 == 0 && funct7 == 0) return 0x2;  // ADD
        if (funct3 == 0 && funct7 == 32) return 0x6; // SUB
        if (funct3 == 7) return 0x0;  // AND
        if (funct3 == 6) return 0x1;  // OR
        if (funct3 == 0) return 0x2;  // addi
        if (funct3 == 7) return 0x0;  // andi
        if (funct3 == 6) return 0x1;  // ori
    }
    return 0x2;
}

// --------------- Control Unit: opcode (7-bit) -> 7 control signals ---------------
void ControlUnit(int opcode, int funct3, int funct7) {
    RegWrite = MemRead = MemWrite = Branch = MemtoReg = ALUSrc = 0;
    ALUOp = 0;
    Jump = Jalr = 0;
    // R-type: 0110011
    if (opcode == 51) {
        RegWrite = 1; ALUSrc = 0; MemtoReg = 0; ALUOp = 2;
        return;
    }
    // I-type arith: 0010011
    if (opcode == 19) {
        RegWrite = 1; ALUSrc = 1; MemtoReg = 0; ALUOp = 2;
        return;
    }
    // load: 0000011
    if (opcode == 3) {
        RegWrite = 1; MemRead = 1; ALUSrc = 1; MemtoReg = 1; ALUOp = 0;
        return;
    }
    // store: 0100011
    if (opcode == 35) {
        MemWrite = 1; ALUSrc = 1; ALUOp = 0;
        return;
    }
    // branch: 1100011
    if (opcode == 99) {
        Branch = 1; ALUSrc = 0; ALUOp = 1;
        return;
    }
    // JAL: 1101111
    if (opcode == 111) {
        RegWrite = 1; Jump = 1; ALUOp = 0;
        return;
    }
    // JALR: 1100111
    if (opcode == 103) {
        RegWrite = 1; Jalr = 1; ALUSrc = 1; MemtoReg = 0; ALUOp = 0;
        return;
    }
}

// --------------- Fetch: read instruction at PC, update next_pc; at start use prev branch ---------------
string Fetch(const vector<string>& program) {
    if (!first_fetch) {
        // Use previous cycle's Branch/Jump to update PC
        if (prev_Jump || prev_Jalr)
            pc = branch_target;
        else
            pc = (prev_Branch && prev_alu_zero) ? branch_target : next_pc;
    }
    int idx = pc / 4;
    if (idx < 0 || (size_t)idx >= program.size()) return "";
    next_pc = pc + 4;
    first_fetch = 0;
    return program[idx];
}

// --------------- Decode: decode instruction, read rf, sign-extend, call ControlUnit ---------------
void Decode(const string& binary, RISCV_Instruction& instr,
            int32_t& rs1_val, int32_t& rs2_val, int32_t& imm_sext) {
    instr = RISCV_Instruction(binary);
    int opcode = stoi(binary.substr(25, 7), nullptr, 2);
    int f3 = stoi(binary.substr(17, 3), nullptr, 2);
    int f7 = stoi(binary.substr(0, 7), nullptr, 2);
    ControlUnit(opcode, f3, f7);
    rs1_val = rf[instr.rs1_num];
    rs2_val = rf[instr.rs2_num];
    imm_sext = instr.immediate;
    store_data = rs2_val;  // for SW
    writeback_rd = instr.rd_num;
    do_writeback = RegWrite;
}

// --------------- Execute: ALU, alu_zero, branch_target ---------------
void Execute(int32_t rs1_val, int32_t rs2_val, int32_t imm_sext,
             const RISCV_Instruction& instr) {
    int opcode = 0;
    if (instr.opName == "lw" || instr.opName == "sw") opcode = 3;
    else if (instr.opName == "add" || instr.opName == "sub" || instr.opName == "and" || instr.opName == "or") opcode = 51;
    else if (instr.opName == "addi" || instr.opName == "andi" || instr.opName == "ori") opcode = 19;
    else if (instr.opName == "beq") opcode = 99;
    else if (instr.opName == "jal") opcode = 111;
    else if (instr.opName == "jalr") opcode = 103;
    int alu_ctrl = get_alu_ctrl(ALUOp, instr.funct3, instr.funct7, opcode);
    int32_t op2 = ALUSrc ? imm_sext : rs2_val;
    switch (alu_ctrl) {
        case 0x0: alu_result = rs1_val & op2; break;
        case 0x1: alu_result = rs1_val | op2; break;
        case 0x2: alu_result = rs1_val + op2; break;
        case 0x6: alu_result = rs1_val - op2; break;
        default:  alu_result = rs1_val + op2; break;
    }
    alu_zero = (alu_result == 0) ? 1 : 0;
    // Branch target: (PC+4) + (imm << 1). Use current PC and next_pc.
    branch_target = next_pc + (instr.immediate << 1);
    // JAL: jump target = PC + (UJ imm << 1). We have next_pc = pc+4, so PC = next_pc-4. JAL imm is already in bytes in many refs; RISC-V JAL imm is in multiples of 2 bytes. So target = pc + (imm<<1) = (next_pc-4) + (imm<<1). Actually JAL immediate is sign-extended 20-bit then << 1, so offset in bytes. So branch_target = pc + instr.immediate. Our assign_UJ stores the immediate already decoded (the full offset). So JAL: branch_target = pc + immediate. We need to set branch_target in Execute for JAL to pc + imm. So set branch_target = pc + instr.immediate for JAL (pc is current instruction address).
    if (Jump) branch_target = pc + instr.immediate;
    if (Jalr) branch_target = (rs1_val + imm_sext) & ~1;  // LSB cleared
}

// --------------- Mem: load or store ---------------
void Mem() {
    mem_read_data = 0;
    if (!MemRead && !MemWrite) return;
    int addr = alu_result;
    int idx = addr / 4;
    if (idx < 0 || idx >= 32) return;
    if (MemRead)  mem_read_data = d_mem[idx];
    if (MemWrite) d_mem[idx] = store_data;
}

// --------------- Writeback: update rf, increment cycle ---------------
void Writeback() {
    if (do_writeback && writeback_rd > 0) {
        writeback_data = MemtoReg ? mem_read_data : alu_result;
        if (Jump || Jalr) writeback_data = next_pc;
        rf[writeback_rd] = writeback_data;
    }
    total_clock_cycles++;
    prev_Branch = Branch;
    prev_alu_zero = alu_zero;
    prev_Jump = Jump;
    prev_Jalr = Jalr;
}

// use_abi_names: 0 = x0,x1,... (part 1), 1 = zero,ra,... (part 2)
int use_abi_names = 0;

// --------------- Print changes for this cycle ---------------
void print_cycle_changes(int prev_rf[32], int prev_d_mem[32], const RISCV_Instruction& instr) {
    cout << "total_clock_cycles " << total_clock_cycles << " :" << endl;
    if (do_writeback && writeback_rd > 0) {
        if (rf[writeback_rd] != prev_rf[writeback_rd]) {
            if (use_abi_names) cout << reg_name(writeback_rd);
            else cout << "x" << writeback_rd;
            cout << " is modified to 0x" << hex << (unsigned)rf[writeback_rd] << dec << endl;
        }
    }
    if (MemWrite) {
        int idx = alu_result / 4;
        if (idx >= 0 && idx < 32 && d_mem[idx] != prev_d_mem[idx])
            cout << "memory 0x" << hex << alu_result << " is modified to 0x" << (unsigned)d_mem[idx] << dec << endl;
    }
    int32_t next_pc_val = (Jump || Jalr) ? branch_target : ((Branch && alu_zero) ? branch_target : next_pc);
    cout << "pc is modified to 0x" << hex << next_pc_val << dec << endl;
    cout << endl;
}

// --------------- Initialize for Part 1 (10 instructions) ---------------
void init_part1() {
    for (int i = 0; i < 32; i++) rf[i] = 0;
    for (int i = 0; i < 32; i++) d_mem[i] = 0;
    rf[1] = 0x20; rf[2] = 0x5; rf[10] = 0x70; rf[11] = 0x4;
    d_mem[0x70/4] = 0x5;   // 0x70 = 28*4
    d_mem[0x74/4] = 0x10;
    use_abi_names = 0;
}

// --------------- Initialize for Part 2 (JAL/JALR) ---------------
void init_part2() {
    for (int i = 0; i < 32; i++) rf[i] = 0;
    for (int i = 0; i < 32; i++) d_mem[i] = 0;
    rf[8] = 0x20;   // s0
    rf[10] = 0x5;   // a0
    rf[11] = 0x2;   // a1
    rf[12] = 0xa;   // a2
    rf[13] = 0xf;   // a3
    use_abi_names = 1;
}

int main(int argc, char *argv[])
{
    string filename;
    cout << "Enter the program file name to run:" << endl;
    getline(cin, filename);
    if (filename.empty()) getline(cin, filename);

    ifstream f(filename);
    if (!f) { cerr << "Cannot open " << filename << endl; return 1; }
    vector<string> program;
    string line;
    while (getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty()) continue;
        program.push_back(line);
    }
    f.close();
    if (program.empty()) { cerr << "Empty program." << endl; return 1; }

    // Reset CPU state
    pc = 0; next_pc = 4; branch_target = 0; alu_zero = 0;
    total_clock_cycles = 0; first_fetch = 1;
    prev_Branch = prev_alu_zero = prev_Jump = prev_Jalr = 0;

    // Use part 2 init if filename contains "part2", else part 1
    if (filename.find("part2") != string::npos) init_part2();
    else init_part1();

    while (true) {
        string binary = Fetch(program);
        if (binary.empty()) break;

        int prev_rf[32], prev_d_mem[32];
        for (int i = 0; i < 32; i++) { prev_rf[i] = rf[i]; prev_d_mem[i] = d_mem[i]; }

        RISCV_Instruction instr;
        int32_t rs1_val, rs2_val, imm_sext;
        Decode(binary, instr, rs1_val, rs2_val, imm_sext);
        Execute(rs1_val, rs2_val, imm_sext, instr);
        Mem();
        Writeback();

        print_cycle_changes(prev_rf, prev_d_mem, instr);
    }

    cout << "program terminated:" << endl;
    cout << "total execution time is " << total_clock_cycles << " cycles" << endl;
    return 0;
}
