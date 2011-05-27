/*
 * =====================================================================================
 *
 *       Filename:  sph_lib.c
 *
 *    Description:  
 *
 *        Created:  04/28/2009 04:51:24 PM EDT
 *         Author:  Dinesh Kumar (dkumar), dkumar@buffalo.edu
 *        License:  GNU General Public License
 *
 * This software can be redistributed free of charge.  See COPYING
 * file in the top distribution directory for more details.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * =====================================================================================
 * $Id:$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MPI_H
#include <mpi.h>
#endif

#include <cmath>
using namespace std;

#include "particle.h"
#include "sph_header.h"
#include "constants.h"

void rotate (double *u, double cost, double sint)
{
  int i,j;
  double temp[NO_OF_EQNS];
  double tempv[DIMENSION],vel[DIMENSION];

#ifdef THREE_D
  double rot[3][3]={{cost, sint, 0},{-sint, cost, 0}, { 0, 0, 1}};
#else
  double rot[2][2] = {{ cost, sint},{ -sint, cost }};
#endif

  // store the orignal vector 
  for (i=0; i<NO_OF_EQNS; i++)
    temp[i]=u[i];

  // don't need to rotate u[0], as  
  // u[0] = density, is a scalar

  //rotate the velocity part of it
  for (i=0; i<DIMENSION; i++)
    tempv[i] = u[i+1];
  matrix_vec_mult (&rot[0][0], DIMENSION, tempv, vel);

  for (i=0; i<DIMENSION; i++)
    u[i+1]=vel[i];
  return;
}


void unrotate(double *u, double cost, double sint)
{
  //swtich signs on sin(a)'s and call rotate
  sint *= -1;
  rotate(u,cost,sint);
  return;
}

/****************************************************
 * h is the smoothing length
 * the weight extends up to 3*h in all directions
 ***************************************************/
const double cutoff=3.0;
const double t1=0.564189583547756; // 1/sqrt(pi)

// weight function
double weight(double *s, double h)
{
  int i;
  double norm,expt,w;
  
  for (i=0; i<DIMENSION; i++)
    if (abs(*(s+i)) > cutoff)
      return 0;

  norm = 1;
  expt = 0;
  for (i=0; i<DIMENSION; i++)
  {
    norm *= t1/h;
    expt += pow(*(s+i),2);
  }
  w=norm*exp(-expt);
  return w;
}

/****************************************************
 * d_weight is derivative of weight function in 
 * any given direction
 * x-dir = 0
 * y-dir = 1
 * z-dir = 2
 ****************************************************/
// d(weight function)
double d_weight (double *s, double h, int dir)
{
   int i;
   double dw,tmp;
   for ( i=0; i<DIMENSION; i++)
     if ( abs(*(s+i)) > cutoff )
      return 0;

   tmp=-2*(*(s+dir)/h);
   dw=tmp*weight(s,h);
   return dw;
}

void matrix_vec_mult(double *A, int N, double *b, double *c)
{
  register int i,j;
  for (i=0; i<N; i++)
    c[i]=0; 

  for (i=0; i<N; i++)
    for (j=0; j<N; j++)
      *(c+i) += *(A+i*N+j)*(*(b + j));

  return;
}

void matrix_matrix_mult(double  *A, int N, int P, double *B, int M, double *C)
{
  register int i,j,k;
  for (i=0; i < N; i++)
    for (j=0; j < M; j++)
      *(C + i*M + j) = 0;

  for (i=0; i< N; i++)
    for (j=0; j < M; j++)
       for (k=0; k < P; k++)
         *(C + i*M + j) += *(A + i*P + k)*( *(B+ k*M + j) );

  return;
}

// code generated by Maple ( assummed correct ) 
void inv3d(double *A, double *Ap)
{
  double t4  = A[2]*A[3];
  double t6  = A[2]*A[6];
  double t8  = A[1]*A[3];
  double t10 = A[1]*A[6];
  double t12 = A[0]*A[4];
  double t14 = A[0]*A[7];
  double t17 = 1.0/(t4*A[7]-t6*A[4]-t8*A[8]+t10*A[5]+t12*A[8]-t14*A[5]);
  if ( abs(t17) < 1.0E-06 )
  {
    cerr <<"ERROR: Singular matix encountered while calculating linsolve"<<endl;
    exit(1);
  }
  Ap[0] = (A[4]*A[8] - A[7]*A[5])*t17;
  Ap[1] =-(A[3]*A[8] - A[6]*A[5])*t17;
  Ap[2] = (A[3]*A[7] - A[6]*A[4])*t17;
  Ap[3] =-(-A[2]*A[7] + A[1]*A[8])*t17;
  Ap[4] = (-t6 + A[0]*A[8])*t17;
  Ap[5] = -(-t10 + t14) * t17;
  Ap[6] = (-A[2]*A[4] + A[1]*A[5])*t17;
  Ap[7] = -(-t4 + A[0]*A[5])*t17;
  Ap[8] = (-t8 + t12)*t17;
}

void inv2d(double *A, double *Ap)
{
  double t1 = 1.0/(A[0]*A[3] - A[1]*A[2]);
  Ap[0] = A[4]*t1;
  Ap[1]= -A[2]*t1;
  Ap[2]= -A[1]*t1;
  Ap[3]=  A[0]*t1;
}

void linsolve(double *A, int N, double *b, int M, double *d)
{

#ifdef THREE_D
  double Ap[9];
  inv3d(A, Ap);
#else
  double Ap[4];
  inv2d(A, Ap);
#endif
  matrix_matrix_mult(Ap, N, N, b, M, d);
  return;
}

void sph_exit(int ierr)
{
#ifdef MULTI_PROC 
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Abort(MPI_COMM_WORLD, ierr);
#endif
  exit(ierr);
}

FILE *debug_fopen()
{
  int myid=0;
  char filename[15];
#ifdef MULTI_PROC
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
#endif
  sprintf(filename,"debug%04d.log",myid);
  FILE *fp = fopen(filename,"a+");
  return fp;
}
