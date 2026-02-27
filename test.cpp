#include <iostream>

using namespace std;  

struct RISCV_Instruction{
    string type, rs1, rs2, rd, opName;
    int funct3, funct7;
    int32_t immediate;

    RISCV_Instruction(const string& binary){
        // Initialize everything to 0/empty first
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;
        
        // Immediately trigger the parsing logic
        this->get_instruction_type(binary);
        
    }


    RISCV_Instruction(){
        this->rd = this->rs1 = this->rs2 = this->type = this->opName = "";
        this->funct3 = this->funct7 = 0;
        this->immediate = 0;

    }
// 00000000001100100 000 00101 0110011 
    void get_instruction_type(const string& binary){
        int opcode_decimal = stoi(binary.substr(25, 7), nullptr, 2);
        int funct3 = std::stoi(binary.substr(17, 3), nullptr, 2);

        if(opcode_decimal == 51){
            this->type = "R";
            this->assign_R_attributes(binary, funct3); 
        }
        else if(opcode_decimal == 19 || opcode_decimal == 3 || opcode_decimal == 103){
            this->type = "I";
            this->assign_I_attributes(binary, funct3); 
        } 
        else if(opcode_decimal == 99){
            this->type = "SB";
            this->assign_SB_attributes(binary, funct3); 
        }
        else if(opcode_decimal == 111){
            this->type = "UJ";
            this->assign_UJ_attributes(binary, funct3);
        }   
        else{
            this->type = "S";
            this->assign_S_attributes(binary, funct3);
        }
        
    }

    

   private:

    void assign_I_attributes(const string& binary, int f3){
        if(funct3==0) opName="addi";
        else if(funct3==7) opName="andi";
        else if(funct3==6) opName="ori";
        else if(funct3==4) opName="xori";
        else if(funct3==2) opName="slti";
        else if(funct3==3) opName="sltiu";
        else if(funct3==1) opName="slli";
        else if(funct3==5 && binary.substr(0,7)=="0000000") opName="srli";
        else if(funct3==5 && binary.substr(0,7)=="0100000") opName="srai";
        else if(funct3==0 && stoi(binary.substr(25,7),nullptr,2)==3) opName="lb";
        else if(funct3==1 && stoi(binary.substr(25,7),nullptr,2)==3) opName="lh";
        else if(funct3==2 && stoi(binary.substr(25,7),nullptr,2)==3) opName="lw";
        else if(stoi(binary.substr(25,7),nullptr,2)==103) opName="jalr";

    }

    void assign_S_attributes(const string& binary, int f3){

        if(funct3==0) opName="sb";
        else if(funct3==1) opName="sh";
        else if(funct3==2) opName="sw";

    }
    void assign_R_attributes(const string& binary, int f3){
         if(funct3==0 && funct7==0) opName="add";
        else if(funct3==0 && funct7==32) opName="sub";
        else if(funct3==7) opName="and";
        else if(funct3==6) opName="or";
        else if(funct3==4) opName="xor";
        else if(funct3==1) opName="sll";
        else if(funct3==2) opName="slt";
        else if(funct3==3) opName="sltu";
        else if(funct3==5 && funct7==0) opName="srl";
        else if(funct3==5 && funct7==32) opName="sra";

    }

    void assign_SB_attributes(const string& binary, int f3){
        if(funct3==0) opName="beq";
        else if(funct3==1) opName="bne";
        else if(funct3==4) opName="blt";
        else if(funct3==5) opName="bge";

    }

    void assign_UJ_attributes(const string& binary, int f3){

    }

};

int main(int argc, char* argv[]){
    RISCV_Instruction test = RISCV_Instruction("00000000001100100000001010110011");   

    return 0;
}