/*  reducer_opor.h                  -*- C++ -*-
 *
 *  @copyright
 *  Copyright (C) 2009-2013
 *  Intel Corporation
 *  
 *  @copyright
 *  This file is part of the Intel Cilk Plus Library.  This library is free
 *  software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *  
 *  @copyright
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  @copyright
 *  Under Section 7 of GPL version 3, you are granted additional
 *  permissions described in the GCC Runtime Library Exception, version
 *  3.1, as published by the Free Software Foundation.
 *  
 *  @copyright
 *  You should have received a copy of the GNU General Public License and
 *  a copy of the GCC Runtime Library Exception along with this program;
 *  see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 *  <http://www.gnu.org/licenses/>.
 */

/** @file reducer_opor.h
 *
 *  @brief Defines classes for doing parallel bitwise or reductions.
 *
 *  @ingroup ReducersOr
 *
 *  @see ReducersOr
 */

#ifndef REDUCER_OPOR_H_INCLUDED
#define REDUCER_OPOR_H_INCLUDED

#include <cilk/reducer.h>

/** @defgroup ReducersOr Bitwise Or Reducers
 *
 *  Bitwise and reducers allow the computation of the bitwise and of a set of
 *  values in parallel.
 *
 *  @ingroup Reducers
 *
 *  You should be familiar with @ref pagereducers "Cilk reducers", described in
 *  file `reducers.md`, and particularly with @ref reducers_using, before trying
 *  to use the information in this file.
 *
 *  @section redopor_usage Usage Example
 *
 *      cilk::reducer< cilk::op_or<unsigned> > r;
 *      cilk_for (int i = 0; i != N; ++i) {
 *          *r |= a[i];
 *      }
 *      unsigned result;
 *      r.move_out(result);
 *
 *  @section redopor_monoid The Monoid
 *
 *  @subsection redopor_monoid_values Value Set
 *
 *  The value set of a bitwise or reducer is the set of values of `Type`, which
 *  is expected to be a builtin integer type which has a representation as a
 *  sequence of bits (or something like it, such as `bool` or `std::bitset`).
 *
 *  @subsection redopor_monoid_operator Operator
 *
 *  The operator of a bitwise or reducer is the bitwise or operator, defined by
 *  the “`|`” binary operator on `Type`.
 *
 *  @subsection redopor_monoid_identity Identity
 *
 *  The identity value of the reducer is the value whose representation 
 *  contains all 0-bits. This is expected to be the value of the default
 *  constructor `Type()`.
 *
 *  @section redopor_operations Operations
 *
 *  @subsection redopor_constructors Constructors
 *
 *      reducer()   // identity
 *      reducer(const Type& value)
 *      reducer(move_in(Type& variable))
 *
 *  @subsection redopor_get_set Set and Get
 *
 *      r.set_value(const Type& value)
 *      const Type& = r.get_value() const
 *      r.move_in(Type& variable)
 *      r.move_out(Type& variable)
 *
 *  @subsection redopor_initial Initial Values
 *
 *  If a bitwise or reducer is constructed without an explicit initial value, 
 *  then its initial value will be its identity value, as long as `Type` 
 *  satisfies the requirements of @ref redopor_types.
 *
 *  @subsection redopor_view_ops View Operations
 *
 *      *r |= a
 *      *r = *r | a
 *      *r = *r | a1 | a2 … | an
 *
 *  @section redopor_types Type and Operator Requirements
 *
 *  `Type` must be `Copy Constructible`, `Default Constructible`, and
 *  `Assignable`.
 *
 *  The operator “`|=`” must be defined on `Type`, with `x |= a` having the 
 *  same meaning as `x = x | a`.
 *
 *  The expression `Type()` must be a valid expression which yields the
 *  identity value (the value of `Type` whose representation consists of all
 *  0-bits).
 *
 *  @section redopor_in_c Bitwise Or Reducers in C
 *
 *  The @ref CILK_C_REDUCER_OPOR and @ref CILK_C_REDUCER_OPOR_TYPE macros can
 *  be used to do bitwise or reductions in C. For example:
 *
 *      CILK_C_REDUCER_OPOR(r, uint, 0);
 *      CILK_C_REGISTER_REDUCER(r);
 *      cilk_for(int i = 0; i != n; ++i) {
 *          REDUCER_VIEW(r) |= a[i];
 *      }
 *      CILK_C_UNREGISTER_REDUCER(r);
 *      printf("The bitwise OR of the elements of a is %x\n", REDUCER_VIEW(r));
 *
 *  See @ref reducers_c_predefined.
 */

