#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <string>

#include "lower.hpp"

using namespace std;

unordered_map<string, unordered_map<string, int>> varOffsets;

unordered_set<string> global_var;

unordered_set<string> global_fn;

unordered_map<string, unordered_map<string, int>> structOffsets;

unordered_map<string, unordered_map<string, string>> varStructType;

// get the correct variable stack offset, or return the variable name if it is a global variable
string get_var_stack(string var, string funcName){
    if(varOffsets[funcName].find(var) != varOffsets[funcName].end()){
        return to_string(varOffsets[funcName][var]) + "(%rbp)";
    }
    else if(global_var.find(var) != global_var.end()){
        return var + "(%rip)";
    }
    else if(global_fn.find(var) != global_fn.end()){
        return var + "_(%rip)";
    }
    return "ERROR";
}

void LIR_Program::codeGenString(){
    cout << ".data\n" << endl;
    // generate global variables
    //We need to initialize globals differently if its a function vs a variable
    for(auto it = globals.begin(); it != globals.end(); it++){
        if(it->second->type == Type::Ptr && it->second->value.Ptr.ref->type == Type::Fn){
            cout << ".globl " << it->first << "_" << endl;
            cout << it->first << "_: .quad \"" << it->first << "\"" << endl << endl << endl;
            global_fn.insert(it->first);
        }
        else{
            cout << ".globl " << it->first << endl;
            cout << it->first << ": .zero 8" << endl << endl << endl;
            global_var.insert(it->first);
        }
    }

    // generate struct offsets
    for(auto it = structs.begin(); it != structs.end(); it++){
        int offset = 0;
        for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++){
            structOffsets[it->first][it2->first] = offset;
            offset += 8;
        }
    }

    cout << "out_of_bounds_msg: .string \"out-of-bounds array access\"\ninvalid_alloc_msg: .string \"invalid allocation amount\"\n        \n.text\n\n";

        
    for(auto it = functions.begin(); it != functions.end(); it++){
        cout << ".globl " << it->first << endl;
        it->second->codeGenString();
    }
    //.out_of_bounds:
    cout << ".out_of_bounds:" << endl;
    cout << "  lea out_of_bounds_msg(%rip), %rdi" << endl;
    cout << "  call _cflat_panic\n" << endl;

    //.invalid_alloc_length:
    cout << ".invalid_alloc_length:" << endl;
    cout << "  lea invalid_alloc_msg(%rip), %rdi" << endl;
    cout << "  call _cflat_panic" << endl;
    cout << "        " << endl;
}

void LIR_Function::codeGenString(){

    cout << name << ":" << endl;
    
    // prologue
    cout << "  pushq %rbp\n  movq %rsp, %rbp" << endl;
    int stack_size = locals.size() * 8;
    if(stack_size % 16 != 0)
        stack_size += 8;

    // this is how much space we need for locals
    cout << "  subq $" << stack_size << ", %rsp" << endl;

    // ensure that size > 0

    auto it = params.begin();
    for(int i = 16; i <= params.size() * 8 + 8; i += 8){
        if(it->second->type == Type::Ptr && it->second->value.Ptr.ref->type == Type::Struct){
            varStructType[name][it->first] = it->second->value.Ptr.ref->value.Struct.name;
        }
        varOffsets[name][it->first] = i;
        it++;
    }
    if (stack_size > 0){
        // zero out all local variables
        auto it = locals.begin();
        for(int i = -8; i >= -locals.size() * 8; i -= 8){
            if(it->second->type == Type::Ptr && it->second->value.Ptr.ref->type == Type::Struct){
                varStructType[name][it->first] = it->second->value.Ptr.ref->value.Struct.name;
            }
            cout << "  movq $0, " << i << "(%rbp)" << endl;
            varOffsets[name][it->first] = i;
            it++;
        }
    }
    cout << "  jmp " << name << "_entry" << endl;
    cout << endl;

    // generate code for each basic block
    for(auto it = body.begin(); it != body.end(); it++){
        if(it->second->reachable == false)
            continue;
        cout << name << "_" << it->second->label << ":" << endl;
        it->second->codeGenString(name);
    }

    // epilogue
    cout << name << "_epilogue:" << endl;
    cout << "  movq %rbp, %rsp" << endl;
    cout << "  popq %rbp" << endl;
    cout << "  ret\n" << endl;

}

