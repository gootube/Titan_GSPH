#ifndef BND_IMAGE__H
#define BND_IMAGE__H

#include <constants.h>
struct BND_IMAGE 
{
  //!  process id of bucket containing image
  int      buckproc; 
  //!  process id of ghost particle
  int      partproc;
  //!  key of bucket that contains image
  unsigned bucket_key[KEYLENGTH];
  //!  key of ghost whos image it is
  unsigned ghost_key[KEYLENGTH];
  //!  coordinate of image
  double   coord[DIMENSION];
  //!  interpolate values of state_vars
  double   state_vars_interp[NO_OF_EQNS];

  // constructor 1
  BND_IMAGE ()
  {
    buckproc = -1;
    partproc = -1;
    state_vars_interp[0] = 1.0;
    for (int i=1; i<NO_OF_EQNS; i++)
      state_vars_interp[i] = 0;
  }

  // constructor 2
  BND_IMAGE (int p1, int p2, unsigned *bkey, unsigned *pkey, double *crd)
  {
    int i;
    buckproc = p1;
    partproc = p2;
    // keys
    for (i=0;i<KEYLENGTH;i++)
    {
      bucket_key[i] = *(bkey+i);
      ghost_key[i]  = *(pkey+i);
    }
    // coords
    for (i=0;i<DIMENSION;i++)
      coord[i] = *(crd+i);

    // inter polated values
    state_vars_interp[0] = 1.;
    for (i=1;i<NO_OF_EQNS;i++)
      state_vars_interp[i]=0;
  }
};

typedef BND_IMAGE BndImage;


#endif // BND_IMAGE__H
