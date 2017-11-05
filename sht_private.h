/*
 * Copyright (c) 2010-2015 Centre National de la Recherche Scientifique.
 * written by Nathanael Schaeffer (CNRS, ISTerre, Grenoble, France).
 * 
 * nathanael.schaeffer@ujf-grenoble.fr
 * 
 * This software is governed by the CeCILL license under French law and
 * abiding by the rules of distribution of free software. You can use,
 * modify and/or redistribute the software under the terms of the CeCILL
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 * 
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license and that you accept its terms.
 * 
 */

/********************************************************************
 * SHTns : Spherical Harmonic Transform for numerical simulations.  *
 *    written by Nathanael Schaeffer / CNRS                         *
 ********************************************************************/

/// \internal \file sht_private.h private data and options.

#include <stdlib.h>
#include <complex.h>
#include <math.h>
// FFTW la derivee d/dx = ik	(pas de moins !)
#include "fftw3/fftw3.h"

// config file generated by ./configure
#include "sht_config.h"

#define SHTNS_PRIVATE
#include "shtns.h"

#ifdef _OPENMP
  #include <omp.h>
#endif

#ifdef HAVE_LIBCUFFT
#include <cufft.h>
#include "shtns_cuda.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/// private gpu functions:
int cushtns_init_gpu(shtns_cfg);
void cushtns_release_gpu(shtns_cfg);
int cushtns_use_gpu(int);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

/* BEGIN COMPILE-TIME SETTINGS */

/// defines the maximum amount of memory in megabytes that SHTns should use.
#define SHTNS_MAX_MEMORY 2048

/// Minimum performance improve for DCT in \ref sht_auto mode. If not atained, we may switch back to gauss.
#define MIN_PERF_IMPROVE_DCT 1.05

/// Try to enforce at least this accuracy for DCT in sht_auto mode.
#define MIN_ACCURACY_DCT 1.e-8

/// The default \ref opt_polar threshold (0 disabled, 1.e-6 is aggressive, 1.e-10 is safe, 1.e-14 is VERY safe)
#define SHT_DEFAULT_POLAR_OPT 1.e-10

/// The default \ref norm used by shtns_init
#define SHT_DEFAULT_NORM ( sht_orthonormal )
//#define SHT_DEFAULT_NORM ( sht_schmidt | SHT_NO_CS_PHASE )

/// The maximum order of non-linear terms to be resolved by SH transform by default.
/// 1 : no non-linear terms. 2 : quadratic non-linear terms (default), 3 : triadic, ...
/// must be larger or equal to 1.
#define SHT_DEFAULT_NL_ORDER 1

/// minimum NLAT to consider the use of DCT acceleration.
#define SHT_MIN_NLAT_DCT 64

/// time-limit for timing individual transforms (in seconds)
#define SHT_TIME_LIMIT 0.2

/* END COMPILE-TIME SETTINGS */

// sht variants (std, ltr)
enum sht_variants { SHT_STD, SHT_LTR, SHT_M, SHT_NVAR };
// sht types (scal synth, scal analys, vect synth, ...)
enum sht_types { SHT_TYP_SSY, SHT_TYP_SAN, SHT_TYP_VSY, SHT_TYP_VAN,
	SHT_TYP_GSP, SHT_TYP_GTO, SHT_TYP_3SY, SHT_TYP_3AN, SHT_NTYP };

// sht grids
enum sht_grids { GRID_NONE, GRID_GAUSS, GRID_REGULAR, GRID_POLES };

// pointer to various function types
typedef void (*pf2l)(shtns_cfg, void*, void*, long int);
typedef void (*pf3l)(shtns_cfg, void*, void*, void*, long int);
typedef void (*pf4l)(shtns_cfg, void*, void*, void*, void*, long int);
typedef void (*pf6l)(shtns_cfg, void*, void*, void*, void*, void*, void*, long int);
typedef void (*pf2ml)(shtns_cfg, int, void*, void*, long int);
typedef void (*pf3ml)(shtns_cfg, int, void*, void*, void*, long int);
typedef void (*pf4ml)(shtns_cfg, int, void*, void*, void*, void*, long int);
typedef void (*pf6ml)(shtns_cfg, int, void*, void*, void*, void*, void*, void*, long int);

/// structure containing useful information about the SHT.
struct shtns_info {		// MUST start with "int nlm;"
/* PUBLIC PART (if modified, shtns.h should be modified acordingly) */
	unsigned int nlm;			///< total number of (l,m) spherical harmonics components.
	unsigned short lmax;		///< maximum degree (lmax) of spherical harmonics.
	unsigned short mmax;		///< maximum order (mmax*mres) of spherical harmonics.
	unsigned short mres;		///< the periodicity along the phi axis.
	unsigned short nphi;		///< number of spatial points in Phi direction (longitude)
	unsigned short nlat;		///< number of spatial points in Theta direction (latitude) ...
	unsigned short nlat_2;		///< ...and half of it (using (shtns.nlat+1)/2 allows odd shtns.nlat.)
	int *lmidx;					///< (virtual) index in SH array of given im (size mmax+1) : LiM(l,im) = lmidx[im] + l
	unsigned short *li;			///< degree l for given mode index (size nlm) : li[lm]
	unsigned short *mi;			///< order m for given mode index (size nlm) : mi[lm]
	double *ct, *st;			///< cos(theta) and sin(theta) arrays (size nlat)
	unsigned int nspat;			///< number of real numbers that must be allocated in a spatial field.
/* END OF PUBLIC PART */

	short fftc_mode;			///< how to perform the complex fft : -1 = no fft; 0 = interleaved/native; 1 = split/transpose.
	unsigned short nthreads;	///< number of threads (openmp).
	unsigned short *tm;			///< start theta value for SH (polar optimization : near the poles the legendre polynomials go to zero for high m's)
	int k_stride_a;				///< stride in theta direction
	int m_stride_a;				///< stride in phi direction (m)
	double *wg;					///< Gauss weights for Gauss-Legendre quadrature.
	double *st_1;				///< 1/sin(theta);

	fftw_plan ifft, fft;		// plans for FFTW.
	fftw_plan ifftc, fftc;

	/* Legendre function generation arrays */
	double *alm;	// coefficient list for Legendre function recurrence (size 2*NLM)
	double *blm;	// coefficient list for modified Legendre function recurrence for analysis (size 2*NLM)
	double *l_2;	// array of size (LMAX+1) containing 1./l(l+1) for increasing integer l.
	/* matrices for vector transform (to convert to scalar transforms) */
	double *mx_stdt;	// sparse matrix for  sin(theta).d/dtheta,  couples l-1 and l+1
	double *mx_van;		// sparse matrix for  sin(theta).d/dtheta + 2*cos(theta),  couples l-1 and l+1

