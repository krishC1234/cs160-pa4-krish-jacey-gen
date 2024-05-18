#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <string>

#include "lower.hpp"

using namespace std;

unordered_map<string, unordered_map<string, int>> varOffsets;

void LIR_Program::codeGenString(){
    cout << ".data\n\nout_of_bounds_msg: .string \"out-of-bounds array access\"\ninvalid_alloc_msg: .string \"invalid allocation amount\"\n\n.text\n\n.globl main" << endl;
        
    for(auto it = functions.begin(); it != functions.end(); it++){
        it->second->codeGenString();
    }
}

void LIR_Function::codeGenString(){
    cout << name << ":" << endl;
    cout << "  pushq %rbp\n  movq %rsp, %rbp" << endl;
    cout << "  subq $" << locals.size()*8 << ", %rsp" << endl;
    auto it = locals.begin();
    for(int i = -8; i >= -locals.size()*8; i-=8){
        cout << "  movq $0, " << i << "(%rsp)" << endl;
        varOffsets[name][it->first] = i;
        it++;
    }

    // // print out varOffsets
    // for(auto it = varOffsets.begin(); it != varOffsets.end(); it++){
    //     cout << it->first << endl;
    //     for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++){
    //         cout << "  " << it2->first << ": " << it2->second << endl;
    //     }
    // }
    cout << "  jmp " << name << "_entry" << endl;
    cout << endl;

    for(auto it = body.begin(); it != body.end(); it++){
        cout << name << "_" << it->second->label << ":" << endl;
        it->second->codeGenString();
    }
}

void BasicBlock::codeGenString(){
    for(auto it = insts.begin(); it != insts.end(); it++){
        (*it)->codeGenString();
    }
    term->codeGenString();

    cout << endl;
}

void LirInst::codeGenString(){
    cout << "LIR_INST" << endl;
}

void Terminal::codeGenString(){
    cout << "terminal" << endl;
}