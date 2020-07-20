#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "predict.h"


/* Use the AR model to predict the value of the next moment in the future */
float predict(float *data, int length, int degree, int method)
{
	int i, j;
	float *coefficients = NULL;
	float estimatrix, predic t = 0.0;
	float mean = 0.0, rmserror = 0.0;

	/* Maximum number of coefficients */
	if ((coefficients = (float *)malloc(MAXCOEFF * sizeof(float))) == NULL) {
		fprintf(stderr, "Failed to allocate space for coefficients.\n");
		exit(-1);
	}

	if (degree >= MAXCOEFF) {
		fprintf(stderr, "Maximum degree is %d\n", MAXCOEFF-1);
		exit(-1);
	}

	if (method != MAXENTROPY && method != LEASTSQUARES) {
		fprintf(stderr, "Didn't get a valid method\n");
		exit(-1);
	}

	/* Calculate the coefficients */
   if (!AutoRegression(data, length, degree, coefficients, method)) {
		fprintf(stderr, "AR routine failed\n");
		exit(-1);
	}

	/* Preprocess the sequence to make it a zero-mean sequence */
	for (i = 0; i < length; ++i) {
		mean += data[i];
	}
	mean /= (float)length;
	for (i = 0; i < length; ++i) {
		data[i] -= mean;
	}
	
	/* Calculate the rmserror */
	rmserror = 0.0;
	for (i=0; i<length; ++i) {
		estimatrix = 0.0;
		if (i > degree) {
			for (j = 0; j < degree; ++j) {
				estimatrix += coefficients[j] * data[i-j-1];
			}
			rmserror += (estimatrix - data[i]) * (estimatrix - data[i]);
		}
		printf("data: %f   estimatrixed value: %f\n", data[i], estimatrix);
	}

	if (sqrt(rmserror/length) > 0.1) {
		return -1;
	}
	printf("length: %d  degree: %d   rmserror:%f\n", length, degree, sqrt(rmserror/length));
	
	/* Predict the value of the next moment in the future */
	i = length;
	for (j = 0; j < degree; ++j) {
		predict += coefficients[j] * data[i-j-1];
	}

	if (coefficients != NULL) {
		free(coefficients);
	}

	return predict + mean;
}

