//===--- SILDeclRef.h - Defines the SILDeclRef struct ---------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the SILDeclRef struct, which is used to identify a SIL
// global identifier that can be used as the operand of a FunctionRefInst
// instruction or that can have a SIL Function associated with it.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SIL_SILDeclRef_H
#define SWIFT_SIL_SILDeclRef_H

#include "swift/AST/Decl.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/Support/PrettyStackTrace.h"

namespace llvm {
  class raw_ostream;
}

namespace swift {
  class ValueDecl;
  class PipeClosureExpr;
  class ImplicitClosureExpr;
  class ASTContext;
  class ClassDecl;

/// SILDeclRef - A key for referencing a Swift declaration in SIL.
/// This can currently be either a reference to a ValueDecl for functions,
/// methods, constructors, and other named entities, or a reference to a
/// CapturingExpr (that is, a FuncExpr or ClosureExpr) for an anonymous
/// function. In addition to the AST reference, there are discriminators for
/// referencing different implementation-level entities associated with a
/// single language-level declaration, such as uncurry levels of a function,
/// the allocating and initializing entry points of a constructor, the getter
/// and setter for a property, etc.
struct SILDeclRef {
  typedef llvm::PointerUnion3<ValueDecl *, PipeClosureExpr *,
                              ImplicitClosureExpr *> Loc;
  
  /// Represents the "kind" of the SILDeclRef. For some Swift decls there
  /// are multiple SIL entry points, and the kind is used to distinguish them.
  enum class Kind : unsigned {
    /// Func - this constant references the FuncDecl or CapturingExpr in loc
    /// directly.
    Func,
    
    /// Getter - this constant references the getter for the ValueDecl in loc.
    Getter,
    /// Setter - this constant references the setter for the ValueDecl in loc.
    Setter,
    
    /// Allocator - this constant references the allocating constructor
    /// entry point of a class ConstructorDecl or the constructor of a value
    /// ConstructorDecl.
    Allocator,
    /// Initializer - this constant references the initializing constructor
    /// entry point of the class ConstructorDecl in loc.
    Initializer,
    
    /// UnionElement - this constant references the injection function for
    /// a UnionElementDecl.
    UnionElement,
    
    /// Destroyer - this constant references the destroying destructor for the
    /// ClassDecl in loc.
    Destroyer,
    
    /// GlobalAccessor - this constant references the lazy-initializing
    /// accessor for the global VarDecl in loc.
    GlobalAccessor,

    /// References the generator for a default argument of a function.
    DefaultArgGenerator
  };
  
  /// The ValueDecl or CapturingExpr represented by this SILDeclRef.
  Loc loc;
  /// The Kind of this SILDeclRef.
  Kind kind : 4;
  /// The uncurry level of this SILDeclRef.
  unsigned uncurryLevel : 16;
  /// True if the SILDeclRef is a curry thunk.
  unsigned isCurried : 1;
  /// True if this references an ObjC-visible method.
  unsigned isObjC : 1;
  /// The default argument index for a default argument getter.
  unsigned defaultArgIndex : 10;
  
  /// A magic value for SILDeclRef constructors to ask for the natural uncurry
  /// level of the constant.
  enum : unsigned { ConstructAtNaturalUncurryLevel = ~0U };
  
  /// Produces a null SILDeclRef.
  SILDeclRef() : loc(), kind(Kind::Func), uncurryLevel(0),
                 isCurried(0), isObjC(0),
                 defaultArgIndex(0) {}
  
  /// Produces a SILDeclRef of the given kind for the given decl.
  explicit SILDeclRef(ValueDecl *decl, Kind kind,
                       unsigned uncurryLevel = ConstructAtNaturalUncurryLevel,
                       bool isObjC = false);
  
  /// Produces the 'natural' SILDeclRef for the given ValueDecl or
  /// CapturingExpr:
  /// - If 'loc' is a func or closure, this returns a Func SILDeclRef.
  /// - If 'loc' is a getter or setter FuncDecl, this returns the Getter or
  ///   Setter SILDeclRef for the property VarDecl.
  /// - If 'loc' is a ConstructorDecl, this returns the Allocator SILDeclRef
  ///   for the constructor.
  /// - If 'loc' is a UnionElementDecl, this returns the UnionElement
  ///   SILDeclRef for the union element.
  /// - If 'loc' is a DestructorDecl, this returns the Destructor SILDeclRef
  ///   for the containing ClassDecl.
  /// - If 'loc' is a global VarDecl, this returns its GlobalAccessor
  ///   SILDeclRef.
  /// If the uncurry level is unspecified or specified as NaturalUncurryLevel,
  /// then the SILDeclRef for the natural uncurry level of the definition is
  /// used.
  explicit SILDeclRef(Loc loc,
                       unsigned uncurryLevel = ConstructAtNaturalUncurryLevel,
                       bool isObjC = false);

  /// Produce a SIL constant for a default argument generator.
  static SILDeclRef getDefaultArgGenerator(Loc loc, unsigned defaultArgIndex);

  bool isNull() const { return loc.isNull(); }
  
  bool hasDecl() const { return loc.is<ValueDecl *>(); }
  bool hasPipeClosureExpr() const {
    return loc.is<PipeClosureExpr *>();
  }
  bool hasImplicitClosureExpr() const {
    return loc.is<ImplicitClosureExpr *>();
  }

