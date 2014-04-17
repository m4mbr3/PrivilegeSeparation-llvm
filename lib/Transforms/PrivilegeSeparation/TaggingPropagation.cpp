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
            std::vector<CallGraphNode*> *dependencies = new std::vector<CallGraphNode*>();
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
                if (fun->getName().str().compare("main") == 0) { 
                    root = CGN;
                    std::stringstream ss;
                    ss << NUM_OF_LEVELS - 1;
                    std::string sec = ".fun_ps_" + ss.str();
                    StringRef sec_ref = StringRef(sec);
                    root->getFunction()->setSection(sec_ref);
                }
                else {
                    /*TOREMOVE*/
                    std::stringstream ss;
                    ss << NUM_OF_LEVELS -1;
                    std::string sec = ".fun_ps_" + ss.str();
                    StringRef sec_ref = StringRef(sec);
                    fun->setSection(sec_ref);
                }
                std::cout << "Considering : " << fun->getName().str() << std::endl;
                for(Function::iterator BB = fun->begin(), E = fun->end(); BB!= E; ++BB) {
                    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
                        CallSite CS(cast<Value>(I));
                        if (!CS || isa<IntrinsicInst>(I))
                            continue;
                        if (CS.getCalledFunction() && CS.getCalledFunction()->isDeclaration())
                            continue;
                        CallSites->push_back(CS);
                        std::cout << "CallSite found" << std::endl;
                    }
                }    
            }
            std::cout << "HERE" << std::endl;
            while(!isPropagated(CG)) {
                not_visited->push_back(root);
                while(!not_visited->empty()) {
                    CallGraphNode *current = not_visited->back();
                    not_visited->pop_back();
                    /*Check if the parents of the current node have all a tag */
                }
            }

            delete CallSites;
            delete sons;
            delete not_visited;
            delete dependencies;
            delete visited;
            delete CG;
            return true;
        }

    private:
        
        /*
         *  Function to control if all the parents have a valid tag
         *  
         */
        bool is_all_parents_tagged(CallGraphNode *node, std::vector<CallSite> *CallSites) {
           Function *child = node->getFunction();
            std::vector<Function*> *parents = new std::vector<Function*>(); 
            for(unsigned i = 0; i != CallSites->size(); ++i) {
                Function *caller = CallSites[i].back().getCaller();
                Function *callee = CallSites[i].back().getCalledFunction();
                if (callee == child) {
                   std::size_t pos = caller->getName().str().find(".fun_ps_");
                   if (pos == std::string::npos) {
                       delete parents;
                       return false;
                   }
                }
            }
            delete parents; 
            return true;
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
                std::size_t pos = fun->getSection().find(".fun_ps_");
                if (pos == std::string::npos) {
                    std::cout << fun->getName().str() << std::endl;
                    std::cout << fun->getSection() << std::endl;
                    return false;
                }
            }
            return true;
        }
    };
}

char TaggingPropagation::ID = 5;
static RegisterPass<TaggingPropagation> X("TaggingPropagation", "Tagging Propagation pass on the call graph", false, false);