	void* ftable[SHT_NVAR][SHT_NTYP];		// pointers to transform functions.

	/* MEM matrices */
	double **ylm;		// matrix for inverse transform (synthesis)
	struct DtDp** dylm;	// theta and phi derivative of Ylm matrix
	double **zlm;		// matrix for direct transform (analysis)
	struct DtDp** dzlm;

	int ncplx_fft;			///< number of complex numbers to allocate for the fft : -1 = no fft; 0 = in-place fft (no allocation).

	/* rotation stuff */
	unsigned npts_rot;		// number of physical points needed
	fftw_plan fft_rot;		// Fourier transform for rotations
	double* ct_rot;			// cos(theta) array
	double* st_rot;			// sin(theta) array

	/* _to_lat stuff */
	double* ylm_lat;
	double ct_lat;
	fftw_plan ifft_lat;		///< fftw plan for SHqst_to_lat
	int nphi_lat;			///< nphi of previous SHqst_to_lat

	#ifdef HAVE_LIBCUFFT
	/* cuda stuff */
	int cu_flags;
	double* d_alm;
	double* d_ct;
	double* d_mx_stdt;
	double* d_mx_van;
	double* gpu_mem;
	size_t nlm_stride, spat_stride;
	cudaStream_t xfer_stream, comp_stream;		// the cuda streams
	cufftHandle cufft_plan;						// the cufft Handle
	#endif

	/* other misc informations */
	unsigned char nlorder;	// order of non-linear terms to be resolved by SH transform.
	unsigned char grid;		// store grid type.
	short norm;				// store the normalization of the Spherical Harmonics (enum \ref shtns_norm + \ref SHT_NO_CS_PHASE flag)
	unsigned fftw_plan_mode;
	double Y00_1, Y10_ct, Y11_st;
	shtns_cfg next;		// pointer to next sht_setup or NULL (records a chained list of SHT setup).
	// the end should be aligned on the size of int, to allow the storage of small arrays.
};

// define shortcuts to sizes.
#define NLM shtns->nlm
#define LMAX shtns->lmax
#define NLAT shtns->nlat
#define NLAT_2 shtns->nlat_2
#define NPHI shtns->nphi
#define MMAX shtns->mmax
#define MRES shtns->mres
#define SHT_NL_ORDER shtns->nlorder

// define index in alm/blm matrices
#define ALM_IDX(shtns, im) ( (im)*(2*(shtns->lmax+1) - ((im)-1)*shtns->mres) )

// SHT_NORM without CS_PHASE
#define SHT_NORM (shtns->norm & 0x0FF)

#ifndef M_PI
# define M_PI 3.1415926535897932384626433832795
#endif
#ifndef M_PIl
# define M_PIl 3.1415926535897932384626433832795L
#endif

// value for on-the-fly transforms is lower because it allows to optimize some more (don't compute l which are not significant).
#define SHT_L_RESCALE_FLY 1000
// set to a value close to the machine accuracy, it allows to speed-up on-the-fly SHTs with very large l (lmax > SHT_L_RESCALE_FLY).
#define SHT_ACCURACY 1.0e-20
// scale factor for extended range numbers (used in on-the-fly transforms to compute recurrence)
#define SHT_SCALE_FACTOR 2.9073548971824275622e+135
//#define SHT_SCALE_FACTOR 2.0370359763344860863e+90


#if _GCC_VEC_ == 0
	#undef _GCC_VEC_
#endif

#ifdef __NVCC__
		// disable vector extensions when compiling cuda code.
        #undef _GCC_VEC_
#endif

/* are there vector extensions available ? */
#if !(defined __SSE2__ || defined __MIC__ || defined __VECTOR4DOUBLE__ || defined __VSX__ )
	#undef _GCC_VEC_
#endif
#ifdef __INTEL_COMPILER
	#if __INTEL_COMPILER < 1400
		#undef _GCC_VEC_
		#warning "no vector extensions available ! use gcc 4+ or icc 14+ for best performance."
	#endif
#endif
#ifdef __GNUC__
	#if __GNUC__ < 4
		#undef _GCC_VEC_
		#warning "no vector extensions available ! use gcc 4+ or icc 14+ for best performance."
	#endif
#endif

#if _GCC_VEC_ && __VECTOR4DOUBLE__
	// support Blue Gene/Q QPX vectors
	#define MIN_ALIGNMENT 32
	#define VSIZE 2
	typedef complex double v2d __attribute__((aligned (16)));		// vector that contains a complex number
	typedef double s2d __attribute__((aligned (16)));		// scalar number
	#define VSIZE2 4
	#define _SIMD_NAME_ "qpx"
	typedef vector4double rnd;		// vector of 4 doubles.
	#define vall(x) vec_splats(x)
	#define vread(mem, idx) vec_lda((idx)*32, ((double*)mem))
	#define vstor(mem, idx, v) vec_sta(v, (idx)*32, ((double*)mem))
	inline static double reduce_add(rnd a) {
		a += vec_perm(a, a, vec_gpci(02301));
		a += vec_perm(a, a, vec_gpci(01032));
		return( a[0] );
	}
	inline static v2d v2d_reduce(rnd a, rnd b) {
		a = vec_perm(a, b, vec_gpci(00426)) + vec_perm(a, b, vec_gpci(01537));
		a += vec_perm(a, a, vec_gpci(02301));
		return a[0] + I*a[1];
	}
	//#define v2d_reduce(a, b) ( reduce_add(a) +I* reduce_add(b) )
	#define addi(a,b) ((a) + I*(b))

	#define S2D_STORE(mem, idx, ev, od) \
		vstor(mem, idx, ev+od); \
		vstor((double*)mem + NLAT-VSIZE2 - (idx)*VSIZE2, 0, vec_perm(ev-od, ev-od, vec_gpci(03210)));
	#define S2D_CSTORE(mem, idx, er, od, ei, oi)	{	\
		rnd aa = vec_perm(ei+oi, ei+oi, vec_gpci(01032)) + (er+od); \
		rnd bb = (er + od) - vec_perm(ei+oi, ei+oi, vec_gpci(01032)); \
		vstor(mem, idx, vec_perm(bb, aa, vec_gpci(00527))); \
		vstor(((double*)mem) + (NPHI-2*im)*NLAT, idx, vec_perm(aa, bb, vec_gpci(00527))); \
		aa = vec_perm(er-od, er-od, vec_gpci(01032)) + (ei-oi); \
		bb = vec_perm(er-od, er-od, vec_gpci(01032)) - (ei-oi); \
		vstor(((double*)mem) + NLAT, -(idx+1), vec_perm(bb, aa, vec_gpci(02705))); \
		vstor(((double*)mem) + (NPHI+1-2*im)*NLAT, -(idx+1), vec_perm(aa, bb, vec_gpci(02705))); }
	// TODO: S2D_CSTORE2 has not been tested and is probably wrong...
	#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	{	\
		rnd aa = vec_perm(er+od, ei+oi, vec_gpci(00415)); \
		rnd bb = vec_perm(er+od, ei+oi, vec_gpci(02637)); \
		vstor(mem, idx*2, aa); \
		vstor(mem, idx*2+1, bb); \
		aa = vec_perm(er-od, ei-oi, vec_gpci(00415)); \
		bb = vec_perm(er-od, ei-oi, vec_gpci(02637)); \
		vstor(mem, NLAT_2-1-idx*2, aa); \
		vstor(mem, NLAT_2-2-idx*2, bb); }

	#define vdup(x) (x)

	#define vlo(a) (a[0])

	#define vcplx_real(a) creal(a)
	#define vcplx_imag(a) cimag(a)

	#define VMALLOC(s)	malloc(s)
	#define VFREE(s)	free(s)
	#ifdef SHTNS4MAGIC
		#error "Blue Gene/Q not supported."
	#endif