  ValueDecl *getDecl() const { return loc.get<ValueDecl *>(); }
  PipeClosureExpr *getPipeClosureExpr() const {
    return loc.dyn_cast<PipeClosureExpr *>();
  }
  ImplicitClosureExpr *getImplicitClosureExpr() const {
    return loc.dyn_cast<ImplicitClosureExpr *>();
  }

  /// True if the SILDeclRef references a function.
  bool isFunc() const {
    return kind == Kind::Func;
  }
  /// True if the SILDeclRef references a property accessor.
  bool isProperty() const {
    return kind == Kind::Getter || kind == Kind::Setter;
  }
  /// True if the SILDeclRef references a constructor entry point.
  bool isConstructor() const {
    return kind == Kind::Allocator || kind == Kind::Initializer;
  }
  /// True if the SILDeclRef references a union entry point.
  bool isUnionElement() const {
    return kind == Kind::UnionElement;
  }
  /// True if the SILDeclRef references a global variable accessor.
  bool isGlobal() const {
    return kind == Kind::GlobalAccessor;
  }
  /// True if the SILDeclRef references the generator for a default argument of
  /// a function.
  bool isDefaultArgGenerator() const {
    return kind == Kind::DefaultArgGenerator;
  }

  /// \brief True if the function should be treated as transparent.
  bool isTransparent() const {
    return ( hasDecl() &&
             getDecl()->getAttrs().isTransparent() ) ||
           isUnionElement();
  }

  bool operator==(SILDeclRef rhs) const {
    return loc.getOpaqueValue() == rhs.loc.getOpaqueValue()
      && kind == rhs.kind
      && uncurryLevel == rhs.uncurryLevel
      && isObjC == rhs.isObjC
      && defaultArgIndex == rhs.defaultArgIndex;
  }
  bool operator!=(SILDeclRef rhs) const {
    return loc.getOpaqueValue() != rhs.loc.getOpaqueValue()
      || kind != rhs.kind
      || uncurryLevel != rhs.uncurryLevel
      || isObjC != rhs.isObjC
      || defaultArgIndex != rhs.defaultArgIndex;
  }
  
  void print(llvm::raw_ostream &os) const;
  void dump() const;
  
  // Returns the SILDeclRef for an entity at a shallower uncurry level.
  SILDeclRef atUncurryLevel(unsigned level) const {
    assert(level <= uncurryLevel && "can't safely go to deeper uncurry level");
    bool willBeCurried = isCurried || level < uncurryLevel;
    return SILDeclRef(loc.getOpaqueValue(), kind, level,
                      willBeCurried, isObjC,
                      defaultArgIndex);
  }
  
  // Returns the ObjC (or native) entry point corresponding to the same
  // constant.
  SILDeclRef asObjC(bool objc = true) const {
    return SILDeclRef(loc.getOpaqueValue(), kind, uncurryLevel, isCurried, objc,
                       defaultArgIndex);
  }
  
  /// Produces a SILDeclRef from an opaque value.
  explicit SILDeclRef(void *opaqueLoc,
                       Kind kind,
                       unsigned uncurryLevel,
                       bool isCurried,
                       bool isObjC,
                       unsigned defaultArgIndex)
    : loc(Loc::getFromOpaqueValue(opaqueLoc)),
      kind(kind), uncurryLevel(uncurryLevel),
      isCurried(isCurried), isObjC(isObjC),
      defaultArgIndex(defaultArgIndex)
  {}
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, SILDeclRef C) {
  C.print(OS);
  return OS;
}

} // end swift namespace

namespace llvm {

// DenseMap key support for SILDeclRef.
template<> struct DenseMapInfo<swift::SILDeclRef> {
  using SILDeclRef = swift::SILDeclRef;
  using Kind = SILDeclRef::Kind;
  using Loc = SILDeclRef::Loc;
  using PointerInfo = DenseMapInfo<void*>;
  using UnsignedInfo = DenseMapInfo<unsigned>;

  static SILDeclRef getEmptyKey() {
    return SILDeclRef(PointerInfo::getEmptyKey(), Kind::Func, 0,
                      false, false, 0);
  }
  static SILDeclRef getTombstoneKey() {
    return SILDeclRef(PointerInfo::getTombstoneKey(), Kind::Func, 0,
                      false, false, 0);
  }
  static unsigned getHashValue(swift::SILDeclRef Val) {
    unsigned h1 = PointerInfo::getHashValue(Val.loc.getOpaqueValue());
    unsigned h2 = UnsignedInfo::getHashValue(unsigned(Val.kind));
    unsigned h3 = (Val.kind == Kind::DefaultArgGenerator)
                    ? UnsignedInfo::getHashValue(Val.defaultArgIndex)
                    : UnsignedInfo::getHashValue(Val.uncurryLevel);
    unsigned h4 = UnsignedInfo::getHashValue(Val.isObjC);
    return h1 ^ (h2 << 4) ^ (h3 << 9) ^ (h4 << 7);
  }
  static bool isEqual(swift::SILDeclRef const &LHS,
                      swift::SILDeclRef const &RHS) {
    return LHS == RHS;
  }
};

} // end llvm namespace

#endif
