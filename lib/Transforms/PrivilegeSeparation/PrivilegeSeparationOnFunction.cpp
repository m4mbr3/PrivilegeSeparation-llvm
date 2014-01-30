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
            if (cuAttrSet.hasAttribute(AttributeSet::FunctionIndex, "privilege-separation")) {
                Attribute att = cuAttrSet.getAttribute(AttributeSet::FunctionIndex, "privilege-separation");
                std::cout << "Found and the value is ";
                std::cout << att.getValueAsString().str();
                std::cout << " the size of this function is ";
                std::cout << F.size() << std::endl;
                return true;
            }
            return false;
        }
    };
}
char PrivilegeSeparation::ID = 0;
static RegisterPass<PrivilegeSeparation> X("PrivilegeSeparation", "PrivilegeSeparation Counter", false, false);
