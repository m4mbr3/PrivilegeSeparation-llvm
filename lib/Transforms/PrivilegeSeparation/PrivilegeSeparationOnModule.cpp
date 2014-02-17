#define DEBUG_TYPE "PrivilegeSeparationOnModule"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Attributes.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/PrivilegeSeparation.h"
using namespace llvm;

namespace {

class PrivilegeSeparationOnModule : public ModulePass {
    public:
        static char ID;
        PrivilegeSeparationOnModule() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            runOnFunctions(M);
            runOnGlobalVariables(M);
            return true;
        }
        void runOnGlobalVariables(Module &M) {
            std::vector<GlobalVariable*>* my_list[NUM_OF_LEVELS];
            std::vector<GlobalVariable*>* to_delete[NUM_OF_LEVELS];
            for (unsigned int i = 0; i< NUM_OF_LEVELS; ++i) {
                my_list[i] = NULL;
                to_delete[i] = NULL;
            }
            Module::GlobalListType &GVlist = M.getGlobalList();
            Module::global_iterator GVIterCurr = GVlist.begin();
            Module::global_iterator GVIterEnd = GVlist.end();
            ValueToValueMapTy VMap;
            for (; GVIterCurr != GVIterEnd; ++GVIterCurr) {
                GlobalVariable &GV = *GVIterCurr;
                unsigned int privSep = GV.getPrivilegeSeparation();
                GlobalVariable *GV2 = new GlobalVariable(
                                GVIterCurr->getType()->getElementType(),
                                GVIterCurr->isConstant(), GVIterCurr->getLinkage(),
                                (Constant*) 0, GVIterCurr->getName(),
                                GVIterCurr->getThreadLocalMode(),
                                GVIterCurr->getType()->getAddressSpace());
                GV2->copyAttributesFrom(GVIterCurr);
                VMap[GVIterCurr] = GV2;
                if (my_list[privSep] == NULL)
                    my_list[privSep] = new std::vector<GlobalVariable*>();
                my_list[privSep]->push_back(GV2);
                if (to_delete[privSep] == NULL)
                    to_delete[privSep] = new std::vector<GlobalVariable*>();
                to_delete[privSep]->push_back(&GV);
            }
            for (Module::const_global_iterator I = M.global_begin(), E = M.global_end();
                    I != E; ++I) {
                 GlobalVariable *GV = cast<GlobalVariable>(VMap[I]);
                 if (I->hasInitializer())
                   GV->setInitializer(MapValue(I->getInitializer(), VMap));
            }
            //Remove of the Global Variables from the module and clone them into other structure
            for (unsigned int l = 0; l < NUM_OF_LEVELS; ++l) {
                if (to_delete[l] != NULL) {
                    for (unsigned int j = 0; j < to_delete[l]->size(); ++j) {
                        if (to_delete[l] != NULL) {
                            to_delete[l]->at(j)->replaceAllUsesWith(my_list[l]->at(j));
                            to_delete[l]->at(j)->dropAllReferences();
                            to_delete[l]->at(j)->eraseFromParent();
                        }
                    }
                }
            }
            for (unsigned int i = 0; i < NUM_OF_LEVELS; ++i) {
                if(my_list[i] != NULL) {
                    for (unsigned int j=0; j< my_list[i]->size(); ++j) {
                        std::stringstream ss;
                        ss << i;
                        std::string sec = ".dat_ps_" + ss.str();
                        StringRef sec_ref = StringRef(sec);
                        my_list[i]->at(j)->setSection(sec_ref);
                        M.getGlobalList().push_back(my_list[i]->at(j));
                    }
                }
            }
            for (unsigned int i = 0; i< NUM_OF_LEVELS; ++i) {
                if (my_list[i] != NULL)
                    delete my_list[i];
                if (to_delete[i] != NULL)
                    delete to_delete[i];
            }
        }
        void runOnFunctions(Module &M) {
            std::vector<Function*>* my_list[NUM_OF_LEVELS];
            std::vector<Function*>* to_delete[NUM_OF_LEVELS];
            for (unsigned int i = 0; i<NUM_OF_LEVELS; ++i) {
                my_list[i] = NULL;
                to_delete[i] = NULL;
            }
            Module::FunctionListType &funlist = M.getFunctionList();
            Module::iterator funIterCurr = funlist.begin();
            Module::iterator funIterEnd = funlist.end();
            for( ; funIterCurr != funIterEnd; ++funIterCurr) {
                Function &fun = *funIterCurr;
                AttributeSet cuAttrSet = fun.getAttributes();
                if (cuAttrSet.hasAttribute(AttributeSet::FunctionIndex, "privilege-separation")) {
                    Attribute att = cuAttrSet.getAttribute(AttributeSet::FunctionIndex, "privilege-separation");
                    std::string value_str = att.getValueAsString().str();
                    int value_int = atoi(value_str.c_str());
                    ValueToValueMapTy VMap;
                    Function *fun2 = CloneFunction(&fun, VMap, false);
                    if (my_list[value_int] == NULL)
                        my_list[value_int] = new std::vector<Function*>();
                    my_list[value_int]->push_back(fun2);
                    if (to_delete[value_int] == NULL)
                        to_delete[value_int] = new std::vector<Function*>();
                    to_delete[value_int]->push_back(&fun);
                }
                else {
                    std::stringstream ss;
                    ss << NUM_OF_LEVELS-1;
                    std::string sec = ".fun_ps_" + ss.str();
                    fun.setSection(sec);
                }
            }
            //Remove of the functions from the module and clone them into other structure
            for (unsigned int l = 0; l < NUM_OF_LEVELS; ++l) {
                if (to_delete[l] != NULL) {
                    for (unsigned int j = 0; j < to_delete[l]->size(); ++j) {
                            if (to_delete[l] != NULL) {
                                to_delete[l]->at(j)->deleteBody();
                                to_delete[l]->at(j)->replaceAllUsesWith(my_list[l]->at(j));
                                to_delete[l]->at(j)->dropAllReferences();
                                to_delete[l]->at(j)->eraseFromParent();
                            }
                    }
                }
            }
            //Re-insert functions into the module
            for (unsigned int i = 0; i < NUM_OF_LEVELS; ++i) {
                if (my_list[i] != NULL) {
                    for (unsigned int j = 0; j< my_list[i]->size(); ++j) {
                        std::stringstream ss;
                        ss << i;
                        std::string sec = ".fun_ps_" + ss.str();
                        StringRef sec_ref = StringRef(sec);
                        my_list[i]->at(j)->setSection(sec_ref);
                        M.getFunctionList().push_back(my_list[i]->at(j));
                    }
                }
            }
            //Loop to delete all the created elements
            for (unsigned int i = 0; i< NUM_OF_LEVELS; ++i) {
                if (my_list[i] != NULL)
                    delete my_list[i];
                if (to_delete[i] != NULL)
                    delete to_delete[i];
            }
        }
    };
}
char PrivilegeSeparationOnModule::ID = 0;
static RegisterPass<PrivilegeSeparationOnModule> X("PrivilegeSeparationOnModule", "PrivilegeSeparation On Module", false, false);


