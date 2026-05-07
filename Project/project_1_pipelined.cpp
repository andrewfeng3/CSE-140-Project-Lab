/*
 * CSE 140 — Extra credit: 5-stage pipelined RISC-V CPU
 * Stages: IF, ID, EX, MEM, WB with pipeline registers if_id, id_ex, ex_mem, mem_wb.
 * No forwarding / no branch predictor:
 *   - RAW stalls (instruction in EX/MEM/WB will write a register IF/ID reads).
 *   - Flush IF/ID on taken conditional branch or unconditional jump (JAL/JALR).
 * Clock: total_clock_cycles increments once per pipeline cycle (not once per instruction).
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cctype>
#include <string>
using namespace std;

static bool filename_indicates_part2(const string &fn)
{
    string s = fn;
    for (char &c : s)
        c = (char)tolower((unsigned char)c);
    return s.find("part2") != string::npos;
}

// ---------------- Register file & memory ----------------
int32_t pc = 0;
int32_t rf[32] = {0};
int32_t d_mem[32] = {0};
int total_clock_cycles = 0;
int use_abi_names = 0;

const char *reg_name(int i)
{
    static const char *names[] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    return i < 32 ? names[i] : "?";
}

// ---------------- Instruction decode (same as single-cycle project) ----------------
struct RISCV_Instruction
{
    string type, rs1, rs2, rd, opName;
    int funct3, funct7;
    int32_t immediate;
    int rd_num = 0, rs1_num = 0, rs2_num = 0;

    RISCV_Instruction(const string &binary)
    {
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
        this->get_instruction_type(binary);
    }

    RISCV_Instruction()
    {
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
    }

    void get_instruction_type(const string &binary)
    {
        int opcode_decimal = stoi(binary.substr(25, 7), nullptr, 2);
        int funct3 = stoi(binary.substr(17, 3), nullptr, 2);
        int funct7 = stoi(binary.substr(0, 7), nullptr, 2);

        if (opcode_decimal == 51)
        {
            this->type = "R";
            this->assign_R_attributes(binary, funct3, funct7);
        }
        else if (opcode_decimal == 19 || opcode_decimal == 3 || opcode_decimal == 103)
        {
            this->type = "I";
            this->assign_I_attributes(binary, funct7, opcode_decimal, funct3);
        }
        else if (opcode_decimal == 99)
        {
            this->type = "SB";
            this->assign_SB_attributes(binary, funct3);
        }
        else if (opcode_decimal == 111)
        {
            this->type = "UJ";
            this->assign_UJ_attributes(binary, funct3);
        }
        else if (opcode_decimal == 55)
        {
            this->type = "U";
            this->assign_U_attributes(binary, "lui");
        }
        else if (opcode_decimal == 23)
        {
            this->type = "U";
            this->assign_U_attributes(binary, "auipc");
        }
        else
        {
            this->type = "S";
            this->assign_S_attributes(binary, funct3);
        }
    }

    void get_S_opName(const int funct3)
    {
        if (funct3 == 0)
            this->opName = "sb";
        else if (funct3 == 1)
            this->opName = "sh";
        else if (funct3 == 2)
            this->opName = "sw";
    }

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

    void get_I_opName(const int funct3, const int opcode_decimal, const int funct7)
    {
        if (funct3 == 5 && funct7 == 0)
            this->opName = "srli";
        else if (funct3 == 5 && funct7 == 32)
            this->opName = "srai";
        else if (opcode_decimal == 103)
            this->opName = "jalr";
        else if (funct3 == 0 && opcode_decimal == 3)
            this->opName = "lb";
        else if (funct3 == 1 && opcode_decimal == 3)
            this->opName = "lh";
        else if (funct3 == 2 && opcode_decimal == 3)
            this->opName = "lw";
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

    int32_t get_sign_extension_S_type(const string &binary)
    {
        int32_t imm_upper = stoi(binary.substr(0, 7), nullptr, 2);
        int32_t imm_lower = stoi(binary.substr(20, 5), nullptr, 2);
        int32_t imm = (imm_upper << 5) | imm_lower;
        if (imm & (1 << 11))
            imm |= 0xFFFFF000;
        return imm;
    }

    int32_t get_sign_extention_I_type(const string &binary)
    {
        int32_t imm = stoi(binary.substr(0, 12), nullptr, 2);
        if (imm & (1 << 11))
            imm |= 0xFFFFF000;
        return imm;
    }

private:
    void assign_I_attributes(const string &binary, const int f7, const int opcode_decimal, const int f3)
    {
        get_I_opName(f3, opcode_decimal, f7);
        this->immediate = get_sign_extention_I_type(binary);
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->funct3 = f3;
        this->rd = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2));
        this->rd_num = stoi(binary.substr(20, 5), nullptr, 2);
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
    }

    void assign_S_attributes(const string &binary, const int f3)
    {
        get_S_opName(f3);
        this->funct3 = f3;
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        this->immediate = get_sign_extension_S_type(binary);
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs2_num = stoi(binary.substr(7, 5), nullptr, 2);
    }

    void assign_R_attributes(const string &binary, const int f3, const int f7)
    {
        get_R_opName(f7, f3);
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        this->rd = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2));
        this->funct3 = f3;
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
        uint32_t imm20 = (uint32_t)stoi(binary.substr(0, 20), nullptr, 2);
        this->immediate = (int32_t)(imm20 << 12);
        this->rd_num = rd_decimal;
    }

    void assign_SB_attributes(const string &binary, int f3)
    {
        get_SB_opName(f3);
        this->funct3 = f3;
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        string imm_bits =
            string(1, binary[0]) + string(1, binary[24]) + binary.substr(1, 6) + binary.substr(20, 4) + "0";
        int32_t imm = stoi(imm_bits, nullptr, 2);
        if (imm & (1 << 12))
            imm |= 0xFFFFE000;
        this->immediate = imm;
        this->rs1_num = stoi(binary.substr(12, 5), nullptr, 2);
        this->rs2_num = stoi(binary.substr(7, 5), nullptr, 2);
    }

    void assign_UJ_attributes(const string &binary, int)
    {
        this->opName = "jal";
        int rd_decimal = stoi(binary.substr(20, 5), nullptr, 2);
        this->rd = "x" + to_string(rd_decimal);
        string imm_bits =
            string(1, binary[0]) + binary.substr(12, 8) + string(1, binary[11]) + binary.substr(1, 10) + "0";
        int32_t imm = stoi(imm_bits, nullptr, 2);
        if (imm & (1 << 20))
            imm |= 0xFFE00000;
        this->immediate = imm;
        this->rd_num = rd_decimal;
    }
};

// ---------------- ALU & control ----------------
int get_alu_ctrl(int ALUOp, int funct3, int funct7, int /*opcode*/)
{
    if (ALUOp == 0)
        return 0x2;
    if (ALUOp == 1)
        return 0x6;
    if (ALUOp == 2)
    {
        if (funct3 == 0 && funct7 == 0)
            return 0x2;
        if (funct3 == 0 && funct7 == 32)
            return 0x6;
        if (funct3 == 7)
            return 0x0;
        if (funct3 == 6)
            return 0x1;
        if (funct3 == 0)
            return 0x2;
        if (funct3 == 7)
            return 0x0;
        if (funct3 == 6)
            return 0x1;
    }
    return 0x2;
}

