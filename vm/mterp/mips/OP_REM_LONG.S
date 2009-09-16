%verify "executed"
/* ldivmod returns quotient in a0/a1 and remainder in a2/a3 */
%include "mips/binopWide.S" { "result0":"v0", "result1":"v1", "instr":"JAL(__moddi3)", "chkzero":"1"}
