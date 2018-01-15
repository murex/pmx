/*******************************************************************************
*
* Copyright (c) {2003-2018} Murex S.A.S. and its affiliates.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*******************************************************************************/

#ifndef PMXSUPPORT_HH
#define PMXSUPPORT_HH

#define PMX_INSTRUMENT_START_TAG 0xCAFEF00D
#define PMX_INSTRUMENT_END_TAG   0xFABABBA0

/* only work on gcc with Unixes */
#define PMX_ENABLED (defined(__amd64) && defined(__GNUC__) && !defined(WIN32))

#if !(PMX_ENABLED)
#define PMX_INSTRUMENT(...)
#define PMX_INSTRUMENT_METHOD(...)

#else /*PMX_ENABLED*/

#ifdef __GNUC__
#define PMX_INSTRUMENTATION_ATTR __attribute__((unused))
#else /*!__GNUC__*/
#define PMX_INSTRUMENTATION_ATTR
#endif /*__GNUC__*/

// We add the magic tags at the end, as gcc 4.9 creates a duplicate structure in memory if we initialise with them in place
#define PMX_INSTRUMENT_START                    \
    volatile struct {                           \
        unsigned long start_tag;

#define PMX_INSTRUMENT_HEAD_END                 \
    unsigned long stackbase;                     \
    unsigned long end_tag;                       \
    } PMX_INSTRUMENTATION_ATTR mx_instrumentation = { \
          0x0,

#define PMX_INSTRUMENT_END                      \
        0x0 \
    }; \
__asm__ ("movq %%rbp,%0" : "=r" (mx_instrumentation.stackbase) ); \
mx_instrumentation.start_tag=PMX_INSTRUMENT_START_TAG; \
mx_instrumentation.end_tag=PMX_INSTRUMENT_END_TAG;

#define PMX_INSTRUMENT_HEAD_ROW(A)          __typeof__(A) pmx_ ## A;

#define PMX_INSTRUMENT_HEAD1(A)                 PMX_INSTRUMENT_START                  PMX_INSTRUMENT_HEAD_ROW(A)
#define PMX_INSTRUMENT_HEAD2(A,B)               PMX_INSTRUMENT_HEAD1(A)               PMX_INSTRUMENT_HEAD_ROW(B)
#define PMX_INSTRUMENT_HEAD3(A,B,C)             PMX_INSTRUMENT_HEAD2(A,B)             PMX_INSTRUMENT_HEAD_ROW(C)
#define PMX_INSTRUMENT_HEAD4(A,B,C,D)           PMX_INSTRUMENT_HEAD3(A,B,C)           PMX_INSTRUMENT_HEAD_ROW(D)
#define PMX_INSTRUMENT_HEAD5(A,B,C,D,E)         PMX_INSTRUMENT_HEAD4(A,B,C,D)         PMX_INSTRUMENT_HEAD_ROW(E)
#define PMX_INSTRUMENT_HEAD6(A,B,C,D,E,F)       PMX_INSTRUMENT_HEAD5(A,B,C,D,E)       PMX_INSTRUMENT_HEAD_ROW(F)
#define PMX_INSTRUMENT_HEAD7(A,B,C,D,E,F,G)     PMX_INSTRUMENT_HEAD6(A,B,C,D,E,F)     PMX_INSTRUMENT_HEAD_ROW(G)
#define PMX_INSTRUMENT_HEAD8(A,B,C,D,E,F,G,H)   PMX_INSTRUMENT_HEAD7(A,B,C,D,E,F,G)   PMX_INSTRUMENT_HEAD_ROW(H)
#define PMX_INSTRUMENT_HEAD9(A,B,C,D,E,F,G,H,I) PMX_INSTRUMENT_HEAD8(A,B,C,D,E,F,G,H) PMX_INSTRUMENT_HEAD_ROW(I)

#define PMX_INSTRUMENT_VALUES_ROW(A)        A,

#define PMX_INSTRUMENT_VALUES1(A)                 PMX_INSTRUMENT_HEAD_END                 PMX_INSTRUMENT_VALUES_ROW(A)
#define PMX_INSTRUMENT_VALUES2(A,B)               PMX_INSTRUMENT_VALUES1(A)               PMX_INSTRUMENT_VALUES_ROW(B)
#define PMX_INSTRUMENT_VALUES3(A,B,C)             PMX_INSTRUMENT_VALUES2(A,B)             PMX_INSTRUMENT_VALUES_ROW(C)
#define PMX_INSTRUMENT_VALUES4(A,B,C,D)           PMX_INSTRUMENT_VALUES3(A,B,C)           PMX_INSTRUMENT_VALUES_ROW(D)
#define PMX_INSTRUMENT_VALUES5(A,B,C,D,E)         PMX_INSTRUMENT_VALUES4(A,B,C,D)         PMX_INSTRUMENT_VALUES_ROW(E)
#define PMX_INSTRUMENT_VALUES6(A,B,C,D,E,F)       PMX_INSTRUMENT_VALUES5(A,B,C,D,E)       PMX_INSTRUMENT_VALUES_ROW(F)
#define PMX_INSTRUMENT_VALUES7(A,B,C,D,E,F,G)     PMX_INSTRUMENT_VALUES6(A,B,C,D,E,F)     PMX_INSTRUMENT_VALUES_ROW(G)
#define PMX_INSTRUMENT_VALUES8(A,B,C,D,E,F,G,H)   PMX_INSTRUMENT_VALUES7(A,B,C,D,E,F,G)   PMX_INSTRUMENT_VALUES_ROW(H)
#define PMX_INSTRUMENT_VALUES9(A,B,C,D,E,F,G,H,I) PMX_INSTRUMENT_VALUES8(A,B,C,D,E,F,G,H) PMX_INSTRUMENT_VALUES_ROW(I)

