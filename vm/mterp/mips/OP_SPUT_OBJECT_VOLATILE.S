%verify "executed"
%include "mips/OP_SPUT_OBJECT.S" {"barrier":"SMP_DMB"}
