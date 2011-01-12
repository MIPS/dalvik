%verify "executed"
%include "mips/OP_SPUT.S" {"barrier":"SMP_DMB"}