#define GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME
#define PMX_INSTRUMENT(...) GET_MACRO(__VA_ARGS__, PMX_INSTRUMENT9, PMX_INSTRUMENT8, PMX_INSTRUMENT7, PMX_INSTRUMENT6, \
                     PMX_INSTRUMENT5, PMX_INSTRUMENT4, PMX_INSTRUMENT3, PMX_INSTRUMENT2, PMX_INSTRUMENT1)(__VA_ARGS__)
#define PMX_INSTRUMENT_METHOD(...) GET_MACRO(__VA_ARGS__, PMX_INSTRUMENT_METHOD9, PMX_INSTRUMENT_METHOD8, \
        PMX_INSTRUMENT_METHOD7, PMX_INSTRUMENT_METHOD6, PMX_INSTRUMENT_METHOD5, PMX_INSTRUMENT_METHOD4, \
        PMX_INSTRUMENT_METHOD3, PMX_INSTRUMENT_METHOD2, PMX_INSTRUMENT_METHOD1)(__VA_ARGS__)

#define PMX_INSTRUMENT1(A)                      \
    PMX_INSTRUMENT_HEAD1(A)                     \
    PMX_INSTRUMENT_VALUES1(A)                   \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT2(A,B)                    \
    PMX_INSTRUMENT_HEAD2(A,B)                   \
    PMX_INSTRUMENT_VALUES2(A,B)                 \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT3(A,B,C)                  \
    PMX_INSTRUMENT_HEAD3(A,B,C)                 \
    PMX_INSTRUMENT_VALUES3(A,B,C)               \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT4(A,B,C,D)                \
    PMX_INSTRUMENT_HEAD4(A,B,C,D)               \
    PMX_INSTRUMENT_VALUES4(A,B,C,D)             \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT5(A,B,C,D,E)              \
    PMX_INSTRUMENT_HEAD5(A,B,C,D,E)             \
    PMX_INSTRUMENT_VALUES5(A,B,C,D,E)           \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT6(A,B,C,D,E,F)            \
    PMX_INSTRUMENT_HEAD6(A,B,C,D,E,F)           \
    PMX_INSTRUMENT_VALUES6(A,B,C,D,E,F)         \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT7(A,B,C,D,E,F,G)          \
    PMX_INSTRUMENT_HEAD7(A,B,C,D,E,F,G)         \
    PMX_INSTRUMENT_VALUES7(A,B,C,D,E,F,G)       \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT8(A,B,C,D,E,F,G,H)        \
    PMX_INSTRUMENT_HEAD8(A,B,C,D,E,F,G,H)       \
    PMX_INSTRUMENT_VALUES8(A,B,C,D,E,F,G,H)     \
    PMX_INSTRUMENT_END

#define PMX_INSTRUMENT9(A,B,C,D,E,F,G,H,I)      \
    PMX_INSTRUMENT_HEAD9(A,B,C,D,E,F,G,H,I)     \
    PMX_INSTRUMENT_VALUES9(A,B,C,D,E,F,G,H,I)   \
    PMX_INSTRUMENT_END

// The compilers don't allow our macros to work on 'this', so we have to create a void* and use that.
#define PMX_INSTRUMENT_METHOD1(A)               \
    void *zthis = this;                         \
    PMX_INSTRUMENT2(zthis,A)

#define PMX_INSTRUMENT_METHOD2(A,B)             \
    void *zthis = this;                         \
    PMX_INSTRUMENT3(zthis,A,B)

#define PMX_INSTRUMENT_METHOD3(A,B,C)           \
    void *zthis = this;                         \
    PMX_INSTRUMENT4(zthis,A,B,C)

#define PMX_INSTRUMENT_METHOD4(A,B,C,D)         \
    void *zthis = this;                         \
    PMX_INSTRUMENT5(zthis,A,B,C,D)

#define PMX_INSTRUMENT_METHOD5(A,B,C,D,E)       \
    void *zthis = this;                         \
    PMX_INSTRUMENT6(zthis,A,B,C,D,E)

#define PMX_INSTRUMENT_METHOD6(A,B,C,D,E,F)     \
    void *zthis = this;                         \
    PMX_INSTRUMENT7(zthis,A,B,C,D,E,F)

#define PMX_INSTRUMENT_METHOD7(A,B,C,D,E,F,G)   \
    void *zthis = this;                         \
    PMX_INSTRUMENT8(zthis,A,B,C,D,E,F,G)

#define PMX_INSTRUMENT_METHOD8(A,B,C,D,E,F,G,H) \
    void *zthis = this;                         \
    PMX_INSTRUMENT9(zthis,A,B,C,D,E,F,G,H)

#endif /*PMX_ENABLED*/

#endif /*PMXSUPPORT_HH*/



