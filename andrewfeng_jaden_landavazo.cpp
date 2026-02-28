#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

using namespace std;  

struct RISCV_Instruction{
    string type, rs1, rs2, rd, opName, imm_hex;
    int funct3, funct7;
    int32_t immediate;

    // Constructor with binary string
    RISCV_Instruction(const string& binary){
        // Initialize everything to 0/empty first
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = this->imm_hex = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
        
        // Immediately trigger the parsing logic
        this->get_instruction_type(binary);
    }

    RISCV_Instruction(){
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = this->imm_hex = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
    }

    // Parses the string and delegates to the appropriate assignment method
    void get_instruction_type(const string& binary){
        int opcode_decimal = stoi(binary.substr(25, 7), nullptr, 2);
        int f3 = stoi(binary.substr(17, 3), nullptr, 2);

        if(opcode_decimal == 51){
            this->type = "R";
            this->assign_R_attributes(binary, f3); 
        }
        else if(opcode_decimal == 19 || opcode_decimal == 3 || opcode_decimal == 103){
            this->type = "I";
            this->assign_I_attributes(binary, f3); 
        } 
        else if(opcode_decimal == 99){
            this->type = "SB";
            this->assign_SB_attributes(binary, f3); 
        }
        else if(opcode_decimal == 111){
            this->type = "UJ";
            this->assign_UJ_attributes(binary, f3);
        }   
        else if(opcode_decimal == 35){
            this->type = "S";
            this->assign_S_attributes(binary, f3);
        }
    }

    // Formats and prints out the results exactly like the sample outputs
    void print() {
        // Output type (replicating exact spacing from the lab prompt examples)
        if (type == "R") {
            cout << "Instruction Type: " << type << endl;
        } else {
            cout << "Instruction Type : " << type << endl;
        }
        
        cout << "Operation: " << opName << endl;

        // Print fields based on type
        if (type == "R") {
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Rd: " << rd << endl;
            cout << "Funct3: " << funct3 << endl;
            cout << "Funct7: " << funct7 << endl;
        }
        else if (type == "I") {
            cout << "Rs1: " << rs1 << endl;
            cout << "Rd: " << rd << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
        else if (type == "S") {
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
        else if (type == "SB") {
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Immediate: " << immediate << endl; // Lab sample omits hex for SB
        }
        else if (type == "UJ") {
            cout << "Rd: " << rd << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
    }

   private:

    // Helper method to sign-extend the parsed immediate value
    int32_t calculate_imm(const string& bits) {
        uint32_t val = stoul(bits, nullptr, 2);
        // If MSB is 1, sign extend to 32 bits
        if (bits[0] == '1') {
            val |= (0xFFFFFFFF << bits.length());
        }
        return (int32_t)val;
    }

    // Helper method to get the hex representation of the immediate
    string get_hex(const string& bits) {
        uint32_t val = stoul(bits, nullptr, 2);
        stringstream ss;
        ss << "0x" << uppercase << hex << val;
        return ss.str();
    }

    void assign_I_attributes(const string& binary, int f3){
        this->funct3 = f3;
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2));
        
        string imm_bits = binary.substr(0, 12);
        this->immediate = calculate_imm(imm_bits);
        this->imm_hex = get_hex(imm_bits);

        int opcode = stoi(binary.substr(25, 7), nullptr, 2);

        if (opcode == 3) {
            if(f3==0) opName="lb";
            else if(f3==1) opName="lh";
            else if(f3==2) opName="lw";
        } else if (opcode == 103) {
            if(f3==0) opName="jalr";
        } else {
            if(f3==0) opName="addi";
            else if(f3==7) opName="andi";
            else if(f3==6) opName="ori";
            else if(f3==4) opName="xori";
            else if(f3==2) opName="slti";
            else if(f3==3) opName="sltiu";
            else if(f3==1) opName="slli";
            else if(f3==5 && binary.substr(0,7)=="0000000") opName="srli";
            else if(f3==5 && binary.substr(0,7)=="0100000") opName="srai";
        }
    }

    void assign_S_attributes(const string& binary, int f3){
        this->funct3 = f3;
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        
        // imm[11:5] | imm[4:0]
        string imm_bits = binary.substr(0, 7) + binary.substr(20, 5);
        this->immediate = calculate_imm(imm_bits);
        this->imm_hex = get_hex(imm_bits);

        if(f3==0) opName="sb";
        else if(f3==1) opName="sh";
        else if(f3==2) opName="sw";
    }

    void assign_R_attributes(const string& binary, int f3){
        this->funct3 = f3;
        this->funct7 = stoi(binary.substr(0, 7), nullptr, 2);
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2));

        if(this->funct3==0 && this->funct7==0) opName="add";
        else if(this->funct3==0 && this->funct7==32) opName="sub";
        else if(this->funct3==7 && this->funct7==0) opName="and";
        else if(this->funct3==6 && this->funct7==0) opName="or";
        else if(this->funct3==4 && this->funct7==0) opName="xor";
        else if(this->funct3==1 && this->funct7==0) opName="sll";
        else if(this->funct3==2 && this->funct7==0) opName="slt";
        else if(this->funct3==3 && this->funct7==0) opName="sltu";
        else if(this->funct3==5 && this->funct7==0) opName="srl";
        else if(this->funct3==5 && this->funct7==32) opName="sra";
    }

    void assign_SB_attributes(const string& binary, int f3){
        this->funct3 = f3;
        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2));
        
        // imm[12] | imm[11] | imm[10:5] | imm[4:1] | 0
        string imm_bits = string(1, binary[0]) + string(1, binary[24]) + binary.substr(1, 6) + binary.substr(20, 4) + "0";
        this->immediate = calculate_imm(imm_bits);
        this->imm_hex = get_hex(imm_bits);

        if(f3==0) opName="beq";
        else if(f3==1) opName="bne";
        else if(f3==4) opName="blt";
        else if(f3==5) opName="bge";
    }

    void assign_UJ_attributes(const string& binary, int f3){
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2));
        
        // imm[20] | imm[19:12] | imm[11] | imm[10:1] | 0
        string imm_bits = string(1, binary[0]) + binary.substr(12, 8) + string(1, binary[11]) + binary.substr(1, 10) + "0";
        this->immediate = calculate_imm(imm_bits);
        this->imm_hex = get_hex(imm_bits);

        opName="jal";
    }
};

int main(int argc, char* argv[]){
    string binary;

    // The while(true) loop ensures the program keeps running
    while (true) {
        cout << "\nEnter an instruction:" << endl;
        
        // cin >> binary will wait for user input
        // It will also return false if the input stream is closed
        if (cin >> binary) {
            // Basic validation to ensure the user entered 32 bits
            if (binary.length() != 32) {
                cout << "Error: Instruction must be exactly 32 bits." << endl;
                continue; 
            }

            RISCV_Instruction inst(binary);
            inst.print();
        } else {
            // Exit loop if input stream is broken (e.g., Ctrl+D)
            break;
        }
    }
    
    return 0;
}