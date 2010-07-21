%verify "executed"
%include "mips/unopNarrower.S" {"instr":"JAL(__floatdisf)","instr_f":"JAL(__floatdisf)", "load":"LOAD64(a0, a1, a3)"}
