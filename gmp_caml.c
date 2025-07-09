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
#include "caml/version.h"
#include "caml/fail.h"
#include "caml/alloc.h"
#include "caml/custom.h"
#include "caml/memory.h"
#include "caml/callback.h"
#include "caml/camlidlruntime.h"
#include "caml/intext.h"

#include "gmp_caml.h"

/* Debug flag for serialization - can be enabled with -DDEBUG_SERIALIZATION */
#ifdef DEBUG_SERIALIZATION
#define DEBUG_SERIAL_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_SERIAL_PRINTF(...) ((void)0)
#endif


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

/* Serialization for mpz_t */
void camlidl_custom_mpz_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpz_serialize called!\n");
  fflush(stdout);
  
  __mpz_struct* mpz = (__mpz_struct*)(Data_custom_val(val));
  
  /* Serialize size (number of limbs, negative for negative numbers) */
  mp_size_t size = mpz->_mp_size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpz size = %ld, size = %lu\n", (long)size, sizeof(mp_size_t));
  if (sizeof(mp_size_t) != 8)
    caml_deserialize_error("wrong size for mp_size_t");
  caml_serialize_int_8((int64_t)size);
  
  /* Serialize the limbs */
  mp_limb_t *limbs = mpz->_mp_d;
  int abs_size = size < 0 ? -size : size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpz limbs = %d, limb size = %lu\n", abs_size, sizeof(mp_limb_t));
  if (sizeof(mp_limb_t) != 8)
    caml_deserialize_error("wrong size for mp_limb_t");
  for (int i = 0; i < abs_size; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpz limb %d = %lx\n", i, (unsigned long)limbs[i]);
    caml_serialize_int_8(limbs[i]);
  }
  
  *wsize_32 = sizeof(__mpz_struct);
  *wsize_64 = sizeof(__mpz_struct);
  DEBUG_SERIAL_PRINTF("DEBUG: mpz serialization complete, size = %lu\n", (unsigned long)sizeof(__mpz_struct));
}

uintnat camlidl_custom_mpz_deserialize(void *dst)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpz_deserialize called!\n");
  fflush(stdout);
  
  __mpz_struct* mpz = (__mpz_struct*)dst;
  
  /* Deserialize size */
  int64_t size = caml_deserialize_sint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpz deserializing size = %ld\n", (long)size);
  
  /* Initialize with enough space */
  int abs_size = size < 0 ? -size : size;
  mpz_init2(mpz, abs_size * GMP_NUMB_BITS);
  mpz->_mp_size = size;
  
  /* Deserialize the limbs */
  DEBUG_SERIAL_PRINTF("DEBUG: mpz deserializing %d limbs\n", abs_size);
  for (int i = 0; i < abs_size; i++) {
    mpz->_mp_d[i] = caml_deserialize_uint_8();
    DEBUG_SERIAL_PRINTF("DEBUG: mpz limb %d = %lx\n", i, (unsigned long)mpz->_mp_d[i]);
  }
  
  DEBUG_SERIAL_PRINTF("DEBUG: mpz deserialization complete, returning size = %zu\n", sizeof(__mpz_struct));
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

/* Serialization for mpq_t */
void camlidl_custom_mpq_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpq_serialize called!\n");
  fflush(stdout);
  
  __mpq_struct* mpq = (__mpq_struct*)(Data_custom_val(val));
  
  /* Serialize numerator */
  __mpz_struct* num = &(mpq->_mp_num);
  mp_size_t num_size = num->_mp_size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpq numerator size = %ld\n", (long)num_size);
  caml_serialize_int_8((int64_t)num_size);
  
  int abs_num_size = num_size < 0 ? -num_size : num_size;
  for (int i = 0; i < abs_num_size; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpq numerator limb %d = %lx\n", i, (unsigned long)num->_mp_d[i]);
    caml_serialize_int_8(num->_mp_d[i]);
  }
  
  /* Serialize denominator */
  __mpz_struct* den = &(mpq->_mp_den);
  mp_size_t den_size = den->_mp_size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpq denominator size = %ld\n", (long)den_size);
  caml_serialize_int_8((int64_t)den_size);
  
  int abs_den_size = den_size < 0 ? -den_size : den_size;
  for (int i = 0; i < abs_den_size; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpq denominator limb %d = %lx\n", i, (unsigned long)den->_mp_d[i]);
    caml_serialize_int_8(den->_mp_d[i]);
  }
  
  *wsize_32 = sizeof(__mpq_struct);
  *wsize_64 = sizeof(__mpq_struct);
  DEBUG_SERIAL_PRINTF("DEBUG: mpq serialization complete, size = %lu\n", (unsigned long)sizeof(__mpq_struct));
}

