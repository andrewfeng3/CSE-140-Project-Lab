# CSE 140 Project Report

**Team ID:** Andrew Feng & Jaden Landavazo  
**Date:** March 20, 2026

---

## 1. Single-cycle RISC-V CPU

### 1.1 Overview

The single-cycle CPU is implemented in `andrew-feng_jaden-landavazo.cpp` using a classic five-stage organization: Fetch, Decode, Execute, Memory, and Writeback. In hardware, all five stages complete in one clock cycle; in this simulator, they run **one after another inside a single C++ loop iteration**, so each iteration corresponds to one instruction and one count in `total_clock_cycles`. Architectural state lives in global variables (`pc`, register file, data memory, control signals) so the code mirrors the diagram-style datapath where wires and multiplexers connect stages.

The program is stored as a `vector<string>`: each line is a **32-character binary string** with the **most significant bit at index 0** (matching a bit-by-bit print of the instruction word). The program counter is a **byte address**; `pc / 4` indexes the current line. `next_pc` holds `pc + 4` for the sequential successor after fetch.

Control signals (`RegWrite`, `MemRead`, `MemWrite`, `Branch`, `MemtoReg`, `ALUSrc`, `ALUOp`, and in part 2 `Jump`, `Jalr`) are generated in `ControlUnit` from the opcode and drive the same kinds of mux and enable decisions as in the textbook single-cycle datapath.

### 1.2 Code Structure

#### 1.2.1 Functions

**Instruction decode (`RISCV_Instruction`).** The constructor parses a binary string by calling `get_instruction_type`, which reads the 7-bit opcode (from `substr(25, 7)` given MSB-first storage) and dispatches to `assign_R_attributes`, `assign_I_attributes`, `assign_S_attributes`, `assign_SB_attributes`, `assign_U_attributes` (for `lui` / `auipc`), or `assign_UJ_attributes` (for `jal`). Helpers such as `get_R_opName`, `get_I_opName`, `get_S_opName`, and `get_SB_opName` map `funct3` / `funct7` / opcode to mnemonics. **I-type disambiguation:** `jalr` uses opcode `103` with `funct3 = 0`, the same pattern as `addi`; `get_I_opName` therefore checks for opcode `103` **before** treating `funct3 == 0` as `addi`. Immediate fields use RISC-V’s split encodings; I- and S-types use `get_sign_extention_I_type` and `get_sign_extension_S_type`, while SB- and UJ-types assemble scattered bits into one value and sign-extend from the appropriate width. The `operator<<` overload prints decoded fields for debugging.

**ALU and main control.** `get_alu_ctrl` implements the usual ALU control table: `ALUOp == 0` forces ADD (effective addresses for loads/stores and for `jalr`’s `rs1 + imm`), `ALUOp == 1` selects SUB for branch comparison, and `ALUOp == 2` selects among ADD/SUB/AND/OR for R-type and I-type arithmetic based on `funct3` / `funct7`. `ControlUnit` clears all signals each decode, then sets the pattern for each supported opcode (R-type, I-type arithmetic, load, store, branch, JAL, JALR).

**Stage functions.**

- **`Fetch`** returns the instruction bits at the current `pc` (or an empty string if `pc` is out of range). It also **updates `pc` for the next iteration** using the **previous** instruction’s outcome: `prev_Branch`, `prev_alu_zero`, `prev_Jump`, and `prev_Jalr`. That models the idea that the PC multiplexer uses the branch/jump decision from the instruction that **just finished**, not the one about to decode. `first_fetch` skips this update once so execution starts at `pc = 0` without stale `prev_*` values. After choosing `pc`, it sets `next_pc = pc + 4` for the sequential path.

- **`Decode`** constructs `RISCV_Instruction`, calls `ControlUnit`, reads `rf[rs1_num]` and `rf[rs2_num]`, copies the sign-extended immediate, sets `store_data` to `rs2` for stores, and stages `writeback_rd` and `do_writeback` from `RegWrite`.

- **`Execute`** selects the ALU’s second operand with `ALUSrc` (register `rs2` vs immediate). It computes `alu_result` and `alu_zero` (whether the ALU output is zero, for BEQ). It computes **`branch_target`**: for ordinary branches it uses the decoded B-immediate together with `next_pc`; for **JAL** it overwrites with `pc + instr.immediate` (UJ immediate is the byte offset after decode); for **JALR** it uses `(rs1 + imm) & ~1` per the spec’s LSB-clear rule.

- **`Mem`** uses `alu_result` as the byte address, converts to a word index `addr / 4`, and reads or writes `d_mem[idx]` when `MemRead` or `MemWrite` is asserted. Stores use `store_data` (the value read from `rs2` in Decode).

