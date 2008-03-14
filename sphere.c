// base flow. Cartesien spectral.

#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
// FFTW la derivee d/dx = ik	(pas de moins !)
#include <fftw3.h>
// GSL for Legendre functions
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sf_legendre.h>


complex double *Slm;	// spherical harmonics l,m space
complex double *ShF, *ThF;	// Fourier space : theta,m
double *Sh, *Th;		// real space : theta,phi (alias of ShF)

#include "SHT.c"
	
void write_vect(char *fn, double *vec, int N)
{
	FILE *fp;
	int i;
	
	fp = fopen(fn,"w");
	for (i=0;i<N;i++) {
		fprintf(fp,"%.6g ",vec[i]);
	}
	fclose(fp);
}

void write_mx(char *fn, double *mx, int N1, int N2)
{
	FILE *fp;
	int i,j;
	
	fp = fopen(fn,"w");
	for (i=0;i<N1;i++) {
		for(j=0;j<N2;j++) {
			fprintf(fp,"%.6g ",mx[i*N2+j]);
		}
		fprintf(fp,"\n");
	}
	fclose(fp);
}

int main()
{
	double t,tmax;
	int i,im,m,l,jj;

	init_SH();

	write_vect("cost",ct,NLAT/2);
	write_vect("sint",st,NLAT/2);

	ShF = (complex double *) fftw_malloc( (NPHI/2+1) * NLAT * sizeof(complex double));
	Sh = (double *) ShF;
	ThF = (complex double *) fftw_malloc( (NPHI/2+1) * NLAT * sizeof(complex double));
	Th = (double *) ThF;

// test FFT :
	for(i=0;i<NLAT*(NPHI/2+1);i++) {
		ShF[i] = 0;
	}
	ShF[NLAT] = 1.0-I;
//	ShF[NLAT+3] = 2.0+I;
//	ShF[NLAT*2] = 3.0+I;

	if (MMAX>0) {
		fftw_execute_dft_c2r(ifft,ShF,Sh);
		write_mx("sph",Sh,NPHI,NLAT);
		fftw_execute_dft_r2c(fft,Sh,ShF);
		write_mx("sphF",Sh,NPHI/2+1,2*NLAT);
	}

/*
// test Ylm :
	im = 0; l=0; m=im*MRES;
	write_vect("y00",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dy00",&idylm[im][(l-m)*NLAT/2].t,NLAT);
	im = 0; l=1; m=im*MRES;
	write_vect("y10",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dty10",&idylm[im][(l-m)*NLAT/2].t,NLAT);
	im = 0; l=LMAX; m=im*MRES;
	write_vect("yLmax0",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dtyLmax0",&idylm[im][(l-m)*NLAT/2].t,NLAT);
	im = MMAX; m=im*MRES; l=m;
	write_vect("ymmax-mmax",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dtymmax-mmax",&idylm[im][(l-m)*NLAT/2].t,NLAT);
	im = 3; l=8; m=im*MRES;
	write_vect("y83",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dty83",&idylm[im][(l-m)*NLAT/2].t,NLAT);
	im = 30; l=65; m=im*MRES;
	write_vect("y6530",&iylm[im][(l-m)*NLAT/2],NLAT/2);
	write_vect("dty6530",&idylm[im][(l-m)*NLAT/2].t,NLAT);
*/

// test case...
	Slm = (complex double *) malloc(sizeof(complex double)* NLM);
	for (i=0;i<NLM;i++) {
		Slm[i] = 0.0;
	}
	
// 	Slm[LM(0,0)] = 1.0+I;
 	Slm[LM(3,0)] = -3.0+I;
	Slm[LM(4,0)] = 2.0+I;
// 	Slm[LM(10,4)] = -4.0-I;
// 	Slm[LM(55,12)] = 5.0-2.0*I;

	for (jj=0;jj<300;jj++) {

// synthese (inverse legendre)
		SH_to_spat(Slm,ShF);
		write_mx("sph",Sh,NPHI,NLAT);
		SH_to_grad_spat(Slm,ShF,ThF);
		write_mx("Gt",Sh,NPHI,NLAT);
		write_mx("Gp",Th,NPHI,NLAT);

// analyse (direct legendre)
//		spat_to_SH(ShF,Slm);
	}

	write_vect("ylm",Slm,NLM*2);

}

