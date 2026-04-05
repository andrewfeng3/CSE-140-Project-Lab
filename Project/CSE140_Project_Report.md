# CSE 140 Project Report

**Team ID:** Andrew Feng & Jaden Landavazo  
**Date:** March 20, 2026

---

## 1. Single-cycle RISC-V CPU

### 1.1 Overview

The single-cycle CPU is implemented in `andrew-feng_jaden-landavazo.cpp` using a classic five-stage organization: Fetch, Decode, Execute, Memory, and Writeback. All stages run sequentially within one C++ loop per instruction. The design keeps architectural state in global variables: the program counter (`pc`, `next_pc`, `branch_target`), a 32-entry register file `rf[32]`, and a small word-addressable data memory `d_mem[32]`. Control signals such as `RegWrite`, `MemRead`, `MemWrite`, `Branch`, `MemtoReg`, `ALUSrc`, `ALUOp`, and (for part 2) `Jump` and `Jalr` drive the datapath.

Each simulated cycle performs Fetch (read instruction from the program vector), Decode (parse fields and set control signals), Execute (ALU and branch/jump target), Memory (load/store to `d_mem`), and Writeback (update `rf` and cycle count). The main loop reads a text file of 32-bit binary instruction strings into a vector and repeatedly runs these stages until the PC goes past the end of the program.

### 1.2 Code Structure

#### 1.2.1 Functions

Instruction decoding is handled by the `RISCV_Instruction` struct. Its constructor taking a binary string calls `get_instruction_type`, which uses the 7-bit opcode to dispatch to type-specific helpers: `assign_R_attributes`, `assign_I_attributes`, `assign_S_attributes`, `assign_SB_attributes`, `assign_U_attributes` (for `lui`/`auipc`), and `assign_UJ_attributes` (for `jal`). Helper functions like `get_R_opName`, `get_I_opName`, `get_S_opName`, and `get_SB_opName` map funct3/funct7/opcode to mnemonics such as `add`, `lw`, `sw`, `beq`, `jal`, and `jalr`. Immediate extraction and sign-extension for I- and S-type are done in `get_sign_extention_I_type` and `get_sign_extension_S_type`. The stream output operator for `RISCV_Instruction` prints decoded fields for debugging.

Control logic is split between `get_alu_ctrl` and `ControlUnit`. `get_alu_ctrl` maps ALUOp plus funct3/funct7 to a 4-bit ALU control (ADD, SUB, AND, OR) following the standard single-cycle design. `ControlUnit` takes the 7-bit opcode and sets RegWrite, MemRead, MemWrite, Branch, MemtoReg, ALUSrc, ALUOp, and for part 2 Jump and Jalr, covering R-type, I-type arithmetic, loads, stores, branches, JAL, and JALR.

The five stage functions are `Fetch`, `Decode`, `Execute`, `Mem`, and `Writeback`. `Fetch` uses the previous cycle’s branch/jump outcome (`prev_Branch`, `prev_alu_zero`, `prev_Jump`, `prev_Jalr`) to set the next `pc`, then returns the instruction at the current PC or an empty string when out of range. `Decode` builds a `RISCV_Instruction`, calls `ControlUnit`, reads `rf[rs1_num]` and `rf[rs2_num]`, and passes the sign-extended immediate and writeback destination to later stages. `Execute` selects the second ALU operand (register or immediate), computes `alu_result` and `alu_zero`, and sets `branch_target` for branches, JAL, or JALR. `Mem` performs load or store into `d_mem` using `alu_result` as the address. `Writeback` writes either memory data or ALU result (or `next_pc` for JAL/JALR) into `rf[writeback_rd]`, increments `total_clock_cycles`, and saves the current Branch, alu_zero, Jump, and Jalr into the `prev_*` variables for the next Fetch.

Initialization and reporting are handled by `init_part1` (resets state and sets a small test register/memory layout with numeric register names), `init_part2` (similar reset with ABI-style register setup and `use_abi_names = 1`), and `print_cycle_changes` (prints which register or memory location changed and the next PC each cycle). `main` prompts for a program filename, loads lines into a vector, chooses part 1 or part 2 init based on whether the filename contains `"part2"`, and runs the Fetch–Decode–Execute–Mem–Writeback loop until Fetch returns empty.

#### 1.2.2 Variables

Architectural state is held in `rf[32]` and `d_mem[32]`. Control and datapath values include `pc`, `next_pc`, `branch_target`, `alu_zero`, and the control signals RegWrite, MemRead, MemWrite, Branch, MemtoReg, ALUSrc, ALUOp, Jump, Jalr. Stage outputs passed between steps are `alu_result`, `mem_read_data`, `writeback_rd`, `writeback_data`, `do_writeback`, and `store_data`. The previous cycle’s branch/jump state is stored in `prev_Branch`, `prev_alu_zero`, `prev_Jump`, `prev_Jalr`; `first_fetch` disables using those on the very first instruction. `total_clock_cycles` counts completed instructions, and `use_abi_names` toggles numeric vs ABI register names in the printed output.

### 1.3 Execution Results

Compile with:

```bash
g++ -std=c++11 -O2 andrew-feng_jaden-landavazo.cpp -o riscv_cpu
```

