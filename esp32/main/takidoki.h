struct ref_star {
	double time;
	double phi;
	double theta;
	double asc;
	double dec;
};

struct celestial_star {
	double asc;
	double dec;
	double time;
};

void align(struct ref_star *ref_stars);
void eq2tel(double *f, double *h, struct celestial_star *cstar);
