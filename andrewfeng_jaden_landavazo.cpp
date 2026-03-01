#include <iostream>      
#include <string>        
#include <sstream>       
#include <iomanip>       
#include <cstdint>       

using namespace std;     // Avoids needing std:: prefix throughout

// Struct that represents a decoded RISC-V instruction
struct RISCV_Instruction{
    string type, rs1, rs2, rd, opName, imm_hex; // Decoded instruction fields
    int funct3, funct7;                          // Function codes used in decoding
    int32_t immediate;                           // Signed immediate value

    // Constructor that accepts a 32-bit binary string
    RISCV_Instruction(const string& binary){
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = this->imm_hex = ""; // Initialize strings
        this->funct3 = this->funct7 = 0;   // Initialize funct fields
        this->immediate = 0;               // Initialize immediate
        
        this->get_instruction_type(binary); // Begin decoding immediately
    }

    // Default constructor
    RISCV_Instruction(){
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = this->imm_hex = ""; // Initialize strings
        this->funct3 = this->funct7 = 0;   // Initialize funct fields
        this->immediate = 0;               // Initialize immediate
    }

    // Determines instruction type using opcode bits
    void get_instruction_type(const string& binary){
        int opcode_decimal = stoi(binary.substr(25, 7), nullptr, 2); // Extract opcode (bits 6:0)
        int f3 = stoi(binary.substr(17, 3), nullptr, 2);              // Extract funct3 (bits 14:12)

        if(opcode_decimal == 51){          // R-type opcode
            this->type = "R";              // Set type
            this->assign_R_attributes(binary, f3); // Parse R-type fields
        }
        else if(opcode_decimal == 19 || opcode_decimal == 3 || opcode_decimal == 103){ // I-type opcodes
            this->type = "I";              // Set type
            this->assign_I_attributes(binary, f3); // Parse I-type
        } 
        else if(opcode_decimal == 99){     // SB-type opcode
            this->type = "SB";
            this->assign_SB_attributes(binary, f3);
        }
        else if(opcode_decimal == 111){    // UJ-type opcode
            this->type = "UJ";
            this->assign_UJ_attributes(binary, f3);
        }   
        else if(opcode_decimal == 35){     // S-type opcode
            this->type = "S";
            this->assign_S_attributes(binary, f3);
        }
    }

