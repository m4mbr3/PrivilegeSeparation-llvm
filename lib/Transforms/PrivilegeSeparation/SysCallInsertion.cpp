#define DEBUG_TYPE "SysCallInsertion"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include <vector>

using namespace llvm;

namespace {
class SysCallInsertion : public ModulePass{
    public:
        static char ID;
        SysCallInsertion() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            //std::vector<CallSite> *CallSites = new std::vector<CallSite>();
            CallGraph *CG = new CallGraph(M);
            CallGraph::iterator BCG = CG->begin();
            CallGraph::iterator ECG = CG->end();
            for (; BCG != ECG; ++BCG) {
                CallGraphNode *CGN = BCG->second;
                Function *fun = CGN->getFunction();
                for (Function::iterator BB = fun->begin(), E = fun->end(); BB!=E; ++BB) {
                    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
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
                        //HERE I have to detect if the call needs to ask for privileges or not
                        Function *caller = CS.getCaller();
                        Function *callee = CS.getCalledFunction();
                        const char *cr = caller->getSection().c_str();
                        const char *ce = callee->getSection().c_str();
                        int cr_lev = atoi(cr+8);
                        int ce_lev = atoi(ce+8);
                        if (ce_lev > cr_lev) continue;
                        //Save the call instruction and the next instruction
                        Instruction *Inst = I;
                        Instruction *after = ++I;
                        //Remove the call instruction from the basicblock
                        Inst->eraseFromParent();
                        //Crete another basic block for the true case of the if I want insert
                        BasicBlock *TrueB = BasicBlock::Create(M.getContext());
                        //Re-Add the call instruction I removed from the original code
                        TrueB->getInstList().push_back(Inst);
                        //Creating teamplate for the syscall call to add
                        Type *RetVal = Type::getInt64Ty(M.getContext());
                        Type *NumSysCall = Type::getInt64Ty(M.getContext());
                        Type *Parameter = Type::getInt32Ty(M.getContext());
                        Value *SyscallFunc = M.getOrInsertFunction("syscall", RetVal,NumSysCall,Parameter, NULL);
                        //Creating Syscall instruction from the template and add to the code
                        CallInst *SCall = CallInst::Create(SyscallFunc,"syscall", after);
                        //Insert The branch section in the place where we removed the call
                        BranchInst *jump = BranchInst::Create(TrueB, after);
                        jump->setCondition(SyscallFunc);
                        //Valy
                        //CallSites->push_back(CS);
                    }
                }
            }

            delete CG;
            //delete CallSites;
            return true;
        }
    };


}

char SysCallInsertion::ID = 0;
static RegisterPass<SysCallInsertion> X("SysCallInsertion", "Pass the detects privilege level changes and insert the proper syscall to manage the change", false, false);
