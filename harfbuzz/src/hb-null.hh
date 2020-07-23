/*
 * Copyright © 2018  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef RB_NULL_HH
#define RB_NULL_HH

#include "hb.hh"
#include "hb-meta.hh"

/*
 * Static pools
 */

/* Global nul-content Null pool.  Enlarge as necessary. */

#define RB_NULL_POOL_SIZE 384

/* Use SFINAE to sniff whether T has min_size; in which case return T::null_size,
 * otherwise return sizeof(T). */

/* The hard way...
 * https://stackoverflow.com/questions/7776448/sfinae-tried-with-bool-gives-compiler-error-template-argument-tvalue-invol
 */

template <typename T, typename> struct _rb_null_size : rb_integral_constant<unsigned, sizeof(T)>
{
};
template <typename T>
struct _rb_null_size<T, rb_void_t<decltype(T::min_size)>> : rb_integral_constant<unsigned, T::null_size>
{
};

template <typename T> using rb_null_size = _rb_null_size<T, void>;
#define rb_null_size(T) rb_null_size<T>::value

/* These doesn't belong here, but since is copy/paste from above, put it here. */

/* rb_static_size (T)
 * Returns T::static_size if T::min_size is defined, or sizeof (T) otherwise. */

template <typename T, typename> struct _rb_static_size : rb_integral_constant<unsigned, sizeof(T)>
{
};
template <typename T>
struct _rb_static_size<T, rb_void_t<decltype(T::min_size)>> : rb_integral_constant<unsigned, T::static_size>
{
};
template <typename T> using rb_static_size = _rb_static_size<T, void>;
#define rb_static_size(T) rb_static_size<T>::value

/*
 * Null()
 */

extern RB_INTERNAL uint64_t const _rb_NullPool[(RB_NULL_POOL_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

/* Generic nul-content Null objects. */
template <typename Type> struct Null
{
    static Type const &get_null()
    {
        static_assert(rb_null_size(Type) <= RB_NULL_POOL_SIZE, "Increase RB_NULL_POOL_SIZE.");
        return *reinterpret_cast<Type const *>(_rb_NullPool);
    }
};
template <typename QType> struct NullHelper
{
    typedef rb_remove_const<rb_remove_reference<QType>> Type;
    static const Type &get_null()
    {
        return Null<Type>::get_null();
    }
};
#define Null(Type) NullHelper<Type>::get_null()

/* Specializations for arbitrary-content Null objects expressed in bytes. */
#define DECLARE_NULL_NAMESPACE_BYTES(Namespace, Type)                                                                  \
    } /* Close namespace. */                                                                                           \
    extern RB_INTERNAL const unsigned char _rb_Null_##Namespace##_##Type[Namespace::Type::null_size];                  \
    template <> struct Null<Namespace::Type>                                                                           \
    {                                                                                                                  \
        static Namespace::Type const &get_null()                                                                       \
        {                                                                                                              \
            return *reinterpret_cast<const Namespace::Type *>(_rb_Null_##Namespace##_##Type);                          \
        }                                                                                                              \
    };                                                                                                                 \
    namespace Namespace {                                                                                              \
    static_assert(true, "") /* Require semicolon after. */
#define DEFINE_NULL_NAMESPACE_BYTES(Namespace, Type)                                                                   \
    const unsigned char _rb_Null_##Namespace##_##Type[Namespace::Type::null_size]

/* Specializations for arbitrary-content Null objects expressed as struct initializer. */
#define DECLARE_NULL_INSTANCE(Type)                                                                                    \
    extern RB_INTERNAL const Type _rb_Null_##Type;                                                                     \
    template <> struct Null<Type>                                                                                      \
    {                                                                                                                  \
        static Type const &get_null()                                                                                  \
        {                                                                                                              \
            return _rb_Null_##Type;                                                                                    \
        }                                                                                                              \
    };                                                                                                                 \
    static_assert(true, "") /* Require semicolon after. */
#define DEFINE_NULL_INSTANCE(Type) const Type _rb_Null_##Type

/* Global writable pool.  Enlarge as necessary. */

/* To be fully correct, CrapPool must be thread_local. However, we do not rely on CrapPool
 * for correct operation. It only exist to catch and divert program logic bugs instead of
 * causing bad memory access. So, races there are not actually introducing incorrectness
 * in the code. Has ~12kb binary size overhead to have it, also clang build fails with it. */
extern RB_INTERNAL
    /*thread_local*/ uint64_t _rb_CrapPool[(RB_NULL_POOL_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

/* CRAP pool: Common Region for Access Protection. */
template <typename Type> static inline Type &Crap()
{
    static_assert(rb_null_size(Type) <= RB_NULL_POOL_SIZE, "Increase RB_NULL_POOL_SIZE.");
    Type *obj = reinterpret_cast<Type *>(_rb_CrapPool);
    memcpy(obj, &Null(Type), sizeof(*obj));
    return *obj;
}
template <typename QType> struct CrapHelper
{
    typedef rb_remove_const<rb_remove_reference<QType>> Type;
    static Type &get_crap()
    {
        return Crap<Type>();
    }
};
#define Crap(Type) CrapHelper<Type>::get_crap()

template <typename Type> struct CrapOrNullHelper
{
    static Type &get()
    {
        return Crap(Type);
    }
};
template <typename Type> struct CrapOrNullHelper<const Type>
{
    static const Type &get()
    {
        return Null(Type);
    }
};
#define CrapOrNull(Type) CrapOrNullHelper<Type>::get()

/*
 * rb_nonnull_ptr_t
 */

template <typename P> struct rb_nonnull_ptr_t
{
    typedef rb_remove_pointer<P> T;

    rb_nonnull_ptr_t(T *v_ = nullptr)
        : v(v_)
    {
    }
    T *operator=(T *v_)
    {
        return v = v_;
    }
    T *operator->() const
    {
        return get();
    }
    T &operator*() const
    {
        return *get();
    }
    T **operator&() const
    {
        return &v;
    }
    /* Only auto-cast to const types. */
    template <typename C> operator const C *() const
    {
        return get();
    }
    operator const char *() const
    {
        return (const char *)get();
    }
    T *get() const
    {
        return v ? v : const_cast<T *>(&Null(T));
    }
    T *get_raw() const
    {
        return v;
    }

    T *v;
};

#endif /* RB_NULL_HH */
