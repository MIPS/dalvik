%verify "executed"
%include "mips/binflopWide2addr.S" {"instr":"JAL(__divdf3)","instr_f":"JAL(__divdf3)"}
