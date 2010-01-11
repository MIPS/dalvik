%verify "executed"
%include "mips/binflopWide.S" {"instr":"JAL(__divdf3)","instr_f":"JAL(__divdf3)"}