void BasicBlock::codeGenString(string funcName){
    for(auto it = insts.begin(); it != insts.end(); it++){
        (*it)->codeGenString(funcName);
    }
    term->codeGenString(funcName);

    cout << endl;
}

// enum type{Alloc, Arith, CallExt, Cmp, Copy, Gep, Gfp, Load, Store} type;
void LirInst::codeGenString(string funcName){
    if(type == LirInst::Alloc){
        bool num_is_const = value.Alloc.num->type == Operand::Const;
        if(num_is_const){
            cout << "  movq " << value.Alloc.num->codeGenString(funcName) << ", %r8" << endl;
            cout << "  cmpq $0, %r8" << endl;
        }
        else{
            cout << "  cmpq $0, " << value.Alloc.num->codeGenString(funcName) << endl;
        }
        cout << "  jle .invalid_alloc_length" << endl;
        cout << "  movq $1, %rdi" << endl;
        if(num_is_const)
            cout << "  imulq %r8, %rdi" << endl;
        else
            cout << "  imulq " << value.Alloc.num->codeGenString(funcName) << ", %rdi" << endl;
        cout << "  incq %rdi" << endl;
        cout << "  call _cflat_alloc" << endl;
        cout << "  movq " << value.Alloc.num->codeGenString(funcName) << ", %r8" << endl;
        cout << "  movq %r8, 0(%rax)" << endl;
        cout << "  addq $8, %rax" << endl;
        cout << "  movq %rax, " << get_var_stack(value.Alloc.lhs, funcName) << endl;
    } else if(type == LirInst::Arith){
        if(value.Arith.aop->type == ArithmeticOp::Div){
            cout << "  movq " << value.Arith.left->codeGenString(funcName) << ", %rax" << endl;
            cout << "  cqo" << endl;
            if(value.Arith.right->type == Operand::Const){
                cout << "  movq " << value.Arith.right->codeGenString(funcName) << ", %r8" << endl;
                cout << "  idivq %r8" << endl;
            }
            else {
                cout << "  idivq " << value.Arith.right->codeGenString(funcName) << endl;
            }
            cout << "  movq %rax, " << get_var_stack(value.Arith.lhs, funcName) << endl;
        }
        else{
            cout << "  movq " << value.Arith.left->codeGenString(funcName) << ", %r8" << endl;
            cout << "  " << value.Arith.aop->codeGenString() << " " << value.Arith.right->codeGenString(funcName) << ", %r8" << endl;
            cout << "  movq %r8, " << get_var_stack(value.Arith.lhs, funcName) << endl;
        }
    } else if(type == LirInst::CallExt){
        int stack_size = 0;

        // first 6 arguments are passed in registers
        if(value.CallExt.args.size() >= 1){
            cout << "  movq " << value.CallExt.args[0]->codeGenString(funcName) << ", %rdi" << endl;
        }
        if(value.CallExt.args.size() >= 2){
            cout << "  movq " << value.CallExt.args[1]->codeGenString(funcName) << ", %rsi" << endl;
        }
        if(value.CallExt.args.size() >= 3){
            cout << "  movq " << value.CallExt.args[2]->codeGenString(funcName) << ", %rdx" << endl;
        }
        if(value.CallExt.args.size() >= 4){
            cout << "  movq " << value.CallExt.args[3]->codeGenString(funcName) << ", %rcx" << endl;
        }
        if(value.CallExt.args.size() >= 5){
            cout << "  movq " << value.CallExt.args[4]->codeGenString(funcName) << ", %r8" << endl;
        }
        if(value.CallExt.args.size() >= 6){
            cout << "  movq " << value.CallExt.args[5]->codeGenString(funcName) << ", %r9" << endl;
        }
        // if there are more than 6 arguments, push them onto the stack
        if(value.CallExt.args.size() > 6){
            for(int i = value.CallExt.args.size() - 1; i >= 6; i--){
                cout << "  pushq " << value.CallExt.args[i]->codeGenString(funcName) << endl;
                stack_size += 8;
            }
            // align the stack
            if(stack_size % 16 != 0){
                stack_size += 8;
                cout << "  subq $" << 8 << ", %rsp" << endl;
            }
        }
        cout << "  call " << value.CallExt.callee << endl;
        // only return if it is an assignment
        if(value.CallExt.lhs != ""){
            cout << "  movq %rax, " << get_var_stack(value.CallExt.lhs, funcName) << endl;
        }
        if(stack_size > 0){
            cout << "  addq $" << stack_size << ", %rsp" << endl;
        }
    } else if(type == LirInst::Cmp){
        if(value.Cmp.right->type != Operand::Const || (value.Cmp.right->type == Operand::Const && value.Cmp.left->type == Operand::Const)){
            cout << "  movq " << value.Cmp.left->codeGenString(funcName) << ", %r8" << endl;
            cout << "  cmpq " << value.Cmp.right->codeGenString(funcName) << ", %r8" << endl;
        }
        else{
            cout << "  cmpq " << value.Cmp.right->codeGenString(funcName) << ", " << value.Cmp.left->codeGenString(funcName) << endl;
        }
        cout << "  movq $0, %r8" << endl;
        cout << "  " << value.Cmp.aop->codeGenString() << " %r8b" << endl;
        cout << "  movq %r8, " << get_var_stack(value.Cmp.lhs, funcName) << endl;
    } else if(type == LirInst::Copy){
        if(value.Copy.op->type == Operand::Const){
            cout << "  movq " << value.Copy.op->codeGenString(funcName) << ", " << get_var_stack(value.Copy.lhs, funcName) << endl;
        } else if(value.Copy.op->type == Operand::Var){
            cout << "  movq " << value.Copy.op->codeGenString(funcName) << ", %r8" << endl;
            cout << "  movq %r8, " << get_var_stack(value.Copy.lhs, funcName) << endl;
        }
    } else if(type == LirInst::Gep){
        cout << "  movq " << value.Gep.idx->codeGenString(funcName) << ", %r8" << endl;
        cout << "  cmpq $0, %r8" << endl;
        cout << "  jl .out_of_bounds" << endl;
        cout << "  movq " << get_var_stack(value.Gep.src, funcName) << ", %r9" << endl;
        cout << "  movq -8(%r9), %r10" << endl;
        cout << "  cmpq %r10, %r8" << endl;
        cout << "  jge .out_of_bounds" << endl;
        cout << "  imulq $8, %r8" << endl;
        cout << "  addq %r9, %r8" << endl;
        cout << "  movq %r8, " << get_var_stack(value.Gep.lhs, funcName) << endl;
    } else if(type == LirInst::Gfp){
        /*
            movq -6952(%rbp), %r8
            leaq 0(%r8), %r9
            movq %r9, -456(%rbp)
        */
        cout << "  movq " << get_var_stack(value.Gfp.src, funcName) << ", %r8" << endl;
        cout << "  leaq " << structOffsets[varStructType[funcName][value.Gfp.src]][value.Gfp.field] << "(%r8), %r9" << endl;
        cout << "  movq %r9, " << get_var_stack(value.Gfp.lhs, funcName) << endl;
    } else if(type == LirInst::Load){
        cout << "  movq " << get_var_stack(value.Load.src, funcName) << ", %r8" << endl;
        cout << "  movq 0(%r8), %r9" << endl;
        cout << "  movq %r9, " << get_var_stack(value.Load.lhs, funcName) << endl;
    } else if(type == LirInst::Store){
        cout << "  movq " << value.Store.op->codeGenString(funcName) << ", %r8" << endl;
        cout << "  movq " << get_var_stack(value.Store.dst, funcName) << ", %r9" << endl;
        cout << "  movq %r8, 0(%r9)" << endl;
    }
}
	// enum type{Branch, CallDirect, CallIndirect, Jump, Ret} type;