#elif _GCC_VEC_ && __VSX__
	// support VSX (IBM Power)
	#include <altivec.h>
	#define MIN_ALIGNMENT 16
	#define VSIZE 2
	//typedef double s2d __attribute__ ((vector_size (8*VSIZE)));		// vector that should behave like a real scalar for complex number multiplication.
	//typedef double v2d __attribute__ ((vector_size (8*VSIZE)));		// vector that contains a complex number
	typedef __vector double s2d;
	typedef __vector double v2d;
	typedef __vector double rnd;
	#define VSIZE2 2
	#define _SIMD_NAME_ "vsx"
	//typedef double rnd __attribute__ ((vector_size (VSIZE2*8)));		// vector of 2 doubles.
	#define vall(x) vec_splats((double)x)
	inline static s2d vxchg(s2d a) {
		const vector unsigned char perm = { 8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7 };
		return vec_perm(a,a,perm);
	}
	#define vread(mem, idx) vec_ld((int)(idx)*16, ((const vector double*)mem))
	#define vstor(mem, idx, v) vec_st((v2d)v, (int)(idx)*16, ((vector double*)mem))
	inline static double reduce_add(rnd a) {
		rnd b = a + vec_mergel(a, a);
		return( vec_extract(b,0) );
	}
	inline static v2d v2d_reduce(rnd a, rnd b) {
		v2d c = vec_mergel(a, b);		b = vec_mergeh(a, b);
		return b + c;
	}
	inline static v2d addi(v2d a, v2d b) {
		const s2d mp = {-1.0, 1.0};
		return a + vxchg(b)*mp;
	}
	inline static s2d subadd(s2d a, s2d b) {
		const s2d mp = {-1.0, 1.0};
		return a + b*mp;
	}

	#define vdup(x) vec_splats((double)x)
	#define vlo(a) vec_extract(a, 0)
	#define vhi(a) vec_extract(a, 1)
	#define vcplx_real(a) vec_extract(a, 0)
	#define vcplx_imag(a) vec_extract(a, 1)
	inline static s2d vec_mix_lohi(s2d a, s2d b) {	// same as _mm_shuffle_pd(a,b,2)
		const vector unsigned char perm = {0,1,2,3,4,5,6,7, 24,25,26,27,28,29,30,31};
		return vec_perm(a,b,perm);
	}

		#define S2D_STORE(mem, idx, ev, od)		((s2d*)mem)[idx] = ev+od;		((s2d*)mem)[NLAT_2-1 - (idx)] = vxchg(ev-od);
		#define S2D_CSTORE(mem, idx, er, od, ei, oi)	{	\
			rnd aa = vxchg(ei + oi) + (er + od);		rnd bb = (er + od) - vxchg(ei + oi);	\
			((s2d*)mem)[idx] = vec_mix_lohi(bb, aa);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)] = vec_mix_lohi(aa, bb);	\
			aa = vxchg(er - od) + (ei - oi);		bb = vxchg(er - od) - (ei - oi);	\
			((s2d*)mem)[NLAT_2-1 -(idx)] = vec_mix_lohi(bb, aa);	\
			((s2d*)mem)[(NPHI+1-2*im)*NLAT_2 -1 -(idx)] = vec_mix_lohi(aa, bb);	}
		#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	{	\
			((s2d*)mem)[(idx)*2]   = vec_mergeh(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*2+1] = vec_mergel(er+od, ei+oi);	\
			((s2d*)mem)[NLAT-1-(idx)*2] = vec_mergeh(er-od, ei-oi);	\
			((s2d*)mem)[NLAT-2-(idx)*2] = vec_mergel(er-od, ei-oi);	}

		#define S2D_STORE_4MAGIC(mem, idx, ev, od)		{	\
			s2d n = ev+od;		s2d s = ev-od;	\
			((s2d*)mem)[(idx)*2] = vec_mergeh(n, s);	\
			((s2d*)mem)[(idx)*2+1] = vec_mergel(n, s);	}
		#define S2D_CSTORE_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			rnd nr = er + od;		rnd ni = ei + oi;	\
			rnd sr = er - od;		rnd si = ei - oi;	\
			((s2d*)mem)[(idx)*2] = vec_mergeh(nr-si,  sr+ni);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*2] = vec_mergeh(nr+si,  sr-ni);	\
			((s2d*)mem)[(idx)*2+1] = vec_mergel(nr-si,  sr+ni);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*2+1] = vec_mergel(nr+si,  sr-ni);	}
		#define S2D_CSTORE2_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			((s2d*)mem)[(idx)*4]   = vec_mergeh(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*4+1] = vec_mergeh(er-od, ei-oi);	\
			((s2d*)mem)[(idx)*4+2] = vec_mergel(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*4+3] = vec_mergel(er-od, ei-oi);	}

	/*inline static void* VMALLOC(size_t s) {
		void* ptr;
		posix_memalign(&ptr, MIN_ALIGNMENT, s);
		return ptr;
	}*/
	#define VMALLOC(s)	malloc(s)
	#define VFREE(s)	free(s)
