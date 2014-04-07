# set terminal pngcairo  transparent enhanced font "arial,10" fontscale 1.0 size 500, 350 
# set output 'spline.5.png'
set dummy t,y
# set grid nopolar
# set grid xtics nomxtics ytics nomytics noztics nomztics \
#  nox2tics nomx2tics noy2tics nomy2tics nocbtics nomcbtics
# set grid layerdefault   linetype -1 linecolor rgb "gray"  linewidth 0.200,  linetype -1 linecolor rgb "gray"  linewidth 0.200
# set key inside right top vertical Right noreverse enhanced autotitles box linetype -1 linewidth 1.000
# set arrow 1 from 0, 1, 0 to 0.33, 0.2, 0 nohead back nofilled linetype -1 linewidth 1.000
# set arrow 2 from 0.33, 0.2, 0 to 0.66, 0.8, 0 nohead back nofilled linetype -1 linewidth 1.000
# set arrow 3 from 0.66, 0.8, 0 to 1, 0, 0 nohead back nofilled linetype -1 linewidth 1.000
set parametric
set title "The cubic Bezier/Bspline basis functions in use" 
set trange [ 0.00000 : 1.00000 ] noreverse nowriteback
set xrange [ 0.00000 : 4096.00000 ] noreverse nowriteback
set yrange [ 0.00000 : 1024.00000 ] noreverse nowriteback


bez0(x) = (1 - x)**3
bez1(x) = 3 * (1 - x)**2 * x
bez2(x) = 3 * (1 - x) * x**2
bez3(x) = x**3

y0 = 0
y1 = 0.99
y2 = 0.7
y3 = 1
x0 = 0
x1 = 0.1
x2 = 0.9
x3 = 1

cub_bezier_x(t) = bez0(t) * x0 + bez1(t) * x1 + bez2(t) * x2 + bez3(t) * x3
cub_bezier_y(t) = bez0(t) * y0 + bez1(t) * y1 + bez2(t) * y2 + bez3(t) * y3

plot 4096*cub_bezier_x(t), 1024*cub_bezier_y(t) with lines lt 2

