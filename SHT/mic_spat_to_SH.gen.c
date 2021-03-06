/*
 * Copyright (c) 2010-2018 Centre National de la Recherche Scientifique.
 * written by Nathanael Schaeffer (CNRS, ISTerre, Grenoble, France).
 * 
 * nathanael.schaeffer@univ-grenoble-alpes.fr
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

# This file is meta-code for SHT.c (spherical harmonic transform).
# it is intended for "make" to generate C code for 3 similar SHT functions,
# (namely spat_to_SH [Q tag]), spat_to_SHsphtor [V tag], spat_to_SH3 [both Q&V tags])
# from one generic function + tags.
# Basically, there are tags at the beginning of lines (Q,V) that are information
# to keep or remove the line depending on the function to build. (Q for scalar, V for vector, # for comment)
#
//////////////////////////////////////////////////

	static
QX	void GEN3(_an1,NWAY,SUFFIX)(shtns_cfg shtns, double *BrF, cplx *Qlm, const long int llim, const int imlim)
VX	void GEN3(_an2,NWAY,SUFFIX)(shtns_cfg shtns, double *BtF, double *BpF, cplx *Slm, cplx *Tlm, const long int llim, const int imlim)
3	void GEN3(_an3,NWAY,SUFFIX)(shtns_cfg shtns, double *BrF, double *BtF, double *BpF, cplx *Qlm, cplx *Slm, cplx *Tlm, const long int llim, const int imlim)
  {
	#define NW (NWAY*2)

	double *alm, *al;
	double *wg, *ct, *st;
V	double *l_2;
	long int nk, k, l,m;
	unsigned m0, mstep;
	int k_inc, m_inc;
  #ifndef SHT_AXISYM
	unsigned im;
  #endif
Q	v2d qq[llim+2];
V	v2d vw[2*llim+4];

Q	double rer[NLAT_2 + NW*VSIZE2] SSE;
Q	double ror[NLAT_2 + NW*VSIZE2] SSE;
V	double ter[NLAT_2 + NW*VSIZE2] SSE;
V	double tor[NLAT_2 + NW*VSIZE2] SSE;
V	double per[NLAT_2 + NW*VSIZE2] SSE;
V	double por[NLAT_2 + NW*VSIZE2] SSE;
  #ifndef SHT_AXISYM
Q	double rei[NLAT_2 + NW*VSIZE2] SSE;
Q	double roi[NLAT_2 + NW*VSIZE2] SSE;
V	double tei[NLAT_2 + NW*VSIZE2] SSE;
V	double toi[NLAT_2 + NW*VSIZE2] SSE;
V	double pei[NLAT_2 + NW*VSIZE2] SSE;
V	double poi[NLAT_2 + NW*VSIZE2] SSE;
  #endif

	nk = NLAT_2;	// copy NLAT_2 to a local variable for faster access (inner loop limit)
	#if _GCC_VEC_
	  nk = ((unsigned) nk+(VSIZE2-1))/VSIZE2;
	#endif
	wg = shtns->wg;		ct = shtns->ct;		st = shtns->st;
V	l_2 = shtns->l_2;
	for (k=nk*VSIZE2; k<(nk-1+NW)*VSIZE2; ++k) {		// never written, so this is now done for all m's
Q		rer[k] = 0.0;		ror[k] = 0.0;
V		ter[k] = 0.0;		tor[k] = 0.0;
V		per[k] = 0.0;		por[k] = 0.0;
	  #ifndef SHT_AXISYM
Q		rei[k] = 0.0;		roi[k] = 0.0;
V		tei[k] = 0.0;		toi[k] = 0.0;
V		pei[k] = 0.0;		poi[k] = 0.0;
	  #endif
	}

	// ACCESS PATTERN
	k_inc = shtns->k_stride_a;		m_inc = shtns->m_stride_a;

	#ifndef _OPENMP
		m0 = 0;		mstep = 1;
	#else
		m0 = omp_get_thread_num();
		mstep = omp_get_num_threads();
		if (m0 == 0)
	#endif
	{		// im=0 : dzl.p = 0.0 and evrything is REAL
		alm = shtns->blm;
Q		double r0 = 0.0;
		// compute symmetric and antisymmetric parts. (do not weight here, it is cheaper to weight y0)
Q		SYM_ASYM_M0_Q(BrF, rer, ror, r0)
V		SYM_ASYM_M0_V(BtF, ter, tor)
V		SYM_ASYM_M0_V(BpF, per, por)
Q		Qlm[0] = r0 * alm[0];			// l=0 is done.
V		Slm[0] = 0.0;		Tlm[0] = 0.0;		// l=0 is zero for the vector transform.
		k = 0;
Q		double* q_ = (double*) qq;
V		double* v_ = (double*) vw;
		for (l=0;l<llim;++l) {
Q			q_[l] = 0.0;
V			v_[2*l] = 0.0;		v_[2*l+1] = 0.0;
		}
		do {
			al = alm;
			rnd cost[NW], y0[NW], y1[NW];
V			rnd sint[NW], dy0[NW], dy1[NW];
Q			rnd rerk[NW], rork[NW];		// help the compiler to cache into registers.
V			rnd terk[NW], tork[NW], perk[NW], pork[NW];
			for (int j=0; j<NW; ++j) {
				cost[j] = vread(ct, k+j);
				y0[j] = vall(al[0]) * vread(wg, k+j);		// weight of Gauss quadrature appears here
V				dy0[j] = vall(0.0);
V				sint[j] = -vread(st, k+j);
				y1[j] =  (vall(al[1])*y0[j]) * cost[j];
V				dy1[j] = (vall(al[1])*y0[j]) * sint[j];
Q				rerk[j] = vread(rer, k+j);		rork[j] = vread(ror, k+j);		// cache into registers.
V				terk[j] = vread(ter, k+j);		tork[j] = vread(tor, k+j);
V				perk[j] = vread(per, k+j);		pork[j] = vread(por, k+j);
			}
			al+=2;	l=1;
			while(l<llim) {
				for (int j=0; j<NW; ++j) {
V					dy0[j] = vall(al[1])*(cost[j]*dy1[j] + y1[j]*sint[j]) + vall(al[0])*dy0[j];
					y0[j]  = vall(al[1])*(cost[j]*y1[j]) + vall(al[0])*y0[j];
				}
Q				rnd q = y1[0] * rork[0];
V				rnd s = dy1[0] * terk[0];
V				rnd t = dy1[0] * perk[0];
				for (int j=1; j<NW; ++j) {
Q					q += y1[j] * rork[j];
V					s += dy1[j] * terk[j];
V					t += dy1[j] * perk[j];
				}
Q				q_[l-1]   += reduce_add(q);
V				v_[2*l-2] += reduce_add(s);
V				v_[2*l-1] -= reduce_add(t);
				for (int j=0; j<NW; ++j) {
V					dy1[j] = vall(al[3])*(cost[j]*dy0[j] + y0[j]*sint[j]) + vall(al[2])*dy1[j];
					y1[j]  = vall(al[3])*(cost[j]*y0[j]) + vall(al[2])*y1[j];
				}
Q				q = y0[0] * rerk[0];
V				s = dy0[0] * tork[0];
V				t = dy0[0] * pork[0];
				for (int j=1; j<NW; ++j) {
Q					q += y0[j] * rerk[j];
V					s += dy0[j] * tork[j];
V					t += dy0[j] * pork[j];
				}
Q				q_[l]     += reduce_add(q);
V				v_[2*l]   += reduce_add(s);
V				v_[2*l+1] -= reduce_add(t);
				al+=4;	l+=2;
			}
			if (l==llim) {
Q				rnd q = y1[0] * rork[0];
V				rnd s = dy1[0] * terk[0];
V				rnd t = dy1[0] * perk[0];
				for (int j=1; j<NW; ++j) {
Q					q += y1[j] * rork[j];
V					s += dy1[j] * terk[j];
V					t += dy1[j] * perk[j];
				}
Q				q_[l-1]   += reduce_add(q);
V				v_[2*l-2] += reduce_add(s);
V				v_[2*l-1] -= reduce_add(t);
			}
			k+=NW;
		} while (k < nk);
		for (l=1; l<=llim; ++l) {
Q			Qlm[l] = q_[l-1];
V			Slm[l] = v_[2*l-2]*l_2[l];		Tlm[l] = v_[2*l-1]*l_2[l];
		}
		#ifdef SHT_VAR_LTR
			for (l=llim+1; l<= LMAX; ++l) {
Q				((v2d*)Qlm)[l] = vdup(0.0);
V				((v2d*)Slm)[l] = vdup(0.0);		((v2d*)Tlm)[l] = vdup(0.0);
			}
			#ifndef SHT_AXISYM
			if (imlim <= MMAX) {		// zero out m >= imlim
				l = LiM(shtns, imlim*MRES, imlim);
				do {
Q					((v2d*)Qlm)[l] = vdup(0.0);
V					((v2d*)Slm)[l] = vdup(0.0);		((v2d*)Tlm)[l] = vdup(0.0);
				} while(++l < shtns->nlm);
			}
			#endif
		#endif
		m0=mstep;
	}

  #ifndef SHT_AXISYM
	for (im=m0; im<imlim; im+=mstep) {
		m = im*MRES;
		l = shtns->tm[im] / VSIZE2;
		alm = shtns->blm + im*(2*(LMAX+1) -m+MRES);
		// compute symmetric and anti-symmetric parts:
QX		SYM_ASYM_Q(BrF, rer, ror, rei, roi, l)
3		SYM_ASYM_Q3(BrF, rer, ror, rei, roi, l)
V		SYM_ASYM_V(BtF, ter, tor, tei, toi, l)
V		SYM_ASYM_V(BpF, per, por, pei, poi, l)
		k=l;
		for (l=0; l<=llim-m+1; l++) {
Q			qq[l] = vdup(0.0);
V			vw[2*l] = vdup(0.0);		vw[2*l+1] = vdup(0.0);
		}
		do {
Q			v2d* q = qq;
V			v2d* v = vw;
			al = alm;
			rnd cost[NW], y0[NW], y1[NW];
Q			rnd rerk[NW], reik[NW], rork[NW], roik[NW];		// help the compiler to cache into registers.
V			rnd terk[NW], teik[NW], tork[NW], toik[NW];
V			rnd perk[NW], peik[NW], pork[NW], poik[NW];
			for (int j=0; j<NW; ++j) {
				cost[j] = vread(st, k+j);
				y0[j] = vall(0.5);
			}
Q			l=m;
V			l=m-1;
			long int ny = 0;	// exponent to extend double precision range.
		if ((int)llim <= SHT_L_RESCALE_FLY) {
			do {		// sin(theta)^m
				if (l&1) for (int j=0; j<NW; ++j) y0[j] *= cost[j];
				for (int j=0; j<NW; ++j) cost[j] *= cost[j];
			} while(l >>= 1);
		} else {
			long int nsint = 0;
			do {		// sin(theta)^m		(use rescaling to avoid underflow)
				if (l&1) {
					for (int j=NW-1; j>=0; --j) y0[j] *= cost[j];
					ny += nsint;
					if (vlo(y0[NW-1]) < (SHT_ACCURACY+1.0/SHT_SCALE_FACTOR)) {
						ny--;
						for (int j=NW-1; j>=0; --j) y0[j] *= vall(SHT_SCALE_FACTOR);
					}
				}
				for (int j=NW-1; j>=0; --j) cost[j] *= cost[j];
				nsint += nsint;
				if (vlo(cost[NW-1]) < 1.0/SHT_SCALE_FACTOR) {
					nsint--;
					for (int j=NW-1; j>=0; --j) cost[j] *= vall(SHT_SCALE_FACTOR);
				}
			} while(l >>= 1);
		}
			for (int j=0; j<NW; ++j) {
				y0[j] *= vall(al[0]);
				cost[j] = vread(ct, k+j);
				y1[j]  = (vall(al[1])*y0[j]) *cost[j];
			}
			l=m;	al+=2;
			while ((ny<0) && (l<llim)) {		// ylm treated as zero and ignored if ny < 0
				for (int j=0; j<NW; ++j) {
					y0[j] = vall(al[1])*(cost[j]*y1[j]) + vall(al[0])*y0[j];
				}
				for (int j=0; j<NW; ++j) {
					y1[j] = vall(al[3])*(cost[j]*y0[j]) + vall(al[2])*y1[j];
				}
				l+=2;	al+=4;
				if (fabs(vlo(y0[NW-1])) > SHT_ACCURACY*SHT_SCALE_FACTOR + 1.0) {		// rescale when value is significant
					++ny;
					for (int j=0; j<NW; ++j) {
						y0[j] *= vall(1.0/SHT_SCALE_FACTOR);		y1[j] *= vall(1.0/SHT_SCALE_FACTOR);
					}
				}
			}
		  if (ny == 0) {
Q			q+=(l-m);
V			v+=2*(l-m);
			for (int j=0; j<NW; ++j) {	// prefetch
				y0[j] *= vread(wg, k+j);		y1[j] *= vread(wg, k+j);		// weight appears here (must be after the previous accuracy loop).
Q				rerk[j] = vread( rer, k+j);		reik[j] = vread( rei, k+j);		rork[j] = vread( ror, k+j);		roik[j] = vread( roi, k+j);
V				terk[j] = vread( ter, k+j);		teik[j] = vread( tei, k+j);		tork[j] = vread( tor, k+j);		toik[j] = vread( toi, k+j);
V				perk[j] = vread( per, k+j);		peik[j] = vread( pei, k+j);		pork[j] = vread( por, k+j);		poik[j] = vread( poi, k+j);
			}
			while (l<llim) {	// compute even and odd parts
Q				rnd qq0 = y0[0] * rerk[0];
Q				rnd qq1 = y0[0] * reik[0];
Q				for (int j=1; j<NW; ++j) {	qq0 += y0[j] * rerk[j];		qq1 += y0[j] * reik[j];	}	// real even, imag even
V				rnd vv0 = y0[0] * terk[0];
V				rnd vv1 = y0[0] * teik[0];
V				for (int j=1; j<NW; ++j) {	vv0 += y0[j] * terk[j];		vv1 += y0[j] * teik[j];	}	// real even, imag even
V				rnd ww0 = y0[0] * perk[0];
V				rnd ww1 = y0[0] * peik[0];
V				for (int j=1; j<NW; ++j) {	ww0 += y0[j] * perk[j];		ww1 += y0[j] * peik[j];	}	// real even, imag even
Q				q[0] += v2d_reduce(qq0, qq1);
V				v[0] += v2d_reduce(vv0, vv1);
V				v[1] += v2d_reduce(ww0, ww1);
				for (int j=0; j<NW; ++j) {
					y0[j] = vall(al[1])*(cost[j]*y1[j]) + vall(al[0])*y0[j];
				}
Q				qq0 = y1[0] * rork[0];
Q				qq1 = y1[0] * roik[0];
Q				for (int j=1; j<NW; ++j) {	qq0 += y1[j] * rork[j];		qq1 += y1[j] * roik[j]; }	// real odd, imag odd
V				vv0 = y1[0] * tork[0];
V				vv1 = y1[0] * toik[0];
V				for (int j=1; j<NW; ++j) {	vv0 += y1[j] * tork[j];		vv1 += y1[j] * toik[j]; }	// real odd, imag odd
V				ww0 = y1[0] * pork[0];
V				ww1 = y1[0] * poik[0];
V				for (int j=1; j<NW; ++j) {	ww0 += y1[j] * pork[j];		ww1 += y1[j] * poik[j]; }	// real odd, imag odd
Q				q[1] += v2d_reduce(qq0, qq1);
V				v[2] += v2d_reduce(vv0, vv1);
V				v[3] += v2d_reduce(ww0, ww1);
Q				q+=2;
V				v+=4;
				for (int j=0; j<NW; ++j) {
					y1[j] = vall(al[3])*(cost[j]*y0[j]) + vall(al[2])*y1[j];
				}
				l+=2;	al+=4;
			}
			{
V				rnd vv0 = y0[0] * terk[0];
V				rnd vv1 = y0[0] * teik[0];
V				rnd ww0 = y0[0] * perk[0];
V				rnd ww1 = y0[0] * peik[0];
V				for (int j=1; j<NW; ++j) {	vv0 += y0[j] * terk[j];		vv1 += y0[j] * teik[j];	}	// real even, imag even
V				for (int j=1; j<NW; ++j) {	ww0 += y0[j] * perk[j];		ww1 += y0[j] * peik[j];	}	// real even, imag even
V				v[0] += v2d_reduce(vv0, vv1);
V				v[1] += v2d_reduce(ww0, ww1);
			}
			if (l==llim) {
Q				rnd qq0 = y0[0] * rerk[0];
Q				rnd qq1 = y0[0] * reik[0];
V				rnd vv0 = y1[0] * tork[0];
V				rnd vv1 = y1[0] * toik[0];
V				rnd ww0 = y1[0] * pork[0];
V				rnd ww1 = y1[0] * poik[0];
Q				for (int j=1; j<NW; ++j) {	qq0 += y0[j] * rerk[j];		qq1 += y0[j] * reik[j];	}	// real even, imag even
V				for (int j=1; j<NW; ++j) {	vv0 += y1[j] * tork[j];		vv1 += y1[j] * toik[j]; }	// real odd, imag odd
V				for (int j=1; j<NW; ++j) {	ww0 += y1[j] * pork[j];		ww1 += y1[j] * poik[j]; }	// real odd, imag odd
Q				q[0] += v2d_reduce(qq0, qq1);
V				v[2] += v2d_reduce(vv0, vv1);
V				v[3] += v2d_reduce(ww0, ww1);
			}
		  }
			k+=NW;
		} while (k < nk);
		l = LiM(shtns, m, im);
Q		v2d *Ql = (v2d*) &Qlm[l];
V		v2d *Sl = (v2d*) &Slm[l];
V		v2d *Tl = (v2d*) &Tlm[l];

Q		for (l=0; l<=llim-m; ++l)	Ql[l] = qq[l];

V		{	// convert from the two scalar SH to vector SH
V			// Slm = - (I*m*Wlm + MX*Vlm) / (l*(l+1))		=> why does this work ??? (aliasing of 1/sin(theta) ???)
V			// Tlm = - (I*m*Vlm - MX*Wlm) / (l*(l+1))
V			double* mx = shtns->mx_van + 2*LM(shtns,m,m);	//(im*(2*(LMAX+1)-(m+MRES))) + 2*m;
V			s2d em = vdup(m);
V			v2d vl = vw[0];
V			v2d wl = vw[1];
V			v2d sl = vdup( 0.0 );
V			v2d tl = vdup( 0.0 );
V			for (int l=0; l<=llim-m; l++) {
V				s2d mxu = vdup( mx[2*l] );
V				s2d mxl = vdup( mx[2*l+1] );		// mxl for next iteration
V				sl = addi( sl ,  em*wl );
V				tl = addi( tl ,  em*vl );
V				v2d sl1 =  mxl*vl;			// vs for next iter
V				v2d tl1 = -mxl*wl;			// wt for next iter
V				vl = vw[2*l+2];		// kept for next iteration
V				wl = vw[2*l+3];
V				sl += mxu*vl;
V				tl -= mxu*wl;
V				Sl[l] = -sl * vdup(l_2[l+m]);
V				Tl[l] = -tl * vdup(l_2[l+m]);
V				sl = sl1;
V				tl = tl1;
V			}
V		}

		#ifdef SHT_VAR_LTR
			for (l=llim+1-m; l<=LMAX-m; ++l) {
Q				Ql[l] = vdup(0.0);
V				Sl[l] = vdup(0.0);		Tl[l] = vdup(0.0);
			}
		#endif
	}
  #endif
  }

	static
QX	void GEN3(spat_to_SH_mic,NWAY,SUFFIX)(shtns_cfg shtns, double *Vr, cplx *Qlm, long int llim) {
VX	void GEN3(spat_to_SHsphtor_mic,NWAY,SUFFIX)(shtns_cfg shtns, double *Vt, double *Vp, cplx *Slm, cplx *Tlm, long int llim) {
3	void GEN3(spat_to_SHqst_mic,NWAY,SUFFIX)(shtns_cfg shtns, double *Vr, double *Vt, double *Vp, cplx *Qlm, cplx *Slm, cplx *Tlm, long int llim) {

Q	double *BrF;		// contains the Fourier transformed data
V	double *BtF, *BpF;	// contains the Fourier transformed data
	unsigned imlim=0;

Q	BrF = Vr;
V	BtF = Vt;	BpF = Vp;
  #ifndef SHT_AXISYM
	imlim = MTR;
	#ifdef SHT_VAR_LTR
		if (imlim*MRES > (unsigned) llim) imlim = ((unsigned) llim)/MRES;		// 32bit mul and div should be faster
	#endif
	if (shtns->fftc_mode >= 0) {
		if (shtns->fftc_mode > 0) {		// alloc memory for out-of-place FFT
			unsigned long nv = shtns->nspat;
QX			BrF = (double*) VMALLOC( nv * sizeof(double) );
VX			BtF = (double*) VMALLOC( 2*nv * sizeof(double) );
VX			BpF = BtF + nv;
3			BrF = (double*) VMALLOC( 3*nv * sizeof(double) );
3			BtF = BrF + nv;		BpF = BtF + nv;
		}
	    if (shtns->fftc_mode != 1) {	// regular FFT
Q			fftw_execute_dft(shtns->fftc,(cplx*)Vr, (cplx*)BrF);
V			fftw_execute_dft(shtns->fftc,(cplx*)Vt, (cplx*)BtF);
V			fftw_execute_dft(shtns->fftc,(cplx*)Vp, (cplx*)BpF);
		} else {	// split FFT
Q			fftw_execute_split_dft(shtns->fftc, Vr+NPHI, Vr, BrF+1, BrF);
V			fftw_execute_split_dft(shtns->fftc, Vt+NPHI, Vt, BtF+1, BtF);
V			fftw_execute_split_dft(shtns->fftc, Vp+NPHI, Vp, BpF+1, BpF);
	    }
	}
  #endif
	imlim += 1;

  #pragma omp parallel num_threads(shtns->nthreads)
  {
QX	GEN3(_an1,NWAY,SUFFIX)(shtns, BrF, Qlm, llim, imlim);
VX	GEN3(_an2,NWAY,SUFFIX)(shtns, BtF, BpF, Slm, Tlm, llim, imlim);
3	GEN3(_an3,NWAY,SUFFIX)(shtns, BrF, BtF, BpF, Qlm, Slm, Tlm, llim, imlim);
  }

  #ifndef SHT_AXISYM
  	if (shtns->fftc_mode > 0) {		// free memory
Q	    VFREE(BrF);
VX	    VFREE(BtF);	// this frees also BpF.
	}
  #endif

  }
