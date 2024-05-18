#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "lower.hpp"

struct CodeGen{
    LIR_Program* lir;
    void toString();
};

CodeGen* codegen(LIR_Program* lir);

#endif