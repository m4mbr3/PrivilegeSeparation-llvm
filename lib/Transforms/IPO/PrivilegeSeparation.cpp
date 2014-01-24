#define DEBUG_TYPE "PrivilegeSeparation"
#include <iostream>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
/// \brief Privilege Separation pass handles "privilegeSeparation" functions.

class PrivilegeSeparation : public FunctionPass {
        //int PrivilegeSeparation = 0;
    public:
        static char ID;
        PrivilegeSeparation() : FunctionPass(ID) {}
        virtual bool runOnFunction(Function &F) {
            AttributeSet cuAttrSet = F.getAttributes();
            std::cout << cuAttrSet.getAsString(AttributeSet::FunctionIndex) << std::endl;
            if (cuAttrSet.hasAttribute(AttributeSet::FunctionIndex, Attribute::PrivilegeSeparation)) {
                //++PrivilegeSeparation;
                //errs() << PrivilegeSeparation;
                //errs().write_escaped(F.getName()) << '\n';
                std::cout << "Found" << std::endl;
                return true;
            }
            std::cout << "Not Found" << std::endl;
            return false;
        }
    };
}
char PrivilegeSeparation::ID = 0;
static RegisterPass<PrivilegeSeparation> X("PrivilegeSeparation", "PrivilegeSeparation Counter", false, false);
