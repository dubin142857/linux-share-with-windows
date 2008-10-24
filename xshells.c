  /////////////////////////////////////////////////////////////////////////
 // XSHELLS : eXtendable Spherical Harmonic Earth-Like Liquid Simulator //
/////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>
// FFTW : spatial derivative is d/dx = ik	(no minus sign !)
#include <fftw3.h>
// GSL for Legendre functions
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sf_legendre.h>

#include "SHT.c"

long int NR, NU;	// NR: total radial grid points. NU: grid points for velocity field. (=NR-NG)
long int NG = 0;	// NG: grid points for inner core.
double nu, eta;		// viscosity and magnetic diffusivity.
double dtU, dtB;	// time step for navier-stokes and induction equation.
double Omega0;		// global rotation rate (of outer boundary) => Coriolis force .
double DeltaOmega;	// differential rotation (of inner core)

#include "grid.c"

struct TriDiagL *MB, *MB_1, *MUt, *MUt_1;
struct PentaDiag **MUp, **MUp_1;

//#define DEB printf("%s:%u pass\n", __FILE__, __LINE__)
#define DEB (0)

#include "xshells_fields.c"
#include "xshells_io.c"

struct VectField B, U, W, J, B0;
struct PolTor Blm, Ulm, NLb1, NLb2, NLu1, NLu2;

int MAKE_MOVIE = 0;	// no output by default.
int SAVE_QUIT = 0;	// for signal catching.


void init_Bmatrix()
{
	double dx_1,dx_2;
	long int i,l;

	MB = (struct TriDiagL *) malloc( NR* sizeof(struct TriDiagL));
	MB_1 = (struct TriDiagL *) malloc( NR* sizeof(struct TriDiagL));

// Boundary conditions
//	r=0 : T=0, P=0  => not computed at i=0.
//	r=1 : T=0, dP/dr= -(l+1)/r P  (insulator) => only P is computed.

	for(i=1; i<NR-1; i++) {
					MB[i].l =              0.5*eta*Lr[i].l;
		for (l=0; l<=LMAX; l++)	MB[i].d[l] = 1.0/dtB + 0.5*eta*(Lr[i].d - r_2[i]*l*(l+1));
					MB[i].u =              0.5*eta*Lr[i].u;
	}
	i = NR-1;		// CL poloidale : dP/dr = -(l+1)/r P => allows to aproximate d2P/dr2 withe only 2 points.
		dx_1 = 1.0/(r[i]-r[i-1]);	dx_2 = dx_1*dx_1;
					MB[i].l =              eta * dx_2;
		for (l=0; l<=LMAX; l++) MB[i].d[l] = 1.0/dtB + eta*( -dx_2 - (l+1.0)*r_1[i]*dx_1 -(l+1.0 + 0.5*l*(l+1))*r_2[i] );
					MB[i].u = 0.0;

	for(i=1; i<NR; i++) {
					MB_1[i].l =            - MB[i].l;
		for (l=0; l<=LMAX; l++) MB_1[i].d[l] = 2.0/dtB - MB[i].d[l];
					MB_1[i].u =            - MB[i].u;
	}
	TriDec(MB_1, 1, NR-1);		// for Btor : 1 to NR-2, for Bpol : 1 to NR-1
// for poloidal, use : cTriSolve(MB_1, RHS, Plm, 1, NR-1)
// for toroidal, use : cTriSolve(MB_1, RHS, Tlm, 1, NR-2)
}

