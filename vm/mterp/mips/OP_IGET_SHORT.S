%verify "executed"
%verify "negative value is sign-extended"
%include "mips/OP_IGET.S" { "load":"lh", "sqnum":"4" }
