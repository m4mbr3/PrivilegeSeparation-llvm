//===-- llvm/GlobalValue.h - Class to represent a global value --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a common base class of all globally definable objects.  As such,
// it is subclassed by GlobalVariable, GlobalAlias and by Function.  This is
// used because you can do certain things with these global objects that you
// can't do to anything else.  For example, use the address of one as a
// constant.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_GLOBALVALUE_H
#define LLVM_IR_GLOBALVALUE_H

#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/PrivilegeSeparation.h"

namespace llvm {

class PointerType;
class Module;

class GlobalValue : public Constant {
  GlobalValue(const GlobalValue &) LLVM_DELETED_FUNCTION;
public:
  /// @brief An enumeration for the kinds of linkage for global values.
  enum LinkageTypes {
    ExternalLinkage = 0,///< Externally visible function
    AvailableExternallyLinkage, ///< Available for inspection, not emission.
    LinkOnceAnyLinkage, ///< Keep one copy of function when linking (inline)
    LinkOnceODRLinkage, ///< Same, but only replaced by something equivalent.
    WeakAnyLinkage,     ///< Keep one copy of named function when linking (weak)
    WeakODRLinkage,     ///< Same, but only replaced by something equivalent.
    AppendingLinkage,   ///< Special purpose, only applies to global arrays
    InternalLinkage,    ///< Rename collisions when linking (static functions).
    PrivateLinkage,     ///< Like Internal, but omit from symbol table.
    LinkerPrivateLinkage, ///< Like Private, but linker removes.
    LinkerPrivateWeakLinkage, ///< Like LinkerPrivate, but weak.
    ExternalWeakLinkage,///< ExternalWeak linkage description.
    CommonLinkage       ///< Tentative definitions.
  };

  /// @brief An enumeration for the kinds of visibility of global values.
  enum VisibilityTypes {
    DefaultVisibility = 0,  ///< The GV is visible
    HiddenVisibility,       ///< The GV is hidden
    ProtectedVisibility     ///< The GV is protected
  };

  /// @brief Storage classes of global values for PE targets.
  enum DLLStorageClassTypes {
    DefaultStorageClass   = 0,
    DLLImportStorageClass = 1, ///< Function to be imported from DLL
    DLLExportStorageClass = 2  ///< Function to be accessible from DLL.
  };

protected:
  GlobalValue(Type *ty, ValueTy vty, Use *Ops, unsigned NumOps,
              LinkageTypes linkage, const Twine &Name)
    : Constant(ty, vty, Ops, NumOps), Linkage(linkage),
      Visibility(DefaultVisibility), Alignment(0), UnnamedAddr(0),
      DllStorageClass(DefaultStorageClass), Parent(0) {
    setName(Name);
    privilegeSeparation = NUM_OF_LEVELS - 1;
  }

  // Note: VC++ treats enums as signed, so an extra bit is required to prevent
  // Linkage and Visibility from turning into negative values.
  LinkageTypes Linkage : 5;   // The linkage of this global
  unsigned Visibility : 2;    // The visibility style of this global
  unsigned Alignment : 16;    // Alignment of this symbol, must be power of two
  unsigned UnnamedAddr : 1;   // This value's address is not significant
  unsigned DllStorageClass : 2; // DLL storage class
  Module *Parent;             // The containing module.
  std::string Section;        // Section to emit this into, empty mean default
  unsigned int privilegeSeparation;

public:
  ~GlobalValue() {
    removeDeadConstantUsers();   // remove any dead constants using this.
  }

  ///Getter and Setters for the privilegeSeparation level
  void setPrivilegeSeparation(unsigned int pS) {
      privilegeSeparation = pS;
  }

  unsigned int getPrivilegeSeparation(void) {
      return privilegeSeparation;
  }
  unsigned int getPrivilegeSeparation(void) const {
    return privilegeSeparation;
  }


  unsigned getAlignment() const {
    return (1u << Alignment) >> 1;
  }
  void setAlignment(unsigned Align);

  bool hasUnnamedAddr() const { return UnnamedAddr; }
  void setUnnamedAddr(bool Val) { UnnamedAddr = Val; }

  VisibilityTypes getVisibility() const { return VisibilityTypes(Visibility); }
  bool hasDefaultVisibility() const { return Visibility == DefaultVisibility; }
  bool hasHiddenVisibility() const { return Visibility == HiddenVisibility; }
  bool hasProtectedVisibility() const {
    return Visibility == ProtectedVisibility;
  }
  void setVisibility(VisibilityTypes V) { Visibility = V; }