uintnat camlidl_custom_mpq_deserialize(void *dst)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpq_deserialize called!\n");
  fflush(stdout);
  
  __mpq_struct* mpq = (__mpq_struct*)dst;
  mpq_init(mpq);
  
  /* Deserialize numerator */
  __mpz_struct* num = &(mpq->_mp_num);
  int64_t num_size = caml_deserialize_sint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpq deserializing numerator size = %ld\n", (long)num_size);
  
  int abs_num_size = num_size < 0 ? -num_size : num_size;
  if (abs_num_size > 0) {
    _mpz_realloc(num, abs_num_size);
    num->_mp_size = num_size;
    for (int i = 0; i < abs_num_size; i++) {
      num->_mp_d[i] = caml_deserialize_uint_8();
      DEBUG_SERIAL_PRINTF("DEBUG: mpq numerator limb %d = %lx\n", i, (unsigned long)num->_mp_d[i]);
    }
  } else {
    num->_mp_size = 0;
  }
  
  /* Deserialize denominator */
  __mpz_struct* den = &(mpq->_mp_den);
  int64_t den_size = caml_deserialize_sint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpq deserializing denominator size = %ld\n", (long)den_size);
  
  int abs_den_size = den_size < 0 ? -den_size : den_size;
  if (abs_den_size > 0) {
    _mpz_realloc(den, abs_den_size);
    den->_mp_size = den_size;
    for (int i = 0; i < abs_den_size; i++) {
      den->_mp_d[i] = caml_deserialize_uint_8();
      DEBUG_SERIAL_PRINTF("DEBUG: mpq denominator limb %d = %lx\n", i, (unsigned long)den->_mp_d[i]);
    }
  } else {
    den->_mp_size = 0;
  }
  
  DEBUG_SERIAL_PRINTF("DEBUG: mpq deserialization complete, returning size = %lu\n", (unsigned long)sizeof(__mpq_struct));
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

/* Serialization for mpf_t */
void camlidl_custom_mpf_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpf_serialize called!\n");
  fflush(stdout);
  
  __mpf_struct* mpf = (__mpf_struct*)(Data_custom_val(val));
  
  /* Serialize precision (in bits) */
  mp_bitcnt_t prec = mpf->_mp_prec;
  DEBUG_SERIAL_PRINTF("DEBUG: mpf precision = %lu, size = %lu\n", (unsigned long)prec, sizeof(mp_bitcnt_t));
  if (sizeof(mp_bitcnt_t) != 8)
    caml_deserialize_error("wrong size for mp_bitcnt_t");
  caml_serialize_int_8((uint64_t)prec);
  
  /* Serialize size (number of limbs used) */
  mp_size_t size = mpf->_mp_size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpf size = %ld, size = %lu\n", (long)size, sizeof(mp_size_t));
  if (sizeof(mp_size_t) != 8)
    caml_deserialize_error("wrong size for mp_size_t");
  caml_serialize_int_8((int64_t)size);
  
  /* Serialize exponent (stored as size_t but represents position) */
  mp_exp_t exp = mpf->_mp_exp;
  DEBUG_SERIAL_PRINTF("DEBUG: mpf exp = %ld, size = %lu\n", (long)exp, sizeof(mp_exp_t));
  if (sizeof(mp_exp_t) != 8)
    caml_deserialize_error("wrong size for mp_exp_t");
  caml_serialize_int_8((int64_t)exp);
  
  /* Serialize the limbs */
  mp_limb_t *limbs = mpf->_mp_d;
  int abs_size = size < 0 ? -size : size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpf limbs = %d, limb size = %lu\n", abs_size, sizeof(mp_limb_t));
  if (sizeof(mp_limb_t) != 8)
    caml_deserialize_error("wrong size for mp_limb_t");
  for (int i = 0; i < abs_size; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpf limb %d = %lx\n", i, (unsigned long)limbs[i]);
    caml_serialize_int_8(limbs[i]);
  }
  
  *wsize_32 = sizeof(__mpf_struct);
  *wsize_64 = sizeof(__mpf_struct);
  DEBUG_SERIAL_PRINTF("DEBUG: mpf serialization complete, size = %lu\n", (unsigned long)sizeof(__mpf_struct));
}