- **`Writeback`** writes to `rf[writeback_rd]` when `RegWrite` is set and `rd != 0`. The written value is `mem_read_data` if `MemtoReg`, else `alu_result`, except for **JAL/JALR**, where `rd` receives **`next_pc`** (the address of the following instruction, i.e. the return address). It then increments `total_clock_cycles` and copies `Branch`, `alu_zero`, `Jump`, and `Jalr` into the `prev_*` registers for the next `Fetch`.

**Support code.** `init_part1` and `init_part2` reset `rf` and `d_mem` and load the handout’s initial values; part 2 also sets `use_abi_names` so traces print ABI names (`ra`, `a0`, …). `print_cycle_changes` compares against snapshots of `rf` and `d_mem` to print only changed registers and memory words, plus the **next PC** the trace format expects. `main` loads the program file, selects init based on whether the filename contains `"part2"` (case-insensitive), and runs the stage loop until `Fetch` returns empty.

#### 1.2.2 Variables

**Architectural state:** `rf[32]` is the register file; `d_mem[32]` is data memory (one word per entry; addresses are still expressed as byte addresses when printing). **`pc`** and **`next_pc`** implement the PC and sequential PC+4. **`branch_target`** holds the branch or jump destination computed in Execute (branches and jumps share this mux input in the conceptual datapath).

**Control:** `RegWrite`, `MemRead`, `MemWrite`, `Branch`, `MemtoReg`, `ALUSrc`, `ALUOp`, `Jump`, `Jalr`.

**Inter-stage values:** `alu_result`, `alu_zero`, `mem_read_data`, `writeback_rd`, `writeback_data`, `do_writeback`, `store_data`.

**Timing / PC latches:** `prev_Branch`, `prev_alu_zero`, `prev_Jump`, `prev_Jalr` record the control and zero flag from the instruction that completed in the previous iteration so `Fetch` can update `pc` correctly. `first_fetch` suppresses the first use of those latches.

**Miscellaneous:** `total_clock_cycles` counts completed instructions; `use_abi_names` selects numeric (`x0`, `x1`, …) vs ABI register names in the trace.

#### 1.2.3 Why `prev_*` is needed (PC update semantics)

In a real single-cycle machine, the adder produces **PC+4** and the branch/add unit produces a **branch target** during the same cycle as the current instruction; the PC register loads one of those at the **end** of the cycle. In software we run stages in order: **`Fetch` runs first** in the next iteration, but it must use the **branch/jump decision from the previous instruction**. Without `prev_Branch` / `prev_alu_zero` / `prev_Jump` / `prev_Jalr`, `Fetch` would not know whether the prior instruction was a taken branch or a jump. `Writeback` therefore **latches** those values at the end of each simulated cycle, and `Fetch` consumes them at the start of the next.

### 1.3 Execution Results

Compile with:

```bash
g++ -std=c++11 -O2 andrew-feng_jaden-landavazo.cpp -o riscv_cpu
```

Provide a program file where each line is a 32-bit binary instruction. Run `./riscv_cpu` with the filename as an argument or enter it when prompted (e.g. `part1_program.txt`). The simulator prints, each logical cycle: a `total_clock_cycles` header, any register updates (`xN` or ABI name) and memory updates in hex, and `pc is modified to 0x...` for the **next** PC. At termination it prints total execution time in cycles. Comparing this trace to the project handout’s reference output validates correct decoding, control, ALU, memory, and PC behavior for the assigned test programs.

### 1.4 Challenges and Limitations

**Immediates.** SB- and UJ-type fields are non-contiguous in the 32-bit instruction; assembling bit slices in the correct order and sign-extending from the correct high bit was the main source of subtle bugs (wrong branch targets or jump offsets).

**Control matching.** Each opcode must assert a consistent set of signals; errors (for example forgetting `MemtoReg` on loads) often produce plausible but wrong numeric results.

**PC timing.** Aligning `Fetch` with **previous** cycle control via `prev_*` was necessary to avoid off-by-one PC behavior relative to the handout trace.

**Scope of ISA support.** The simulator is aimed at the **course subset**: R-type and I-type arithmetic used in the labs, loads/stores, branches, **JAL**, and **JALR**. The decoder also recognizes **`lui` and `auipc`** (U-type), but **`ControlUnit` does not yet assign control for those opcodes**, so they are not fully executed as specified until that logic is added. Many other RISC-V instructions are unsupported. Data memory is fixed at 32 words; there is no pipeline, forwarding, or hazard logic. Memory access assumes in-range, word-aligned word accesses for the graded paths.