#endif



#if _GCC_VEC_ && __SSE2__
	#define VSIZE 2
	typedef double s2d __attribute__ ((vector_size (8*VSIZE)));		// vector that should behave like a real scalar for complex number multiplication.
	typedef double v2d __attribute__ ((vector_size (8*VSIZE)));		// vector that contains a complex number
	#ifdef __AVX512F__
		#define MIN_ALIGNMENT 64
		#define VSIZE2 8
		#include <immintrin.h>
		// these values must be adjusted for the larger vectors of the MIC
		#undef SHT_L_RESCALE_FLY
		#undef SHT_ACCURACY
		#define SHT_L_RESCALE_FLY 1800
		#define SHT_ACCURACY 1.0e-40
		// Allocate memory aligned on 64 bytes for AVX-512
		#define VMALLOC(s)	_mm_malloc(s, MIN_ALIGNMENT)
		#define VFREE(s)	_mm_free(s)
		#define _SIMD_NAME_ "avx512"
		typedef double rnd __attribute__ ((vector_size (VSIZE2*8)));		// vector of 8 doubles.
		typedef double s4d __attribute__ ((vector_size (4*8)));				// vector of 4 doubles.
		#define vall(x) ((rnd) _mm512_set1_pd(x))
		#define vread(mem, idx) ((rnd)_mm512_loadu_pd( ((double*)mem) + (idx)*8 ))
		#define vstor(mem, idx, v) _mm512_storeu_pd( ((double*)mem) + (idx)*8 , v)
		inline static double reduce_add(rnd a) {
			return _mm512_reduce_add_pd(a);
		}
		inline static v2d v2d_reduce(rnd a, rnd b) {
			rnd x = (rnd)_mm512_unpackhi_pd(a, b) + (rnd)_mm512_unpacklo_pd(a, b);
			s4d y = (s4d)_mm512_castpd512_pd256(x) + (s4d)_mm512_extractf64x4_pd(x,1);
			return (v2d)_mm256_castpd256_pd128(y) + (v2d)_mm256_extractf128_pd(y,1);
		}
		#define S2D_STORE(mem, idx, ev, od) \
			_mm512_storeu_pd(((double*)mem) + (idx)*8,   ev+od); \
			_mm256_storeu_pd(((double*)mem) + NLAT-4 -(idx)*8,  _mm512_castpd512_pd256( _mm512_permutex_pd(ev-od, 0x1B) ) ); \
			_mm256_storeu_pd(((double*)mem) + NLAT-8 -(idx)*8,  _mm512_extractf64x4_pd( _mm512_permutex_pd(ev-od, 0x1B), 1 ) );
		#define S2D_CSTORE(mem, idx, er, od, ei, oi)	{	\
			rnd aa = (rnd)_mm512_permute_pd(ei+oi, 0x55) + (er+od);	rnd bb = (er+od) - (rnd)_mm512_permute_pd(ei+oi, 0x55); \
			_mm512_storeu_pd(((double*)mem) + (idx)*8, _mm512_shuffle_pd(bb, aa, 0xAA )); \
			_mm512_storeu_pd(((double*)mem) + (NPHI-2*im)*NLAT + (idx)*8, _mm512_shuffle_pd(aa, bb, 0xAA )); \
			aa = (rnd)_mm512_permute_pd(er-od, 0x55) + (ei - oi);		bb = (rnd)_mm512_permute_pd(er-od, 0x55) - (ei - oi);	\
			rnd yy = _mm512_shuffle_pd(bb, aa, 0xAA );		rnd zz = _mm512_shuffle_pd(aa, bb, 0xAA ); \
			yy = _mm512_permutex_pd( yy, 0x4E );			zz = _mm512_permutex_pd(zz, 0x4E ); \
			((s4d*)mem)[(NLAT/4)-1 -2*(idx)] = (s4d)_mm512_castpd512_pd256(yy);	\
			((s4d*)mem)[(NLAT/4)-2 -2*(idx)] = (s4d)_mm512_extractf64x4_pd(yy, 1);	\
			((s4d*)mem)[(NPHI+1-2*im)*(NLAT/4) -1 -2*(idx)] = (s4d)_mm512_castpd512_pd256(zz);	\
			((s4d*)mem)[(NPHI+1-2*im)*(NLAT/4) -2 -2*(idx)] = (s4d)_mm512_extractf64x4_pd(zz, 1); }
		#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	{	\
			rnd rr = (rnd)_mm512_permutex_pd(er+od, 0xD8);	rnd ii = (rnd)_mm512_permutex_pd(ei+oi, 0xD8);	\
			rnd aa = (rnd)_mm512_unpacklo_pd(rr, ii);	rnd bb = (rnd)_mm512_unpackhi_pd(rr, ii);	\
			((s4d*)mem)[(idx)*4]   = _mm512_castpd512_pd256(aa);	\
			((s4d*)mem)[(idx)*4+1] = _mm512_castpd512_pd256(bb);	\
			((s4d*)mem)[(idx)*4+2] = _mm512_extractf64x4_pd(aa, 1);	\
			((s4d*)mem)[(idx)*4+3] = _mm512_extractf64x4_pd(bb, 1);	\
			rr = (rnd)_mm512_permutex_pd(er-od, 0x8D);	ii = (rnd)_mm512_permutex_pd(ei-oi, 0x8D);	\
			aa = (rnd)_mm256_unpacklo_pd(rr, ii);	bb = (rnd)_mm256_unpackhi_pd(rr, ii);	\
			((s4d*)mem)[NLAT-1-(idx)*4] = _mm512_castpd512_pd256(aa);	\
			((s4d*)mem)[NLAT-2-(idx)*4] = _mm512_castpd512_pd256(bb);	\
			((s4d*)mem)[NLAT-3-(idx)*4] = _mm512_extractf64x4_pd(aa, 1);	\
			((s4d*)mem)[NLAT-4-(idx)*4] = _mm512_extractf64x4_pd(bb, 1);	}
	#elif defined __AVX__
		#define MIN_ALIGNMENT 32
		#define VSIZE2 4
		#include <immintrin.h>
		// Allocate memory aligned on 32 bytes for AVX
		#define VMALLOC(s)	_mm_malloc(s, MIN_ALIGNMENT)
		#define VFREE(s)	_mm_free(s)
		#define _SIMD_NAME_ "avx"
		typedef double rnd __attribute__ ((vector_size (VSIZE2*8)));		// vector of 4 doubles.
		#define vall(x) ((rnd) _mm256_set1_pd(x))
		#define vread(mem, idx) ((rnd)_mm256_loadu_pd( ((double*)mem) + (idx)*4 ))
		#define vstor(mem, idx, v) _mm256_storeu_pd( ((double*)mem) + (idx)*4 , v)
		inline static double reduce_add(rnd a) {
			v2d t = (v2d)_mm256_castpd256_pd128(a) + (v2d)_mm256_extractf128_pd(a,1);
			return _mm_cvtsd_f64(t) + _mm_cvtsd_f64(_mm_unpackhi_pd(t,t));
		}
		inline static v2d v2d_reduce(rnd a, rnd b) {
			a = _mm256_hadd_pd(a, b);
			return (v2d)_mm256_castpd256_pd128(a) + (v2d)_mm256_extractf128_pd(a,1);
		}
		#define S2D_STORE(mem, idx, ev, od) \
			_mm256_storeu_pd(((double*)mem) + (idx)*4,   ev+od); \
			((s2d*)mem)[NLAT_2-1 - (idx)*2] = _mm256_castpd256_pd128(_mm256_shuffle_pd(ev-od, ev-od, 5)); \
			((s2d*)mem)[NLAT_2-2 - (idx)*2] = _mm256_extractf128_pd(_mm256_shuffle_pd(ev-od, ev-od, 5), 1);
		#define S2D_CSTORE(mem, idx, er, od, ei, oi)	{	\
			rnd aa = (rnd)_mm256_shuffle_pd(ei+oi,ei+oi,5) + (er + od);		rnd bb = (er + od) - (rnd)_mm256_shuffle_pd(ei+oi,ei+oi,5);	\
			_mm256_storeu_pd(((double*)mem) + (idx)*4, _mm256_shuffle_pd(bb, aa, 10 )); \
			_mm256_storeu_pd(((double*)mem) + (NPHI-2*im)*NLAT + (idx)*4, _mm256_shuffle_pd(aa, bb, 10 )); \
			aa = (rnd)_mm256_shuffle_pd(er-od,er-od,5) + (ei - oi);		bb = (rnd)_mm256_shuffle_pd(er-od,er-od,5) - (ei - oi);	\
			((s2d*)mem)[NLAT_2-1 -(idx)*2] = _mm256_castpd256_pd128(_mm256_shuffle_pd(bb, aa, 10 ));	\
			((s2d*)mem)[NLAT_2-2 -(idx)*2] = _mm256_extractf128_pd(_mm256_shuffle_pd(bb, aa, 10 ), 1);	\
			((s2d*)mem)[(NPHI+1-2*im)*NLAT_2 -1 -(idx)*2] = _mm256_castpd256_pd128(_mm256_shuffle_pd(aa, bb, 10 ));	\
			((s2d*)mem)[(NPHI+1-2*im)*NLAT_2 -2 -(idx)*2] = _mm256_extractf128_pd(_mm256_shuffle_pd(aa, bb, 10 ), 1);	}
		#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	{	\
			rnd aa = (rnd)_mm256_unpacklo_pd(er+od, ei+oi);	rnd bb = (rnd)_mm256_unpackhi_pd(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*4]   = _mm256_castpd256_pd128(aa);	\
			((s2d*)mem)[(idx)*4+1] = _mm256_castpd256_pd128(bb);	\
			((s2d*)mem)[(idx)*4+2] = _mm256_extractf128_pd(aa, 1);	\
			((s2d*)mem)[(idx)*4+3] = _mm256_extractf128_pd(bb, 1);	\
			aa = (rnd)_mm256_unpacklo_pd(er-od, ei-oi);	bb = (rnd)_mm256_unpackhi_pd(er-od, ei-oi);	\
			((s2d*)mem)[NLAT-1-(idx)*4] = _mm256_castpd256_pd128(aa);	\
			((s2d*)mem)[NLAT-2-(idx)*4] = _mm256_castpd256_pd128(bb);	\
			((s2d*)mem)[NLAT-3-(idx)*4] = _mm256_extractf128_pd(aa, 1);	\
			((s2d*)mem)[NLAT-4-(idx)*4] = _mm256_extractf128_pd(bb, 1);	}
		#define S2D_STORE_4MAGIC(mem, idx, ev, od)	{ \
			rnd n = ev+od;		rnd s = ev-od;	\
			rnd a = _mm256_unpacklo_pd(n, s);	rnd b = _mm256_unpackhi_pd(n, s);	\
			((s2d*)mem)[(idx)*4] = _mm256_castpd256_pd128(a);	\
			((s2d*)mem)[(idx)*4+1] = _mm256_castpd256_pd128(b);	\
			((s2d*)mem)[(idx)*4+2] = _mm256_extractf128_pd(a, 1);	\
			((s2d*)mem)[(idx)*4+3] = _mm256_extractf128_pd(b, 1);	}
		#define S2D_CSTORE_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			rnd nr = er + od;		rnd ni = ei + oi;	\
			rnd sr = er - od;		rnd si = ei - oi;	\
			rnd a0 = _mm256_unpacklo_pd(nr-si,  sr+ni);	\
			rnd b0 = _mm256_unpacklo_pd(nr+si,  sr-ni);	\
			rnd a1 = _mm256_unpackhi_pd(nr-si,  sr+ni);	\
			rnd b1 = _mm256_unpackhi_pd(nr+si,  sr-ni);	\
			((s2d*)mem)[(idx)*4] = _mm256_castpd256_pd128(a0);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*4] = _mm256_castpd256_pd128(b0);	\
			((s2d*)mem)[(idx)*4+1] = _mm256_castpd256_pd128(a1);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*4+1] = _mm256_castpd256_pd128(b1);	\
			((s2d*)mem)[(idx)*4+2] = _mm256_extractf128_pd(a0, 1);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*4+2] = _mm256_extractf128_pd(b0, 1);	\
			((s2d*)mem)[(idx)*4+3] = _mm256_extractf128_pd(a1, 1);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*4+3] = _mm256_extractf128_pd(b1, 1);	}		
		#define S2D_CSTORE2_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			rnd aa = (rnd)_mm256_unpacklo_pd(er+od, ei+oi);	rnd bb = (rnd)_mm256_unpackhi_pd(er+od, ei+oi);	\
			rnd cc = (rnd)_mm256_unpacklo_pd(er-od, ei-oi);	rnd dd = (rnd)_mm256_unpackhi_pd(er-od, ei-oi);	\
			((s2d*)mem)[(idx)*8]   = _mm256_castpd256_pd128(aa);	\
			((s2d*)mem)[(idx)*8+1] = _mm256_castpd256_pd128(cc);	\
			((s2d*)mem)[(idx)*8+2] = _mm256_castpd256_pd128(bb);	\
			((s2d*)mem)[(idx)*8+3] = _mm256_castpd256_pd128(dd);	\
			((s2d*)mem)[(idx)*8+4] = _mm256_extractf128_pd(aa, 1);	\
			((s2d*)mem)[(idx)*8+5] = _mm256_extractf128_pd(cc, 1);	\
			((s2d*)mem)[(idx)*8+6] = _mm256_extractf128_pd(bb, 1);	\
			((s2d*)mem)[(idx)*8+7] = _mm256_extractf128_pd(dd, 1);	}
	#else
		#define MIN_ALIGNMENT 16
		#define VSIZE2 2
		typedef double rnd __attribute__ ((vector_size (VSIZE2*8)));		// vector of 2 doubles.
		// Allocate memory aligned on 16 bytes for SSE2 (fftw_malloc works only if fftw was compiled with --enable-sse2)
		// in 64 bit systems, malloc should be 16 bytes aligned anyway.
		#define VMALLOC(s)	( (sizeof(void*) >= 8) ? malloc(s) : _mm_malloc(s, MIN_ALIGNMENT) )
		#define VFREE(s)	( (sizeof(void*) >= 8) ? free(s) : _mm_free(s) )
		#ifdef __SSE3__
			#include <pmmintrin.h>
			#define _SIMD_NAME_ "sse3"
			inline static v2d v2d_reduce(v2d a, v2d b) {
				return _mm_hadd_pd(a,b);
			}
		#else
			#include <emmintrin.h>
			#define _SIMD_NAME_ "sse2"
			inline static v2d v2d_reduce(v2d a, v2d b) {
				v2d c = _mm_unpacklo_pd(a, b);		b = _mm_unpackhi_pd(a, b);
				return b + c;
			}
		#endif
		#define reduce_add(a) ( _mm_cvtsd_f64(a) + _mm_cvtsd_f64(_mm_unpackhi_pd(a,a)) )
		#define vall(x) ((rnd) _mm_set1_pd(x))
		#define vread(mem, idx) ((s2d*)mem)[idx]
		#define vstor(mem, idx, v) ((s2d*)mem)[idx] = v
		#define S2D_STORE(mem, idx, ev, od)		((s2d*)mem)[idx] = ev+od;		((s2d*)mem)[NLAT_2-1 - (idx)] = vxchg(ev-od);
		#define S2D_CSTORE(mem, idx, er, od, ei, oi)	{	\
			rnd aa = vxchg(ei + oi) + (er + od);		rnd bb = (er + od) - vxchg(ei + oi);	\
			((s2d*)mem)[idx] = _mm_shuffle_pd(bb, aa, 2 );	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)] = _mm_shuffle_pd(aa, bb, 2 );	\
			aa = vxchg(er - od) + (ei - oi);		bb = vxchg(er - od) - (ei - oi);	\
			((s2d*)mem)[NLAT_2-1 -(idx)] = _mm_shuffle_pd(bb, aa, 2 );	\
			((s2d*)mem)[(NPHI+1-2*im)*NLAT_2 -1 -(idx)] = _mm_shuffle_pd(aa, bb, 2 );	}
		#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	{	\
			((s2d*)mem)[(idx)*2]   = _mm_unpacklo_pd(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*2+1] = _mm_unpackhi_pd(er+od, ei+oi);	\
			((s2d*)mem)[NLAT-1-(idx)*2] = _mm_unpacklo_pd(er-od, ei-oi);	\
			((s2d*)mem)[NLAT-2-(idx)*2] = _mm_unpackhi_pd(er-od, ei-oi);	}
		#define S2D_STORE_4MAGIC(mem, idx, ev, od)		{	\
			s2d n = ev+od;		s2d s = ev-od;	\
			((s2d*)mem)[(idx)*2] = _mm_unpacklo_pd(n, s);	\
			((s2d*)mem)[(idx)*2+1] = _mm_unpackhi_pd(n, s);	}
		#define S2D_CSTORE_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			rnd nr = er + od;		rnd ni = ei + oi;	\
			rnd sr = er - od;		rnd si = ei - oi;	\
			((s2d*)mem)[(idx)*2] = _mm_unpacklo_pd(nr-si,  sr+ni);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*2] = _mm_unpacklo_pd(nr+si,  sr-ni);	\
			((s2d*)mem)[(idx)*2+1] = _mm_unpackhi_pd(nr-si,  sr+ni);	\
			((s2d*)mem)[(NPHI-2*im)*NLAT_2 + (idx)*2+1] = _mm_unpackhi_pd(nr+si,  sr-ni);	}
		#define S2D_CSTORE2_4MAGIC(mem, idx, er, od, ei, oi)	{	\
			((s2d*)mem)[(idx)*4]   = _mm_unpacklo_pd(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*4+1] = _mm_unpacklo_pd(er-od, ei-oi);	\
			((s2d*)mem)[(idx)*4+2] = _mm_unpackhi_pd(er+od, ei+oi);	\
			((s2d*)mem)[(idx)*4+3] = _mm_unpackhi_pd(er-od, ei-oi);	}
	#endif
	#ifdef __SSE3__
		#define addi(a,b) _mm_addsub_pd(a, _mm_shuffle_pd((b),(b),1))		// a + I*b
		#define subadd(a,b) _mm_addsub_pd(a, b)		// [al-bl, ah+bh]
		//#define CMUL(a,b) _mm_addsub_pd(_mm_shuffle_pd(a,a,0)*b, _mm_shuffle_pd(a,a,3)*_mm_shuffle_pd(b,b,1))
	#else
		#define addi(a,b) ( (a) + (_mm_shuffle_pd((b),(b),1) * _mm_set_pd(1.0, -1.0)) )		// a + I*b		[note: _mm_set_pd(imag, real)) ]
		#define subadd(a,b) ( (a) + (b) * _mm_set_pd(1.0, -1.0) )		// [al-bl, ah+bh]
	#endif

	// build mask (-0, -0) to change sign of both hi and lo values using xorpd
	#define SIGN_MASK_2  _mm_castsi128_pd(_mm_slli_epi64(_mm_cmpeq_epi16(_mm_set1_epi64x(0), _mm_set1_epi64x(0)), 63))
	// build mask (0, -0) to change sign of hi value using xorpd (used in CFFT_TO_2REAL)
	#define SIGN_MASK_HI  _mm_unpackhi_pd(vdup(0.0), SIGN_MASK_2 )
	// build mask (-0, 0) to change sign of lo value using xorpd
	#define SIGN_MASK_LO  _mm_unpackhi_pd(SIGN_MASK_2, vdup(0.0) )

	// vset(lo, hi) takes two doubles and pack them in a vector
	#define vset(lo, hi) _mm_set_pd(hi, lo)
	// vdup(x) takes a double and duplicate it to a vector of 2 doubles.
	#define vdup(x) ((s2d)_mm_set1_pd(x))
	// vxchg(a) exchange hi and lo component of vector a
	#define vxchg(a) ((v2d)_mm_shuffle_pd(a,a,1))
	#define vlo_to_cplx(a) _mm_unpacklo_pd(a, vdup(0.0))
	#define vhi_to_cplx(a) _mm_unpackhi_pd(a, vdup(0.0))
	#define vcplx_real(a) vlo_to_dbl(a)
	#define vcplx_imag(a) vhi_to_dbl(a)
	#ifdef __clang__
		// allow to compile with clang (llvm)
		#define vlo(a) (a)[0]
		#define vlo_to_dbl(a) (a)[0]
		#define vhi_to_dbl(a) (a)[1]
	#else
		// gcc extensions
		#ifdef __AVX512F__
			#define vlo(a) _mm_cvtsd_f64(_mm512_castpd512_pd128(a))
		#elif defined __AVX__
			#define vlo(a) _mm_cvtsd_f64(_mm256_castpd256_pd128(a))
		#else
			#define vlo(a) _mm_cvtsd_f64(a)
		#endif
		#define vlo_to_dbl(a) _mm_cvtsd_f64(a)
		#define vhi_to_dbl(a) _mm_cvtsd_f64(_mm_unpackhi_pd(a,a))
	#endif