// BC : boundary condition, can be BC_NO_SLIP or BC_FREE_SLIP
void init_Umatrix(long int BC)
{
	double dx_1,dx_2;
	double Lp0d[LMAX+1], Lp0u[LMAX+1];	// for r=0 condition, useful for Poloidal.
	long int i,l;

// allocate toroidal matrix
	MUt = (struct TriDiagL *) malloc( 2*NU* sizeof(struct TriDiagL));
	MUt_1 = MUt + NU;
// allocate poloidal matrix
	MUp = (struct PentaDiag **) malloc( 2*NU * sizeof(struct PentaDiag *) );
	MUp_1 = MUp + NU;
	MUp[0] = (struct PentaDiag *) malloc( 2*NU*(LMAX+1)* sizeof(struct PentaDiag));
	MUp_1[0] = MUp[0] + NU*(LMAX+1);
	for (i=1; i<NU; i++) {
		MUp[i] = MUp[i-1] + (LMAX+1);
		MUp_1[i] = MUp_1[i-1] + (LMAX+1);
	}

// r=0 constraint :
//	T=0, P=0  => not computed at i=0.
//	for l=1 : d2/dr2 P = 0
//	for l>1 : dP/dr = 0
// Boundary conditions (NO SLIP)
//	P=0, T=0, dP/dr = 0  => not computed at boundary
//	with DeltaOmega (couette), at inner core : T(l=1,m=0) = r.DeltaOmega*Y10_ct
// Boundary conditions (FREE SLIP)
//	P=0, d2/dr2 P = 0, d(T/r)/dr = 0  => only T computed at boundary

/// POLOIDAL
/// (d/dt - nu.Lap)(-Lap.Up) = Toroidal[rot_NL]
// use toroidal matrix as temporary matrix...
	for(i=NG+1; i<NR-1; i++) {
					MUt[i-NG].l =    Lr[i].l;
		for (l=0; l<=LMAX; l++)	MUt[i-NG].d[l] = (Lr[i].d - r_2[i]*l*(l+1));
					MUt[i-NG].u =    Lr[i].u;
	}

	i = NR-1;		// BC poloidal
		if (BC == BC_FREE_SLIP) {	// free-slip
			dx_2 = -2.0/(r[i]*(r[i]-r[i-1]));	// -2/(r.dr)
		} else {	// default to no-slip
			dx_2 = 1.0/(r[i]-r[i-1]);	dx_2 = 2.0*dx_2*dx_2;	// 2/(dr.dr)
		}
					MUt[i-NG].l =    dx_2;
		for (l=0; l<=LMAX; l++) MUt[i-NG].d[l] = ( -dx_2 - r_2[i]*l*(l+1) );
					MUt[i-NG].u =    0.0;

	i = NG;		// dx_1 for l=1,  dx_2 for l=2
		if (r[i] == 0.0) {	// no inner core, use central condition
			dx_2 = 1.0/(r[i+1]-r[i]);	dx_2 = 2.0*dx_2*dx_2;	// 2/(dr.dr)
			dx_1 = 2.0/(r[i]*(r[i+1]-r[i]));	// 2/(r.dr)
		} else if (BC == BC_FREE_SLIP) {	// free-slip
			dx_2 = 2.0/(r[i]*(r[i+1]-r[i]));	// 2/(r.dr)
			dx_1 = dx_2;	// same for l=1
		} else {	// default to no-slip
			dx_2 = 1.0/(r[i+1]-r[i]);	dx_2 = 2.0*dx_2*dx_2;	// 2/(dr.dr)
			dx_1 = dx_2;	// same for l=1
		}
		for (l=0; l<=LMAX; l++) {	// Lp0d and Lp0u are the coeficient for i = NG
			Lp0d[l] = ( -dx_2 - r_2[i]*l*(l+1) );
			Lp0u[l] = dx_2;
		}
		l=1;
			Lp0d[l] = ( -dx_1 - r_2[i]*l*(l+1) );
			Lp0u[l] = dx_1;

	i=1;
		for (l=0; l<=LMAX; l++) {
			MUp[i][l].l2 = 0.0;
			MUp[i][l].l1 = -( 1.0/dtU*MUt[i].l    + 0.5*nu*( MUt[i].l*Lp0d[l] + MUt[i].d[l]*MUt[i].l ) );
			MUp[i][l].d  = -( 1.0/dtU*MUt[i].d[l] + 0.5*nu*( MUt[i].l*Lp0u[l] + MUt[i].d[l]*MUt[i].d[l] + MUt[i].u*MUt[i+1].l ) );
			MUp[i][l].u1 = -( 1.0/dtU*MUt[i].u    + 0.5*nu*(                    MUt[i].d[l]*MUt[i].u    + MUt[i].u*MUt[i+1].d[l] ) );
			MUp[i][l].u2 = -(                       0.5*nu*(                                              MUt[i].u*MUt[i+1].u ) );
		}
	for (i=2; i<NU-1; i++) {
		for (l=0; l<=LMAX; l++) {
			MUp[i][l].l2 = -(                       0.5*nu*( MUt[i].l*MUt[i-1].l ) );
			MUp[i][l].l1 = -( 1.0/dtU*MUt[i].l    + 0.5*nu*( MUt[i].l*MUt[i-1].d[l] + MUt[i].d[l]*MUt[i].l ) );
			MUp[i][l].d  = -( 1.0/dtU*MUt[i].d[l] + 0.5*nu*( MUt[i].l*MUt[i-1].u    + MUt[i].d[l]*MUt[i].d[l] + MUt[i].u*MUt[i+1].l ) );
			MUp[i][l].u1 = -( 1.0/dtU*MUt[i].u    + 0.5*nu*(                          MUt[i].d[l]*MUt[i].u    + MUt[i].u*MUt[i+1].d[l] ) );
			MUp[i][l].u2 = -(                       0.5*nu*(                                                    MUt[i].u*MUt[i+1].u ) );
		}
	}

	for (i=1; i<NU-1; i++) {
		for (l=0; l<=LMAX; l++) {
			MUp_1[i][l].l2 =                     - MUp[i][l].l2;
			MUp_1[i][l].l1 = -2.0/dtU*MUt[i].l    - MUp[i][l].l1;
			MUp_1[i][l].d  = -2.0/dtU*MUt[i].d[l] - MUp[i][l].d;
			MUp_1[i][l].u1 = -2.0/dtU*MUt[i].u    - MUp[i][l].u1;
			MUp_1[i][l].u2 =                     - MUp[i][l].u2;
		}
	}
	PentaDec(MUp_1, 1, NU-2);

/// TOROIDAL
/// (d/dt - nu.Lap) Ut = Poloidal(rot_NL)
	for(i=NG+1; i<NR-1; i++) {
					MUt[i-NG].l =              0.5*nu*Lr[i].l;
		for (l=0; l<=LMAX; l++)	MUt[i-NG].d[l] = 1.0/dtU + 0.5*nu*(Lr[i].d - r_2[i]*l*(l+1));
					MUt[i-NG].u =              0.5*nu*Lr[i].u;
	}
	i = NR-1;		// BC toroidal (free-slip) : d(T/r)/dr = 0 =>  dT/dr = T/r
		dx_1 = 1.0/(r[i]-r[i-1]);	dx_2 = dx_1*dx_1;
					MUt[i-NG].l =              nu * dx_2;
		for (l=0; l<=LMAX; l++) MUt[i-NG].d[l] = 1.0/dtU + nu*( -dx_2 + r_1[i]*dx_1 + (1.0 - 0.5*l*(l+1))*r_2[i] );
					MUt[i-NG].u = 0.0;

	for(i=1; i<NU; i++) {
					MUt_1[i].l =            - MUt[i].l;
		for (l=0; l<=LMAX; l++) MUt_1[i].d[l] = 2.0/dtU - MUt[i].d[l];
					MUt_1[i].u =            - MUt[i].u;
	}
	TriDec(MUt_1, 1, NU-1);		// for no-slip : 1 to NU-2, for free-slip : 1 to NU-1
// for NO-SLIP, use : cTriSolve(MUt_1, RHS, Tlm, 1, NU-2)
// for FREE-SLIP, use : cTriSolve(MUt_1, RHS, Tlm, 1, NU-1)
}

