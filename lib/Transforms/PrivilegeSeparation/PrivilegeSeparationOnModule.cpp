#define DEBUG_TYPE "PrivilegeSeparationOnModule"
#include <iostream>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Attributes.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Transforms/Utils/Cloning.h"
using namespace llvm;

namespace {

class PrivilegeSeparationOnModule : public ModulePass {
    public:
        static char ID;
        PrivilegeSeparationOnModule() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            std::vector<Function*> *my_list = new std::vector<Function*>();
            std::vector<Function*> *to_delete = new std::vector<Function*>();
            Module::FunctionListType &funlist = M.getFunctionList();
            Module::iterator funIterCurr = funlist.begin();
            Module::iterator funIterEnd = funlist.end();
            unsigned int i = 0;
            for( ; funIterCurr != funIterEnd; ++funIterCurr) {
                Function &fun = *funIterCurr;
                //uint32_t ICount = getInstructionCount(fun);
                //std::cout << "Function: " << fun.getName().str() << " and the instruction count: " << ICount << std::endl;
                AttributeSet cuAttrSet = fun.getAttributes();
                if (cuAttrSet.hasAttribute(AttributeSet::FunctionIndex, "privilege-separation")) {
                    Attribute att = cuAttrSet.getAttribute(AttributeSet::FunctionIndex, "privilege-separation");
                    //std::cout << "Found and the value is ";
                    //std::cout << att.getValueAsString().str();
                    //std::cout << std::endl;
                    ValueToValueMapTy VMap;
                    Function *fun2 = CloneFunction(&fun, VMap, false);
                    my_list->push_back(fun2);
                    to_delete->push_back(&fun);
/*                  for (llvm::Function::iterator I = fun.begin(),
                                                        E = fun.end();
                                                        I != E;
                                                        ++I) {
                        BasicBlock &B = *I;
                        //B.replaceAllUsesWith(UndefValue::get((Type*) B.getType()));
                        B.dropAllReferences();
                        B.eraseFromParent();
                    }*/
                    i++;
                }
            }
            for (unsigned int j=0; j < to_delete->size(); j++) {
                    to_delete->at(j)->deleteBody();
                    to_delete->at(j)->replaceAllUsesWith(my_list->at(j));
                    to_delete->at(j)->dropAllReferences();
                    to_delete->at(j)->eraseFromParent();
            }
            for (unsigned int j=0; j<i; j++) {
                M.getFunctionList().push_back(my_list->at(i-j-1));
            }
            delete my_list;
            delete to_delete;
            return true;
        }
        uint32_t getInstructionCount (Function &Fun) {
            uint32_t ICount = 0;

            // A llvm::Function is just a list of llvm::BasicBlock. In order to get
            // instruction count we can visit all llvm::BasicBlocks ...
            for(llvm::Function::const_iterator I = Fun.begin(),
                                               E = Fun.end();
                                               I != E;
                                               ++I)
              // ... and sum the llvm::BasicBlock size -- A llvm::BasicBlock size is just
              // a list of instructions!
              ICount += I->size();

            return ICount;
        }
    };
}
char PrivilegeSeparationOnModule::ID = 0;
static RegisterPass<PrivilegeSeparationOnModule> X("PrivilegeSeparationOnModule", "PrivilegeSeparation On Module", false, false);