#ifdef __cplusplus

namespace cilk {

/** The bitwise or reducer view class.
 *
 *  This is the view class for reducers created with 
 *  `cilk::reducer< cilk::op_or<Type> >`. It holds the accumulator variable for
 *  the reduction, and allows only `or` operations to be performed on it.
 *
 *  @note   The reducer “dereference” operation (`reducer::operator *()`) 
 *          yields a reference to the view. Thus, for example, the view class’s
 *          `|=` operation would be used in an expression like `*r |= a`, where
 *          `r` is an opmod reducer variable.
 *
 *  @tparam Type    The type of the contained accumulator variable. This will
 *                  be the value type of a monoid_with_view that is
 *                  instantiated with this view.
 *
 *  @see ReducersOr
 *  @see op_or
 *
 *  @ingroup ReducersOr
 */
template <typename Type>
class op_or_view : public scalar_view<Type>
{
    typedef scalar_view<Type> base;
    
public:
    /** Class to represent the right-hand side of `*reducer = *reducer | value`.
     *
     *  The only assignment operator for the op_or_view class takes an 
     *  rhs_proxy as its operand. This results in the syntactic restriction
     *  that the only expressions that can be assigned to an op_or_view are
     *  ones which generate an rhs_proxy — that is, expressions of the form
     *  `op_or_view | value ... | value`.
     *
     *  @warning
     *  The lhs and rhs views in such an assignment must be the same; 
     *  otherwise, the behavior will be undefined. (I.e., `v1 = v1 | x` is
     *  legal; `v1 = v2 | x` is illegal.) This condition will be checked with
     *  a runtime assertion when compiled in debug mode.
     *
     *  @see op_or_view
     */
    class rhs_proxy {
        friend class op_or_view;

        const op_or_view* m_view;
        Type              m_value;

        // Constructor is invoked only from op_or_view::operator|().
        //
        rhs_proxy(const op_or_view* view, const Type& value) : m_view(view), m_value(value) {}

        rhs_proxy& operator=(const rhs_proxy&); // Disable assignment operator
        rhs_proxy();                            // Disable default constructor

    public:
        /** Bitwise or with an additional rhs value. If `v` is an op_or_view
         *  and `a1` is a value, then the expression `v | a1` invokes the 
         *  view’s `operator|()` to create an rhs_proxy for `(v, a1)`; then 
         *  `v | a1 | a2` invokes the rhs_proxy’s `operator|()` to create a new
         *  rhs_proxy for `(v, a1|a2)`. This allows the right-hand side of an
         *  assignment to be not just `view | value`, but 
         (  `view | value | value ... | value`. The effect is that
         *
         *      v = v | a1 | a2 ... | an;
         *
         *  is evaluated as
         *
         *      v = v | (a1 | a2 ... | an);
         */
        rhs_proxy& operator|(const Type& x) { m_value |= x; return *this; }
    };


    /** Default/identity constructor. This constructor initializes the
     *  contained value to `Type()`.
     */
    op_or_view() : base() {}

    /** Construct with a specified initial value.
     */
    explicit op_or_view(const Type& v) : base(v) {}
    
    /** Reduction operation.
     *
     *  This function is invoked by the @ref op_or monoid to combine the views
     *  of two strands when the right strand merges with the left one. It
     *  “ors” the value contained in the left-strand view by the value
     *  contained in the right-strand view, and leaves the value in the
     *  right-strand view undefined.
     *
     *  @param  right   A pointer to the right-strand view. (`this` points to
     *                  the left-strand view.)
     *
     *  @note   Used only by the @ref op_or monoid to implement the monoid
     *          reduce operation.
     */
    void reduce(op_or_view* right) { this->m_value |= right->m_value; }
    
    /** @name Accumulator variable updates.
     *
     *  These functions support the various syntaxes for “oring” the
     *  accumulator variable contained in the view with some value.
     */
    //@{

    /** Or the accumulator variable with @a x.
     */
    op_or_view& operator|=(const Type& x) { this->m_value |= x; return *this; }

