## "version" identification string
HGID=`hg id -ti`

## compiler :
## generic gcc
cmd = gcc -O3 -ffast-math -D_HGID_="\"$(HGID)\"" -D_GNU_SOURCE
## profiling
#cmd = gcc -O3 -p -fno-inline
## recent gcc with native support
#cmd = gcc -O3 -march=native -mfpmath=sse
## gcc k8 (lgitl3)
#cmd = gcc -O3 -march=k8
## gcc core2 (calcul1&2)
#cmd = gcc -O3 -march=core2 -mfpmath=sse
## icare 64bits opteron
#cmd = cc -fast -xarch=amd64 -I/users/nschaeff/include -L/users/nschaeff/lib
## r2d2
#cmd = gcc -march=core2 -O3 -ffast-math -m64 -I/home/ciment/nschaeff/include -L/home/ciment/nschaeff/lib

shtfiles = SHT.c SHT/SH_to_spat.c SHT/spat_to_SH.c SHT/hyb_SH_to_spat.gen.c SHT/hyb_spat_to_SH.gen.c SHT/Makefile

default : SHT.o

SHT/SH_to_spat.c : SHT/hyb_SH_to_spat.gen.c SHT/Makefile
	$(MAKE) SH_to_spat.c -C SHT
SHT/spat_to_SH.c : SHT/hyb_spat_to_SH.gen.c SHT/Makefile
	$(MAKE) spat_to_SH.c -C SHT

SHT.o : SHT.c Makefile $(shtfiles)
	$(cmd) -c SHT.c -o SHT.o

SHTg.o : SHT.c Makefile $(shtfiles)
	$(cmd) -c -DSHT_NO_DCT SHT.c -o SHTg.o

SHTaxi.o : SHT.c Makefile $(shtfiles)
	$(cmd) -c -DSHT_AXISYM SHT.c -o SHTaxi.o

time_SHT : time_SHT.c SHT.o Makefile
	$(cmd) time_SHT.c SHT.o -lfftw3 -lgsl -lgslcblas -lm -o time_SHT

time_SHTg : time_SHT.c SHTg.o Makefile
	$(cmd) time_SHT.c SHTg.o -lfftw3 -lgsl -lgslcblas -lm -o time_SHTg

time_SHTaxi : time_SHT.c SHTaxi.o Makefile
	$(cmd) -DSHT_AXISYM time_SHT.c SHTaxi.o -lfftw3 -lgsl -lgslcblas -lm -o time_SHTaxi


#fftw compiling options :
#-O3 -fomit-frame-pointer -fstrict-aliasing -ffast-math -fno-schedule-insns -fno-web -fno-loop-optimize --param inline-unit-growth=1000 --param large-function-growth=1000
