/*
 * =====================================================================================
 *
 *       Filename:  createfunky.cc
 *
 *    Description:  
 *
 *        Created:  03/27/2010 05:41:33 PM EDT
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
# include <config.h>
#endif

#ifdef HAVE_HDF5_H
# include <hdf5.h>
# include <hdf5calls.h>
#endif

#include <iomanip>
#include <sstream>
#include <string>
#include <cstdio>
using namespace std;

#include "buckstr.h"
#include "preprocess.h"

string create_filename (string base, string ext, const int padding, int myid)
{
  ostringstream ss;
  ss << base << setw(padding) << setfill('0') << myid << ext; 
  return ss.str();
}

void createfunky(int np, int nhtvars, double *htvars, 
                 int nbucket, BucketStruct *bg)
{

  int i, j;
  string basename = "funky";
  string exten    = ".h5";
  const int padding = 4;

  // create filenames
  hid_t *fp = new hid_t [np];
  for (i=0; i < np; i++ )
  {
    string fname = create_filename (basename, exten, padding, i);
    fp[i] = GH5_fopen(fname.c_str(),'w');
  }


  for (i=0; i< np; i++)
  {
    // write HASH TABLE constants
    int dims1[2]={nhtvars,0};
    GH5_writedata(fp[i] ,"/hashtable_constants", dims1, htvars);

    // copy cells for each proc
    int size2=0, ibuck = 0;
    for ( j=0; j < nbucket; j++)
      if ( bg[j].myproc == i )
        size2++;

    BucketStruct *myBucks = new BucketStruct [size2];
    for ( j=0; j < nbucket; j++ )
      if ( bg[j].myproc == i )
        myBucks[ibuck++] = bg[j];

    if (ibuck != size2)
    {
      fprintf(stderr,"CRAP!!! size = %d, count=%d\n", size2, ibuck);
      exit(1);
    }

    // write Background Grid data
    printf("size2 = %d\n", size2);
    GH5_write_grid_data(fp[i],"/Buckets", size2, myBucks);

    delete [] myBucks;
    // close file
    GH5_fclose(fp[i]);
  }

  delete []fp;
  return;
}