    /** Create an object representing `*this | x`.
     *
     *  @see rhs_proxy
     */
    rhs_proxy operator|(const Type& x) const { return rhs_proxy(this, x); }

    /** Assign the result of a `view | value` expression to the view. Note that
     *  this is the only assignment operator for this class.
     *
     *  @see rhs_proxy
     */
    op_or_view& operator=(const rhs_proxy& rhs) {
        __CILKRTS_ASSERT(this == rhs.m_view);
        this->m_value |= rhs.m_value;
        return *this;
    }
    
    //@}
};

/** Monoid class for bitwise or reductions. Instantiate the cilk::reducer 
 *  template class with an op_or monoid to create a bitwise or reducer
 *  class. For example, to compute the bitwise or of a set of `unsigned long`
 *  values:
 *
 *      cilk::reducer< cilk::op_or<unsigned long> > r;
 *
 *  @tparam Type    The reducer value type.
 *  @tparam Align   If `false` (the default), reducers instantiated on this
 *                  monoid will be naturally aligned (the Cilk library 1.0
 *                  behavior). If `true`, reducers instantiated on this monoid
 *                  will be cache-aligned for binary compatibility with 
 *                  reducers in Cilk library version 0.9.
 *
 *  @see ReducersOr
 *  @see op_or_view
 *
 *  @ingroup ReducersOr
 */
template <typename Type, bool Align = false>
struct op_or : public monoid_with_view<op_or_view<Type>, Align> {};

/** Deprecated bitwise or reducer class.
 *
 *  reducer_opor is the same as @ref reducer<@ref op_or>, except that
 *  reducer_opor is a proxy for the contained view, so that accumulator
 *  variable update operations can be applied directly to the reducer. For
 *  example, a value is ored with  a `reducer<%op_or>` with `*r |= a`, but a
 *  value can be ored with a `%reducer_opor` with `r |= a`.
 *
 *  @deprecated Users are strongly encouraged to use `reducer<monoid>`
 *              reducers rather than the old wrappers like reducer_opor. 
 *              The `reducer<monoid>` reducers show the reducer/monoid/view
 *              architecture more clearly, are more consistent in their
 *              implementation, and present a simpler model for new
 *              user-implemented reducers.
 *
 *  @note   Implicit conversions are provided between `%reducer_opor` 
 *          and `reducer<%op_or>`. This allows incremental code
 *          conversion: old code that used `%reducer_opor` can pass a
 *          `%reducer_opor` to a converted function that now expects a
 *          pointer or reference to a `reducer<%op_or>`, and vice
 *          versa.
 *
 *  @tparam Type    The value type of the reducer.
 *
 *  @see op_or
 *  @see reducer
 *  @see ReducersOr
 *
 *  @ingroup ReducersOr
 */
template <typename Type>
class reducer_opor : public reducer< op_or<Type, true> >
{
    typedef reducer< op_or<Type, true> > base;
    using base::view;

  public:
    /// The view type for the reducer.
    typedef typename base::view_type        view_type;
    
    /// The view’s rhs proxy type.
    typedef typename view_type::rhs_proxy   rhs_proxy;
    
    /// The view type for the reducer.
    typedef view_type                       View;

    /// The monoid type for the reducer.
    typedef typename base::monoid_type      Monoid;
    
    /** @name Constructors
     */
    //@{
    
    /** Default (identity) constructor.
     *
     * Constructs the wrapper with the default initial value of `Type()`.
     */
    reducer_opor() {}

    /** Value constructor.
     *
     *  Constructs the wrapper with a specified initial value.
     */
    explicit reducer_opor(const Type& initial_value) : base(initial_value) {}
    
    //@}

    /** @name Forwarded functions
     *  @details Functions that update the contained accumulator variable are
     *  simply forwarded to the contained @ref op_and_view. */
    //@{

    /// @copydoc op_or_view::operator|=(const Type&)
    reducer_opor& operator|=(const Type& x)
    {
        view() |= x; return *this; 
    }
    
    // The legacy definition of reducer_opor::operator|() has different
    // behavior and a different return type than this definition. The legacy
    // version is defined as a member function, so this new version is defined
    // as a free function to give it a different signature, so that they won’t 
    // end up sharing a single object file entry.

