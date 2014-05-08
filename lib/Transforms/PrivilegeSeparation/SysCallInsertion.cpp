#define DEBUG_TYPE "SysCallInsertion"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <vector>
#include <iostream>
#define SYSCALLNUM 352
using namespace llvm;

namespace {
class SysCallInsertion : public ModulePass{
    public:
        static char ID;
        SysCallInsertion() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            //std::vector<CallSite> *CallSites = new std::vector<CallSite>();
            std::vector<Instruction *> *visited = NULL;
            Instruction *I = NULL;
            while ((I = getList(visited, M)) != NULL ) {
                if (!visited) visited = new std::vector<Instruction *> ();
                visited->push_back(I);
                CallSite CS (cast<Value>(I));
                Function *caller = CS.getCaller();
                Function *callee = CS.getCalledFunction();
                const char *cr = caller->getSection().c_str();
                const char *ce = callee->getSection().c_str();
                int cr_lev = atoi(cr+8);
                int ce_lev = atoi(ce+8);
                if (ce_lev >= cr_lev) {
                    continue;
                }
                //Save the call instruction and the next instruction
                //Split function in multiple basic block
                BasicBlock *head = I->getParent();
                BasicBlock *tail = head->splitBasicBlock(I);
                TerminatorInst *headOldTerm = head->getTerminator();
                //tail->getInstList().pop_front();
                //headOldTerm->moveBefore(I);
                I->removeFromParent();
                //Crete another basic block for the true case of the if I want insert
                BasicBlock *label_if_then = BasicBlock::Create(M.getContext(), "if.then",head->getParent(), tail);
                //Remove the call instruction from the basicblock
                //Re-Add the call instruction I removed from the original code

                BranchInst::Create(tail, label_if_then);
                //Syscall Prototype and pointer
                std::vector<Type*> FuncTy_2_args;
                FuncTy_2_args.push_back(IntegerType::get(M.getContext(), 32));
                FuncTy_2_args.push_back(IntegerType::get(M.getContext(), 32));
                FunctionType *FuncTy_2 = FunctionType::get(IntegerType::get(M.getContext(), 32), FuncTy_2_args, true);
                PointerType *PointerTy_1 = PointerType::get(FuncTy_2, 0);

                //Constant Definition
                ConstantInt* num_syscall = ConstantInt::get(M.getContext(), APInt(32, SYSCALLNUM, false));
                ConstantInt* num_caller = ConstantInt::get(M.getContext(), APInt(32, StringRef(cr+8),10));
                ConstantInt* num_callee  = ConstantInt::get(M.getContext(), APInt(32, StringRef(ce+8),10));
                ConstantInt* one = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"),10));
                ConstantInt* mone = ConstantInt::get(M.getContext(), APInt(32, StringRef("-1"),10));

                //Creating template for the exit call to add
                std::vector<Type *> exit_ty_args;
                exit_ty_args.push_back(IntegerType::get(M.getContext(), 32));
                FunctionType* exit_ty = FunctionType::get(
                        Type::getVoidTy(M.getContext()),
                        exit_ty_args,
                        false);
                Function* func_exit = M.getFunction("exit");
                if (!func_exit) {
                    func_exit = Function::Create(
                            exit_ty,
                            GlobalValue::ExternalLinkage,
                            "exit",
                            &M);
                    func_exit->setCallingConv(CallingConv::C);
                }
                AttributeSet func_exit_PAL;
                {
                    SmallVector<AttributeSet, 4> Attrs;
                    AttributeSet PAS;
                    {
                        AttrBuilder B;
                        B.addAttribute(Attribute::NoReturn);
                        PAS = AttributeSet::get(M.getContext(), ~0U, B);
                    }
                    Attrs.push_back(PAS);
                    func_exit_PAL = AttributeSet::get(M.getContext(), Attrs);
                }
                func_exit->setAttributes(func_exit_PAL);

                //Creating template for the syscall call to add
                std::vector<Type*> FuncTy_9_args;
                FunctionType *FuncTy_9 = FunctionType::get(IntegerType::get(M.getContext(), 32), FuncTy_9_args, true);
                Function *func_syscall = M.getFunction("syscall");
                if(!func_syscall){
                    func_syscall = Function::Create(FuncTy_9, GlobalValue::ExternalLinkage, "syscall", &M);
                    func_syscall->setCallingConv(CallingConv::C);
                }
                AttributeSet func_syscall_PAL;
                {
                    SmallVector<AttributeSet, 4> Attrs;
                    AttributeSet PAS;
                    {
                        AttrBuilder B;
                        PAS = AttributeSet::get(M.getContext(), ~0U, B);
                    }
                Attrs.push_back(PAS);
                func_syscall_PAL = AttributeSet::get(M.getContext(), Attrs);
                }
                func_syscall->setAttributes(func_syscall_PAL);
                //End template syscall

                //Creating Template for the  ???
                Constant* sys_ptr = ConstantExpr::getCast(Instruction::BitCast,  func_syscall, PointerTy_1);
                std::vector<Value *> upgrade;
                upgrade.push_back(num_syscall);
                upgrade.push_back(num_callee);
                std::vector<Value *> downgrade;
                downgrade.push_back(num_syscall);
                downgrade.push_back(num_caller);

                CallInst::Create(func_exit, mone, "", label_if_then->getTerminator());
                CallInst *syscall_downgrade = CallInst::Create(sys_ptr, downgrade, "syscall");
                tail->getInstList().push_front(syscall_downgrade);
                tail->getInstList().push_front(I);
                CallInst *syscall_upgrade  = CallInst::Create(sys_ptr, upgrade, "syscall", head->getTerminator());
                syscall_upgrade->setCallingConv(CallingConv::C);
                AttributeSet upgrade_PAL;
                {
                    SmallVector<AttributeSet, 4> Attrs;
                    AttributeSet PAS;
                    {
                        AttrBuilder B;
                        B.addAttribute(Attribute::NoUnwind);
                        PAS = AttributeSet::get(M.getContext(), Attrs);
                    }
                    Attrs.push_back(PAS);
                    upgrade_PAL = AttributeSet::get(M.getContext(), Attrs);
                }
                syscall_upgrade->setAttributes(upgrade_PAL);

                syscall_downgrade->setCallingConv(CallingConv::C);
                AttributeSet downgrade_PAL;
                {
                    SmallVector<AttributeSet, 4> Attrs;
                    AttributeSet PAS;
                    {
                        AttrBuilder B;
                        B.addAttribute(Attribute::NoUnwind);
                        PAS = AttributeSet::get(M.getContext(), Attrs);
                    }
                    Attrs.push_back(PAS);
                    downgrade_PAL = AttributeSet::get(M.getContext(), Attrs);
                }
                syscall_upgrade->setAttributes(downgrade_PAL);

                //End template

                //Condition
                ICmpInst* condition = new ICmpInst(head->getTerminator(), ICmpInst::ICMP_NE, syscall_upgrade, one, "cmp");
                BranchInst *headNewTerm = BranchInst::Create(label_if_then, tail, condition);
                headNewTerm->setDebugLoc(I->getDebugLoc());
                ReplaceInstWithInst (headOldTerm, headNewTerm);
            }
            if(visited) delete visited;
            return true;
        }
    private:
        Instruction *getList(std::vector<Instruction*> *visited, Module &M) {
            Module::FunctionListType &funlist = M.getFunctionList();
            Module::iterator BCG = funlist.begin();
            Module::iterator ECG = funlist.end();
            for (; BCG != ECG; ++BCG) {
                Function &fun = *BCG;
                for (Function::iterator BB = fun.begin(), E = fun.end(); BB!=E; ++BB) {
                    for (BasicBlock::iterator I = BB->begin(), End = BB->end(); I != End; ++I) {
                        CallSite CS (cast<Value>(I));
                        if (!CS || isa<IntrinsicInst>(I))
                            continue;
                        if (CS.getCalledFunction() && CS.getCalledFunction()->isDeclaration())
                            continue;
                        if (CS.getCaller() && CS.getCaller()->isDeclaration())
                            continue;
                        if (CS.getCaller()->getSection().compare(".text.startup")== 0
                                && CS.getCaller()->getSection().compare(".text.startup") == 0)
                            continue;
                        if (!CS.getCalledFunction()) continue;
                        if (isVisited(I, visited))
                            continue;
                        std::cout << "Returning callsite: Caller =  " << CS.getCaller()->getName().str() << " Callee = "<< CS.getCalledFunction()->getName().str() << std::endl;
                        //HERE I have to detect if the call needs to ask for privileges or not
                        return I;
                    }
                }
            }
            return NULL;
        }
        bool isVisited (Instruction *I, std::vector<Instruction*> *visited) {
            if (visited == NULL) return false;
            for (unsigned i = 0; i < visited->size(); ++i) {
                 CallSite CS (cast<Value>(I));
                 CallSite Other (cast<Value>(visited->at(i)));
                 if (CS.getCalledFunction()->getName().compare(Other.getCalledFunction()->getName())== 0
                         && CS.getCaller()->getName().compare(Other.getCaller()->getName()) == 0){
                    return true;
                 }
            }
            return false;
        }
        void printBasicBlock (Instruction *I) {
            BasicBlock *b = I->getParent();
            BasicBlock::iterator b_it = b->begin();
            BasicBlock::iterator e_it = b->end();
            std::cout << "DEBUG INFO FOR " << I->getName().str() << std::endl;
            for (; b_it != e_it; ++b_it) {
                Instruction *is = b_it;
                if (TerminatorInst::classof(is))
                    std::cout << "Instruction : TerminatorInst" << std::endl;
                else
                    std::cout << "Instruction : " << is->getName().str() << std::endl;
            }
        }
        void printBasicBlock (BasicBlock *B) {
            BasicBlock::iterator b_it = B->begin();
            BasicBlock::iterator e_it = B->end();
            std::cout << "DEBUG INFO FOR " << B->getParent()->getName().str() << std::endl;
            for (; b_it != e_it; ++b_it) {
                Instruction *is = b_it;
                if (TerminatorInst::classof(is))
                    std::cout << "Instruction : TerminatorInst" << std::endl;
                else
                    std::cout << "Instruction : " << is->getName().str() << std::endl;
            }
        }

    };
}

char SysCallInsertion::ID = 0;
static RegisterPass<SysCallInsertion> X("SysCallInsertion", "Pass the detects privilege level changes and insert the proper syscall to manage the change", false, false);