#endif



#ifndef _GCC_VEC_
	#define MIN_ALIGNMENT 16
	#define VSIZE 1
	#define VSIZE2 1
	#define _SIMD_NAME_ "scalar"
	typedef double s2d;
	typedef complex double v2d;
	typedef double rnd;
	#define vread(mem, idx) ((double*)mem)[idx]
	#define vstor(mem, idx, v) ((double*)mem)[idx] = v;
	#define reduce_add(a) (a)
	#define v2d_reduce(a,b) ((a) +I*(b))	
	#define vlo(a) (a)
	#define vall(x) (x)
	#define vdup(x) (x)
	#define vxchg(x) (x)
	#define addi(a,b) ((a) + I*(b))
	#define vlo_to_dbl(a) (a)
	#define vhi_to_dbl(a) (a)
	#define vcplx_real(a) creal(a)
	#define vcplx_imag(a) cimag(a)

	// allocate memory aligned for FFTW. In 64 bit systems, malloc should be 16 bytes aligned.
	#define VMALLOC(s)	( (sizeof(void*) >= 8) ? malloc(s) : fftw_malloc(s) )
	#define VFREE(s)	( (sizeof(void*) >= 8) ? free(s) : fftw_free(s) )

	#define S2D_STORE(mem, idx, ev, od)		(mem)[idx] = ev+od;		(mem)[NLAT-1 - (idx)] = ev-od;
	#define S2D_CSTORE(mem, idx, er, od, ei, oi)	mem[idx] = (er+od) + I*(ei+oi); 	mem[NLAT-1-(idx)] = (er-od) + I*(ei-oi);
	#define S2D_CSTORE2(mem, idx, er, od, ei, oi)	mem[idx] = (er+od) + I*(ei+oi); 	mem[NLAT-1-(idx)] = (er-od) + I*(ei-oi);

	#define S2D_STORE_4MAGIC(mem, idx, ev, od)	(mem)[2*(idx)+1] = ev+od;		(mem)[2*(idx)+1] = ev-od;
	#define S2D_CSTORE_4MAGIC(mem, idx, er, od, ei, oi)		mem[2*(idx)] = (er+od) + I*(ei+oi);		mem[2*(idx)+1] = (er-od) + I*(ei-oi);
	#define S2D_CSTORE2_4MAGIC(mem, idx, er, od, ei, oi)	mem[2*(idx)] = (er+od) + I*(ei+oi);		mem[2*(idx)+1] = (er-od) + I*(ei-oi);