struct ControlSignals
{
    int RegWrite = 0, MemRead = 0, MemWrite = 0, Branch = 0;
    int MemtoReg = 0, ALUSrc = 0, ALUOp = 0;
    int Jump = 0, Jalr = 0;
};

void ControlUnit(int opcode, int /*funct3*/, int /*funct7*/, ControlSignals &c)
{
    c = ControlSignals{};
    if (opcode == 51)
    {
        c.RegWrite = 1;
        c.ALUSrc = 0;
        c.MemtoReg = 0;
        c.ALUOp = 2;
        return;
    }
    if (opcode == 19)
    {
        c.RegWrite = 1;
        c.ALUSrc = 1;
        c.MemtoReg = 0;
        c.ALUOp = 2;
        return;
    }
    if (opcode == 3)
    {
        c.RegWrite = 1;
        c.MemRead = 1;
        c.ALUSrc = 1;
        c.MemtoReg = 1;
        c.ALUOp = 0;
        return;
    }
    if (opcode == 35)
    {
        c.MemWrite = 1;
        c.ALUSrc = 1;
        c.ALUOp = 0;
        return;
    }
    if (opcode == 99)
    {
        c.Branch = 1;
        c.ALUSrc = 0;
        c.ALUOp = 1;
        return;
    }
    if (opcode == 111)
    {
        c.RegWrite = 1;
        c.Jump = 1;
        c.ALUOp = 0;
        return;
    }
    if (opcode == 103)
    {
        c.RegWrite = 1;
        c.Jalr = 1;
        c.ALUSrc = 1;
        c.MemtoReg = 0;
        c.ALUOp = 0;
        return;
    }
}