    // Prints decoded instruction in required format
    void print() {
        if (type == "R") {                             
            cout << "Instruction Type: " << type << endl;
        } else {
            cout << "Instruction Type : " << type << endl;
        }
        
        cout << "Operation: " << opName << endl;       // Print operation name

        if (type == "R") {                             // R-type fields
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Rd: " << rd << endl;
            cout << "Funct3: " << funct3 << endl;
            cout << "Funct7: " << funct7 << endl;
        }
        else if (type == "I") {                        // I-type fields
            cout << "Rs1: " << rs1 << endl;
            cout << "Rd: " << rd << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
        else if (type == "S") {                        // S-type fields
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
        else if (type == "SB") {                       // SB-type fields
            cout << "Rs1: " << rs1 << endl;
            cout << "Rs2: " << rs2 << endl;
            cout << "Immediate: " << immediate << endl;
        }
        else if (type == "UJ") {                       // UJ-type fields
            cout << "Rd: " << rd << endl;
            cout << "Immediate: " << immediate << " (or " << imm_hex << ")" << endl;
        }
    }

private:

    // Sign-extend binary immediate to 32 bits
    int32_t calculate_imm(const string& bits) {
        uint32_t val = stoul(bits, nullptr, 2);        // Convert binary to unsigned

        if (bits[0] == '1') {                           // If negative
            val |= (0xFFFFFFFF << bits.length());       // Perform sign extension
        }

        return (int32_t)val;                            // Return signed value
    }

    // Convert immediate bits to uppercase hexadecimal string
    string get_hex(const string& bits) {
        uint32_t val = stoul(bits, nullptr, 2);         // Convert to integer
        stringstream ss;                                // Create formatter
        ss << "0x" << uppercase << hex << val;          // Format as hex
        return ss.str();                                // Return string
    }

    // Parse I-type instruction
    void assign_I_attributes(const string& binary, int f3){
        this->funct3 = f3;                              // Store funct3

        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2)); // Extract rs1
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2)); // Extract rd
        
        string imm_bits = binary.substr(0, 12);         // Immediate bits [31:20]
        this->immediate = calculate_imm(imm_bits);      // Sign-extend immediate
        this->imm_hex = get_hex(imm_bits);              // Store hex version

        int opcode = stoi(binary.substr(25, 7), nullptr, 2); // Get opcode

        if (opcode == 3) {                              // Load instructions
            if(f3==0) opName="lb";
            else if(f3==1) opName="lh";
            else if(f3==2) opName="lw";
        } else if (opcode == 103) {                     // jalr
            if(f3==0) opName="jalr";
        } else {                                        // ALU immediates
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

    // Parse S-type instruction (stores)
    void assign_S_attributes(const string& binary, int f3){
        this->funct3 = f3;                              // Store funct3

        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));  // Extract rs2
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2)); // Extract rs1
        
        string imm_bits = binary.substr(0, 7) + binary.substr(20, 5); // Combine split immediate
        this->immediate = calculate_imm(imm_bits);      // Sign-extend
        this->imm_hex = get_hex(imm_bits);              // Hex version

        if(f3==0) opName="sb";                          // Store byte
        else if(f3==1) opName="sh";                     // Store half
        else if(f3==2) opName="sw";                     // Store word
    }

    // Parse R-type instruction
    void assign_R_attributes(const string& binary, int f3){
        this->funct3 = f3;                              // Store funct3
        this->funct7 = stoi(binary.substr(0, 7), nullptr, 2); // Extract funct7

        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));  // Extract rs2
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2)); // Extract rs1
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2)); // Extract rd

        // Determine ALU operation
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

    // Parse SB-type (branch)
    void assign_SB_attributes(const string& binary, int f3){
        this->funct3 = f3;                              // Store funct3

        this->rs2 = "x" + to_string(stoi(binary.substr(7, 5), nullptr, 2));  // Extract rs2
        this->rs1 = "x" + to_string(stoi(binary.substr(12, 5), nullptr, 2)); // Extract rs1
        
        string imm_bits = string(1, binary[0]) + string(1, binary[24]) +
                          binary.substr(1, 6) + binary.substr(20, 4) + "0"; // Rebuild immediate

        this->immediate = calculate_imm(imm_bits);      // Sign-extend
        this->imm_hex = get_hex(imm_bits);              // Hex version

        if(f3==0) opName="beq";
        else if(f3==1) opName="bne";
        else if(f3==4) opName="blt";
        else if(f3==5) opName="bge";
    }

    // Parse UJ-type (jal)
    void assign_UJ_attributes(const string& binary, int f3){
        this->rd  = "x" + to_string(stoi(binary.substr(20, 5), nullptr, 2)); // Extract rd
        
        string imm_bits = string(1, binary[0]) +
                          binary.substr(12, 8) +
                          string(1, binary[11]) +
                          binary.substr(1, 10) + "0"; // Rebuild immediate

        this->immediate = calculate_imm(imm_bits);      // Sign-extend
        this->imm_hex = get_hex(imm_bits);              // Hex version

        opName="jal";                                   // Only UJ instruction here
    }
};

int main(int argc, char* argv[]){
    string binary;                                      // Stores user input instruction

    while (true) {                                      // Loop until EOF
        cout << "\nEnter an instruction:" << endl;
        
        if (cin >> binary) {                            // Read binary string
            if (binary.length() != 32) {                // Validate length
                cout << "Error: Instruction must be exactly 32 bits." << endl;
                continue; 
            }

            RISCV_Instruction inst(binary);             // Decode instruction
            inst.print();                               // Display results
        } else {
            break;                                      // Exit on EOF
        }
    }
    
    return 0;                                           // Program finished successfully
}