#endif


#define SSE __attribute__((aligned (MIN_ALIGNMENT)))

/// align pointer on MIN_ALIGNMENT (must be a power of 2)
#define PTR_ALIGN(p) ((((size_t)(p)) + (MIN_ALIGNMENT-1)) & (~((size_t)(MIN_ALIGNMENT-1))))


struct DtDp {		// theta and phi derivatives stored together.
	double t, p;
};

#define GLUE2(a,b) a##b
#define GLUE3(a,b,c) a##b##c

// verbose printing
#if SHT_VERBOSE > 1
  #define PRINT_VERB(msg) printf(msg)
#else
  #define PRINT_VERB(msg) (0)
#endif

// compute symmetric and antisymmetric parts, and reorganize data.
#ifndef SHTNS4MAGIC
  #define SYM_ASYM_M0_V(F, er, od) { \
	long int k=0; do { \
		double an = F[k*k_inc];				double bn = F[k*k_inc +1]; \
		double bs = F[(NLAT-2-k)*k_inc];	double as = F[(NLAT-2-k)*k_inc +1]; \
		er[k] = an+as;			od[k] = an-as; \
		er[k+1] = bn+bs;		od[k+1] = bn-bs; \
		k+=2; \
	} while(k < nk*VSIZE2); }
  #define SYM_ASYM_M0_Q(F, er, od, acc0) { \
	double r0a = 0.0;	double r0b = 0.0; \
	long int k=0; do { \
		double an = F[k*k_inc];				double bn = F[k*k_inc +1]; \
		double bs = F[(NLAT-2-k)*k_inc];	double as = F[(NLAT-2-k)*k_inc +1]; \
		er[k] = an+as;			od[k] = an-as; \
		er[k+1] = bn+bs;		od[k+1] = bn-bs; \
		r0a += (an+as)*wg[k];	r0b += (bn+bs)*wg[k+1]; \
		k+=2; \
	} while(k < nk*VSIZE2); 	acc0 = r0a+r0b; }
  #define SYM_ASYM_Q(F, er, od, ei, oi, k0v) { \
	long int k = ((k0v*VSIZE2)>>1)*2; \
	do { \
		double an, bn, ani, bni, bs, as, bsi, asi, t; \
		ani = F[im*m_inc + k*k_inc];		bni = F[im*m_inc + k*k_inc +1]; \
		an  = F[(NPHI-im)*m_inc + k*k_inc];	bn = F[(NPHI-im)*m_inc + k*k_inc +1]; \
		t = ani-an;	an += ani;		ani = bn-bni;		bn += bni;		bni = t; \
		bsi = F[im*m_inc + (NLAT-2 -k)*k_inc];		asi = F[im*m_inc + (NLAT-2-k)*k_inc + 1]; \
		bs = F[(NPHI-im)*m_inc +(NLAT-2-k)*k_inc];	as = F[(NPHI-im)*m_inc +(NLAT-2-k)*k_inc +1]; \
		t = bsi-bs;		bs += bsi;		bsi = as-asi;		as += asi;		asi = t; \
		er[k] = an+as;		ei[k] = ani+asi;		er[k+1] = bn+bs;		ei[k+1] = bni+bsi; \
		od[k] = an-as;		oi[k] = ani-asi;		od[k+1] = bn-bs;		oi[k+1] = bni-bsi; \
		k+=2; \
		} while (k<nk*VSIZE2); }
  #define SYM_ASYM_Q3(F, er, od, ei, oi, k0v) { \
	long int k = ((k0v*VSIZE2)>>1)*2; \
	do { \
		double an, bn, ani, bni, bs, as, bsi, asi, t; \
		double sina = st[k];	double sinb = st[k+1]; \
		ani = F[im*m_inc + k*k_inc];		bni = F[im*m_inc + k*k_inc +1]; \
		an  = F[(NPHI-im)*m_inc + k*k_inc];	bn = F[(NPHI-im)*m_inc + k*k_inc +1]; \
		t = ani-an;	an += ani;		ani = bn-bni;		bn += bni;		bni = t; \
		bsi = F[im*m_inc + (NLAT-2 -k)*k_inc];		asi = F[im*m_inc + (NLAT-2-k)*k_inc + 1]; \
		bs = F[(NPHI-im)*m_inc +(NLAT-2-k)*k_inc];	as = F[(NPHI-im)*m_inc +(NLAT-2-k)*k_inc +1]; \
		t = bsi-bs;		bs += bsi;		bsi = as-asi;		as += asi;		asi = t; \
		er[k] = (an+as)*sina;	ei[k] = (ani+asi)*sina;		er[k+1] = (bn+bs)*sinb;		ei[k+1] = (bni+bsi)*sinb; \
		od[k] = (an-as)*sina;	oi[k] = (ani-asi)*sina;		od[k+1] = (bn-bs)*sinb;		oi[k+1] = (bni-bsi)*sinb; \
		k+=2; \
		} while (k<nk*VSIZE2); }
  #define SYM_ASYM_V SYM_ASYM_Q