  DLLStorageClassTypes getDLLStorageClass() const {
    return DLLStorageClassTypes(DllStorageClass);
  }
  bool hasDLLImportStorageClass() const {
    return DllStorageClass == DLLImportStorageClass;
  }
  bool hasDLLExportStorageClass() const {
    return DllStorageClass == DLLExportStorageClass;
  }
  void setDLLStorageClass(DLLStorageClassTypes C) { DllStorageClass = C; }

  bool hasSection() const { return !Section.empty(); }
  const std::string &getSection() const { return Section; }
  void setSection(StringRef S) { Section = S; }

  /// If the usage is empty (except transitively dead constants), then this
  /// global value can be safely deleted since the destructor will
  /// delete the dead constants as well.
  /// @brief Determine if the usage of this global value is empty except
  /// for transitively dead constants.
  bool use_empty_except_constants();

  /// getType - Global values are always pointers.
  inline PointerType *getType() const {
    return cast<PointerType>(User::getType());
  }

  static LinkageTypes getLinkOnceLinkage(bool ODR) {
    return ODR ? LinkOnceODRLinkage : LinkOnceAnyLinkage;
  }
  static LinkageTypes getWeakLinkage(bool ODR) {
    return ODR ? WeakODRLinkage : WeakAnyLinkage;
  }

  static bool isExternalLinkage(LinkageTypes Linkage) {
    return Linkage == ExternalLinkage;
  }
  static bool isAvailableExternallyLinkage(LinkageTypes Linkage) {
    return Linkage == AvailableExternallyLinkage;
  }
  static bool isLinkOnceLinkage(LinkageTypes Linkage) {
    return Linkage == LinkOnceAnyLinkage || Linkage == LinkOnceODRLinkage;
  }
  static bool isWeakLinkage(LinkageTypes Linkage) {
    return Linkage == WeakAnyLinkage || Linkage == WeakODRLinkage;
  }
  static bool isAppendingLinkage(LinkageTypes Linkage) {
    return Linkage == AppendingLinkage;
  }
  static bool isInternalLinkage(LinkageTypes Linkage) {
    return Linkage == InternalLinkage;
  }
  static bool isPrivateLinkage(LinkageTypes Linkage) {
    return Linkage == PrivateLinkage;
  }
  static bool isLinkerPrivateLinkage(LinkageTypes Linkage) {
    return Linkage == LinkerPrivateLinkage;
  }
  static bool isLinkerPrivateWeakLinkage(LinkageTypes Linkage) {
    return Linkage == LinkerPrivateWeakLinkage;
  }
  static bool isLocalLinkage(LinkageTypes Linkage) {
    return isInternalLinkage(Linkage) || isPrivateLinkage(Linkage) ||
      isLinkerPrivateLinkage(Linkage) || isLinkerPrivateWeakLinkage(Linkage);
  }
  static bool isExternalWeakLinkage(LinkageTypes Linkage) {
    return Linkage == ExternalWeakLinkage;
  }
  static bool isCommonLinkage(LinkageTypes Linkage) {
    return Linkage == CommonLinkage;
  }

  /// isDiscardableIfUnused - Whether the definition of this global may be
  /// discarded if it is not used in its compilation unit.
  static bool isDiscardableIfUnused(LinkageTypes Linkage) {
    return isLinkOnceLinkage(Linkage) || isLocalLinkage(Linkage);
  }

  /// mayBeOverridden - Whether the definition of this global may be replaced
  /// by something non-equivalent at link time.  For example, if a function has
  /// weak linkage then the code defining it may be replaced by different code.
  static bool mayBeOverridden(LinkageTypes Linkage) {
    return Linkage == WeakAnyLinkage ||
           Linkage == LinkOnceAnyLinkage ||
           Linkage == CommonLinkage ||
           Linkage == ExternalWeakLinkage ||
           Linkage == LinkerPrivateWeakLinkage;
  }

  /// isWeakForLinker - Whether the definition of this global may be replaced at
  /// link time.  NB: Using this method outside of the code generators is almost
  /// always a mistake: when working at the IR level use mayBeOverridden instead
  /// as it knows about ODR semantics.
  static bool isWeakForLinker(LinkageTypes Linkage)  {
    return Linkage == AvailableExternallyLinkage ||
           Linkage == WeakAnyLinkage ||
           Linkage == WeakODRLinkage ||
           Linkage == LinkOnceAnyLinkage ||
           Linkage == LinkOnceODRLinkage ||
           Linkage == CommonLinkage ||
           Linkage == ExternalWeakLinkage ||
           Linkage == LinkerPrivateWeakLinkage;
  }