/* AR model calculates the autoregressive coefficient of the sequence */
static bool AutoRegression(float *inputseries, int length, int degree,
						   float *coefficients, int method)
{
	int i, t; 
	float mean;      
	float *data = NULL; 
	float *h = NULL, *g = NULL, *per = NULL, *pef = NULL; 
	float **ar=NULL;     /* AR coefficients, all degrees */

	/* Allocate space for dataorking variables */
	if ((data = (float *)malloc(length*sizeof(float))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	if ((h = (float *)malloc((degree + 1)*sizeof(float))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	if ((g = (float *)malloc((degree + 2)*sizeof(float))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	if ((per = (float *)malloc((length + 1)*sizeof(float))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	if ((pef = (float *)malloc((length + 1)*sizeof(float))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	if ((ar = (float **)malloc((degree + 1)*sizeof(float*))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}
	for (i = 0; i < degree + 1; i++) {
		if ((ar[i] = (float *)malloc((degree + 1)*sizeof(float))) == NULL ) {
			fprintf(stderr, "Unable to malloc memory - fatal!\n");
			exit(-1);
		}
	}

	/* Preprocess the sequence to make it a zero-mean sequence */
	mean = 0.0;
	for (t = 0; t < length; t++) {
		mean += inputseries[t];
	}
	mean /= (float)length;
	for (t = 0; t < length; t++) {
		data[t] = inputseries[t] - mean;
	}

	/* Perform the appropriate AR calculation */
	if (method == MAXENTROPY) {
		if (!ARMaxEntropy(data, length, degree, ar, per, pef, h, g)) {
			fprintf(stderr, "Max entropy failed - fatal!\n");
			exit(-1);
		}
		for (i = 1; i <= degree; i++) {
			coefficients[i-1] = -ar[degree][i];
		}
	} else if (method == LEASTSQUARES) {
		if (!ARLeastSquare(data, length, degree, coefficients)) {
			fprintf(stderr, "Least squares failed - fatal!\n");
			exit(-1);
		}
	} else {
		fprintf(stderr, "Unknodatan method\n");
		exit(-1);
	}

	if (data != NULL) {
		free(data);
	}
	if (h != NULL) {
		free(h);
	}
	if (g != NULL) {
		free(g);
	}
	if ( per != NULL ) {
		free(per);
	}
	if (pef != NULL) {
		free(pef);
	}
	if (ar != NULL) {
		for (i = 0; i < degree + 1; i++) {
			if (ar[i] != NULL) {
				free(ar[i]);
			}
		}
		free(ar);
	}

	return true;
}

/* Maximum entropy method */
static bool ARMaxEntropy(float *inputseries, int length, int degree, float **ar,
						 float *per, float *pef, float *h, float *g)
{
	int j, n, nn, jj;
	float sn, sd;
	float t1, t2;

	for (j = 1; j <= length; j++) {
		pef[j] = 0;
		per[j] = 0;
	}

	for (nn = 2; nn <= degree + 1; nn++) {
		n  = nn - 2;
		sn = 0.0;
		sd = 0.0;
		jj = length - n - 1;

		for (j = 1; j <= jj; j++) {
			t1 = inputseries[j + n] + pef[j];
			t2 = inputseries[j - 1] + per[j];
			sn -= 2.0 * t1 * t2;
			sd += (t1 * t1) + (t2 * t2);
		}
		g[nn] = sn / sd;
		t1 = g[nn];

		if (n != 0) {
			for (j = 2; j < nn; j++) {
				h[j] = g[j] + (t1 * g[n - j + 3]);
			}
			for (j = 2; j < nn; j++) {
				g[j] = h[j];
			}
			jj--;
		}
		for (j = 1; j <= jj; j++) {
			per[j] += (t1 * pef[j]) + (t1 * inputseries[j + nn - 2]);
			pef[j]  = pef[j + 1] + (t1 * per[j + 1]) + (t1 * inputseries[j]);
		}

		for (j = 2; j <= nn; j++) {
			ar[nn-1][j-1] = g[j];
		}
	}

	return true;
}

/* Least squares solution */
static bool ARLeastSquare(float *inputseries, int length, int degree, float *coefficients)
{
	int i, j, k, hj, hi;
	float **matrix;

	if ((matrix = (float **)malloc(degree * sizeof(float *))) == NULL) {
		fprintf(stderr, "Unable to malloc memory - fatal!\n");
		exit(-1);
	}

	for (i = 0; i < degree; i++) {
		if ((matrix[i] = (float *)malloc(degree * sizeof(float))) == NULL) {
			fprintf(stderr, "Unable to malloc memory - fatal!\n");
			exit(-1);
		}
	}

	for (i = 0; i < degree; i++) {
		coefficients[i] = 0.0;
		for (j = 0; j < degree; j++) {
			matrix[i][j] = 0.0;
		}
	}

	for (i = degree-1; i < length - 1; i++) {
		hi = i + 1;
		for (j = 0; j < degree; j++) {
			hj = i - j;
			coefficients[j] += (inputseries[hi] * inputseries[hj]);
			for (k=j; k<degree; k++) {
				matrix[j][k] += (inputseries[hj] * inputseries[i-k]);
			}
		}
	}

	for (i = 0; i < degree; i++) {
		coefficients[i] /= (length - degree);
		for (j = i; j < degree; j++) {
			matrix[i][j] /= (length - degree);
			matrix[j][i] = matrix[i][j];
		}
	}

   /* Solve the linear equations */
	if (!SolveLE(matrix, coefficients, degree)) {
		fprintf(stderr, "Linear solver failed - fatal!\n");
		exit(-1);
	}
     
	for ( i = 0; i < degree; i++) {
		if (matrix[i] != NULL) {
			free(matrix[i]);
		}
	}

	if (matrix != NULL) {
		free(matrix);
	}
	
   return true;
}

/* Gaussian elimination method to solve matrix */
static bool SolveLE(float **matrix, float *vec, unsigned int n)
{
	int i, j, k, maxi;
	float vswap, *mswap, *hvec, max, h, pivot, q; 
  
	for (i = 0; i < n-1; i++) {
		max = fabs(matrix[i][i]);
		maxi = i;

		for (j = i + 1; j < n; j++) {
			if ((h = fabs(matrix[j][i])) > max) {
				max = h;
				maxi = j;
			}
		}

		if (maxi != i) {
			mswap = matrix[i];
			matrix[i] = matrix[maxi];
			matrix[maxi] = mswap;
			vswap = vec[i];
			vec[i] = vec[maxi];
			vec[maxi] = vswap;
		}

		hvec = matrix[i];
		pivot = hvec[i];
		if (fabs(pivot) == 0.0) {
			fprintf(stderr, "Singular matrixrix - fatal!\n");
			return false;
		}

		for (j = i + 1; j < n; j++) {
			q = - matrix[j][i] / pivot;
			matrix[j][i] = 0.0;
			for (k = i + 1; k < n; k++) {
				matrix[j][k] += q * hvec[k];
			}
			vec[j] += (q * vec[i]);
		}
	}
	vec[n-1] /= matrix[n-1][n-1];

	for (i = n - 2; i >= 0; i--) {
		hvec = matrix[i];
		for (j = n - 1; j > i; j--) {
			vec[i] -= (hvec[j] * vec[j]);
		}
		vec[i] /= hvec[i];
	}
   
   return true;
}
