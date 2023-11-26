/*
 * Standalone CLI program for equatorial coordinate -> telescope coodinate transformation
 * Direct translation of TAKI.BAS, invented by Toshimi Taki
 * mirror copy at https://www.cloudynights.com/topic/671194-takibas-what-basic-to-use/
 */
#include <string.h>
#include <math.h>
#include <stdio.h>

#define K 1.002738 // siderial time
#define G 57.2958 // radian <-> degree
#define Z1 -0.04
#define Z2 0.4
#define Z3 -1.63

struct star {
	char S[5]; // name
	double B; // right ascension in degree
	double D; // declination in degree
};

double V[3][3];
double R[3][3]; // T, equation 5 LHS
double Q[3][3]; // invert of X, invert of R (taki.bas 370, 450)
double X[3][3]; // Right matrix in equation 5 RHS
double Y[3][3]; // Left matrix in equation 5 RHS

struct star star_list[17] = {
	{"A UMI",  37.960,  89.264},
	{"A TAU",  68.980,  16.509},
	{"B ORI",  78.634,  -8.202},
	{"A AUR",  79.172,  45.998},
	{"A ORI",  88.793,   7.407},
	{"A CMA", 101.287, -16.716},
	{"A GEM", 113.650,  31.888},
	{"A CMI", 114.825,   5.225},
	{"B GEM", 116.329,  28.026},
	{"A LEO", 152.093,  11.967},
	{"A VIR", 201.298, -11.161},
	{"A BOO", 213.915,  19.183},
	{"A SCO", 247.352, -26.432},
	{"A LYR", 279.234,  38.784},
	{"A AQL", 297.695,   8.868},
	{"A CYG", 310.358,  45.280},
	{"A PSA", 344.413, -29.622},
};

struct ref_star {
	char name[5];
	double time;
	double telescope_direction;
	double telescope_elevation;
};

struct celestial_star {
	double asc;
	double dec;
	double time;
};

struct celestial_star target_star = {
	.asc = 71.53,
	.dec = 17.07,
	.time = 62.0,
};

struct ref_star stars[2] = {
	{"A AUR", 39.2, 39.9, 39.9},
	{"A UMI", 40.3, 94.6, 36.2}
};

double sq(double x) {
	return x*x;
}

void make3rdPoint(double (*M)[3]) {
	double a;
	int i;
	M[0][2] = M[1][0]*M[2][1] - M[2][0]*M[1][1];
	M[1][2] = M[2][0]*M[0][1] - M[0][0]*M[2][1];
	M[2][2] = M[0][0]*M[1][1] - M[1][0]*M[0][1];
	a = sqrt(sq(M[0][2]) + sq(M[1][2]) + sq(M[2][2]));
	for (i=0; i<3; i++) M[i][2] = M[i][2] / a;
}

double det(double (*M)[3]) {
	return M[0][0]*M[1][1]*M[2][2] + M[0][1]*M[1][2]*M[2][0] \
		+ M[0][2]*M[2][1]*M[1][0] - M[0][2]*M[1][1]*M[2][0] \
		- M[0][0]*M[2][1]*M[1][2] - M[0][1]*M[1][0]*M[2][2];
}

