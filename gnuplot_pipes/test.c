#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <math.h>
#include "gnuplot_pipes.h"

int getkey() {
	int character;
	struct termios orig_term_attr;
	struct termios new_term_attr;

	/* set the terminal to raw mode */
	tcgetattr(fileno(stdin), &orig_term_attr);
	memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
	new_term_attr.c_lflag &= ~(ECHO|ICANON);
	new_term_attr.c_cc[VTIME] = 0;
	new_term_attr.c_cc[VMIN] = 0;
	tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

	/* read a character from the stdin stream without blocking */
	/*   returns EOF (-1) if no character is available */
	character = fgetc(stdin);

	/* restore the original terminal attributes */
	tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

	return character;
}

#define PLUS 0
#define MINUS 1

#define COARSE 0
#define FINE 1

#define YONE 0
#define YTWO 1
#define XONE 2
#define XTWO 3

float yone = 0.99;
float ytwo = 0.7;
float xone = 0.1;
float xtwo = 0.9;

int target = YONE;

/* 调节项 粗调还是细调 加还是减 */
int bez_adj(gnuplot_ctrl *h, int target, int s, int how)
{
	float *item, step;


	if (target == YONE){
		item = &yone;
	}
	
	if (target == YTWO){
		item = &ytwo;
	}
	
	if (target == XONE){
		item = &xone;
	}
	
	if (target == XTWO){
		item = &xtwo;
	}

	if (!s)
		step = 0.1;
	else
		step = 0.01;

	if (!how){
		*item += step;
	}
	else{
		*item -= step;
	}

	if (*item > 0.99)
		*item = 0.99;
	
	if (*item < 0.01)
		*item = 0.01;
	printf("\n y1: %f, y2: %f, x1: %f, x2: %f\n", yone, ytwo, xone, xtwo);

	return 0;
}
int print_curve(void)
{
	float t = 0.0;
	float x, y;
	float bez0,bez1,bez2,bez3;
	int i;
	
	printf("\n");
	for(i=0,t=0.0;t<1.0;t += 1.0/4096){
		bez0 = powf((1-t), 3);
		bez1 = powf((1-t), 2) * 3 * t;
		bez2 = 3 * (1-t) * t * t;
		bez3 = powf(t, 3);
		x =  bez0 * 0.0 + bez1 * xone + bez2 * xtwo + bez3 * 1.0;
		y =  bez0 * 0.0 + bez1 * yone + bez2 * ytwo + bez3 * 1.0;
		printf("%f, %f,%f,%f,%f,  %f,%f\n", t,
		       bez0,bez1,bez2,bez3,
		      x*4096, y*1023);
		i++;
	}
	printf("%d\n", i);
	
}

int plot_bezier(gnuplot_ctrl *h)
{
	int i;

	
	char bez[20][100]={
		"set parametric",
		"set title \"The cubic Bezier/Bspline basis functions in use\"",
		"set trange [ 0.00000 : 1.00000 ] noreverse nowriteback",
		"set xrange [ 0.00000 : 4096.00000 ] noreverse nowriteback",
		"set yrange [ 0.00000 : 1024.00000 ] noreverse nowriteback",
		"bez0(x) = (1 - x)**3",
		"bez1(x) = 3 * (1 - x)**2 * x",
		"bez2(x) = 3 * (1 - x) * x**2",
		"bez3(x) = x**3",
		"y0 = 0",
		"y1 = 0.99",//10
		"y2 = 0.7",//11
		"y3 = 1",
		"x0 = 0",
		"x1 = 0.1",//14
		"x2 = 0.9",//15
		"x3 = 1",
		"cub_bezier_x(t) = bez0(t) * x0 + bez1(t) * x1 + bez2(t) * x2 + bez3(t) * x3",
		"cub_bezier_y(t) = bez0(t) * y0 + bez1(t) * y1 + bez2(t) * y2 + bez3(t) * y3",
		"plot 4096*cub_bezier_x(t), 1024*cub_bezier_y(t) with lines lt 2",
	};

	sprintf(bez[10], "y1 = %f", yone);	/* y1 */
	sprintf(bez[11], "y2 = %f", ytwo);	/* y2 */
	sprintf(bez[14], "x1 = %f", xone);	/* x1 */
	sprintf(bez[15], "x2 = %f", xtwo);	/* x2 */
	
	for (i=0;i<20;i++)
		gnuplot_cmd(h, bez[i]);

	return 0;
}

int main(int argc, char **argv)
{
	gnuplot_ctrl *gam;
	char kcode;
	int exit_flag = 1;
	
	gam = gnuplot_init();
	
	gnuplot_setstyle(gam, "lines");

	plot_bezier(gam);
	
//	gnuplot_plot_slope(gam, 2.0, 0.0, "y=2x");
	while(1){
		switch(getkey()){
		case 'x':
			exit_flag = 0;
			break;
		case 'a':
			gnuplot_resetplot(gam);
			gnuplot_plot_equation(gam, "log(x)", "logarithm") ;
			break;
		case '=':
			gnuplot_resetplot(gam);
			bez_adj(gam, target, COARSE, PLUS);
			plot_bezier(gam);
			break;
		case '-':
			gnuplot_resetplot(gam);
			bez_adj(gam, target, COARSE, MINUS);
			plot_bezier(gam);
			break;
		case '+':
			gnuplot_resetplot(gam);
			bez_adj(gam, target, FINE, PLUS);
			plot_bezier(gam);
			break;
		case '_':
			gnuplot_resetplot(gam);
			bez_adj(gam, target, FINE, MINUS);
			plot_bezier(gam);
			break;
		case '1':
			target = YONE;
			printf("\nset YONE as adjust target: %f\n", yone);
			break;
		case '2':
			target = YTWO;
			printf("\nset YTWO as adjust target: %f\n", ytwo);
			break;
		case '3':
			target = XONE;
			printf("\nset XONE as adjust target: %f\n", xone);
			break;
		case '4':
			target = XTWO;
			printf("\nset XTWO as adjust target: %f\n", xtwo);
			break;
			
		default:
			break;
		}

		if (!exit_flag)
			break;
		usleep(500*1000);
	}
	print_curve();
	
	gnuplot_close(gam);
	return 0;
}