// **************************
// ***** TIME STEPPING ******
// **************************

/// substepB : substep advance for B, with NL and NLo set as non-linear terms at time t and t-dtB
inline substepB(struct PolTor *NL, struct PolTor * NLo, complex double **Btmp)
{
	long int i,l;

	cTriMul(MB, Blm.P, Btmp, 1, NR-1);
	for (i=0; i<NR; i++) {
		for (l=0;l<NLM;l++) {
			Btmp[i][l] += 1.5*NL->P[i][l] - 0.5*NLo->P[i][l];
		}
	}
	cTriSolve(MB_1, Btmp, Blm.P, 1, NR-1);

	cTriMul(MB, Blm.T, Btmp, 1, NR-2);
	for (i=0; i<NR-1; i++) {
		for (l=0;l<NLM;l++) {
			Btmp[i][l] += 1.5*NL->T[i][l] - 0.5*NLo->T[i][l];
		}
	}
	cTriSolve(MB_1, Btmp, Blm.T, 1, NR-2);
}

/// substepU : substep advance for U, with NL and NLo set as non-linear terms at time t and t-dtU
inline substepU(struct PolTor *NL, struct PolTor *NLo, complex double **Alm)
{
	long int i,l;

	cTriMulBC(MUt, Ulm.T +NG, Alm, 1, NU-2);
	for (i=1; i<NU-1; i++) {
		for (l=1;l<NLM;l++) {
			Alm[i][l] += 1.5*NL->T[i+NG][l] - 0.5*NLo->T[i+NG][l];
		}
	}
	cTriSolveBC(MUt_1, Alm, Ulm.T +NG, 1, NU-2);

	cPentaMul(MUp, Ulm.P +NG, Alm, 1, NU-2);
	for (i=1; i<NU-1; i++) {
		for (l=1;l<NLM;l++) {
			Alm[i][l] += 1.5*NL->P[i+NG][l] - 0.5*NLo->P[i+NG][l];
		}
	}
	cPentaSolve(MUp_1, Alm, Ulm.P +NG, 1, NU-2);
}

