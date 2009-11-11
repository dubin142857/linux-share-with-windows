/**************************************************
 * Spherical Harmonics (pre)computation           *
 *    some ideas and code taken from the GSL 1.13 *
 *    written by Nathanael Schaeffer / LGIT,CNRS  *
 **************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


//#define SHT_LEGENDRE_GSL
#ifdef SHT_LEGENDRE_GSL

#include <gsl/gsl_sf_legendre.h>

double legendre_sphPlm(int l, int m, double x) {
	return gsl_sf_legendre_sphPlm(l, m, x);
}

int legendre_sphPlm_array(int lmax, int m, double x, double *yl) {
	return gsl_sf_legendre_sphPlm_array(lmax, m, x, yl); 
}

int legendre_sphPlm_deriv_array(int lmax, int m, double x, double *yl, double *dyl)
{
	double st,stmin;
	int res, l;

	st = sqrt((1.-x)*(1.+x));
	stmin = 1.e-6;
	if ( (m==1) && (st < stmin ) ) {		// gsl function diverges for m=1 and sint=0 => use approximation
		printf("this should not happen !\n");
		gsl_sf_legendre_sphPlm_array(lmax, m, x, yl);
		res=gsl_sf_legendre_sphPlm_array(lmax, m, sqrt((1.-stmin)*(1.+stmin)) *((x<0.)? -1.:1.), dyl);
		for (l=m; l<=lmax; l++) {		// d(Pl1)/dt |(t=0) = Pl1(epsilon)/sin(epsilon)
			dyl[l-m] /= stmin;
		}
	} else {
		res = gsl_sf_legendre_sphPlm_deriv_array(lmax, m, x, yl, dyl);
		for (l=m; l<=lmax; l++) {
			dyl[l-m] *= -st;		// multiply by - sin(theta) to compute dyl/dtheta
		}
	}
	return res;
}

void legendre_precomp(int lmax, int mmax, int mres)
{
#if SHT_VERBOSE > 0
	printf(" > using GSL for legendre polynomials\n");
#endif
	// nothing to do when using the gsl.
}

#else

/// computes sin(t)^n from cos(t). ie returns (1-x^2)^(n/2), with x = cos(t)
inline double sint_pow_n(double cost, int n)
{
	double val = 1.0;
	double s2 = (1.-cost)*(1.+cost);		// sin(t)^2 = 1 - cos(t)^2
	if (n&1) val *= sqrt(s2);	// = sin(t)
	n >>= 1;
	while(n>0) {
		if (n&1) val *= s2;
		n >>= 1;
		s2 *= s2;
	}
	return val;		// = sint(t)^n
}

double *alm;
double *blm;
int *mmidx;
int MRES = 0;

/// compute value of a legendre polynomial, using recursion.
/// (requires a previous call to legendre_precompute())
double legendre_sphPlm(int l, int m, double x)
{
	double ymm, ymmp1, yl;
	int i,lm;

	if ( ((m>0)&&((x==1.)||(x==-1.))) || (m%MRES) ) return 0.0;

	lm = mmidx[m/MRES];
	ymm = alm[lm] * sint_pow_n(x, m);
	lm++;
	if (l==m) return ymm;

	ymmp1 = ymm * alm[lm] * x;
	lm++;
	for (i=m+2; i<=l; i++) {
		yl = alm[lm]*x*ymmp1 + blm[lm]*ymm;
		ymm = ymmp1;	ymmp1 = yl;
		lm++;
	}
#if SHT_VERBOSE > 1
	printf("l=%d, i=%d, x=%g, plm=%g\n",l,i,x,ymmp1);
#endif
	return ymmp1;
}

/// compute value of a legendre polynomial for a range of l, using recursion.
/// requires a previous call to legendre_precompute()
int legendre_sphPlm_array(int lmax, int m, double x, double *yl)
{
	int l,lm;

	if ((lmax < m)||(m % MRES)) return -1;

	if ((m>0)&&((x==1.)||(x==-1.))) {
		for(l=m; l<=lmax; l++) {
			yl[l-m] = 0.0;
		}
		return 0;
	}

	lm = mmidx[m/MRES];
	yl[0] = alm[lm] * sint_pow_n(x, m);	// l=m
	lm++;
	if (lmax==m) return 0;		// done.

	yl[1] = yl[0] * alm[lm] * x;		// l=m+1
	lm++;

	for (l=m+2; l<=lmax; l++) {
		yl[l-m] = alm[lm]*x*yl[l-m-1] + blm[lm]*yl[l-m-2];
		lm++;
	}

	return 0;
}

/// compute value of a legendre polynomial for a range of l, using recursion.
/// requires a previous call to legendre_precompute()
int legendre_sphPlm_deriv_array(int lmax, int m, double x, double *yl, double *dyl)
{
	double st = sqrt((1.-x)*(1.+x));
	int l,lm;
		
	if ((lmax < m)||(m % MRES)) return -1;

	if ((m>1)&&((x==1.)||(x==-1.))) {
		for(l=m; l<=lmax; l++) {
			yl[l-m] = 0.0;	dyl[l-m] = 0.0;
		}
		return 0;
	}

	lm = mmidx[m/MRES];
	yl[0] = alm[lm] * sint_pow_n(x, m);	// l=m
	if (m>0) {
		dyl[0] = alm[lm]* m*x * sint_pow_n(x, m-1);
	} else { dyl[0] = 0.; }
	lm++;
	if (lmax==m) return 0;		// done.

	yl[1] = alm[lm] * x * yl[0];		// l=m+1
	dyl[1] = alm[lm]*( x*dyl[0] - st*yl[0] );
	lm++;

	for (l=m+2; l<=lmax; l++) {
		yl[l-m] = alm[lm]*x*yl[l-m-1] + blm[lm]*yl[l-m-2];
		dyl[l-m] = alm[lm]*(x*dyl[l-m-1] - yl[l-m-1]*st) + blm[lm]*dyl[l-m-2];
		lm++;
	}

	return 0;
}


/// precompute constants for the legendre recursion.
void legendre_precomp(int lmax, int mmax, int mres)
{
	double t1, t2;
	int im, m, l, lm, nlm;

#if SHT_VERBOSE > 0
	printf(" > using custom fast recursion for legendre polynomials\n");
#endif

	if (mmax*mres > lmax) mmax = lmax/mres;
	nlm = (mmax+1)*(lmax+1) - mres*(mmax*(mmax+1))/2;

	alm = (double *) malloc( 2*nlm * sizeof(double) );
	blm = alm + nlm;
	mmidx = (int *) malloc( (mmax+1) * sizeof(int) );
	MRES = mres;

// precompute the factors of the recurrence relation :
// y(l,m) = alm[l,m]*x*y(l-1,m) - blm[l,m]*y(l-2,m)
	for (im=0, lm=0; im<=mmax; im++) {
		m = im*mres;
		mmidx[im] = lm;
		//l=m;
			alm[lm] = 0.0;	blm[lm] = 0.0;	lm++;
		if (m < lmax) {	// l=m+1
			alm[lm] = sqrt(m+m+3);
			blm[lm] = 0.0;
			t2 = (m+m+1);	lm++;		
		}
		for (l=m+2; l<=lmax; l++) {
			t1 = (l+m)*(l-m);
			alm[lm] = sqrt((l+l+1.)*(l+l-1.)/t1);
			blm[lm] = - sqrt(((l+l+1.)*t2)/((l+l-3.)*t1));
			t2 = t1;	lm++;
		}
	}

// Starting value for recursion.
// Y_m^m(x) = sqrt( (2m+1)/(4pi m) gamma(m+1/2)/gamma(m) ) (-1)^m (1-x^2)^(m/2) / pi^(1/4)
// alternate method : direct recursive computation, using the following properties:
//	gamma(x+1) = x*gamma(x)   et   gamma(1.5)/gamma(1) = sqrt(pi)/2
	alm[0] = 0.5/sqrt(M_PI);          // Y00 = 1/sqrt(4pi)
	m=1;	t1 = 1.0;
	for (im=1; im<=mmax; im++) {
		while(m<im*mres) {
			t1 *= (m+0.5)/m;
			m++;
		}
		t2 = (m&1) ? -1.0 : 1.0;	// (-1)^m
		alm[mmidx[im]] = t2 * sqrt( (2.0+1.0/m)/(8.0*M_PI) * t1);
	}
}

/* test code

int main()
{
	double x,v1, v2, dmax;
	int n=30;
	int mmax=15;
	int mres=1;
	int lmax=50;
	int i,im,m,l;
	
	legendre_precomp(lmax, mmax, mres);
	dmax = 0.;
	
	for (i=0;i<n;i++) {
		x = cos((i+0.5)*M_PI/n);
		for (im=0; im<=mmax; im++) {
			m=im*mres;
			for (l=m; l<=lmax; l+=mres) {
				v1 = gsl_sf_legendre_sphPlm(l,m,x);
				v2 = legendre_sphPlm(l,m,x);
//				v1 = legendre_sphPlm(l,m,x+1.e-16);
				if ((fabs((v2 - v1)/v1) > dmax)&&(fabs(v1)>1e-16)) {
					printf("m=%d l=%d x=%f   gsl=%e   me=%e   diff=%e  err=%e\n",m,l,x,v1,v2,v2-v1,(v2-v1)/v1);
					dmax = fabs((v2-v1)/v1);
				}
			}
		} 
	}
	printf("max error = %e\n",dmax);
}
*/
#endif