  bool hasExternalLinkage() const { return isExternalLinkage(Linkage); }
  bool hasAvailableExternallyLinkage() const {
    return isAvailableExternallyLinkage(Linkage);
  }
  bool hasLinkOnceLinkage() const {
    return isLinkOnceLinkage(Linkage);
  }
  bool hasWeakLinkage() const {
    return isWeakLinkage(Linkage);
  }
  bool hasAppendingLinkage() const { return isAppendingLinkage(Linkage); }
  bool hasInternalLinkage() const { return isInternalLinkage(Linkage); }
  bool hasPrivateLinkage() const { return isPrivateLinkage(Linkage); }
  bool hasLinkerPrivateLinkage() const { return isLinkerPrivateLinkage(Linkage); }
  bool hasLinkerPrivateWeakLinkage() const {
    return isLinkerPrivateWeakLinkage(Linkage);
  }
  bool hasLocalLinkage() const { return isLocalLinkage(Linkage); }
  bool hasExternalWeakLinkage() const { return isExternalWeakLinkage(Linkage); }
  bool hasCommonLinkage() const { return isCommonLinkage(Linkage); }

  void setLinkage(LinkageTypes LT) { Linkage = LT; }
  LinkageTypes getLinkage() const { return Linkage; }

  bool isDiscardableIfUnused() const {
    return isDiscardableIfUnused(Linkage);
  }

  bool mayBeOverridden() const { return mayBeOverridden(Linkage); }

  bool isWeakForLinker() const { return isWeakForLinker(Linkage); }

  /// copyAttributesFrom - copy all additional attributes (those not needed to
  /// create a GlobalValue) from the GlobalValue Src to this one.
  virtual void copyAttributesFrom(const GlobalValue *Src);

  /// getRealLinkageName - If special LLVM prefix that is used to inform the asm
  /// printer to not emit usual symbol prefix before the symbol name is used
  /// then return linkage name after skipping this special LLVM prefix.
  static StringRef getRealLinkageName(StringRef Name) {
    if (!Name.empty() && Name[0] == '\1')
      return Name.substr(1);
    return Name;
  }

/// @name Materialization
/// Materialization is used to construct functions only as they're needed. This
/// is useful to reduce memory usage in LLVM or parsing work done by the
/// BitcodeReader to load the Module.
/// @{

  /// isMaterializable - If this function's Module is being lazily streamed in
  /// functions from disk or some other source, this method can be used to check
  /// to see if the function has been read in yet or not.
  bool isMaterializable() const;

  /// isDematerializable - Returns true if this function was loaded from a
  /// GVMaterializer that's still attached to its Module and that knows how to
  /// dematerialize the function.
  bool isDematerializable() const;

  /// Materialize - make sure this GlobalValue is fully read.  If the module is
  /// corrupt, this returns true and fills in the optional string with
  /// information about the problem.  If successful, this returns false.
  bool Materialize(std::string *ErrInfo = 0);

  /// Dematerialize - If this GlobalValue is read in, and if the GVMaterializer
  /// supports it, release the memory for the function, and set it up to be
  /// materialized lazily.  If !isDematerializable(), this method is a noop.
  void Dematerialize();

/// @}

  /// Override from Constant class.
  virtual void destroyConstant();

  /// isDeclaration - Return true if the primary definition of this global
  /// value is outside of the current translation unit.
  bool isDeclaration() const;

  /// removeFromParent - This method unlinks 'this' from the containing module,
  /// but does not delete it.
  virtual void removeFromParent() = 0;

  /// eraseFromParent - This method unlinks 'this' from the containing module
  /// and deletes it.
  virtual void eraseFromParent() = 0;

  /// getParent - Get the module that this global value is contained inside
  /// of...
  inline Module *getParent() { return Parent; }
  inline const Module *getParent() const { return Parent; }

  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const Value *V) {
    return V->getValueID() == Value::FunctionVal ||
           V->getValueID() == Value::GlobalVariableVal ||
           V->getValueID() == Value::GlobalAliasVal;
  }
};

} // End llvm namespace

#endif
