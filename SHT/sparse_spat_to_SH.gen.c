# This file is meta-code for SHT.c (spherical harmonic transform).
# it is intended for "make" to generate C code for 3 similar SHT functions,
# (namely spat_to_SH [Q tag]), spat_to_SHsphtor [V tag], spat_to_SH3 [both Q&V tags])
# from one generic function + tags.
# Basically, there are tags at the beginning of lines (Q,V) that are information
# to keep or remove the line depending on the function to build. (Q for scalar, V for vector, # for comment)
#
//////////////////////////////////////////////////
//  Spherical Harmonics Transform
// input  : BrF, BtF, BpF = spatial/fourrier data : complex double array of size NLAT_2*(NPHI/2+1) or double array of size NLAT_2*(NPHI/2+1)*2
// output : Qlm, Slm, Tlm = spherical harmonics coefficients : complex double array of size NLM
#Q void spat_to_SH(double *Vr, complex double *Qlm)
#V void spat_to_SHsphtor(double *Vt, double *Vp, complex double *Slm, complex double *Tlm)
# {
Q	complex double *Ql;		// virtual pointers for given im
V	complex double *Sl, *Tl;	// virtual pointers for given im
Q	complex double *BrF;		// contains the Fourier transformed data
V	complex double *BtF, *BpF;	// contains the Fourier transformed data
Q	double *zl;
V	double *dzl0;
V	struct DtDp *dzl;
	long int ni;
	long int i,i0, im,l;
Q	complex double q0;
V	complex double s0,t1;
V  	double sgn_sph, sgn_tor;

// defines how to access even and odd parts of data
  #ifndef SHT_AXISYM
Q	#define re	BrF[i]
V	#define to	BtF[i]
V	#define pe	BpF[i]
Q	#define re0	BrF[i]
V	#define to0	BtF[i]
V	#define pe0	BpF[i]
  #else
Q	#define re	((double *)BrF)[i]
V	#define to	((double *)BtF)[i]
V	#define pe	((double *)BpF)[i]
Q	#define re0	((double *)BrF)[i]
V	#define to0	((double *)BtF)[i]
V	#define pe0	((double *)BpF)[i]
  #endif

	ni = NLAT_2;	// copy NLAT_2 to a local variable for faster access (inner loop limit)
Q	BrF = (complex double *) Vr;
V	BtF = (complex double *) Vt;	BpF = (complex double *) Vp;
V	sgn_tor = -1.0;		sgn_sph = 1.0;

  #ifndef SHT_AXISYM
	if (SHT_FFT > 0) {
	    if (SHT_FFT > 1) {		// alloc memory for the FFT
Q	    	BrF = fftw_malloc( (NPHI/2+1)*NLAT_2 * sizeof(complex double) );
V	    	BtF = fftw_malloc( 2* (NPHI/2+1)*NLAT_2 * sizeof(complex double) );
V	    	BpF = BtF + (NPHI/2+1)*NLAT_2;
	    }
Q	    fftw_execute_dft_r2c(fft_eo,Vr, BrF);
V	    fftw_execute_dft_r2c(fft_eo,Vt, BtF);
V	    fftw_execute_dft_r2c(fft_eo,Vp, BpF);
	}
  #endif

V	if (parity) {	// antisymmetric scalar or vector.
V		Sl = Slm;		Slm = Tlm;		Tlm = Sl;		// exchange Slm and Tlm pointers.
V		Sl = BtF;		BtF = BpF;		BpF = Sl;		// exchange BtF and BpF pointers.
V		sgn_tor = 1.0;		sgn_sph = -1.0;
V	}

	im = 0;		// dzl.p = 0.0 : and evrything is REAL
		l=0;