    /// @copydoc op_or_view::operator|(const Type&) const
    friend rhs_proxy operator|(const reducer_opor& r, const Type& x)
    { 
        return r.view() | x; 
    }

    /// @copydoc op_and_view::operator=(const rhs_proxy&)
    reducer_opor& operator=(const rhs_proxy& temp)
    {
        view() = temp; return *this; 
    }
    //@}

    /** @name Dereference
     *  @details Dereferencing a wrapper is a no-op. It simply returns the
     *  wrapper. Combined with the rule that the wrapper forwards view
     *  operations to its contained view, this means that view operations can
     *  be written the same way on reducers and wrappers, which is convenient
     *  for incrementally converting old code using wrappers to use reducers
     *  instead. That is:
     *
     *      reducer< op_and<int> > r;
     *      *r &= a;    // *r returns the view
     *                  // operator &= is a view member function
     *
     *      reducer_opand<int> w;
     *      *w &= a;    // *w returns the wrapper
     *                  // operator &= is a wrapper member function that
     *                  // calls the corresponding view function
     */
    //@{
    reducer_opor&       operator*()       { return *this; }
    reducer_opor const& operator*() const { return *this; }

    reducer_opor*       operator->()       { return this; }
    reducer_opor const* operator->() const { return this; }
    //@}
    
    /** @name Upcast
     *  @details In Cilk library 0.9, reducers were always cache-aligned. In
     *  library  1.0, reducer cache alignment is optional. By default, reducers
     *  are unaligned (i.e., just naturally aligned), but legacy wrappers
     *  inherit from cache-aligned reducers for binary compatibility.
     *
     *  This means that a wrapper will automatically be upcast to its aligned
     *  reducer base class. The following conversion operators provide
     *  pseudo-upcasts to the corresponding unaligned reducer class.
     */
    //@{
    operator reducer< op_or<Type, false> >& ()
    {
        return *reinterpret_cast< reducer< op_or<Type, false> >* >(this);
    }
    operator const reducer< op_or<Type, false> >& () const
    {
        return *reinterpret_cast< const reducer< op_or<Type, false> >* >(this);
    }
    //@}
    
};

/// @cond internal
/** Metafunction specialization for reducer conversion.
 *
 *  This specialization of the @ref legacy_reducer_downcast template class 
 *  defined in reducer.h causes the `reducer< op_or<Type> >` class to have an 
 *  `operator reducer_opor<Type>& ()` conversion operator that statically
 *  downcasts the `reducer<op_or>` to the corresponding `reducer_opor` type.
 *  (The reverse conversion, from `reducer_opor` to `reducer<op_or>`, is just
 *  an upcast, which is provided for free by the language.)
 *
 *  @ingroup ReducersOr
 */
template <typename Type, bool Align>
struct legacy_reducer_downcast<reducer<op_or<Type, Align> > >
{
    typedef reducer_opor<Type> type;
};
/// @endcond

} // namespace cilk

#endif /* __cplusplus */


/** @ingroup ReducersOr
 */
//@{

/** @name C language reducer macros
 *
 *  These macros are used to declare and work with op_or reducers in C code.
 *
 *  @see @ref page_reducers_in_c
 */
 //@{
 
__CILKRTS_BEGIN_EXTERN_C

/** Opor reducer type name.
 *
 *  This macro expands into the identifier which is the name of the op_or
 *  reducer type for a specified numeric type.
 *
 *  @param  tn  The @ref reducers_c_type_names "numeric type name" specifying
 *              the type of the reducer.
 *
 *  @see @ref reducers_c_predefined
 *  @see ReducersOr
 */
#define CILK_C_REDUCER_OPOR_TYPE(tn)                                         \
    __CILKRTS_MKIDENT(cilk_c_reducer_opor_,tn)

/** Declare an op_or reducer object.
 *
 *  This macro expands into a declaration of an op_or reducer object for a
 *  specified numeric type. For example:
 *
 *      CILK_C_REDUCER_OPOR(my_reducer, ulong, 0);
 *
 *  @param  obj The variable name to be used for the declared reducer object.
 *  @param  tn  The @ref reducers_c_type_names "numeric type name" specifying
 *              the type of the reducer.
 *  @param  v   The initial value for the reducer. (A value which can be
 *              assigned to the numeric type represented by @a tn.)
 *
 *  @see @ref reducers_c_predefined
 *  @see ReducersOr
 */
