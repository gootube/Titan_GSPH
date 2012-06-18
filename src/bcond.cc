
/*
 * =====================================================================================
 *
 *       Filename:  bcond.cc
 *
 *    Description:  
 *
 *        Created:  08/04/2010 02:25:33 PM EDT
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

#include <vector>
#include <cassert>
#include <iostream>
using namespace std;

#include <hashtab.h>
#include <bucket.h>
#include <particle.h>
#include <bnd_image.h>

#include "constants.h"
#include "particler.h"
#include "sph_header.h"

int
apply_bcond(int myid, HashTable * P_table, HashTable * BG_mesh,
            MatProps * matprops, vector < BndImage > & Image_table)
{

  int i, j;
  double refc[DIMENSION]; 
  double normal[DIMENSION], velrot[DIMENSION];
  double uvec[NO_OF_EQNS], state_vars[NO_OF_EQNS]; 
  double dx[DIMENSION], s[DIMENSION];
  double supp, bnddist, wnorm, smlen;

  Key * neighbors;
  Particle * p_ghost = NULL;
  vector < Key > plist;
  vector < Key >::iterator p_itr;
  vector < BndImage >::iterator i_img;

  for (i_img = Image_table.begin(); i_img != Image_table.end(); i_img++)
    if (i_img->buckproc == myid)
    {
      // reflection coordinates
      for (i = 0; i < DIMENSION; i++)
        refc[i] = i_img->coord[i];

      // grab to particle from image-reflection
      if (i_img->partproc == myid )
      {
        p_ghost = (Particle *) P_table->lookup(i_img->ghost_key);
        if ( ! p_ghost )
        {
          fprintf (stderr, "Error: ghost particle missing on proc %d\n", myid);
          return 3;
        }

/*--------------
        int icrd[2];
        icrd[0] = (int) ( * p_ghost->get_coords () * 1000);
        icrd[1] = (int) ( * (p_ghost->get_coords () + 1) * 1000);
        if ((icrd[0] == 1105) && (icrd[1] == 845))
          fprintf (stderr, "stick break-point here\n");
 --------------*/

        double d = 0.;
        for (i = 0; i < DIMENSION; i++)
        {
          normal[i] = refc[i] - * (p_ghost->get_coords() + i);
          d += normal[i] * normal[i];
        }

        for (i = 0; i < DIMENSION; i++)
          normal[i] /= sqrt(d);
      }
      
      // reset variables
      bnddist = 1.0E10;
      wnorm = 0.;
      for (i = 0; i < NO_OF_EQNS; i++)
        uvec[i] = 0.;

      // get hold of bucket containing the image
      Bucket *buck = (Bucket *) BG_mesh->lookup(i_img->bucket_key);
      assert(buck);

      // go through every particle in the bucket to calculate its
      // contribution at reflection
      plist = buck->get_plist();
      for (p_itr = plist.begin(); p_itr != plist.end(); p_itr++)
      {
        Particle *pj = (Particle *) P_table->lookup (*p_itr);

        if (!pj->is_ghost())
        {
          double h = pj->get_smlen();

          supp = 3 * h;
          for (j = 0; j < DIMENSION; j++)
            dx[j] = refc[j] - *(pj->get_coords() + j);

          // if Particle(j) is in 3-h of reflection ...
          if (in_support(dx, supp))
          {
            for (j = 0; j < DIMENSION; j++)
              s[j] = dx[j] / h;
            for (j = 0; j < NO_OF_EQNS; j++)
              state_vars[j] = *(pj->get_state_vars() + j);
            double w = weight(s, h);
            double mj = pj->get_mass();
            uvec[0] += mj * w;
            uvec[1] += mj * w * state_vars[1] / state_vars[0];
            uvec[2] += mj * w * state_vars[2] / state_vars[0];
            uvec[3] += mj * w * state_vars[3] / state_vars[0];
            wnorm += mj * w / state_vars[0];
          }
        }
      }

      // now search neighboring buckets ...
      neighbors = buck->get_neighbors();
      for (i = 0; i < NEIGH_SIZE; i++)
        if (*(buck->get_neigh_proc() + i) > -1)
        {
          Bucket *buck_neigh = (Bucket *) BG_mesh->lookup(neighbors[i]);

          // if the neighbor is not found, and it belongs to different
          // to different proc, skip the iteration
          if ((!buck_neigh) && (*(buck->get_neigh_proc() + i) != myid))
            continue;

          // otherwise the neighbor must exist, or it is a bug
          else
            assert(buck_neigh);

          // search buckets for real particles in 3h neighborhood
          if (buck_neigh->have_real_particles())
          {
            plist = buck_neigh->get_plist();
            for (p_itr = plist.begin(); p_itr != plist.end(); p_itr++)
            {
              Particle *pj = (Particle *) P_table->lookup(*p_itr);
              assert(pj);

              if (!pj->is_ghost())
              {
                double h = pj->get_smlen();

                supp = 3 * h;
                for (j = 0; j < DIMENSION; j++)
                  dx[j] = refc[j] - *(pj->get_coords() + j);

                // if Particle(j) is in 3-h of reflection ...
                if (in_support(dx, supp))
                {
                  for (j = 0; j < DIMENSION; j++)
                    s[j] = dx[j] / h;
                  for (j = 0; j < NO_OF_EQNS; j++)
                    state_vars[j] = *(pj->get_state_vars() + j);
                  double w = weight(s, h);
                  double mj = pj->get_mass();

                  uvec[0] += mj * w;
                  uvec[1] += mj * w * state_vars[1] / state_vars[0];
                  uvec[2] += mj * w * state_vars[2] / state_vars[0];
                  uvec[3] += mj * w * state_vars[3] / state_vars[0];
                  wnorm += mj * w / state_vars[0];
                }
              }
            }
          }
        }

      // renormalize
      if ( wnorm > 0. )
        for (i = 0; i < NO_OF_EQNS; i++)
          uvec[i] /= wnorm;
      else
      {
        uvec[0] = 1.;
        for (i = 1; i < NO_OF_EQNS; i++)
          uvec[i] = 0.;
      }

      // if ghost belongs to my proc, update it
      if (i_img->partproc == myid)
      {
        reflect (&uvec[1], velrot, normal);
        for (i = 0; i < DIMENSION; i++)
          uvec[i+1] = velrot[i];
        p_ghost->put_state_vars(uvec);
        p_ghost->put_update_delayed(false);
      }
      // else store the values to snyc at appropriate time
      else
      {
        for (i = 0; i < NO_OF_EQNS; i++)
          i_img->state_vars[i] = uvec[i];
      }
    }
  return 0;
}