#else /* SHTNS4MAGIC */
  #define SYM_ASYM_M0_V(F, er, od) { \
	long int k=0; do { \
		double st_1 = 1.0/st[k]; \
		double an = F[2*k*k_inc];		double as = F[2*k*k_inc +1]; \
		er[k] = (an+as)*st_1;			od[k] = (an-as)*st_1; \
		k+=1; \
	} while(k < nk*VSIZE2); }
  #define SYM_ASYM_M0_Q(F, er, od, acc0) { \
	acc0 = 0.0; \
	long int k=0; do { \
		double an = F[2*k*k_inc];	double as = F[2*k*k_inc +1]; \
		er[k] = (an+as);			od[k] = (an-as); \
		acc0 += (an+as)*wg[k]; \
		k+=1; \
	} while(k < nk*VSIZE2); }
  #define SYM_ASYM_Q(F, er, od, ei, oi, k0v) { \
	k = ((k0v*VSIZE2)>>1)*2; \
	do { \
		double ar,ai,br,bi, sr,si,nr,ni; \
		br = F[im*m_inc + 2*k*k_inc];			bi = F[im*m_inc + 2*k*k_inc +1]; \
		ar = F[(NPHI-im)*m_inc + 2*k*k_inc];	ai = F[(NPHI-im)*m_inc + 2*k*k_inc +1]; \
		nr = ar + br;		ni = ai - bi; \
		sr = ai + bi;		si = br - ar; \
		er[k] = nr+sr;		ei[k] = ni+si; \
		od[k] = nr-sr;		oi[k] = ni-si; \
		k+=1; \
	} while(k < nk*VSIZE2); }
  #define SYM_ASYM_Q3(F, er, od, ei, oi, k0v) { \
	k = ((k0v*VSIZE2)>>1)*2; \
	do { \
		double ar,ai,br,bi, sr,si,nr,ni; \
		double sina = st[k]; \
		br = F[im*m_inc + 2*k*k_inc];			bi = F[im*m_inc + 2*k*k_inc +1]; \
		ar = F[(NPHI-im)*m_inc + 2*k*k_inc];	ai = F[(NPHI-im)*m_inc + 2*k*k_inc +1]; \
		nr = (ar + br)*sina;		ni = (ai - bi)*sina; \
		sr = (ai + bi)*sina;		si = (br - ar)*sina; \
		er[k] = nr+sr;		ei[k] = ni+si; \
		od[k] = nr-sr;		oi[k] = ni-si; \
		k+=1; \
	} while(k < nk*VSIZE2); }
  #define SYM_ASYM_V(F, er, od, ei, oi, k0v) { \
	k = ((k0v*VSIZE2)>>1)*2; \
	do { \
		double ar,ai,br,bi, sr,si,nr,ni; \
		double sina = 1.0/st[k]; \
		br = F[im*m_inc + 2*k*k_inc];			bi = F[im*m_inc + 2*k*k_inc +1]; \
		ar = F[(NPHI-im)*m_inc + 2*k*k_inc];	ai = F[(NPHI-im)*m_inc + 2*k*k_inc +1]; \
		nr = (ar + br)*sina;		ni = (ai - bi)*sina; \
		sr = (ai + bi)*sina;		si = (br - ar)*sina; \
		er[k] = nr+sr;		ei[k] = ni+si; \
		od[k] = nr-sr;		oi[k] = ni-si; \
		k+=1; \
	} while(k < nk*VSIZE2); }
#endif