static bool insn_uses_rs1(const RISCV_Instruction &i)
{
    if (i.type == "UJ" || i.type == "U")
        return false;
    return true;
}

static bool insn_uses_rs2(const RISCV_Instruction &i)
{
    return i.type == "R" || i.type == "S" || i.type == "SB";
}

// ---------------- Pipeline registers ----------------

struct IF_ID
{
    string instr_bits;
    int32_t npc = 0; // PC+4 of fetched instruction (sequential next PC)
    int32_t pc = 0;  // address of this instruction
    bool valid = false;
};

struct ID_EX
{
    bool valid = false;
    int32_t pc = 0;
    int32_t npc = 0;
    RISCV_Instruction instr;
    int32_t rs1_val = 0, rs2_val = 0, imm = 0;
    int32_t rs1_num = 0, rs2_num = 0, rd_num = 0;
    ControlSignals ctrl;
};

struct EX_MEM
{
    bool valid = false;
    int32_t pc = 0;
    int32_t npc = 0;
    int32_t alu_result = 0;
    int32_t store_data = 0;
    int alu_zero = 0;
    int32_t branch_target = 0;
    RISCV_Instruction instr;
    int rd_num = 0;
    ControlSignals ctrl;
};

struct MEM_WB
{
    bool valid = false;
    int32_t mem_read_data = 0;
    int32_t alu_result = 0;
    int rd_num = 0;
    ControlSignals ctrl;
};

IF_ID if_id;
ID_EX id_ex;
EX_MEM ex_mem;
MEM_WB mem_wb;

static ID_EX make_bubble_id_ex()
{
    ID_EX b;
    b.valid = false;
    return b;
}

static IF_ID make_bubble_if_id()
{
    IF_ID b;
    b.valid = false;
    b.instr_bits.clear();
    return b;
}

void decode_to_id_ex(const string &binary, int32_t insn_pc, int32_t insn_npc, ID_EX &out)
{
    out = ID_EX{};
    if (binary.empty() || (size_t)binary.size() < 32)
    {
        out.valid = false;
        return;
    }
    out.valid = true;
    out.pc = insn_pc;
    out.npc = insn_npc;
    out.instr = RISCV_Instruction(binary);
    int opcode = stoi(binary.substr(25, 7), nullptr, 2);
    int f3 = stoi(binary.substr(17, 3), nullptr, 2);
    int f7 = stoi(binary.substr(0, 7), nullptr, 2);
    ControlUnit(opcode, f3, f7, out.ctrl);
    out.rd_num = out.instr.rd_num;
    out.rs1_num = out.instr.rs1_num;
    out.rs2_num = out.instr.rs2_num;
    out.rs1_val = rf[out.instr.rs1_num];
    out.rs2_val = rf[out.instr.rs2_num];
    out.imm = out.instr.immediate;
}

