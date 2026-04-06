/* ********************************************************************** */
/* gmp_caml.c */
/* ********************************************************************** */

/* This file is part of the MLGmpIDL interface, released under LGPL license
   with an exception allowing the redistribution of statically linked
   executables.
   Please read the COPYING file packaged in the distribution  */

#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "caml/version.h"
#include "caml/fail.h"
#include "caml/alloc.h"
#include "caml/custom.h"
#include "caml/memory.h"
#include "caml/callback.h"
#include "caml/camlidlruntime.h"
#include "caml/intext.h"

#include "gmp_caml.h"

/* Maximum number of bytes we accept in a serialized value.
   Prevents pathological allocations from corrupt data. */
#define MAX_SERIALIZED_BYTES (1 << 26) /* ~64 MB */

/*
 * MPZ_EXPORT_BYTES(countp, z) — export |z| as a big-endian byte array.
 *   countp: pointer to a size_t receiving the number of bytes written.
 *   z:      the mpz_t value to export.
 *   Returns a GMP-allocated buffer (caller must free with gmp_free).
 *
 * Uses a fixed format: most-significant word first (order=1), one byte
 * per word (size=1), big-endian within each word (endian=1), no nail
 * bits (nails=0).  The first argument to mpz_export is NULL so that GMP
 * allocates the buffer itself.
 */
#define MPZ_EXPORT_BYTES(countp, z) \
  mpz_export(NULL, (countp), 1, 1, 1, 0, (z))

/*
 * MPZ_IMPORT_BYTES(z, count, buf) — import a big-endian byte array into z.
 *   z:     the mpz_t destination (must already be initialized).
 *   count: number of bytes in buf.
 *   buf:   pointer to the byte array to import.
 *
 * Mirrors MPZ_EXPORT_BYTES: most-significant word first, one byte per
 * word, big-endian, no nails.
 */
#define MPZ_IMPORT_BYTES(z, count, buf) \
  mpz_import((z), (count), 1, 1, 1, 0, (buf))

/* Helper: free memory allocated by GMP */
static void gmp_free(void *ptr, size_t size)
{
  void (*freefunc)(void *, size_t);
  mp_get_memory_functions(NULL, NULL, &freefunc);
  freefunc(ptr, size);
}

/* ********************************************************************** */
/* I. Custom datatypes: fastest coding */
/* ********************************************************************** */

/* ====================================================================== */
/* I.1 mpz_t */
/* ====================================================================== */

void camlidl_custom_mpz_finalize(value val)
{
  __mpz_struct* mpz = (__mpz_struct*)(Data_custom_val(val));
  mpz_clear(mpz);
}

int camlidl_custom_mpz_compare(value val1, value val2)
{
  int res;
  __mpz_struct* mpz1;
  __mpz_struct* mpz2;

  mpz1 = (__mpz_struct*)(Data_custom_val(val1));
  mpz2 = (__mpz_struct*)(Data_custom_val(val2));
  res = mpz_cmp(mpz1,mpz2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  return res;
}
long camlidl_custom_mpz_hash(value val)
{
  __mpz_struct* mpz = (__mpz_struct*)(Data_custom_val(val));
  long hash = mpz_get_si(mpz);
  return hash;
}

/* Serialize an mpz value to the marshalling stream. */
static void camlidl_serialize_mpz(mpz_ptr z)
{
  int sgn = mpz_sgn(z);
  caml_serialize_int_1(sgn);

  if (sgn != 0) {
    size_t count;
    void *buf = MPZ_EXPORT_BYTES(&count, z);
    if (count > MAX_SERIALIZED_BYTES)
      caml_failwith("mlgmpidl: mpz value too large to serialize");
    caml_serialize_int_4(count);
    caml_serialize_block_1(buf, count);
    gmp_free(buf, count);
  }
}

/* Deserialize an mpz value from the marshalling stream.
   The destination must already be initialized (via mpz_init or mpq_init). */
static void camlidl_deserialize_mpz(mpz_ptr z)
{
  signed char sgn = caml_deserialize_sint_1();

  if (sgn == 0) {
    mpz_set_ui(z, 0);
  } else {
    uint32_t count = caml_deserialize_uint_4();
    if (count == 0 || count > MAX_SERIALIZED_BYTES)
      caml_deserialize_error("mlgmpidl: invalid mpz byte count");

    unsigned char *buf = (unsigned char *)malloc(count);
    if (buf == NULL)
      caml_deserialize_error("mlgmpidl: out of memory in mpz deserialize");
    caml_deserialize_block_1(buf, count);

    MPZ_IMPORT_BYTES(z, count, buf);
    if (sgn < 0)
      mpz_neg(z, z);

    free(buf);
  }
}

void camlidl_custom_mpz_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpz_struct* mpz = (__mpz_struct*)(Data_custom_val(val));
  camlidl_serialize_mpz(mpz);
  *wsize_32 = sizeof(__mpz_struct);
  *wsize_64 = sizeof(__mpz_struct);
}

