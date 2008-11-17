#!/usr/bin/python

############################
# plot an axisymmetric slice of poltor field (calls xspp)
# uasge :
#   plot_axi.py <poltor.field.from.xshells> [ang]
# the optional argument [ang] shows angular velocity instead of simply phi component
############################

from pylab import *     # matplotlib
from subprocess import *	# lance une ligne de commande
import sys              # acces a la ligne de commande.

rg=0.35
ir0=1
ang=0	# draw as is

retcode = call("./xspp " + sys.argv[1] + " axi", shell=True)
if retcode != 0:
	print 'error from xspp'
	exit()

if len(sys.argv) > 2 :
	if sys.argv[2] == 'ang' :
		ang=1	# draw as angular velocity (dividing by s)

#Up
#print 'loading',sys.argv[1]
#a=load(sys.argv[1])
a=load('o_Vp',comments='%')
s = a.shape

ct = a[0,1:s[1]]
st = sqrt(1-ct*ct)
r = transpose(matrix(a[ir0:s[0],0]))

a = a[ir0:s[0],1:s[1]]
x = r*matrix(st)
y = r*matrix(ct)

#Poloidal scalar
#print 'loading',sys.argv[2]
#b = load(sys.argv[2])
b = load('o_Vpol',comments='%')
b = b[ir0:s[0],1:s[1]]

#convert Up to angular velocity
c=a/array(x)
c[:,s[1]-2] = c[:,s[1]-3]	#remove nan
c[:,0] = c[:,1]			#remove nan

if ang==1 :
	m=amax(abs(c))
	print 'max angular velocity (absolute value) =',m
	contourf(array(x),array(y),c,15,cmap=cm.RdBu)
	#pcolor(array(x),array(y),c,shading='interp')
else:
	m=amax(abs(a))
	print 'max vphi (absolute value) =',m
	contourf(array(x),array(y),a,15,cmap=cm.RdBu)
	#pcolor(array(x),array(y),a,shading='interp')

colorbar()
clim(-m,m)

#contour for poloidal scalar
m=amax(abs(b))
contour(array(x),array(y),b,arange(m/6,m,m/3),colors='k')
contour(array(x),array(y),b,arange(-m/6,-m,-m/3),colors='k')
#axvline(x=0, ymin=-1, ymax=1, color='k')
theta = linspace(-pi/2,pi/2,100)
#r = ones(100)*rg
#polar(theta,r)
#plot(rg*cos(theta),rg*sin(theta),'k')

axis('equal')
axis('off')
xlim(0,1)
ylim(0,1)
#subplots_adjust(left=0.02, bottom=0.02, right=0.98, top=0.98, wspace=0.1, hspace=0.1)

savefig('axisym.png')
#savefig('axisym.pdf')
show()