double step_NS(long int nstep, complex double **Alm)
{
	inline step1(struct PolTor *NLu, struct PolTor *NLuo)
	{
		CALC_U(BC_NO_SLIP);
		calc_Vort(&Ulm, Omega0, &W);
		NL_Fluid(&U, &W, NLu);
		substepU(NLu, NLuo, Alm);
	}

	while(nstep > 0) {
		nstep--;
		step1(&NLu1, &NLu2);
		step1(&NLu2, &NLu1);
	}
	return 2*nstep*dtU;
}

/// perform nstep steps of Navier-Stokes equation, with temporary array Alm.
double step_MHD(long int nstep, complex double **Alm)
{
	inline step1(struct PolTor *NLu, struct PolTor *NLb, struct PolTor *NLuo, struct PolTor *NLbo)
	{
		CALC_U(BC_NO_SLIP);
		calc_Vort(&Ulm, Omega0, &W);
		calc_J(&Blm, &J);
		NL_MHD(&U, &W, &B0, &J, NLu);
		NL_Induction(&U, &B0, &B, NLb);
		substepU(NLu, NLuo, Alm);
		substepB(NLb, NLbo, Alm);
	}

	while(nstep > 0) {
		nstep--;
		step1(&NLu1, &NLb1, &NLu2, &NLb2);
		step1(&NLu2, &NLb2, &NLu1, &NLb1);
	}
	return 2*nstep*dtU;
}


/// perform nstep steps of Induction equation, with temporary array Alm.
double step_Induction(long int nstep, complex double **Alm)
{
	inline step1(struct PolTor *NLb, struct PolTor *NLbo)
	{
		NL_Induction(&U, &B0, &B, NLb);
		substepB(NLb, NLbo, Alm);
	}

	while(nstep > 0) {
		nstep--;
		step1(&NLb1, &NLb2);
		step1(&NLb2, &NLb1);
	}
	return 2*nstep*dtB;
}