uintnat camlidl_custom_mpz_deserialize(void *dst)
{
  __mpz_struct* mpz = (__mpz_struct*)dst;
  mpz_init(mpz);
  camlidl_deserialize_mpz(mpz);
  return sizeof(__mpz_struct);
}

struct custom_operations camlidl_custom_mpz = {
  "camlidl_gmp_custom_mpz",
  &camlidl_custom_mpz_finalize,
  &camlidl_custom_mpz_compare,
  &camlidl_custom_mpz_hash,
  &camlidl_custom_mpz_serialize,
  &camlidl_custom_mpz_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpz_ptr_c2ml(mpz_ptr* mpz)
{
  value val;

  val = caml_alloc_custom(&camlidl_custom_mpz, sizeof(__mpz_struct), 0, 1);
  *(((__mpz_struct*)(Data_custom_val(val)))) = *(*mpz);
  return val;
}
void camlidl_mpz_ptr_ml2c(value val, mpz_ptr* mpz)
{
  *mpz = (mpz_ptr)(Data_custom_val(val));
}
/*
void camlidl_mpz_ml2c(value val, __mpz_struct* mpz)
{
  *mpz = *((mpz_ptr)(Data_custom_val(val)));
}
*/

/* ====================================================================== */
/* I.2 mpq_t */
/* ====================================================================== */

void camlidl_custom_mpq_finalize(value val)
{
  __mpq_struct* mpq = (__mpq_struct*)(Data_custom_val(val));
  mpq_clear(mpq);
}

int camlidl_custom_mpq_compare(value val1, value val2)
{
  int res;
  __mpq_struct* mpq1;
  __mpq_struct* mpq2;

  mpq1 = (__mpq_struct*)(Data_custom_val(val1));
  mpq2 = (__mpq_struct*)(Data_custom_val(val2));
  res = mpq_cmp(mpq1,mpq2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  return res;
}
long camlidl_custom_mpq_hash(value val)
{
 __mpq_struct* mpq = (__mpq_struct*)(Data_custom_val(val));
  unsigned long num = mpz_get_ui(mpq_numref(mpq));
  unsigned long den = mpz_get_ui(mpq_denref(mpq));
  long hash;
  if (num==0) hash = 0;
  else if (den==0) hash = num>0 ? LONG_MAX : LONG_MIN;
  else hash = (num<den ? den/num : num/den);
  return hash;
}

void camlidl_custom_mpq_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpq_struct* mpq = (__mpq_struct*)(Data_custom_val(val));
  camlidl_serialize_mpz(mpq_numref(mpq));
  camlidl_serialize_mpz(mpq_denref(mpq));
  *wsize_32 = sizeof(__mpq_struct);
  *wsize_64 = sizeof(__mpq_struct);
}

uintnat camlidl_custom_mpq_deserialize(void *dst)
{
  __mpq_struct* mpq = (__mpq_struct*)dst;
  mpq_init(mpq);
  camlidl_deserialize_mpz(mpq_numref(mpq));
  camlidl_deserialize_mpz(mpq_denref(mpq));
  return sizeof(__mpq_struct);
}

struct custom_operations camlidl_custom_mpq = {
  "camlidl_gmp_custom_mpq",
  &camlidl_custom_mpq_finalize,
  &camlidl_custom_mpq_compare,
  &camlidl_custom_mpq_hash,
  &camlidl_custom_mpq_serialize,
  &camlidl_custom_mpq_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpq_ptr_c2ml(mpq_ptr* mpq)
{
  value val;

  val = caml_alloc_custom(&camlidl_custom_mpq, sizeof(__mpq_struct), 0, 1);
  *(((__mpq_struct*)(Data_custom_val(val)))) = *(*mpq);
  return val;
}
void camlidl_mpq_ptr_ml2c(value val, mpq_ptr* mpq)
{
  *mpq = (mpq_ptr)(Data_custom_val(val));
}
/*
void camlidl_mpq_ml2c(value val, __mpq_struct* mpq)
{
  *mpq = *((mpq_ptr)(Data_custom_val(val)));
}
*/
int mpz_fits_int_p (mpz_t OP)
{
  if (mpz_fits_slong_p(OP)){
    long v = mpz_get_si(OP);
    return (Min_long <= v && v<=Max_long);
  }
  else {
    return 0;
  }
}


/* ====================================================================== */
/* I.3 mpf_t */
/* ====================================================================== */

void camlidl_custom_mpf_finalize(value val)
{
  __mpf_struct* mpf = (__mpf_struct*)(Data_custom_val(val));
  mpf_clear(mpf);
}

int camlidl_custom_mpf_compare(value val1, value val2)
{
  int res;
  __mpf_struct* mpf1;
  __mpf_struct* mpf2;

  mpf1 = (__mpf_struct*)(Data_custom_val(val1));
  mpf2 = (__mpf_struct*)(Data_custom_val(val2));
  res = mpf_cmp(mpf1,mpf2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  return res;
}
long camlidl_custom_mpf_hash(value val)
{
 __mpf_struct* mpf = (__mpf_struct*)(Data_custom_val(val));
  long hash;
  double d;
  signed long int exp;
  d = mpf_get_d_2exp (&exp, mpf);
  hash = (long)d;
  return hash;
}

/* Serialize mpf_t portably using a canonical representation:
   - precision in bits (from mpf_get_prec, public API)
   - sign (1 byte)
   - binary exponent in bits: exp2 = GMP_NUMB_BITS * (_mp_exp - abs(_mp_size))
   - mantissa as a big-endian byte array (via mpz_export on a temporary mpz
     holding the absolute mantissa integer from the used limbs)

   This format is independent of GMP_NUMB_BITS and limb size, so it is
   portable across 32-bit and 64-bit GMP builds. */
void camlidl_custom_mpf_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpf_struct* mpf = (__mpf_struct*)(Data_custom_val(val));

  mp_bitcnt_t prec = mpf_get_prec(mpf);
  caml_serialize_int_4(prec);

  mp_size_t sz = mpf->_mp_size;
  int sgn = (sz > 0) ? 1 : (sz < 0) ? -1 : 0;
  caml_serialize_int_1(sgn);

  if (sgn == 0) {
    /* Zero: nothing more to serialize */
  } else {
    mp_size_t abs_sz = sz < 0 ? -sz : sz;

    /* Binary exponent in bits, independent of limb size */
    int64_t exp2 = (int64_t)GMP_NUMB_BITS * ((int64_t)mpf->_mp_exp - (int64_t)abs_sz);
    caml_serialize_int_8(exp2);

    /* Build a temporary mpz from the used limbs */
    mpz_t mantissa;
    mpz_init(mantissa);
    mpz_import(mantissa, abs_sz, -1, sizeof(mp_limb_t), 0, 0, mpf->_mp_d);

    /* Serialize the mantissa as big-endian bytes */
    size_t count;
    void *buf = MPZ_EXPORT_BYTES(&count, mantissa);
    if (count > MAX_SERIALIZED_BYTES)
      caml_failwith("mlgmpidl: mpf mantissa too large to serialize");
    caml_serialize_int_4(count);
    caml_serialize_block_1(buf, count);
    gmp_free(buf, count);

    mpz_clear(mantissa);
  }

  *wsize_32 = sizeof(__mpf_struct);
  *wsize_64 = sizeof(__mpf_struct);
}

uintnat camlidl_custom_mpf_deserialize(void *dst)
{
  __mpf_struct* mpf = (__mpf_struct*)dst;

  uint32_t prec = caml_deserialize_uint_4();
  if (prec == 0 || prec > MAX_SERIALIZED_BYTES)
    caml_deserialize_error("mlgmpidl: invalid mpf precision");

  signed char sgn = caml_deserialize_sint_1();

  if (sgn == 0) {
    mpf_init2(mpf, prec);
  } else {
    int64_t exp2 = caml_deserialize_sint_8();

    uint32_t count = caml_deserialize_uint_4();
    if (count == 0 || count > MAX_SERIALIZED_BYTES)
      caml_deserialize_error("mlgmpidl: invalid mpf mantissa byte count");

    unsigned char *buf = (unsigned char *)malloc(count);
    if (buf == NULL)
      caml_deserialize_error("mlgmpidl: out of memory in mpf deserialize");
    caml_deserialize_block_1(buf, count);

    /* Import mantissa into a temporary mpz */
    mpz_t mantissa;
    mpz_init(mantissa);
    MPZ_IMPORT_BYTES(mantissa, count, buf);
    free(buf);

    /* Determine how many limbs the mantissa occupies */
    size_t mant_bits = mpz_sizeinbase(mantissa, 2);

    /* Initialize with enough precision to hold all mantissa bits */
    mp_bitcnt_t init_prec = mant_bits > prec ? mant_bits : prec;
    mpf_init2(mpf, init_prec);

    /* Set the integer value */
    mpf_set_z(mpf, mantissa);
    mpz_clear(mantissa);

    /* Apply the binary exponent */
    if (exp2 > 0) {
      mpf_mul_2exp(mpf, mpf, (mp_bitcnt_t)exp2);
    } else if (exp2 < 0) {
      mpf_div_2exp(mpf, mpf, (mp_bitcnt_t)(-exp2));
    }

    /* Negate if needed */
    if (sgn < 0)
      mpf_neg(mpf, mpf);

    /* Restore the requested precision */
    if (init_prec != prec)
      mpf_set_prec_raw(mpf, prec);
  }

  return sizeof(__mpf_struct);
}

struct custom_operations camlidl_custom_mpf = {
  "camlidl_gmp_custom_mpf",
  &camlidl_custom_mpf_finalize,
  &camlidl_custom_mpf_compare,
  &camlidl_custom_mpf_hash,
  &camlidl_custom_mpf_serialize,
  &camlidl_custom_mpf_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpf_ptr_c2ml(mpf_ptr* mpf)
{
  value val;

  val = caml_alloc_custom(&camlidl_custom_mpf, sizeof(__mpf_struct), 0, 1);
  *(((__mpf_struct*)(Data_custom_val(val)))) = *(*mpf);
  return val;
}
void camlidl_mpf_ptr_ml2c(value val, mpf_ptr* mpf)
{
  *mpf = (mpf_ptr)(Data_custom_val(val));
}
/*
void camlidl_mpf_ml2c(value val, __mpf_struct* mpf)
{
  *mpf = *((mpf_ptr)(Data_custom_val(val)));
}
*/
int mpf_fits_int_p (mpf_t OP)
{
  if (mpf_fits_slong_p(OP)){
    long v = mpf_get_si(OP);
    return (Min_long <= v && v <= Max_long);
  }
  else {
    return 0;
  }
}

/* ====================================================================== */
/* I.4 mpfr_t */
/* ====================================================================== */

void camlidl_custom_mpfr_finalize(value val)
{
  __mpfr_struct* mpfr = (__mpfr_struct*)(Data_custom_val(val));
  mpfr_clear(mpfr);
}

int camlidl_custom_mpfr_compare(value val1, value val2)
{
  int res;
  __mpfr_struct* mpfr1;
  __mpfr_struct* mpfr2;

  mpfr1 = (__mpfr_struct*)(Data_custom_val(val1));
  mpfr2 = (__mpfr_struct*)(Data_custom_val(val2));
  res = mpfr_cmp(mpfr1,mpfr2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  return res;
}
long camlidl_custom_mpfr_hash(value val)
{
 __mpfr_struct* mpfr = (__mpfr_struct*)(Data_custom_val(val));
  long hash;
  double d;
  int exp;
  d = mpfr_get_d(mpfr, GMP_RNDN);
  d = frexp(d,&exp);
  hash = (long)d;
  return hash;
}

/* Tag values for special MPFR numbers */
#define MPFR_SERIAL_FINITE    0
#define MPFR_SERIAL_NAN       1
#define MPFR_SERIAL_POS_INF   2
#define MPFR_SERIAL_NEG_INF   3
#define MPFR_SERIAL_POS_ZERO  4
#define MPFR_SERIAL_NEG_ZERO  5

/* We access MPFR internals (_mpfr_d, _mpfr_sign, _mpfr_exp) directly.
   Guard against silent struct changes across MPFR versions. */
_Static_assert(sizeof(((mpfr_t){0})->_mpfr_d) == sizeof(mp_limb_t *),
               "unexpected _mpfr_d type");
_Static_assert(sizeof(((mpfr_t){0})->_mpfr_exp) == sizeof(mpfr_exp_t),
               "unexpected _mpfr_exp type");
_Static_assert(sizeof(((mpfr_t){0})->_mpfr_sign) == sizeof(mpfr_sign_t),
               "unexpected _mpfr_sign type");

/* Number of limbs needed for a given precision. */
#define MPFR_NLIMBS(prec) \
  (((prec) + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS)

/* Serialize mpfr_t using direct binary limb access.
   Format:
     - precision in bits (8 bytes)
     - tag (1 byte): NaN / ±Inf / ±Zero / Finite
     - for finite values:
       - exponent (8 bytes, mpfr_exp_t)
       - sign (1 byte, +1 or -1)
       - mantissa as big-endian byte array (via mpz_export on a temporary mpz
         holding the absolute significand from the limb array) */
void camlidl_custom_mpfr_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpfr_struct* mpfr = (__mpfr_struct*)(Data_custom_val(val));

  mpfr_prec_t prec = mpfr_get_prec(mpfr);
  caml_serialize_int_8((int64_t)prec);

  if (mpfr_nan_p(mpfr)) {
    caml_serialize_int_1(MPFR_SERIAL_NAN);
  } else if (mpfr_inf_p(mpfr)) {
    caml_serialize_int_1(mpfr_sgn(mpfr) > 0 ? MPFR_SERIAL_POS_INF : MPFR_SERIAL_NEG_INF);
  } else if (mpfr_zero_p(mpfr)) {
    caml_serialize_int_1(mpfr_signbit(mpfr) ? MPFR_SERIAL_NEG_ZERO : MPFR_SERIAL_POS_ZERO);
  } else {
    caml_serialize_int_1(MPFR_SERIAL_FINITE);

    caml_serialize_int_8((int64_t)mpfr->_mpfr_exp);
    caml_serialize_int_1(mpfr->_mpfr_sign);

    mp_size_t nlimbs = MPFR_NLIMBS(prec);

    mpz_t mantissa;
    mpz_init(mantissa);
    mpz_import(mantissa, nlimbs, -1, sizeof(mp_limb_t), 0, 0, mpfr->_mpfr_d);

    size_t count;
    void *buf = MPZ_EXPORT_BYTES(&count, mantissa);
    if (count > MAX_SERIALIZED_BYTES)
      caml_failwith("mlgmpidl: mpfr mantissa too large to serialize");
    caml_serialize_int_4(count);
    caml_serialize_block_1(buf, count);
    gmp_free(buf, count);

    mpz_clear(mantissa);
  }

  *wsize_32 = sizeof(__mpfr_struct);
  *wsize_64 = sizeof(__mpfr_struct);
}

uintnat camlidl_custom_mpfr_deserialize(void *dst)
{
  __mpfr_struct* mpfr = (__mpfr_struct*)dst;

  int64_t prec = caml_deserialize_sint_8();
  if (prec < MPFR_PREC_MIN || prec > MPFR_PREC_MAX || prec > (1LL << 26))
    caml_deserialize_error("mlgmpidl: invalid mpfr precision");

  mpfr_init2(mpfr, (mpfr_prec_t)prec);

  signed char tag = caml_deserialize_sint_1();

  switch (tag) {
    case MPFR_SERIAL_NAN:
      mpfr_set_nan(mpfr);
      break;
    case MPFR_SERIAL_POS_INF:
      mpfr_set_inf(mpfr, +1);
      break;
    case MPFR_SERIAL_NEG_INF:
      mpfr_set_inf(mpfr, -1);
      break;
    case MPFR_SERIAL_POS_ZERO:
      mpfr_set_zero(mpfr, +1);
      break;
    case MPFR_SERIAL_NEG_ZERO:
      mpfr_set_zero(mpfr, -1);
      break;
    case MPFR_SERIAL_FINITE: {
      mpfr_exp_t exp = (mpfr_exp_t)caml_deserialize_sint_8();
      signed char sign = caml_deserialize_sint_1();

      uint32_t count = caml_deserialize_uint_4();
      if (count == 0 || count > MAX_SERIALIZED_BYTES)
        caml_deserialize_error("mlgmpidl: invalid mpfr mantissa byte count");

      unsigned char *buf = (unsigned char *)malloc(count);
      if (buf == NULL)
        caml_deserialize_error("mlgmpidl: out of memory in mpfr deserialize");
      caml_deserialize_block_1(buf, count);

      mpz_t mantissa;
      mpz_init(mantissa);
      MPZ_IMPORT_BYTES(mantissa, count, buf);
      free(buf);

      mp_size_t nlimbs = MPFR_NLIMBS(prec);
      mp_size_t src_limbs = (mp_size_t)mpz_size(mantissa);

      /* Copy limbs into the mpfr significand, zero-padding if needed */
      const mp_limb_t *src = mpz_limbs_read(mantissa);
      mp_size_t copy = src_limbs < nlimbs ? src_limbs : nlimbs;
      if (copy > 0)
        memcpy(mpfr->_mpfr_d, src, copy * sizeof(mp_limb_t));
      if (copy < nlimbs)
        memset(mpfr->_mpfr_d + copy, 0, (nlimbs - copy) * sizeof(mp_limb_t));

      mpfr->_mpfr_exp = exp;
      mpfr->_mpfr_sign = sign;

      mpz_clear(mantissa);
      break;
    }
    default:
      caml_deserialize_error("mlgmpidl: invalid mpfr tag");
  }

  return sizeof(__mpfr_struct);
}

struct custom_operations camlidl_custom_mpfr = {
  "camlidl_gmp_custom_mpfr",
  &camlidl_custom_mpfr_finalize,
  &camlidl_custom_mpfr_compare,
  &camlidl_custom_mpfr_hash,
  &camlidl_custom_mpfr_serialize,
  &camlidl_custom_mpfr_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpfr_ptr_c2ml(mpfr_ptr* mpfr)
{
  value val;

  val = caml_alloc_custom(&camlidl_custom_mpfr, sizeof(__mpfr_struct), 0, 1);
  *(((__mpfr_struct*)(Data_custom_val(val)))) = *(*mpfr);
  return val;
}
void camlidl_mpfr_ptr_ml2c(value val, mpfr_ptr* mpfr)
{
  *mpfr = (mpfr_ptr)(Data_custom_val(val));
}
/*
void camlidl_mpfr_ml2c(value val, __mpfr_struct* mpfr)
{
  *mpfr = *((mpfr_ptr)(Data_custom_val(val)));
}
*/
/* ====================================================================== */
/* I.5 gmp_randstate_t */
/* ====================================================================== */

void camlidl_custom_gmp_randstate_finalize(value val)
{
 __gmp_randstate_struct* gmp_randstate = (__gmp_randstate_struct*)(Data_custom_val(val));
  gmp_randclear(gmp_randstate);
}

struct custom_operations camlidl_custom_gmp_randstate = {
  "camlidl_gmp_custom_randstate",
  &camlidl_custom_gmp_randstate_finalize,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_gmp_randstate_ptr_c2ml(gmp_randstate_ptr* gmp_randstate)
{
  value val;

  val = caml_alloc_custom(&camlidl_custom_gmp_randstate, sizeof(__gmp_randstate_struct), 0, 1);
  *((__gmp_randstate_struct*)(Data_custom_val(val))) = *(*gmp_randstate);
  return val;
}
void camlidl_gmp_randstate_ptr_ml2c(value val, gmp_randstate_ptr* gmp_randstate)
{
  *gmp_randstate = (gmp_randstate_ptr)(Data_custom_val(val));
}


CAMLprim value mlgmpidl_init_custom_ops(value unit)
{
  static int done = 0;
  CAMLparam1(unit);
  if (!done) {
    done = 1;
    caml_register_custom_operations(&camlidl_custom_mpz);
    caml_register_custom_operations(&camlidl_custom_mpq);
    caml_register_custom_operations(&camlidl_custom_mpf);
    caml_register_custom_operations(&camlidl_custom_mpfr);
  }
  CAMLreturn(Val_unit);
}