uintnat camlidl_custom_mpf_deserialize(void *dst)
{
  __mpf_struct* mpf = (__mpf_struct*)dst;
  
  /* Deserialize precision and initialize */
  uint64_t prec = caml_deserialize_uint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpf deserializing precision = %lu\n", (unsigned long)prec);
  mpf_init2(mpf, prec);
  
  /* Deserialize size */
  int64_t size = caml_deserialize_sint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpf deserializing size = %ld\n", (long)size);
  mpf->_mp_size = size;
  
  /* Deserialize exponent */
  int64_t exp = caml_deserialize_sint_8();
  DEBUG_SERIAL_PRINTF("DEBUG: mpf deserializing exp = %ld\n", (long)exp);
  mpf->_mp_exp = exp;
  
  /* Deserialize the limbs */
  int abs_size = size < 0 ? -size : size;
  DEBUG_SERIAL_PRINTF("DEBUG: mpf deserializing %d limbs\n", abs_size);
  for (int i = 0; i < abs_size; i++) {
    mpf->_mp_d[i] = caml_deserialize_uint_8();
    DEBUG_SERIAL_PRINTF("DEBUG: mpf limb %d = %lx\n", i, (unsigned long)mpf->_mp_d[i]);
  }
  
  DEBUG_SERIAL_PRINTF("DEBUG: mpf deserialization complete, returning size = %lu\n", (unsigned long)sizeof(__mpf_struct));
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

/*
typedef struct {
  mpfr_prec_t  _mpfr_prec;
  mpfr_sign_t  _mpfr_sign;
  mpfr_exp_t   _mpfr_exp;
  mp_limb_t   *_mpfr_d;
} __mpfr_struct;
*/ 

/* Serialization for mpfr_t */
void camlidl_custom_mpfr_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  DEBUG_SERIAL_PRINTF("DEBUG: camlidl_custom_mpfr_serialize called!\n");
  
  __mpfr_struct* mpfr = (__mpfr_struct*)(Data_custom_val(val));
  
  /* Serialize precision first */
  mpfr_prec_t prec = mpfr->_mpfr_prec;
  DEBUG_SERIAL_PRINTF("DEBUG: mpfr precision = %llu, %ld\n", (long long unsigned)prec, sizeof(mpfr_prec_t));
  if (sizeof(mpfr_prec_t) != 8)
    caml_deserialize_error("wrong size for mpfr_prec_t");
  caml_serialize_int_8((uint64_t)prec);

  mpfr_sign_t sign = mpfr->_mpfr_sign;
  DEBUG_SERIAL_PRINTF("DEBUG: mpfr sign = %d, %ld\n", sign, sizeof(mpfr_sign_t));
  if ((sign != 0) && (sign != 1))
    caml_deserialize_error("wrong value for sign");
  caml_serialize_int_1((unsigned char)sign);
  
  mpfr_exp_t  exp = mpfr->_mpfr_exp;
  DEBUG_SERIAL_PRINTF("DEBUG: mpfr exp = %lld, %ld\n", (long long)exp, sizeof(mpfr_exp_t));
  if (sizeof(mpfr_exp_t) != 8)
    caml_deserialize_error("wrong size for mpfr_exp_t");  
  caml_serialize_int_8((int64_t)exp);
  
  mp_limb_t *limbs = mpfr->_mpfr_d;
  int nb_limbs = (prec + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS;
  DEBUG_SERIAL_PRINTF("DEBUG: mpfr limbs = %d, %ld\n", nb_limbs, sizeof(mp_limb_t));
  if (sizeof(mp_limb_t) != 8)
    caml_deserialize_error("wrong size for mp_limb_t");  
  for (int i=0; i<nb_limbs; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpfr limb %d\n", i);  
    caml_serialize_int_8(limbs[i]);
  }
  DEBUG_SERIAL_PRINTF("DEBUG: limbs serialized\n");  

  *wsize_32 = sizeof(__mpfr_struct);
  *wsize_64 = sizeof(__mpfr_struct);
  DEBUG_SERIAL_PRINTF("DEBUG: mpfr serialization complete, size = %lu\n", (unsigned long)sizeof(__mpfr_struct));
}

uintnat camlidl_custom_mpfr_deserialize(void *dst)
{
  __mpfr_struct *x = (__mpfr_struct*)dst;

  uint64_t prec = caml_deserialize_uint_8();

  mpfr_init2(x, prec);
  
  x->_mpfr_sign = caml_deserialize_uint_1();
  x->_mpfr_exp = caml_deserialize_sint_8();

  int nb_limbs = (prec + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS;
  for (int i=0; i<nb_limbs; i++) {
    DEBUG_SERIAL_PRINTF("DEBUG: mpfr deserialize limb %d\n", i);
    x->_mpfr_d[i] = caml_deserialize_uint_8();
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

/* Serialization for gmp_randstate_t */
/* Note: This is a simplified implementation since GMP doesn't provide direct serialization */
void camlidl_custom_gmp_randstate_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  /* We can't serialize the exact internal state, so we'll use a marker */
  /* to indicate that this is a random state that needs to be re-initialized */
  uint32_t marker = 0xDEADBEEF;
  caml_serialize_int_4(marker);
  
  *wsize_32 = sizeof(__gmp_randstate_struct);
  *wsize_64 = sizeof(__gmp_randstate_struct);
}

uintnat camlidl_custom_gmp_randstate_deserialize(void *dst)
{
  uint32_t marker = caml_deserialize_uint_4();
  
  /* Initialize a new random state with default algorithm */
  __gmp_randstate_struct* gmp_randstate = (__gmp_randstate_struct*)dst;
  gmp_randinit_default(gmp_randstate);
  
  /* Use the marker as a seed to at least make it deterministic */
  gmp_randseed_ui(gmp_randstate, marker);
  
  return sizeof(__gmp_randstate_struct);
}

struct custom_operations camlidl_custom_gmp_randstate = {
  "camlidl_gmp_custom_randstate",
  &camlidl_custom_gmp_randstate_finalize,
  custom_compare_default,
  custom_hash_default,
  &camlidl_custom_gmp_randstate_serialize,
  &camlidl_custom_gmp_randstate_deserialize,
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

/* ********************************************************************** */
/* II. Custom datatypes: safer coding in case of repeated callbacks */
/* ********************************************************************** */

/* ====================================================================== */
/* II.1 mpz2_t */
/* ====================================================================== */

void camlidl_custom_mpz2_finalize(value val)
{
  CAMLparam1(val);
  __mpz_struct** mpz = (__mpz_struct**)(Data_custom_val(val));
  mpz_clear(*mpz);
  free(*mpz);
}

int camlidl_custom_mpz2_compare(value val1, value val2)
{
  CAMLparam2(val1,val2);
  int res;
  __mpz_struct** mpz1;
  __mpz_struct** mpz2;

  mpz1 = (__mpz_struct**)(Data_custom_val(val1));
  mpz2 = (__mpz_struct**)(Data_custom_val(val2));
  res = mpz_cmp(*mpz1,*mpz2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  CAMLreturn(res);
}
long camlidl_custom_mpz2_hash(value val)
{
  CAMLparam1(val);
  __mpz_struct** mpz = (__mpz_struct**)(Data_custom_val(val));
  long hash = mpz_get_si(*mpz);
  CAMLreturn(hash);
}

/* Serialization for mpz2_t */
void camlidl_custom_mpz2_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpz_struct** mpz = (__mpz_struct**)(Data_custom_val(val));
  char *str = mpz_get_str(NULL, 10, *mpz);
  size_t len = strlen(str);
  
  caml_serialize_int_4((uint32_t)len);
  caml_serialize_block_1(str, len);
  free(str);
  
  *wsize_32 = sizeof(__mpz_struct*);
  *wsize_64 = sizeof(__mpz_struct*);
}

uintnat camlidl_custom_mpz2_deserialize(void *dst)
{
  uint32_t len = caml_deserialize_uint_4();
  char *str = (char*)caml_stat_alloc(len + 1);
  
  caml_deserialize_block_1(str, len);
  str[len] = '\0';
  
  __mpz_struct** mpz = (__mpz_struct**)dst;
  *mpz = malloc(sizeof(__mpz_struct));
  mpz_init(*mpz);
  mpz_set_str(*mpz, str, 10);
  
  caml_stat_free(str);
  return sizeof(__mpz_struct*);
}

struct custom_operations camlidl_custom_mpz2 = {
  "camlidl_gmp_custom_mpz2",
  &camlidl_custom_mpz2_finalize,
  &camlidl_custom_mpz2_compare,
  &camlidl_custom_mpz2_hash,
  &camlidl_custom_mpz2_serialize,
  &camlidl_custom_mpz2_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpz2_ptr_c2ml(mpz_ptr* mpz)
{
  value val;
  __mpz_struct* p;

  p = malloc(sizeof(__mpz_struct));
  *p = *(*mpz);
  val = caml_alloc_custom(&camlidl_custom_mpz2, sizeof(__mpz_struct*), 0, 1);
  *(((__mpz_struct**)(Data_custom_val(val)))) = p;
  return val;
}
void camlidl_mpz2_ptr_ml2c(value val, mpz_ptr* mpz)
{
  *mpz = *(__mpz_struct**)(Data_custom_val(val));
}

/* ====================================================================== */
/* II.2 mpq2_t */
/* ====================================================================== */

void camlidl_custom_mpq2_finalize(value val)
{
  CAMLparam1(val);
  __mpq_struct** mpq = (__mpq_struct**)(Data_custom_val(val));
  mpq_clear(*mpq);
}

int camlidl_custom_mpq2_compare(value val1, value val2)
{
  CAMLparam2(val1,val2);
  int res;
  __mpq_struct** mpq1;
  __mpq_struct** mpq2;

  mpq1 = (__mpq_struct**)(Data_custom_val(val1));
  mpq2 = (__mpq_struct**)(Data_custom_val(val2));
  res = mpq_cmp(*mpq1,*mpq2);
  res = res > 0 ? 1 : res==0 ? 0 : -1;
  CAMLreturn(res);
}
long camlidl_custom_mpq2_hash(value val)
{
  CAMLparam1(val);
  __mpq_struct** mpq = (__mpq_struct**)(Data_custom_val(val));
  unsigned long num = mpz_get_ui(mpq_numref(*mpq));
  unsigned long den = mpz_get_ui(mpq_denref(*mpq));
  long hash = num<den ? den/num : num/den;
  CAMLreturn(hash);
}

/* Serialization for mpq2_t */
void camlidl_custom_mpq2_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  __mpq_struct** mpq = (__mpq_struct**)(Data_custom_val(val));
  char *str = mpq_get_str(NULL, 10, *mpq);
  size_t len = strlen(str);
  
  caml_serialize_int_4((uint32_t)len);
  caml_serialize_block_1(str, len);
  free(str);
  
  *wsize_32 = sizeof(__mpq_struct*);
  *wsize_64 = sizeof(__mpq_struct*);
}

uintnat camlidl_custom_mpq2_deserialize(void *dst)
{
  uint32_t len = caml_deserialize_uint_4();
  char *str = (char*)caml_stat_alloc(len + 1);
  
  caml_deserialize_block_1(str, len);
  str[len] = '\0';
  
  __mpq_struct** mpq = (__mpq_struct**)dst;
  *mpq = malloc(sizeof(__mpq_struct));
  mpq_init(*mpq);
  mpq_set_str(*mpq, str, 10);
  
  caml_stat_free(str);
  return sizeof(__mpq_struct*);
}

struct custom_operations camlidl_custom_mpq2 = {
  "camlidl_gmp_custom_mpq2",
  &camlidl_custom_mpq2_finalize,
  &camlidl_custom_mpq2_compare,
  &camlidl_custom_mpq2_hash,
  &camlidl_custom_mpq2_serialize,
  &camlidl_custom_mpq2_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_mpq2_ptr_c2ml(mpq_ptr* mpq)
{
  value val;
  __mpq_struct* p;
  p = malloc(sizeof(__mpq_struct));
  *p = *(*mpq);
  val = caml_alloc_custom(&camlidl_custom_mpq2, sizeof(__mpq_struct), 0, 1);
  *((__mpq_struct**)(Data_custom_val(val))) = p;
  return val;
}
void camlidl_mpq2_ptr_ml2c(value val, mpq_ptr* mpq)
{
  *mpq = *(__mpq_struct**)(Data_custom_val(val));
}

/* ====================================================================== */
/* II.3 gmp_randstate2_t */
/* ====================================================================== */

void camlidl_custom_gmp_randstate2_finalize(value val)
{
  CAMLparam1(val);
 __gmp_randstate_struct** gmp_randstate = (__gmp_randstate_struct**)(Data_custom_val(val));
  gmp_randclear(*gmp_randstate);
  free(*gmp_randstate);
}

/* Serialization for gmp_randstate2_t */
void camlidl_custom_gmp_randstate2_serialize(value val, uintnat *wsize_32, uintnat *wsize_64)
{
  /* We can't serialize the exact internal state, so we'll use a marker */
  uint32_t marker = 0xBEEFDEAD;
  caml_serialize_int_4(marker);
  
  *wsize_32 = sizeof(__gmp_randstate_struct*);
  *wsize_64 = sizeof(__gmp_randstate_struct*);
}

uintnat camlidl_custom_gmp_randstate2_deserialize(void *dst)
{
  uint32_t marker = caml_deserialize_uint_4();
  
  /* Initialize a new random state with default algorithm */
  __gmp_randstate_struct** gmp_randstate = (__gmp_randstate_struct**)dst;
  *gmp_randstate = malloc(sizeof(__gmp_randstate_struct));
  gmp_randinit_default(*gmp_randstate);
  
  /* Use the marker as a seed to at least make it deterministic */
  gmp_randseed_ui(*gmp_randstate, marker);
  
  return sizeof(__gmp_randstate_struct*);
}

struct custom_operations camlidl_custom_gmp_randstate2 = {
  "camlidl_gmp_custom_randstate2",
  &camlidl_custom_gmp_randstate2_finalize,
  custom_compare_default,
  custom_hash_default,
  &camlidl_custom_gmp_randstate2_serialize,
  &camlidl_custom_gmp_randstate2_deserialize,
  custom_compare_ext_default,
#if OCAML_VERSION >= 40800
  custom_fixed_length_default
#endif
};

value camlidl_gmp_randstate2_ptr_c2ml(gmp_randstate_ptr* gmp_randstate)
{
  value val;
  __gmp_randstate_struct* p;
  p = malloc(sizeof(__gmp_randstate_struct));
  *p = *(*gmp_randstate);

  val = caml_alloc_custom(&camlidl_custom_gmp_randstate2, sizeof(__gmp_randstate_struct*), 0, 1);
  *((__gmp_randstate_struct**)(Data_custom_val(val))) = p;
  return val;
}
void camlidl_gmp_randstate2_ptr_ml2c(value val, gmp_randstate_ptr* gmp_randstate)
{
  *gmp_randstate = *(__gmp_randstate_struct**)(Data_custom_val(val));
}


CAMLprim value mlgmpidl_init_custom_ops(value unit)
{
  CAMLparam1(unit);
  caml_register_custom_operations(&camlidl_custom_mpz);
  caml_register_custom_operations(&camlidl_custom_mpq);
  caml_register_custom_operations(&camlidl_custom_mpf);
  caml_register_custom_operations(&camlidl_custom_mpfr);
  caml_register_custom_operations(&camlidl_custom_gmp_randstate);
  // TODO: register missing ones
  CAMLreturn(Val_unit);
}