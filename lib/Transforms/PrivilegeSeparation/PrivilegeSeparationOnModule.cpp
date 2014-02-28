#define DEBUG_TYPE "PrivilegeSeparationOnModule"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
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
            generateLinkScript();
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
        void generateLinkScript() {
            std::string begin_script =      "/* Script for -z combreloc: combine and sort reloc sections */\n"
#ifdef __x86_64__
                                "OUTPUT_FORMAT(\"elf64-x86-64\", \"elf64-x86-64\",\n"
                                "              \"elf64-x86-64\")\n"
                                "OUTPUT_ARCH(i386:x86-64)\n"
                                "ENTRY(_start)\n"
                                "SEARCH_DIR(\"/usr/x86_64-pc-linux-gnu/lib64\"); SEARCH_DIR(\"/usr/lib64/binutils/x86_64-pc-linux-gnu/*\"); SEARCH_DIR(\"/usr/local/lib64\"); SEARCH_DIR(\"/lib64\"); SEARCH_DIR(\"/usr/lib64\"); SEARCH_DIR(\"/usr/x86_64-pc-linux-gnu/lib\"); SEARCH_DIR(\"/usr/lib64/binutils/x86_64-pc-linux-gnu/*\"); SEARCH_DIR(\"/usr/local/lib\"); SEARCH_DIR(\"/lib\"); SEARCH_DIR(\"/usr/lib\");\n"
                                "SECTIONS\n"
                                "{\n"
                                "  /* Read-only sections, merged into text segment: */\n"
                                "  PROVIDE (__executable_start = SEGMENT_START(\"text-segment\", 0x400000)); . = SEGMENT_START(\"text-segment\", 0x400000) + SIZEOF_HEADERS;\n"
#elif __i386__
                                "OUTPUT_FORMAT(\"elf32-i386\", \"elf32-i386\",\n"
                                "              \"elf32-i386\")\n"
                                "OUTPUT_ARCH(i386)\n"
                                "ENTRY(_start)\n"
                                "SEARCH_DIR(\"/usr/i686-pc-linux-gnu/lib\"); SEARCH_DIR(\"/usr/lib64/binutils/i686-pc-linux-gnu/*\"); SEARCH_DIR(\"/usr/local/lib\"); SEARCH_DIR(\"/lib\"); SEARCH_DIR(\"/usr/lib\");\n"
                                "SECTIONS\n"
                                "{\n"
                                "  /* Read-only sections, merged into text segment: */\n"
                                "  PROVIDE (__executable_start = SEGMENT_START(\"text-segment\", 0x08048000)); . = SEGMENT_START(\"text-segment\", 0x08048000) + SIZEOF_HEADERS;\n"
#endif
                                "  .interp         : { *(.interp) } : text : interp \n"
                                "  .note.gnu.build-id : { *(.note.gnu.build-id) } :text\n"
                                "  .hash           : { *(.hash) }\n"
                                "  .gnu.hash       : { *(.gnu.hash) }\n"
                                "  .dynsym         : { *(.dynsym) }\n"
                                "  .dynstr         : { *(.dynstr) }\n"
                                "  .gnu.version    : { *(.gnu.version) }\n"
                                "  .gnu.version_d  : { *(.gnu.version_d) }\n"
                                "  .gnu.version_r  : { *(.gnu.version_r) }\n"
                                "  .rela.dyn       :\n"
                                "    {\n"
                                "      *(.rela.init)\n"
                                "      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)\n"
                                "      *(.rela.fini)\n"
                                "      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)\n"
                                "      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)\n"
                                "      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)\n"
                                "      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)\n"
                                "      *(.rela.ctors)\n"
                                "      *(.rela.dtors)\n"
                                "      *(.rela.got)\n"
                                "      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)\n"
                                "      *(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*)\n"
                                "      *(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*)\n"
                                "      *(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*)\n"
                                "      *(.rela.ifunc)\n"
                                "    }\n"
                                "  .rela.plt       :\n"
                                "    {\n"
                                "      *(.rela.plt)\n"
                                "      PROVIDE_HIDDEN (__rela_iplt_start = .);\n"
                                "      *(.rela.iplt)\n"
                                "      PROVIDE_HIDDEN (__rela_iplt_end = .);\n"
                                "    }\n"
                                "  .init           :\n"
                                "  {\n"
                                "    KEEP (*(SORT_NONE(.init)))\n"
                                "  }\n"
                                "  .plt            : { *(.plt) *(.iplt) }\n";
std::string text_section =      "  . = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1));\n"
                                "  .text :\n"
                                "  {\n"
                                "    *(.text.unlikely .text.*_unlikely)\n"
                                "    *(.text.exit .text.exit.*)\n"
                                "    *(.text.startup .text.startup.*)\n"
                                "    *(.text.hot .text.hot.*)\n"
                                "    *(.text .stub .text.* .gnu.linkonce.t.*)\n"
                                "    /* .gnu.warning sections are handled specially by elf32.em.  */\n"
                                "    *(.gnu.warning)\n"
                                "  } : text\n";
std::string before_data =       "  .fini           :\n"
                                "  {\n"
                                "    KEEP (*(SORT_NONE(.fini)))\n"
                                "  } : text\n"
                                "  PROVIDE (__etext = .);\n"
                                "  PROVIDE (_etext = .);\n"
                                "  PROVIDE (etext = .);\n"
                                "  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) } : text\n"
                                "  .rodata1        : { *(.rodata1) }\n"
                                "  .eh_frame_hdr : { *(.eh_frame_hdr) }\n"
                                "  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) }\n"
                                "  .gcc_except_table   : ONLY_IF_RO { *(.gcc_except_table\n"
                                "  .gcc_except_table.*) }\n"
                                "  /* These sections are generated by the Sun/Oracle C++ compiler.  */\n"
                                "  .exception_ranges   : ONLY_IF_RO { *(.exception_ranges\n"
                                "  .exception_ranges*) }\n"
                                "  /* Adjust the address for the data segment.  We want to adjust up to\n"
                                "     the same address within the page on the next page up.  */\n"
                                "  . = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1)); . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));\n"
                                "  /* Exception handling  */\n"
                                "  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) }\n"
                                "  .gcc_except_table   : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }\n"
                                "  .exception_ranges   : ONLY_IF_RW { *(.exception_ranges .exception_ranges*) }\n"
                                "  /* Thread Local Storage sections  */\n"
                                "  .tdata          : { *(.tdata .tdata.* .gnu.linkonce.td.*) }\n"
                                "  .tbss           : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }\n"
                                "  .preinit_array     :\n"
                                "  {\n"
                                "    PROVIDE_HIDDEN (__preinit_array_start = .);\n"
                                "    KEEP (*(.preinit_array))\n"
                                "    PROVIDE_HIDDEN (__preinit_array_end = .);\n"
                                "  }\n"
                                "  .init_array     :\n"
                                "  {\n"
                                "    PROVIDE_HIDDEN (__init_array_start = .);\n"
                                "    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))\n"
                                "    KEEP (*(.init_array))\n"
                                "    KEEP (*(EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtbeginTS.o *crtend.o *crtend?.o ) .ctors))\n"
                                "    PROVIDE_HIDDEN (__init_array_end = .);\n"
                                "  } : data\n"
                                "  .fini_array     :\n"
                                "  {\n"
                                "    PROVIDE_HIDDEN (__fini_array_start = .);\n"
                                "    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))\n"
                                "    KEEP (*(.fini_array))\n"
                                "    KEEP (*(EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtbeginTS.o *crtend.o *crtend?.o ) .dtors))\n"
                                "    PROVIDE_HIDDEN (__fini_array_end = .);\n"
                                "  }\n"
                                "  .ctors          :\n"
                                "  {\n"
                                "    /* gcc uses crtbegin.o to find the start of\n"
                                "       the constructors, so we make sure it is\n"
                                "       first.  Because this is a wildcard, it\n"
                                "       doesn't matter if the user does not\n"
                                "       actually link against crtbegin.o; the\n"
                                "       linker won't look for a file to match a\n"
                                "       wildcard.  The wildcard also means that it\n"
                                "       doesn't matter which directory crtbegin.o\n"
                                "       is in.  */\n"
                                "    KEEP (*crtbegin.o(.ctors))\n"
                                "    KEEP (*crtbegin?.o(.ctors))\n"
                                "    KEEP (*crtbeginTS.o(.ctors))\n"
                                "    /* We don't want to include the .ctor section from\n"
                                "       the crtend.o file until after the sorted ctors.\n"
                                "       The .ctor section from the crtend file contains the\n"
                                "       end of ctors marker and it must be last */\n"
                                "    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))\n"
                                "    KEEP (*(SORT(.ctors.*)))\n"
                                "    KEEP (*(.ctors))\n"
                                "  }\n"
                                "  .dtors          :\n"
                                "  {\n"
                                "    KEEP (*crtbegin.o(.dtors))\n"
                                "    KEEP (*crtbegin?.o(.dtors))\n"
                                "    KEEP (*crtbeginTS.o(.dtors))\n"
                                "    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))\n"
                                "    KEEP (*(SORT(.dtors.*)))\n"
                                "    KEEP (*(.dtors))\n"
                                "  }\n"
                                "  .jcr            : { KEEP (*(.jcr)) }\n"
                                "  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*) }\n"
                                "  .dynamic        : { *(.dynamic) } : data : dynamic\n"
                                "  .got            : { *(.got) *(.igot) }\n"
                                "  . = DATA_SEGMENT_RELRO_END (SIZEOF (.got.plt) >= 24 ? 24 : 0, .);\n"
                                "  .got.plt        : { *(.got.plt)  *(.igot.plt) }\n"
                                "  . = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1));\n"
                                "  .data :\n"
                                "  {\n"
                                "    *(.data .data.* .gnu.linkonce.d.*)\n"
                                "    SORT(CONSTRUCTORS)\n"
                                "  } : data\n";
std::string after_data =        "  .data1          : { *(.data1) } : data \n"
                                "  _edata = .; PROVIDE (edata = .);\n"
                                "  . = .;\n"
                                "  __bss_start = .;\n"
                                "  .bss            :\n"
                                "  {\n"
                                "   *(.dynbss)\n"
                                "   *(.bss .bss.* .gnu.linkonce.b.*)\n"
                                "   *(COMMON)\n"
                                "   /* Align here to ensure that the .bss section occupies space up to\n"
                                "      _end.  Align after .bss to ensure correct alignment even if the\n"
                                "      .bss section disappears because there are no input sections.\n"
                                "      FIXME: Why do we need it? When there is no .bss section, we don't\n"
                                "      pad the .data section.  */\n"
                                "   . = ALIGN(. != 0 ? 64 / 8 : 1);\n"
                                "  }\n"
                                "  .lbss   :\n"
                                "  {\n"
                                "    *(.dynlbss)\n"
                                "    *(.lbss .lbss.* .gnu.linkonce.lb.*)\n"
                                "    *(LARGE_COMMON)\n"
                                "  }\n"
                                "  . = ALIGN(64 / 8);\n"
                                "  .lrodata   ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1)) :\n"
                                "  {\n"
                                "    *(.lrodata .lrodata.* .gnu.linkonce.lr.*)\n"
                                "  }\n"
                                "  .ldata   ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1)) :\n"
                                "  {\n"
                                "    *(.ldata .ldata.* .gnu.linkonce.l.*)\n"
                                "    . = ALIGN(. != 0 ? 64 / 8 : 1);\n"
                                "  }\n"
                                "  . = ALIGN(64 / 8);\n"
                                "  _end = .; PROVIDE (end = .);\n"
                                "  . = DATA_SEGMENT_END (.);\n"
                                "  /* Stabs debugging sections.  */\n"
                                "  .stab          0 : { *(.stab) }\n"
                                "  .stabstr       0 : { *(.stabstr) }\n"
                                "  .stab.excl     0 : { *(.stab.excl) }\n"
                                "  .stab.exclstr  0 : { *(.stab.exclstr) }\n"
                                "  .stab.index    0 : { *(.stab.index) }\n"
                                "  .stab.indexstr 0 : { *(.stab.indexstr) }\n"
                                "  .comment       0 : { *(.comment) }\n"
                                "  /* DWARF debug sections.\n"
                                "     Symbols in the DWARF debugging sections are relative to the beginning\n"
                                "     of the section so we begin them at 0.  */\n"
                                "  /* DWARF 1 */\n"
                                "  .debug          0 : { *(.debug) }\n"
                                "  .line           0 : { *(.line) }\n"
                                "  /* GNU DWARF 1 extensions */\n"
                                "  .debug_srcinfo  0 : { *(.debug_srcinfo) }\n"
                                "  .debug_sfnames  0 : { *(.debug_sfnames) }\n"
                                "  /* DWARF 1.1 and DWARF 2 */\n"
                                "  .debug_aranges  0 : { *(.debug_aranges) }\n"
                                "  .debug_pubnames 0 : { *(.debug_pubnames) }\n"
                                "  /* DWARF 2 */\n"
                                "  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }\n"
                                "  .debug_abbrev   0 : { *(.debug_abbrev) }\n"
                                "  .debug_line     0 : { *(.debug_line) }\n"
                                "  .debug_frame    0 : { *(.debug_frame) }\n"
                                "  .debug_str      0 : { *(.debug_str) }\n"
                                "  .debug_loc      0 : { *(.debug_loc) }\n"
                                "  .debug_macinfo  0 : { *(.debug_macinfo) }\n"
                                "  /* SGI/MIPS DWARF 2 extensions */\n"
                                "  .debug_weaknames 0 : { *(.debug_weaknames) }\n"
                                "  .debug_funcnames 0 : { *(.debug_funcnames) }\n"
                                "  .debug_typenames 0 : { *(.debug_typenames) }\n"
                                "  .debug_varnames  0 : { *(.debug_varnames) }\n"
                                "  /* DWARF 3 */\n"
                                "  .debug_pubtypes 0 : { *(.debug_pubtypes) }\n"
                                "  .debug_ranges   0 : { *(.debug_ranges) }\n"
                                "  /* DWARF Extension.  */\n"
                                "  .debug_macro    0 : { *(.debug_macro) }\n"
                                "  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }\n"
                                "  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }\n"
                                "}\n";
std::string phdrs_front =       "  PHDRS\n"
                                "  {\n"
                                "    headers PT_PHDR PHDRS ;\n"
                                "    interp PT_INTERP ; \n"
                                "    text PT_LOAD FILEHDR PHDRS ; \n";
std::string phdrs_data =        "    data PT_LOAD ;\n";
std::string phdrs_back =        "    dynamic PT_DYNAMIC ; \n"
                                "  }\n";
            std::ofstream script;
            script.open("ps_link_script.ld");
            script << begin_script;
            script << text_section;
            for (unsigned int i=0; i< NUM_OF_LEVELS; ++i) {
                if ( i == 0)
                    script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF(.text);\n";
                else
                    script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF(.fun_ps_"<< i-1<<");\n";
                script << "  .fun_ps_" << i << " :\n";
                script << "  {\n";
                script << "    *(fun_ps_"<< i << ")\n";
                script << "  } : fun_ps_"<< i <<"\n";
            }
            script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF (.fun_ps_"<< NUM_OF_LEVELS -1 <<");\n";
            script << before_data;
            for (unsigned int i=0; i< NUM_OF_LEVELS; ++i) {
                if ( i == 0)
                    script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF(.data);\n";
                else
                    script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF(.dat_ps_"<< i-1 <<");\n";
                script << "  .dat_ps_" << i << " :\n";
                script << "  {\n";
                script << "    *(dat_ps_"<< i << ")\n";
                script << "  } : dat_ps_"<< i <<"\n";
            }
            script << "  . = . + CONSTANT (COMMONPAGESIZE) - SIZEOF(.dat_ps_"<< NUM_OF_LEVELS -1 <<");\n";
            script << after_data;
            script << phdrs_front;
            for (unsigned int i=0; i< NUM_OF_LEVELS; ++i) {
                script << "    fun_ps_"<<i<<" PT_LOAD ;\n";
            }
            script << phdrs_data;
            for (unsigned int i=0; i< NUM_OF_LEVELS; ++i) {
                script << "    dat_ps_"<<i<<" PT_LOAD ;\n";
            }
            script << phdrs_back;
            script.close();
            return;
        }
    };
}
char PrivilegeSeparationOnModule::ID = 0;
static RegisterPass<PrivilegeSeparationOnModule> X("PrivilegeSeparationOnModule", "PrivilegeSeparation On Module", false, false);