void ex_compute(const ID_EX &in, int32_t &alu_result, int &alu_zero, int32_t &branch_target)
{
    if (!in.valid)
    {
        alu_result = 0;
        alu_zero = 0;
        branch_target = 0;
        return;
    }
    const RISCV_Instruction &instr = in.instr;
    int opcode = 0;
    if (instr.opName == "lw" || instr.opName == "sw")
        opcode = 3;
    else if (instr.opName == "add" || instr.opName == "sub" || instr.opName == "and" || instr.opName == "or")
        opcode = 51;
    else if (instr.opName == "addi" || instr.opName == "andi" || instr.opName == "ori")
        opcode = 19;
    else if (instr.opName.size() >= 2 && instr.opName.substr(0, 2) == "be")
        opcode = 99;
    else if (instr.opName == "beq")
        opcode = 99;
    else if (instr.opName == "jal")
        opcode = 111;
    else if (instr.opName == "jalr")
        opcode = 103;
    else if (instr.type == "SB")
        opcode = 99;

    int alu_ctrl = get_alu_ctrl(in.ctrl.ALUOp, instr.funct3, instr.funct7, opcode);
    int32_t op2 = in.ctrl.ALUSrc ? in.imm : in.rs2_val;
    switch (alu_ctrl)
    {
    case 0x0:
        alu_result = in.rs1_val & op2;
        break;
    case 0x1:
        alu_result = in.rs1_val | op2;
        break;
    case 0x2:
        alu_result = in.rs1_val + op2;
        break;
    case 0x6:
        alu_result = in.rs1_val - op2;
        break;
    default:
        alu_result = in.rs1_val + op2;
        break;
    }
    alu_zero = (alu_result == 0) ? 1 : 0;
    branch_target = in.pc + in.imm;
    if (in.ctrl.Jump)
        branch_target = in.pc + in.imm;
    if (in.ctrl.Jalr)
        branch_target = (in.rs1_val + in.imm) & ~1;
}

bool branch_taken_from_ex(const ID_EX &in, int alu_zero)
{
    if (!in.valid)
        return false;
    if (in.ctrl.Jump || in.ctrl.Jalr)
        return true;
    if (in.ctrl.Branch && alu_zero)
        return true;
    return false;
}

// RAW hazard with no forwarding: stall ID if any earlier stage will write a register
// this instruction reads (values are not in rf until after that instruction's WB).
bool data_hazard_stall(const ID_EX &ex, const EX_MEM &mem, const MEM_WB &wb, const IF_ID &fd)
{
    if (!fd.valid || fd.instr_bits.empty())
        return false;
    RISCV_Instruction d(fd.instr_bits);
    auto conflicts = [&](int r) -> bool
    {
        if (r == 0)
            return false;
        if (ex.valid && ex.ctrl.RegWrite && ex.rd_num == r)
            return true;
        if (mem.valid && mem.ctrl.RegWrite && mem.rd_num == r)
            return true;
        // No MEM/WB check: WB writes rf earlier in the same cycle before ID reads.
        return false;
    };
    if (insn_uses_rs1(d) && conflicts(d.rs1_num))
        return true;
    if (insn_uses_rs2(d) && conflicts(d.rs2_num))
        return true;
    return false;
}

void init_part1()
{
    for (int i = 0; i < 32; i++)
        rf[i] = 0;
    for (int i = 0; i < 32; i++)
        d_mem[i] = 0;
    rf[1] = 0x20;
    rf[2] = 0x5;
    rf[10] = 0x70;
    rf[11] = 0x4;
    d_mem[0x70 / 4] = 0x5;
    d_mem[0x74 / 4] = 0x10;
    use_abi_names = 0;
}