void inv(double (*N)[3], double (*M)[3]) {
	double a;
	a = 1 / det(X);
	N[0][0] = (M[1][1] * M[2][2] - M[2][1] * M[1][2]) * a;
	N[0][1] = (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * a;
	N[0][2] = (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * a;
	N[1][0] = (M[1][2] * M[2][0] - M[1][0] * M[2][2]) * a;
	N[1][1] = (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * a;
	N[1][2] = (M[1][0] * M[0][2] - M[0][0] * M[1][2]) * a;
	N[2][0] = (M[1][0] * M[2][1] - M[2][0] * M[1][1]) * a;
	N[2][1] = (M[2][0] * M[0][1] - M[0][0] * M[2][1]) * a;
	N[2][2] = (M[0][0] * M[1][1] - M[1][0] * M[0][1]) * a;
}

void angle(double *h, double *f, double *v_c) {
	double c, a;
	c = sqrt(sq(v_c[0]) + sq(v_c[1]));
	if (c == 0) {
		*h = v_c[2] > 0 ? 90.0 : -90.0;
	} else {
		*h = atan(v_c[2] / c) * G;
	}

	if (c == 0) {
		a = 0;  // value does not matter
	} else if (v_c[0] == 0) {
		a = v_c[1] > 0 ? 90.0 : -90.0;
	} else if (v_c[0] > 0) {
		a = atan(v_c[1] / v_c[0]) * G;
	} else {
		a = atan(v_c[1] / v_c[0]) * G + 180;
	}

	*f = a - ((int) floor(a/360)) * 360;
}

void apparent_angle(double *a_app, double *d_app, double a, double d) {
	double v[3];
	a = a / G;
	d = d / G;
	v[0] = cos(a) * cos(d) + sin(a) * Z2/G - sin(a) * sin(d) * Z1/G;
	v[1] = sin(a) * cos(d) - cos(a) * Z2/G + cos(a) * sin(d) * Z1/G;
	v[2] = sin(d);
	angle(d_app, a_app, v);
	*d_app = *d_app - Z3;
}

void eq2tel() {  // taki.bas 485
	double v_t[3];
	double v_c[3];
	double f, h, d, b, a_app, d_app;
	int i;

	d = target_star.dec / G;
	b = (target_star.asc - K * target_star.time * 0.25) / G;
	v_t[0] = cos(d) * cos(b);
	v_t[1] = cos(d) * sin(b);
	v_t[2] = sin(d);
	for (i=0; i<3; i++) {
		v_c[i] = R[i][0]*v_t[0] + R[i][1]*v_t[1] + R[i][2]*v_t[2];
	}
	angle(&h, &f, v_c);
	apparent_angle(&a_app, &d_app, f, h);

	printf("TELESCOPE ELEVATION (DEG): %'.2f\n", d_app);
	printf("TELESCOPE DIRECTION (DEG): %'.2f\n", a_app);
}

void tel2eq(double f, double h) { // taki.bas 570
	int i;
	double v_t[3];
	double v_c[3];
	struct celestial_star cstar = {
		.time = 62.0,
	};

	f = f / G;
	h = (h + Z3) / G;
	v_t[0] = cos(f)*cos(h)-sin(f)*(Z2/G) + sin(f)*sin(h)*(Z1/G);
	v_t[1] = sin(f)*cos(h)+cos(f)*(Z2/G) - cos(f)*sin(h)*(Z1/G);
	v_t[2] = sin(h);

	for (i=0; i<3; i++) {
		v_c[i] = Q[i][0]*v_t[0] + Q[i][1]*v_t[1] + Q[i][2]*v_t[2];
	}
	angle(&h, &f, v_c);
	cstar.asc = f + K * cstar.time*0.25;
	cstar.asc = cstar.asc - ((int)(cstar.asc/360)) * 360;
	cstar.dec = h;

	printf("STAR ASC (DEG): %'.2f\n", cstar.asc);
	printf("STAR DEC (DEG): %'.2f\n", cstar.dec);
}


int main() {
	double d, b, f, h;
	for (int i=0; i<2; i++) {
		struct ref_star *s = &stars[i];
		int n;
		for (n=0; n<17; n++) {
			if (strcmp(star_list[n].S, s->name) == 0) break;
		}
		d = star_list[n].D / G;
		// earth rotate ~0.25deg / minute
		b = (star_list[n].B - K*s->time*0.25) / G;
		X[0][i] = cos(d) * cos(b);  // taki.bas 220
		X[1][i] = cos(d) * sin(b);
		X[2][i] = sin(d);
		f = s->telescope_direction;
		h = s->telescope_elevation;
		f = f / G;
		h = (h + Z3) / G;  // taki.bas 235
		Y[0][i] = cos(f)*cos(h) - sin(f)*(Z2/G) + sin(f)*sin(h)*(Z1/G);
		Y[1][i] = sin(f)*cos(h) + cos(f)*(Z2/G) - cos(f)*sin(h)*(Z1/G);
		Y[2][i] = sin(h);
	}

	make3rdPoint(X);  // taki.bas 255
	make3rdPoint(Y);

	inv(Q, X);  // taki.bas 335-380
	for (int i=0; i<3; i++)
		for (int j=0; j<3; j++)
			for (int k=0; k<3; k++)
				R[i][j] = R[i][j] + Y[i][k]*Q[k][j];  // line 400
	inv(Q, R);  // taki.bas 450
#if 1
	//tel2eq(359.986, 40.309);
	eq2tel();
#endif

	return 0;
}