Provide a program file where each line is a 32-bit binary instruction. Run `./riscv_cpu`, enter the filename when prompted (e.g. `part1_program.txt`). The program prints per-cycle changes: which register or memory location was updated and its new value in hex, and the next PC. At the end it reports total execution time in cycles. This serves as supporting evidence that each instruction completes in one logical cycle of the simulator loop.

### 1.4 Challenges and Limitations

Correctly decoding and sign-extending immediates for S-, SB-, U-, and UJ-types required careful handling of RISC-V’s split immediate layouts (e.g. SB and UJ). Matching control signals to each instruction type in `ControlUnit` was error-prone; mistakes could corrupt results without obvious symptoms. Modeling branch and jump behavior so that the PC updates only after the previous instruction’s outcome required the `prev_*` variables in Fetch.

The implementation supports a subset of the ISA: R-type and I-type arithmetic, loads/stores, branches, lui, auipc, jal, and jalr. Many other instructions are not implemented. Data memory is limited to 32 words. The design is strictly single-cycle with no pipelining, forwarding, or hazard handling. Memory indexing assumes in-range, word-aligned addresses; there is no explicit handling of misaligned or out-of-bounds accesses.

---

## 2. Extended RISC-V CPU (JAL and JALR)

### 2.1 Overview

The single-cycle CPU was extended to support JAL and JALR so that function-call-style control flow and return addresses are handled correctly. New control signals Jump and Jalr are set by the control unit for opcodes 111 and 103. UJ-type immediate decoding in `assign_UJ_attributes` provides the JAL offset; JALR uses the I-type immediate. PC update logic in Execute sets `branch_target` to `pc + instr.immediate` for JAL and to `(rs1_val + imm_sext) & ~1` for JALR. In Writeback, when Jump or Jalr is set, the value written to the destination register is `next_pc` (the return address). Part 2 initialization (`init_part2`) sets up registers with ABI-oriented values and turns on ABI register names in the output.

### 2.2 Baseline Code Structure

#### 2.2.1 Functions

`ControlUnit` was extended so that opcode 111 (JAL) sets RegWrite and Jump, and opcode 103 (JALR) sets RegWrite, Jalr, ALUSrc, and MemtoReg = 0. `assign_UJ_attributes` decodes the JAL immediate from the scattered UJ encoding and sign-extends it; `get_I_opName` identifies JALR by opcode 103. In `Execute`, branch_target is computed as above for JAL and JALR. `Writeback` overrides the normal writeback source with `next_pc` when Jump or Jalr is set. `init_part2` clears state and initializes registers (e.g. s0, a0–a3) and sets `use_abi_names = 1`.

#### 2.2.2 Variables

Jump and Jalr are the new control outputs; prev_Jump and prev_Jalr carry them into the next Fetch so the PC is updated correctly. The existing branch_target and next_pc variables are reused for jump targets and return-address writeback.

### 2.4 Execution Results

Compilation and execution are the same as in section 1.3. Use a program file whose name contains `"part2"` (e.g. `part2_program.txt`) so that `init_part2` runs and ABI names are used. A run that includes a JAL will show the link register (e.g. ra) being set to the return address and the PC jumping to the target; a later JALR can then restore the PC to that address. The printed cycle-by-cycle register and PC changes provide evidence that JAL/JALR control flow and return-address writeback behave as intended.

### 2.5 Challenges and Limitations

Getting the UJ and SB immediate bit layouts and sign extension right was tedious and required cross-checking against the RISC-V specification. Ensuring that branch_target is computed in Execute and applied in the next Fetch via prev_Jump/prev_Jalr avoided off-by-one PC updates. The CPU does not implement a call stack or exception handling; correct use of JAL/JALR for nested or recursive calls depends on the program saving and restoring ra and other registers as needed.

---

## 3. Pipelined RISC-V CPU (Optional – Extra Credit)

### 3.1 Overview

A pipelined CPU with distinct IF/ID/EX/MEM/WB pipeline registers, hazard detection, and forwarding is not implemented in the submitted code. The current design only simulates the five stages in sequence within one loop iteration per instruction, so it remains a single-cycle model. The stage functions could in principle be refactored around pipeline registers for a future pipelined version.

### 3.2 Baseline Code Structure

There are no pipeline register structures (e.g. IF_ID, ID_EX, EX_MEM, MEM_WB), no hazard detection unit (no NOP insertion for data hazards), and no explicit flush logic for control hazards beyond the existing branch/jump handling. The same Fetch, Decode, Execute, Mem, and Writeback functions are used; control and data are stored in globals that are overwritten each cycle rather than latched between stages.

#### 3.2.1 Functions

No separate pipelined stage functions or forwarding logic exist.

#### 3.2.2 Variables

No pipeline-register variables are present.

### 3.3 Optimizations

Data forwarding was not implemented. NOP and flush behavior is only implicit in the sense that instructions do not overlap; the design does not model pipeline hazards.

### 3.4 Execution Results

There are no separate pipelined runs. Execution results are those of the single-cycle and extended (JAL/JALR) design; each instruction still contributes one cycle to the total count.

### 3.5 Challenges and Limitations

If pipelining were added, the main work would be introducing pipeline registers, hazard detection (including load-use and branch penalties), and data forwarding paths. As submitted, the project does not include a pipelined implementation or associated hazard handling.