void init_part2()
{
    for (int i = 0; i < 32; i++)
        rf[i] = 0;
    for (int i = 0; i < 32; i++)
        d_mem[i] = 0;
    rf[8] = 0x20;
    rf[10] = 0x5;
    rf[11] = 0x2;
    rf[12] = 0xa;
    rf[13] = 0xf;
    use_abi_names = 1;
}

static string fetch_instr(const vector<string> &program, int32_t fetch_pc)
{
    if (fetch_pc < 0)
        return "";
    size_t idx = (size_t)(fetch_pc / 4);
    if (idx >= program.size())
        return "";
    return program[idx];
}

void print_cycle_trace(int prev_rf[32], int prev_d_mem[32],
                       const MEM_WB &wb_snapshot, int mem_we, int32_t mem_addr,
                       int32_t next_pc_print)
{
    cout << "total_clock_cycles " << total_clock_cycles << " :" << endl;
    if (wb_snapshot.valid && wb_snapshot.ctrl.RegWrite && wb_snapshot.rd_num > 0)
    {
        if (rf[wb_snapshot.rd_num] != prev_rf[wb_snapshot.rd_num])
        {
            if (use_abi_names)
                cout << reg_name(wb_snapshot.rd_num);
            else
                cout << "x" << wb_snapshot.rd_num;
            cout << " is modified to 0x" << hex << (unsigned)rf[wb_snapshot.rd_num] << dec << endl;
        }
    }
    if (mem_we)
    {
        int idx = mem_addr / 4;
        if (idx >= 0 && idx < 32 && d_mem[idx] != prev_d_mem[idx])
            cout << "memory 0x" << hex << mem_addr << " is modified to 0x" << (unsigned)d_mem[idx] << dec << endl;
    }
    cout << "pc is modified to 0x" << hex << next_pc_print << dec << endl;
    cout << endl;
}