---

## 2. Extended RISC-V CPU (JAL and JALR)

### 2.1 Overview

**JAL** (opcode `111`) and **JALR** (opcode `103`) add **unconditional control flow** and **link-register** behavior: the destination register receives the **return address** (PC+4 of the jump/jump-register instruction), and the PC jumps to a new address. We extended the same single-cycle structure with two extra control outputs, **`Jump`** and **`Jalr`**, instead of duplicating the datapath.

**JAL:** `assign_UJ_attributes` decodes the UJ immediate and `rd`. `ControlUnit` asserts `RegWrite` and `Jump`. In Execute, `branch_target = pc + instr.immediate` (byte offset from the JAL instruction). In Writeback, `rd` is written with `next_pc` (return address).

**JALR:** Still decoded as I-type; `ControlUnit` asserts `RegWrite`, `Jalr`, `ALUSrc`, `ALUOp = 0`, and `MemtoReg = 0` so the ALU computes `rs1 + imm` for the target. Execute sets `branch_target = (rs1 + imm) & ~1`. Writeback again writes `next_pc` to `rd`.

**Fetch** treats `prev_Jump || prev_Jalr` like a taken control transfer: `pc = branch_target` on the following fetch, analogous to asserting the jump mux select after the prior instruction.

### 2.2 Baseline Code Structure

#### 2.2.1 Functions

`ControlUnit` adds the `111` and `103` cases as described above. `get_instruction_type` routes opcode `111` to `assign_UJ_attributes` and opcode `103` through the I-type path with `get_I_opName` selecting `jalr`. `Execute` and `Writeback` contain the jump target and return-address mux behavior. `init_part2` reinitializes registers per the part-2 handout (e.g. `s0`, `a0`–`a3`) and enables ABI names. `filename_indicates_part2` switches between `init_part1` and `init_part2` so the same binary can be used with different initial state and trace formatting.

#### 2.2.2 Variables

`Jump` and `Jalr` are the new control lines. `prev_Jump` and `prev_Jalr` extend the same PC-latching scheme as branches. `branch_target` and `next_pc` are reused: jumps do not need a separate `jump_target` variable because the same mux input drives the PC update in `Fetch`.

### 2.3 Execution Results

Compilation and execution follow section 1.3. Use a filename containing `"part2"` (e.g. `sample_part2.txt`) so `init_part2` runs and ABI names appear in the trace. A correct run shows **JAL** updating `ra` (or another `rd`) with the link value and jumping the PC; **JALR** jumps to the computed target and can return to a saved link address. The cycle-by-cycle register and PC lines document control flow and writeback.

### 2.4 Challenges and Limitations

UJ and SB immediate layouts required careful cross-checking with the RISC-V spec. PC update must stay tied to **`prev_Jump` / `prev_Jalr`** so the jump takes effect on the **next** simulated fetch, consistent with the rest of the design. The model does not include a hardware stack, exceptions, or compressed instructions; software must save and restore `ra` (and callee-saved registers) according to normal RISC-V calling conventions for nested calls.

---

## 3. Pipelined RISC-V CPU (Optional – Extra Credit)

### 3.1 Overview

A pipelined implementation would add **pipeline registers** between IF/ID, ID/EX, EX/MEM, and MEM/WB, run multiple instructions overlapped in time, and add **hazard detection**, **stalling**, **forwarding**, and **branch/jump flush** logic. The submitted project **does not** implement that: each loop iteration still completes all five stages for one instruction before starting the next, which is the single-cycle model.

### 3.2 Baseline Code Structure

There are no structures such as `IF_ID`, `ID_EX`, `EX_MEM`, or `MEM_WB`, no hazard detection unit, and no explicit pipeline flush beyond using `prev_*` for PC updates in the single-cycle sense. Globals hold the current instruction’s intermediate values and are overwritten each iteration rather than held in named pipeline latches.

#### 3.2.1 Functions

No separate pipelined stage functions or forwarding logic exist.

#### 3.2.2 Variables

No pipeline-register variables are present.

### 3.3 Optimizations

Data forwarding and structural changes for timing were not applied; “hazards” do not arise because only one instruction is live at a time in the simulator loop.

### 3.4 Execution Results

There are no separate pipelined runs. Reported cycle counts remain **one cycle per retired instruction** in the single-cycle simulator.

### 3.5 Challenges and Limitations

A future pipelined version would require defining pipeline registers, comparing source registers to destinations in earlier stages for forwarding, stalling on load-use conflicts, and flushing or predicting branches. The current submission intentionally stays with the simpler single-cycle model required for the main project milestones.
