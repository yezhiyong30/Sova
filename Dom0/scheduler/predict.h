
#ifndef PREDICT_H
#define PREDICT_H

/* Solve using maximum entropy method */
#define MAXENTROPY 0

/* Solve using least squares method */
#define LEASTSQUARES 1

/* Number of maximum autoregressive coefficients */
#define MAXCOEFF 5

float predict(float *, int, int, int);
static bool AutoRegression(float *, int, int, float *, int);
static bool ARMaxEntropy(float *, int, int, float **, float *, float *, float *, float *);
static bool ARLeastSquare(float *, int, int , float *);
static bool SolveLE(float **, float *, unsigned int );

#endif /* PREDICT_H */