Q		Ql = Qlm + parity;		// virtual pointer for l=0 and im
V		Sl = Slm;	Tl = Tlm;		// virtual pointer for l=0 and im
Q		zl = zlm[im] + parity;
V		dzl0 = (double *) dzlm[im];		// only theta derivative (d/dphi = 0 for m=0)
		do {		// ops : NLAT/2 * (2*(LMAX-m+1) + 4) : almost twice as fast.
			i=0;
Q			q0 = 0.0;
V			s0 = 0.0;	t1 = 0.0;
			do {
Q				q0 += zl[0] * re0;	// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
V				s0 += dzl0[0] * to0;
V				t1 += dzl0[1] * pe0;
Q				zl +=2;
V				dzl0 +=2;
				i++;
			} while(i < ni);
Q			Ql[l] = q0 + q0;
V			Sl[l] = s0*sgn_sph;	Tl[l+1] = t1*sgn_tor;
			l+=2;
		} while (l<LTR);
		if (l==LMAX) {
V			s0 = 0.0;
V			i=0;	do {
V				s0 += dzl0[0] * to0;
V				dzl0 ++;
V				i++;
V			} while(i<ni);
V			Sl[l] = s0*sgn_sph;
Q			if (parity==0) {
Q				q0 = 0.0;
Q				i=0;	do {
Q					q0 += zl[0] * re0;		// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
Q					zl ++;
Q					i++;
Q				} while(i<ni);
Q				Ql[l] = q0 + q0;
Q			}
  #ifdef SHT_VAR_LTR
		} else {
		    if (l==LTR) {
Q				q0 = 0.0;
V				s0 = 0.0;
V				i=0;	do {
V					s0 += dzl0[0] * to0;
V					dzl0 +=2;
V					i++;
V				} while(i<ni);
Q				if (parity==0) {
Q					i=0;	do {
Q						q0 += zl[0] * re0;		// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
Q						zl +=2;
Q						i++;
Q					} while(i<ni);
				}
Q				Ql[l] = q0 + q0;
V				Sl[l] = s0*sgn_sph;
				l++;
			}
			while( l<LMAX ) {
V				Sl[l] = 0.0;	Tl[l+1] = 0.0;
Q				Ql[l] = 0.0;
				l+=2;
			}
			if ( l==LMAX ) {
V				Sl[l] = 0.0;
Q				if (parity==0) Ql[l] == 0.0;
			}
  #endif
		}
  #ifndef SHT_AXISYM
	for (im=1;im<=MTR;im++) {
		i0 = tm[im];
		l=im*MRES;
Q		Ql = &Qlm[LiM(0,im)] + parity;	// virtual pointer for l=0 and im
V		Sl = &Slm[LiM(0,im)];	Tl = &Tlm[LiM(0,im)];
Q		zl = zlm[im] + parity;
V		dzl = dzlm[im];
Q		BrF += NLAT_2;
V		BtF += NLAT_2;	BpF += NLAT_2;
		while (l<LTR) {		// ops : NLAT/2 * (2*(LMAX-m+1) + 4) : almost twice as fast.
Q			zl += 2*i0;
V			dzl += 2*i0;
Q			q0 = 0.0;
V			s0 = 0.0;	t1 = 0.0;		// Slm[LiM(l,im)] = 0.0;	Slm[LiM(l+1,im)] = 0.0;
			i=i0;	do {		// tm[im] : polar optimization
Q				q0 += re * zl[0];		// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
V				s0 += dzl[0].t *to - dzl[0].p *pe*I;		// ref: these E. Dormy p 72.
V				t1 += dzl[1].t *pe - dzl[1].p *to*I;
Q				zl +=2;
V				dzl +=2;
				i++;
			} while (i < ni);
Q			Ql[l] = q0 + q0;
V			Sl[l] = s0*sgn_sph;	Tl[l+1] = t1*sgn_tor;
			l+=2;
		}
		if (l==LMAX) {
V			dzl += i0;
V			s0 = 0.0;
V			i=i0;	do {		// tm[im] : polar optimization
V				s0 += dzl[0].t *to - dzl[0].p *pe*I;
V				dzl++;
V				i++;
V			} while(i<ni);
V			Sl[l] = s0*sgn_sph;
Q			if (parity == 0) {
Q				zl += i0;
Q				q0 = 0.0;	// Qlm[LiM(l,im)] = 0.0;
Q				i=i0;	do {		// tm[im] : polar optimization
Q					q0 += zl[0] * re;	// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
Q					zl++;
Q					i++;
Q				} while(i<ni);
Q				Ql[l] = q0 + q0;
Q			}
    #ifdef SHT_VAR_LTR
		} else {
		    if (l==LTR) {
V				dzl += 2*i0;
V				s0 = 0.0;
V				i=i0;	do {		// tm[im] : polar optimization
V					s0 += dzl[0].t *to - dzl[0].p *pe*I;		// ref: these E. Dormy p 72.
V					dzl +=2;
V					i++;
V				} while (i<ni);
V				Sl[l] = s0*sgn_sph;
Q				zl += 2*i0;
Q				q0 = 0.0;
Q				if (parity == 0) {
Q					i=i0;	do {		// tm[im] : polar optimization
Q						q0 += re * zl[0];		// Qlm[LiM(l,im)] += zlm[im][(l-m)*NLAT/2 + i] * fp[i];
Q						zl +=2;
Q						i++;
Q					} while (i<ni);
Q				}
Q				Ql[l] = q0 + q0;
				l++;
			}
			while( l<LMAX ) {
Q				Ql[l] = 0.0;
V				Sl[l] = 0.0;	Tl[l+1] = 0.0;
				l+=2;
			}
			if ( l==LMAX ) {
Q				if (parity==0) Ql[l] == 0.0;
V				Sl[l] = 0.0;
			}
    #endif
		}
	}

  	if (SHT_FFT > 1) {		// free memory
Q	    fftw_free(BrF - NLAT_2*(MTR+1));
V		if (parity) BtF = BpF;			// free the correct memory location.
V	    fftw_free(BtF - NLAT_2*(MTR+1));	// this frees also BpF.
	}
  #endif

Q	#undef re
Q	#undef ro
V	#undef te
V	#undef to
V	#undef pe
V	#undef po
Q	#undef re0
Q	#undef ro0
V	#undef te0
V	#undef to0
V	#undef pe0
V	#undef po0
V	#undef teo0
V	#undef peo0
# }