#define CILK_C_REDUCER_OPOR(obj,tn,v)                                        \
    CILK_C_REDUCER_OPOR_TYPE(tn) obj =                                       \
        CILK_C_INIT_REDUCER(_Typeof(obj.value),                               \
                        __CILKRTS_MKIDENT(cilk_c_reducer_opor_reduce_,tn),   \
                        __CILKRTS_MKIDENT(cilk_c_reducer_opor_identity_,tn), \
                        __cilkrts_hyperobject_noop_destroy, v)

/// @cond internal

/** Declare the op_or reducer functions for a numeric type.
 *
 *  This macro expands into external function declarations for functions which
 *  implement the reducer functionality for the op_or reducer type for a
 *  specified numeric type.
 *
 *  @param  t   The value type of the reducer.
 *  @param  tn  The value “type name” identifier, used to construct the reducer
 *              type name, function names, etc.
 */
#define CILK_C_REDUCER_OPOR_DECLARATION(t,tn)                             \
    typedef CILK_C_DECLARE_REDUCER(t) CILK_C_REDUCER_OPOR_TYPE(tn);       \
    __CILKRTS_DECLARE_REDUCER_REDUCE(cilk_c_reducer_opor,tn,l,r);         \
    __CILKRTS_DECLARE_REDUCER_IDENTITY(cilk_c_reducer_opor,tn);
 
/** Define the op_or reducer functions for a numeric type.
 *
 *  This macro expands into function definitions for functions which implement
 *  the reducer functionality for the op_or reducer type for a specified 
 *  numeric type.
 *
 *  @param  t   The value type of the reducer.
 *  @param  tn  The value “type name” identifier, used to construct the reducer
 *              type name, function names, etc.
 */
#define CILK_C_REDUCER_OPOR_DEFINITION(t,tn)                              \
    typedef CILK_C_DECLARE_REDUCER(t) CILK_C_REDUCER_OPOR_TYPE(tn);       \
    __CILKRTS_DECLARE_REDUCER_REDUCE(cilk_c_reducer_opor,tn,l,r)          \
        { *(t*)l |= *(t*)r; }                                              \
    __CILKRTS_DECLARE_REDUCER_IDENTITY(cilk_c_reducer_opor,tn)            \
        { *(t*)v = 0; }
 
//@{
/** @def CILK_C_REDUCER_OPOR_INSTANCE 
 *  @brief Declare or define implementation functions for a reducer type.
 *
 *  In the runtime source file c_reducers.c, the macro `CILK_C_DEFINE_REDUCERS`
 *  will be defined, and this macro will generate reducer implementation
 *  functions. Everywhere else, `CILK_C_DEFINE_REDUCERS` will be undefined, and
 *  this macro will expand into external declarations for the functions.
 */
#ifdef CILK_C_DEFINE_REDUCERS
#   define CILK_C_REDUCER_OPOR_INSTANCE(t,tn)  \
        CILK_C_REDUCER_OPOR_DEFINITION(t,tn)
#else
#   define CILK_C_REDUCER_OPOR_INSTANCE(t,tn)  \
        CILK_C_REDUCER_OPOR_DECLARATION(t,tn)
#endif
//@}

/*  Declare or define an instance of the reducer type and its functions for each 
 *  numeric type.
 */
CILK_C_REDUCER_OPOR_INSTANCE(char,                 char)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned char,        uchar)
CILK_C_REDUCER_OPOR_INSTANCE(signed char,          schar)
CILK_C_REDUCER_OPOR_INSTANCE(wchar_t,              wchar_t)
CILK_C_REDUCER_OPOR_INSTANCE(short,                short)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned short,       ushort)
CILK_C_REDUCER_OPOR_INSTANCE(int,                  int)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned int,         uint)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned int,         unsigned) /* alternate name */
CILK_C_REDUCER_OPOR_INSTANCE(long,                 long)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned long,        ulong)
CILK_C_REDUCER_OPOR_INSTANCE(long long,            longlong)
CILK_C_REDUCER_OPOR_INSTANCE(unsigned long long,   ulonglong)

//@endcond

__CILKRTS_END_EXTERN_C

//@}

//@}

#endif /*  REDUCER_OPOR_H_INCLUDED */