void read_Par(char *fname, char *job, long int *iter_max, long int *modulo, double *polar_opt_max, double *Ric, double *B0)
{
	double tmp;
	int id;
	FILE *fp, *fpw;
	char str[256];
	char name[32];

	printf("[read_Par] reading parameters from '%s'...\n",fname);

	fp = fopen(fname,"r");
	if (fp == NULL) runerr("[read_Par] file not found !");

	fpw = fopen("tmp.par","w");	// keep the parameters for that job.

	// DEFAULT VALUES
	Omega0 = 1.0;		// global rotation is on by default.
	DeltaOmega = 0.0;	// no differential rotation.

	while (!feof(fp))
	{
		fgets(str, 256, fp);
		fputs(str, fpw);		// save file...
//		printf(str);
		if ((str[0] != '#' )&&(strlen(str) > 3)) {	// ignore les lignes commentees (#) et les lignes vides.
			sscanf(str,"%s = %lf",name,&tmp);
//			printf("[%s] = %f\n",name,tmp);
			// JOB
			if (strcmp(name,"job") == 0) 		sscanf(str,"%s = %s",name,job);
			if (strcmp(name,"movie") == 0)		MAKE_MOVIE = tmp;
			// PHYSICAL PARAMS
			if (strcmp(name,"Omega0") == 0)		Omega0 = tmp;
			if (strcmp(name,"DeltaOmega") == 0)	DeltaOmega = tmp;
			if (strcmp(name,"nu") == 0)		nu = tmp;
			if (strcmp(name,"eta") == 0)		eta = tmp;
			if (strcmp(name,"Ric") == 0)		*Ric = tmp;
			if (strcmp(name,"B0") == 0)		*B0 = tmp;
			// NUMERICAL SCHEME
			if (strcmp(name,"NR") == 0)		NR = tmp;
			if (strcmp(name,"NG") == 0)		NG = tmp;
			if (strcmp(name,"dtU") == 0)		dtU = tmp;
			if (strcmp(name,"dtB") == 0)		dtB = tmp;
			// TIME DOMAIN
			if (strcmp(name,"iter_max") == 0)	*iter_max = tmp;
			if (strcmp(name,"modulo") == 0)		*modulo = tmp;
			// ALGORITHM TUNING
			if (strcmp(name,"sht_polar_opt_max") == 0)	*polar_opt_max = tmp;
			if (strcmp(name,"fftw_plan_mode") == 0) {
				switch( (int) tmp) {
					case 0: fftw_plan_mode = FFTW_ESTIMATE; break;
					case 1: fftw_plan_mode = FFTW_MEASURE; break;
					case 2: fftw_plan_mode = FFTW_PATIENT; break;	// default
					case 3: fftw_plan_mode = FFTW_EXHAUSTIVE;
				}
			}
		}
	}
	// SOME BASIC COMPUTATIONS

	fclose(fp);	fclose(fpw);
	sprintf(str, "%s.%s",fname,job);	rename("tmp.par", str);		// rename file.	
	fflush(stdout);		// when writing to a file.
}

double j1(double x)
{
	if (x==0.0) return(0.0);
	return (sin(x)/x - cos(x))/x;
}

