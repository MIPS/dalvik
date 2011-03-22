%verify "executed"
%include "mips/OP_IPUT_OBJECT.S" {"barrier":"SMP_DMB"}