void Terminal::codeGenString(string funcName){
    if(type == Terminal::Branch){
        // cmpq $0, -1600(%rbp)
        // jne main_lbl8
        // jmp main_lbl9
        //Step 1: Compare op to 0, set the codes
        if(value.Branch.guard->type == Operand::Var){
            cout << "  cmpq $0, " << value.Branch.guard->codeGenString(funcName) << endl;
            cout << "  jne " << funcName << "_" << value.Branch.tt << endl;
            cout << "  jmp " << funcName << "_" << value.Branch.ff << endl;
        }
        else if(value.Branch.guard->type == Operand::Const){
            cout << "  movq " << value.Branch.guard->codeGenString(funcName) << ", %r8" << endl;
            cout << "  cmpq $0, %r8" << endl;
            cout << "  jne " << funcName << "_" << value.Branch.tt << endl;
            cout << "  jmp " << funcName << "_" << value.Branch.ff << endl;
        } else {
            cout << "uh ohh gen" << endl;
        }
    } else if(type == Terminal::CallDirect){
        // push op1...opn in reverse order to the stack
        int stack_count = 0;
        if(value.CallDirect.args.size() % 2 != 0)
            cout << "  subq $8, %rsp" << endl;
        for (auto it = value.CallDirect.args.rbegin(); it != value.CallDirect.args.rend(); ++it) {
            stack_count+=8;
            Operand* op = *it;  
            if(op->type == Operand::Var){
                cout << "  pushq " << varOffsets[funcName][op->value.Var.id] << "(%rbp)" << endl;
            }
            else{
                cout << "  pushq $" << op->value.Const.num << endl;
            }  
        } 
        // TODO: fix stack alignment - make it divisible by 16 for edge case
        if (stack_count % 16 != 0){
            stack_count += 8;
        }
        // call foo
        cout << "  call " << value.CallDirect.callee << endl;
        // store %rax to x
        if(value.CallDirect.lhs != ""){
            cout << "  movq %rax, " << varOffsets[funcName][value.CallDirect.lhs] << "(%rbp)" << endl;
        }
        // restore stack pointer <-- this was first on ben's notes
        if(stack_count > 0){
            cout << "  addq $" << stack_count << ", %rsp" << endl;
        }
        // jump to bb
        cout << "  jmp " << funcName << "_" << value.CallDirect.next_bb << endl;
    } else if(type == Terminal::CallIndirect){
        // push op1...opn in reverse order to the stack
        int stack_count = 0;
        if(value.CallIndirect.args.size() % 2 != 0)
            cout << "  subq $8, %rsp" << endl;
        for (auto it = value.CallIndirect.args.rbegin(); it != value.CallIndirect.args.rend(); ++it) {
            stack_count+=8;
            Operand* op = *it;
            if(op->type == Operand::Var){
                cout << "  pushq " << varOffsets[funcName][op->value.Var.id] << "(%rbp)" << endl;
            }
            else{
                cout << "  pushq $" << op->value.Const.num << endl;
            }  
        } 
        // TODO: fix stack alignment - make it divisible by 16 for edge case
        if (stack_count % 16 != 0){
            stack_count += 8;
        }
        // call foo
        cout << "  call *" << varOffsets[funcName][value.CallIndirect.callee] << "(%rbp)" <<endl;
        // store %rax to x
        if(value.CallIndirect.lhs != ""){
            cout << "  movq %rax, " << varOffsets[funcName][value.CallIndirect.lhs] << "(%rbp)" << endl;
        }
        // restore stack pointer <-- this was first on ben's notes
        if(stack_count > 0){
            cout << "  addq $" << stack_count << ", %rsp" << endl;
        }
        // jump to bb
        cout << "  jmp " << funcName << "_" << value.CallDirect.next_bb << endl;
    } else if(type == Terminal::Jump){
        cout << "  jmp " << funcName << "_" << value.Jump.next_bb << endl;
    } else if(type == Terminal::Ret){
        // $ret op
        // return value goes into a specific register (%rax)
        cout << "  movq " << value.Ret.op->codeGenString(funcName) << ", %rax" << endl;
        cout << "  jmp " << funcName << "_epilogue" << endl;
        // then jump to main_epilogue
    }
}

string Operand::codeGenString(string funcName){
    if(type == Operand::Var){
        return get_var_stack(value.Var.id, funcName);
    } else if(type == Operand::Const){
        return "$" + to_string(value.Const.num);
    }
    return "";
}

string ArithmeticOp::codeGenString(){
    if(type == ArithmeticOp::Add){
        return "addq";
    } else if(type == ArithmeticOp::Sub){
        return "subq";
    } else if(type == ArithmeticOp::Mul){
        return "imulq";
    } else if(type == ArithmeticOp::Div){
        return "idivq";
    }
    return "";
}

string ComparisonOp::codeGenString(){
    if(type == ComparisonOp::Equal){
        return "sete";
    } else if(type == ComparisonOp::NotEq){
        return "setne";
    } else if(type == ComparisonOp::Lt){
        return "setl";
    } else if(type == ComparisonOp::Lte){
        return "setle";
    } else if(type == ComparisonOp::Gt){
        return "setg";
    } else if(type == ComparisonOp::Gte){
        return "setge";
    }
    return "";
}