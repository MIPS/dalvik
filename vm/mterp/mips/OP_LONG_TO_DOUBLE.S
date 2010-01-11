%verify "executed"
%include "mips/unflopWide.S" {"instr":"JAL(__floatdidf)","ld_arg":"LOAD64(a0, a1, a3)"}