int main (int argc, char *argv[])
{
	double Ric = 0.0;		// default ic radius
	double polar_opt_max = 0.0;	// default SHT optimization.
	double b0 = 0.0;
	double t0,t1,Rm,Rm2,z,time;
	long int i,im,m,l,jj, it, lmtest;
	long int iter_max, modulo;
	complex double **Alm;		// temp scalar spectral (pol or tor) representation
	FILE *fp;
	char command[100] = "xshells.par";
	char job[40];

	printf("[XSHELLS] eXtendable Spherical Harmonic Earth-Like Liquid Simulator\n          by Nathanael Schaeffer / LGIT, build %s, %s\n",__DATE__,__TIME__);

	read_Par(command, job, &iter_max, &modulo, &polar_opt_max, &Ric, &b0);

	init_SH(polar_opt_max);
	init_rad_sph(0.0, Ric, 1.0);

	init_Bmatrix();		init_Umatrix(BC_NO_SLIP);

	alloc_DynamicField(&Blm, &B, &NLb1, &NLb2, 0, NR-1);
	alloc_DynamicField(&Ulm, &U, &NLu1, &NLu2, NG, NR-1);
	alloc_VectField(&B0,0,NR-1);	alloc_VectField(&J,0,NR-1);	alloc_VectField(&W,NG,NR-1);

	printf("[Params] job name : %s -- NG=%d, rg=%f\n",job,NG,r[NG]);
	printf("         Ek=%.2e, Ro=%.2e, Pm=%.2e, Re=%.2e\n",nu/Omega0, r[NG]*DeltaOmega/Omega0, nu/eta, r[NG]*DeltaOmega/nu);
	printf("         dtU.Omega=%.2e, dtU.nu.R^2=%.2e, dtB.eta.R^2=%.2e, dtB/dtU=%.2e\n",dtU*Omega0, dtU*nu, dtB*eta, dtB/dtU);

	Alm = (complex double **) malloc( NR * sizeof(complex double *));
	for (i=0;i<NR;i++)
		Alm[i] = (complex double *) malloc( NLM * sizeof(complex double));

	// init B fields.
	for (i=0; i<NR; i++) {
		for (l=0;l<NLM;l++) {
			Blm.P[i][l] = 0.0;
			Blm.T[i][l] = 0.0;
		}
//		Blm.P[i][1] = b0*j1(pi*r[i]);		// magnetic dipole
		Blm.P[i][1] = b0 / (2*r[i]*r[i]);	// magnetic dipole like in E.Dormy's thesis (current free)
		jj=1;
		if (i<=jj) Blm.P[i][1] = b0/ (2*r[jj]*r[jj]);
	}
	sprintf(command,"poltorB0.%s",job);	save_PolTor(command, &Blm, time, BC_MAGNETIC);
	PolTor_to_spat(&Blm, &B0, 0, NR-1, BC_MAGNETIC);	// background magnetic field.
	for (i=0; i<NR; i++) {
		Blm.P[i][1] = 0.0;	// remove dipole.
	}

	// init U fields
	for (i=NG; i<NR; i++) {
		for (l=0;l<NLM;l++) {
			Ulm.T[i][l] = 0.0;
			Ulm.P[i][l] = 0.0;
		}
//		Ulm.T[i][LM(1,0)] = r[i]*DeltaOmega*(1-(r[i]-r[NG])/(r[NR-1]-r[NG])) * Y10_ct;
	}
		Ulm.T[NG][LM(1,0)]  = r[NG]*DeltaOmega * Y10_ct;	// rotation differentielle de la graine.

	sprintf(command,"poltorU0.%s",job);	save_PolTor(command, &Ulm, time, BC_NO_SLIP);

	CALC_U(BC_NO_SLIP);
	calc_Vort(&Ulm, Omega0, &W);
	calc_J(&Blm, &J);
	NL_MHD(&U, &W, &B0, &J, &NLu2);
	NL_Induction(&U, &B0, &B, &NLb2);

	DEB;

	it =0;
	while(it<iter_max) {
/*		if (it==1) {
			printf(" > inner core stops\n");
			DeltaOmega = 0.0;
		}*/
		Ulm.T[NG][LM(1,0)]  = r[NG]*DeltaOmega * Y10_ct;	// rotation differentielle de la graine.
//		t0 = t1;
//		printf("%g :: tx=%g\n",t1,log(t1/t0)/(2*dtB));
		t0 = creal(Ulm.T[NG+1][1]);
		t1 = creal(Ulm.P[NG+1][2]);
		time = 2*it*modulo*dtU;
		printf("[it %d] t=%g, P0=%g, T0=%g\n",it,time,t1, t0);
		if (isnan(t0)) runerr("NaN encountered");

		step_MHD(modulo, Alm);
//		step_NS(modulo, Alm);

		it++;
		if (MAKE_MOVIE != 0) {
			sprintf(command,"poltorB_%04d.%s",it,job);	save_PolTor(command, &Blm, time, BC_MAGNETIC);
			sprintf(command,"poltorU_%04d.%s",it,job);	save_PolTor(command, &Ulm, time, BC_NO_SLIP);
		}
	}

	if (MAKE_MOVIE == 0) {
		sprintf(command,"poltorB.%s",job);	save_PolTor(command, &Blm, time, BC_MAGNETIC);
		sprintf(command,"poltorU.%s",job);	save_PolTor(command, &Ulm, time, BC_NO_SLIP);
	}
}

