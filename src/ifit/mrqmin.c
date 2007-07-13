/* Copyright Numerical Recipes in C, Cambridge University Press */
/* Not to be distributed with code */

#define NRANSI
#include "vector.h"
#include "matrix.h"
#include "nr.h"

int mrqmin(float x[], float y[], float sig[], int ndata, float a[], int ia[],
	   int ma, float **covar, float **alpha, float *chisq,
	   void (*funcs)(float, float [], float *, float [], int), float *alamda)
{
  int j,k,l;
  static int mfit;
  static float ochisq,*atry,*beta,*da,**oneda;

  if (*alamda < 0.0) {
    atry=vector(1,ma);
    beta=vector(1,ma);
    da=vector(1,ma);
    for (mfit=0,j=1;j<=ma;j++)
      if (ia[j]) mfit++;
    oneda=matrix(1,mfit,1,1);
    *alamda=0.001;
    mrqcof(x,y,sig,ndata,a,ia,ma,alpha,beta,chisq,funcs);
    ochisq=(*chisq);
    for (j=1;j<=ma;j++) atry[j]=a[j];
  }
  for (j=1;j<=mfit;j++) {
    for (k=1;k<=mfit;k++) covar[j][k]=alpha[j][k];
    covar[j][j]=alpha[j][j]*(1.0+(*alamda));
    oneda[j][1]=beta[j];
  }
  if(gaussj(covar,mfit,oneda,1)==-1)return(-1);
  for (j=1;j<=mfit;j++) da[j]=oneda[j][1];
  if (*alamda == 0.0) {
    covsrt(covar,ma,ia,mfit);
    free_matrix(oneda,1,mfit,1,1);
    free_vector(da,1,ma);
    free_vector(beta,1,ma);
    free_vector(atry,1,ma);
    return(0);
  }
  for (j=0,l=1;l<=ma;l++)
    if (ia[l]) atry[l]=a[l]+da[++j];
  mrqcof(x,y,sig,ndata,atry,ia,ma,covar,da,chisq,funcs);
  if (*chisq < ochisq) {
    *alamda *= 0.1;
    ochisq=(*chisq);
    for (j=1;j<=mfit;j++) {
      for (k=1;k<=mfit;k++) alpha[j][k]=covar[j][k];
      beta[j]=da[j];
    }
    for (l=1;l<=ma;l++) a[l]=atry[l];
  } else {
    *alamda *= 10.0;
    *chisq=ochisq;
  }
  return(0);
}
#undef NRANSI