int main(int argc, char *argv[])
{
    string filename;
    if (argc >= 2 && argv[1] != nullptr && argv[1][0] != '\0')
        filename = argv[1];
    else
    {
        cout << "Enter the program file name to run:" << endl;
        getline(cin, filename);
        if (filename.empty())
            getline(cin, filename);
    }

    ifstream f(filename);
    if (!f)
    {
        cerr << "Cannot open " << filename << endl;
        return 1;
    }
    vector<string> program;
    string line;
    while (getline(f, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty())
            continue;
        program.push_back(line);
    }
    f.close();
    if (program.empty())
    {
        cerr << "Empty program." << endl;
        return 1;
    }

    pc = 0;
    total_clock_cycles = 0;
    if_id = make_bubble_if_id();
    id_ex = make_bubble_id_ex();
    ex_mem = EX_MEM{};
    mem_wb = MEM_WB{};

    if (filename_indicates_part2(filename))
        init_part2();
    else
        init_part1();

    const int MAX_CYCLES = 1000000;
    int cycles = 0;

    while (cycles < MAX_CYCLES)
    {
        MEM_WB wb_snap = mem_wb;
        EX_MEM em_snap = ex_mem;
        ID_EX id_snap = id_ex;
        IF_ID fd_snap = if_id;
        int32_t pc_snap = pc;

        int prev_rf[32], prev_d_mem[32];
        for (int i = 0; i < 32; i++)
        {
            prev_rf[i] = rf[i];
            prev_d_mem[i] = d_mem[i];
        }

        // WB first so decode in this cycle sees the newly committed register value.
        if (wb_snap.valid && wb_snap.ctrl.RegWrite && wb_snap.rd_num > 0)
        {
            int32_t wdata = wb_snap.ctrl.MemtoReg ? wb_snap.mem_read_data : wb_snap.alu_result;
            rf[wb_snap.rd_num] = wdata;
        }

        int32_t ex_alu = 0, ex_bt = 0;
        int ex_az = 0;
        ex_compute(id_snap, ex_alu, ex_az, ex_bt);
        bool take_branch = branch_taken_from_ex(id_snap, ex_az);
        bool stall = !take_branch && data_hazard_stall(id_snap, em_snap, wb_snap, fd_snap);

        // ----- MEM: memory access; latch MEM/WB -----
        MEM_WB mem_wb_next{};
        int mem_we_cycle = 0;
        int32_t mem_addr_cycle = 0;
        if (em_snap.valid)
        {
            mem_wb_next.valid = true;
            mem_wb_next.rd_num = em_snap.rd_num;
            mem_wb_next.ctrl = em_snap.ctrl;
            if (em_snap.ctrl.Jump || em_snap.ctrl.Jalr)
                mem_wb_next.alu_result = em_snap.npc;
            else
                mem_wb_next.alu_result = em_snap.alu_result;
            if (em_snap.ctrl.MemRead)
            {
                int idx = em_snap.alu_result / 4;
                mem_wb_next.mem_read_data = (idx >= 0 && idx < 32) ? d_mem[idx] : 0;
            }
            if (em_snap.ctrl.MemWrite)
            {
                mem_we_cycle = 1;
                mem_addr_cycle = em_snap.alu_result;
                int idx = em_snap.alu_result / 4;
                if (idx >= 0 && idx < 32)
                    d_mem[idx] = em_snap.store_data;
            }
        }

        // ----- EX: latch EX/MEM -----
        EX_MEM ex_mem_next{};
        if (id_snap.valid)
        {
            ex_mem_next.valid = true;
            ex_mem_next.pc = id_snap.pc;
            ex_mem_next.npc = id_snap.npc;
            ex_mem_next.alu_result = ex_alu;
            ex_mem_next.store_data = id_snap.rs2_val;
            ex_mem_next.alu_zero = ex_az;
            ex_mem_next.branch_target = ex_bt;
            ex_mem_next.instr = id_snap.instr;
            ex_mem_next.rd_num = id_snap.rd_num;
            ex_mem_next.ctrl = id_snap.ctrl;
        }

        // ----- ID: latch ID/EX (stall freezes IF/ID; flush on branch/jump) -----
        ID_EX id_ex_next;
        if (stall)
            id_ex_next = make_bubble_id_ex();
        else if (take_branch)
            id_ex_next = make_bubble_id_ex();
        else if (fd_snap.valid && !fd_snap.instr_bits.empty())
            decode_to_id_ex(fd_snap.instr_bits, fd_snap.pc, fd_snap.npc, id_ex_next);
        else
            id_ex_next = make_bubble_id_ex();

        // ----- IF -----
        IF_ID if_id_next;
        int32_t pc_next;
        if (stall)
        {
            if_id_next = fd_snap;
            pc_next = pc_snap;
        }
        else if (take_branch)
        {
            if_id_next = make_bubble_if_id();
            pc_next = ex_bt;
        }
        else
        {
            string fb = fetch_instr(program, pc_snap);
            if (!fb.empty())
            {
                if_id_next.valid = true;
                if_id_next.instr_bits = fb;
                if_id_next.pc = pc_snap;
                if_id_next.npc = pc_snap + 4;
                pc_next = pc_snap + 4;
            }
            else
            {
                if_id_next = make_bubble_if_id();
                pc_next = pc_snap;
            }
        }

        mem_wb = mem_wb_next;
        ex_mem = ex_mem_next;
        id_ex = id_ex_next;
        if_id = if_id_next;
        pc = pc_next;

        total_clock_cycles++;
        print_cycle_trace(prev_rf, prev_d_mem, wb_snap, mem_we_cycle, mem_addr_cycle, pc_next);
        cycles++;

        bool pipeline_empty = !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid;
        if (cycles > 0 && pipeline_empty)
            break;
    }

    cout << "program terminated:" << endl;
    cout << "total execution time is " << total_clock_cycles << " cycles" << endl;
    return 0;
}
