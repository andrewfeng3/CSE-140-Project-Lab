#include <iostream>

using namespace std;

struct RISCV_Instruction
{
    string type, rs1, rs2, rd, opName;
    int funct3, funct7;
    int32_t immediate;

    RISCV_Instruction(const string &binary)
    {
        // Initialize everything to 0/empty first
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;

        // Immediately trigger the parsing logic
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
        cout << funct3 << endl;
        cout << funct7 << endl;
        cout << opcode_decimal << endl;

        if (funct3 == 5 && funct7 == 0)
            this->opName = "srli";
        else if (funct3 == 5 && funct7 == 32)
            this->opName = "srai";
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

        else if (opcode_decimal == 103)
            this->opName = "jalr";
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
    }

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
    void assign_I_attributes(const string &binary, int f3)
    {
    }

    void assign_S_attributes(const string &binary, int f3)
    {
    }
    void assign_R_attributes(const string &binary, int f3)
    {
    }

    void assign_SB_attributes(const string &binary, int f3)
    {
    }

    void assign_UJ_attributes(const string &binary, int f3)
    {
    }
};

int main(int argc, char *argv[])
{
    RISCV_Instruction test = RISCV_Instruction("00000000001100100000001010110011");

    return 0;
}