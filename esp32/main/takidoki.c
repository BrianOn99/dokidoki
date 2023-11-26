/*
 * Translation of TAKI.BAS, invented by Toshimi Taki
 * mirror copy at https://www.cloudynights.com/topic/671194-takibas-what-basic-to-use/
 */
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "takidoki.h"

#define K 1.002738 // siderial time
#define G 57.2958 // radian <-> degree
#define Z1 (-0.0/G)
#define Z2 (0.0/G)
#define Z3 (-0.0/G)

double R[3][3]; // T, equation 5 LHS
double Q[3][3]; // invert of X, invert of R (taki.bas 370, 450)

double sq(double x)
{
	return x*x;
}

void make3rdPoint(double (*M)[3])
{
	double a;
	int i;
	M[0][2] = M[1][0]*M[2][1] - M[2][0]*M[1][1];
	M[1][2] = M[2][0]*M[0][1] - M[0][0]*M[2][1];
	M[2][2] = M[0][0]*M[1][1] - M[1][0]*M[0][1];
	a = sqrt(sq(M[0][2]) + sq(M[1][2]) + sq(M[2][2]));
	for (i=0; i<3; i++) M[i][2] = M[i][2] / a;
}

double det(double (*M)[3])
{
	return M[0][0]*M[1][1]*M[2][2] + M[0][1]*M[1][2]*M[2][0] \
		+ M[0][2]*M[2][1]*M[1][0] - M[0][2]*M[1][1]*M[2][0] \
		- M[0][0]*M[2][1]*M[1][2] - M[0][1]*M[1][0]*M[2][2];
}

void inv(double (*N)[3], double (*M)[3])
{
	double a;
	a = 1 / det(M);
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

/*
 * Given vector v_c, calculate the horizontal angle phi f and elevation angle theta h.
 */
void angle(double *h, double *f, double *v_c)
{
	double c;
	c = sqrt(sq(v_c[0]) + sq(v_c[1]));
	if (c == 0) {
		*h = v_c[2] > 0 ? M_PI/2 : -M_PI/2;
		*f = 0;  // value does not matter
	} else {
		*h = atan(v_c[2] / c);
		if (v_c[0] > 0) {
			*f = atan(v_c[1] / v_c[0]);
		} else if (v_c[0] < 0) {
			*f = atan(v_c[1] / v_c[0]) + M_PI;
		} else {
			*f = v_c[1] > 0 ? M_PI/2 : M_PI*3/2;
		}
		if (*f < 0) {
			*f = *f + 2 * M_PI;
		}
	}
}

/*
 * Equation 14 in taki website.  Get apparent telescope angle from true telescope angle.
 * Modified to use cartesian vector for skipping trigonometric function for efficiency.
 * (27% less time for running whole transformation)
 */
void apparent_angle(double *f, double *h, double *v_c)
{
	double v[3];
	double xy = sqrt(sq(v_c[0]) + sq(v_c[1]));
	double xyz = sqrt(sq(v_c[0]) + sq(v_c[1]) + sq(v_c[2]));
	double cos_f = xy / xyz;
	double sin_f = v_c[2] / xyz;
	double cos_h = v_c[0] / xy;
	double sin_h = v_c[1] / xy;

	v[0] = cos_h * cos_f + sin_h * Z2 - sin_h * sin_f * Z1;
	v[1] = sin_h * cos_f - cos_h * Z2 + cos_h * sin_f * Z1;
	v[2] = sin_f;
	angle(h, f, v);
	*h = *h - Z3;
}

void eq2tel(double *f, double *h, struct celestial_star *cstar)  // taki.bas 485
{
	double v_t[3];
	double v_c[3];
	double d, b;
	int i;

	d = cstar->dec;
	b = cstar->asc - K * cstar->time * 0.000072722; //.25 / 60 / G
	v_t[0] = cos(d) * cos(b);
	v_t[1] = cos(d) * sin(b);
	v_t[2] = sin(d);
	for (i=0; i<3; i++) {
		v_c[i] = R[i][0]*v_t[0] + R[i][1]*v_t[1] + R[i][2]*v_t[2];
	}
	apparent_angle(f, h, v_c);
}

void tel2eq(double f, double h, struct celestial_star *cstar)  // taki.bas 570
{
	int i;
	double v_t[3];
	double v_c[3];

	h = h + Z3;
	v_t[0] = cos(f)*cos(h)-sin(f)*Z2 + sin(f)*sin(h)*Z1;
	v_t[1] = sin(f)*cos(h)+cos(f)*Z2 - cos(f)*sin(h)*Z1;
	v_t[2] = sin(h);

	for (i=0; i<3; i++) {
		v_c[i] = Q[i][0]*v_t[0] + Q[i][1]*v_t[1] + Q[i][2]*v_t[2];
	}
	angle(&h, &f, v_c);
	cstar->asc = f + K * cstar->time*0.25;
	cstar->asc = cstar->asc - ((int)(cstar->asc/(2*M_PI))) * (2*M_PI);
	cstar->dec = h - Z3;
}

void align(struct ref_star *ref_stars)
{
	double d, b, f, h;
	double X[3][3]; // Right matrix in equation 5 RHS
	double Y[3][3]; // Left matrix in equation 5 RHS

	for (int i=0; i<2; i++) {
		struct ref_star *s = &ref_stars[i];
		d = s->dec;
		// earth rotate ~0.0041667deg / second
		b = s->asc - K*s->time * 0.000072722;
		X[0][i] = cos(d) * cos(b);  // taki.bas 220
		X[1][i] = cos(d) * sin(b);
		X[2][i] = sin(d);
		f = s->phi;
		h = s->theta;
		h = h + Z3;  // taki.bas 235
		Y[0][i] = cos(f)*cos(h) - sin(f)*(Z2) + sin(f)*sin(h)*(Z1);
		Y[1][i] = sin(f)*cos(h) + cos(f)*(Z2) - cos(f)*sin(h)*(Z1);
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
}

#if 1 // standalone test

struct ref_star stars[2] = {
	{2352, 39.9/G, 39.9/G, 79.172/G, 45.998/G}, //"A AUR"
	{2418, 94.6/G, 36.2/G, 37.960/G, 89.264/G}, //"A UMI"
};

int main()
{
	struct celestial_star target_star = {
		.asc = 71.53/G,
		.dec = 17.07/G,
		.time = 3720,
	};
	double phi, theta;
	printf("A %.2f %.2f %.2f %.2f %.2f\n", stars[0].phi, stars[0].theta, stars[0].dec, stars[0].asc, stars[0].time);
	printf("B %.2f %.2f %.2f %.2f %.2f\n", stars[1].phi, stars[1].theta, stars[1].dec, stars[1].asc, stars[1].time);
	printf("C %.2f %.2f %.2f\n", target_star.asc, target_star.dec, target_star.time);
	align(stars);
	eq2tel(&phi, &theta, &target_star);
	printf("TELESCOPE DIRECTION (RAD): %'.5f\n", phi);
	printf("TELESCOPE ELEVATION (RAD): %'.5f\n", theta);
	return 0;
}
#endif
