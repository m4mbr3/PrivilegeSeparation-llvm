#define DEBUG_TYPE "TaggingPropagation"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/PrivilegeSeparation.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace llvm;
namespace {
class TaggingPropagation : public ModulePass {
    public:
        static char ID;
        TaggingPropagation() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            std::vector<CallGraphNode*> *sons = NULL;
            std::vector<CallGraphNode*> *not_visited = new std::vector<CallGraphNode*>();
            std::vector<CallGraphNode*> *visited = new std::vector<CallGraphNode*>();
            std::vector<CallSite> *CallSites = new std::vector<CallSite>();
            CallGraphNode *root = NULL;
            CallGraph *CG = new CallGraph(M);
            CallGraph::iterator BCG = CG->begin();
            CallGraph::iterator ECG = CG->end();
            for (; BCG != ECG; ++BCG) {
                CallGraphNode *CGN = BCG->second;
                Function *fun = CGN->getFunction();
                if (!fun || fun->isDeclaration()) continue;
                AttributeSet cuAttrSet = fun->getAttributes();
                if (cuAttrSet.hasAttribute(AttributeSet::FunctionIndex, "privilege-separation")) {
                    Attribute att = cuAttrSet.getAttribute(AttributeSet::FunctionIndex, "privilege-separation");
                    std::string value_str = att.getValueAsString().str();
                    fun->setSection(".fun_ps_" + value_str);
                }
                if (fun->getName().str().compare("_Z12exit_wrapperv") == 0) {
                    std::stringstream ss;
                    ss << NUM_OF_LEVELS - 1;
                    std::string sec = ".fun_ps_" + ss.str();
                    StringRef sec_ref = StringRef(sec);
                    fun->setSection(sec_ref);
                }
                if (fun->getName().str().compare("main") == 0) {
                    root = CGN;
                    std::stringstream ss;
                    ss << NUM_OF_LEVELS - 1;
                    std::string sec = ".fun_ps_" + ss.str();
                    StringRef sec_ref = StringRef(sec);
                    root->getFunction()->setSection(sec_ref);
                }
                for(Function::iterator BB = fun->begin(), E = fun->end(); BB!= E; ++BB) {
                    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
                        CallSite CS(cast<Value>(I));
                        if (!CS || isa<IntrinsicInst>(I))
                            continue;
                        if (CS.getCalledFunction() && CS.getCalledFunction()->isDeclaration())
                            continue;
                        if (CS.getCaller() && CS.getCaller()->isDeclaration())
                            continue;
                        if (CS.getCaller()->getSection().compare(".text.startup") == 0 && CS.getCaller()->getSection().compare(".text.startup") == 0)
                            continue;
                        CallSites->push_back(CS);
                    }
                }
            }
            while(!isPropagated(CG)) {
                not_visited->push_back(root);
                while(!not_visited->empty()) {
                    CallGraphNode *current = not_visited->back();
                    not_visited->pop_back();
                    /*Check if the parents of the current node have all a tag */
                    int level = -1;
                    if (!isAlreadyTagged(current)){
                        if (is_all_parents_tagged(current, CallSites, &level)) {
                            //case in which I can tag the function
                            Function *cur = current->getFunction();
                            std::stringstream ss;
                            ss << level;
                            std::string sec = ".fun_ps_" + ss.str();
                            StringRef sec_ref = StringRef(sec);
                            cur->setSection(sec_ref);
                            visited->push_back(current);
                            //once the function is tagged I can put all the childs in the to visit
                            //if there are any
                            sons = get_list_sons(current);
                            while(!sons->empty()) {
                                if (std::find(visited->begin(), visited->end(), sons->back()) ==  visited->end())
                                    not_visited->push_back(sons->back());
                                sons->pop_back();
                            }
                        }
                        else {
                            //case where It's not ready to elaborate the node due some missing dependencies
                            //so the idea is that we put again in the queue the node and we hopefully next time
                            //we will be able to tag it
                            not_visited->insert(not_visited->begin(),current);
                        }
                    }
                    else {
                        //case in which everything is already done, here
                        //I can consider only the childs node
                        visited->push_back(current);
                        sons = get_list_sons(current);
                        while(!sons->empty()){
                            if (std::find(visited->begin(), visited->end(), sons->back()) == visited->end())
                                not_visited->push_back(sons->back());
                            sons->pop_back();
                        }
                    }
                }
            }
            print_tagging_propagation_status(CG);
            delete sons;
            delete CallSites;
            delete not_visited;
            delete visited;
            delete CG;
            return true;
        }

    private:
        bool isAlreadyTagged (CallGraphNode *node) {
            Function *child = node->getFunction();
            if ((child->getSection()).find(".fun_ps_") != std::string::npos) return true;
            else return false;
        }
        /*
         *  Function to control if all the parents have a valid tag
         *
         */
        bool is_all_parents_tagged(CallGraphNode *node, std::vector<CallSite> *CallSites, int *level) {
            Function *child = node->getFunction();
            std::vector<Function*> *parents = new std::vector<Function*>();
            for(unsigned i = 0; i != CallSites->size(); ++i) {
                Function *caller = CallSites->at(i).getCaller();
                Function *callee = CallSites->at(i).getCalledFunction();
                if (callee->getName().str().compare(child->getName().str()) == 0) {
                   std::size_t pos = caller->getSection().find(".fun_ps_");
                   if (pos == std::string::npos) {
                       delete parents;
                       return false;
                   }
                   //set level
                   const char *c = caller->getSection().c_str();
                   int cur_lev = atoi(c+8);
                   if (*level == -1) *level = cur_lev;
                   else {
                        if (*level < cur_lev ) *level = cur_lev;
                   }
                }
            }
            delete parents;
            return true;
        }

        /*
         *  Utility to print the state of every function at the end of the elaboration
         *
         */
        void print_tagging_propagation_status(CallGraph *CG) {
            CallGraph::iterator BCG = CG->begin();
            CallGraph::iterator ECG = CG->end();
            std::cout << "------------------STATUS-----------------------" << std::endl;
            for(; BCG != ECG; ++BCG) {
                CallGraphNode *CGN = BCG->second;
                Function *fun = CGN->getFunction();
                if(!fun || fun->isDeclaration()) continue;
                if(fun->isDefTriviallyDead()) continue;
                std::cout << "Function " << fun->getName().str() << " tag " << fun->getSection() << std::endl;
            }
        }
        /*
         *  Fucntion to obtains all the sons of a certain node already don't removing
         *  eventually recursive dependency
         */
        std::vector<CallGraphNode*>* get_list_sons(CallGraphNode *node) {
            std::vector<CallGraphNode*> *sons_list = new std::vector<CallGraphNode*>();
            CallGraphNode::iterator CGNB = node->begin();
            CallGraphNode::iterator CGNE = node->end();
            for (; CGNB != CGNE; ++CGNB) {
                CallGraphNode *Called = CGNB->second;
                Function *son = Called->getFunction();
                if (!son || son->isDeclaration() || Called == node ) continue;
                sons_list->push_back(Called);
            }
            return sons_list;
        }

        /*
         *  Function to control when the analysis is completed. It travers all the call graph
         *  and control if every node has the
         *
         */
        bool isPropagated(CallGraph *CG) {
            CallGraph::iterator BCG = CG->begin();
            CallGraph::iterator ECG = CG->end();
            for(; BCG != ECG; ++BCG) {
                CallGraphNode *CGN = BCG->second;
                Function *fun = CGN->getFunction();
                if (!fun || fun->isDeclaration()) continue;
                if (fun->getName().str().compare("_Z12exit_wrapperv") == 0) continue;
                std::size_t pos = fun->getSection().find(".fun_ps_");
                if (pos == std::string::npos && fun->getSection().compare(".text.startup") != 0) {
                    return false;
                }
            }
            return true;
        }
    };
}

char TaggingPropagation::ID = 5;
static RegisterPass<TaggingPropagation> X("TaggingPropagation", "Tagging Propagation pass on the call graph", false, false);


