#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <string>

#include "codegen.hpp"
#include "lower.hpp"

using namespace std;

CodeGen* codegen(LIR_Program* lir){
    CodeGen* cg = new CodeGen();
    cg->lir = lir;
    return cg;
}

void CodeGen::toString(){
    cout << ".data\n\nout_of_bounds_msg: .string \"out-of-bounds array access\"\ninvalid_alloc_msg: .string \"invalid allocation amount\"\n\n.text\n\n.globl main" << endl;
}