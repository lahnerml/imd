
/******************************************************************************
*
* IMD -- The ITAP Molecular Dynamics Program
*
* Copyright 1996-2001 Institute for Theoretical and Applied Physics,
* University of Stuttgart, D-70550 Stuttgart
*
******************************************************************************/

/******************************************************************************
*
* imd_deform.c -- deform sample
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

#ifdef HOMDEF   /* homogeneous deformation with pbc */

/*****************************************************************************
*
* expand_sample()
*
*****************************************************************************/

void expand_sample(void)
{
  int k;
  
  /* Apply expansion */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      ORT(p,i,X) *= expansion.x;
      ORT(p,i,Y) *= expansion.y;
#ifndef TWOD
      ORT(p,i,Z) *= expansion.z;
#endif
    }
  }

  /* new box size */
#ifdef TWOD
  box_x.x *= expansion.x;  box_y.x *= expansion.x;
  box_x.y *= expansion.y;  box_y.y *= expansion.y;
#else
  box_x.x *= expansion.x;  box_x.y *= expansion.y;  box_x.z *= expansion.z;
  box_y.x *= expansion.x;  box_y.y *= expansion.y;  box_y.z *= expansion.z;
  box_z.x *= expansion.x;  box_z.y *= expansion.y;  box_z.z *= expansion.z;
#endif
  make_box();

} /* expand sample */


/*****************************************************************************
*
* shear_sample()
*
*****************************************************************************/

void shear_sample(void)
{
  int k;
  real tmpbox;

  /* Apply shear */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    real tmport[2];
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      tmport[0]   = shear_factor.x * ORT(p,i,Y);
      tmport[1]   = shear_factor.y * ORT(p,i,X);
      ORT(p,i,X) += tmport[0];
      ORT(p,i,Y) += tmport[1];
    }
  }

  /* new box size */
  tmpbox = box_x.x;
  box_x.x += shear_factor.x * box_x.y;
  box_x.y += shear_factor.y * tmpbox;

  tmpbox = box_y.x;
  box_y.x += shear_factor.x * box_y.y;
  box_y.y += shear_factor.y * tmpbox;

#ifndef TWOD
  tmpbox = box_z.x;
  box_z.x += shear_factor.x * box_z.y;
  box_z.y += shear_factor.y * tmpbox;
#endif

  make_box();

} /* shear sample */


/*****************************************************************************
* 
* lin_deform()
*
*****************************************************************************/

void lin_deform(void)
{
   int k;
   real tmpbox[3];
   
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    real tmport[3];
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
       /* transform atom positions */
      tmport[0] =   lindef_x.x * ORT(p,i,X) + lindef_x.y * ORT(p,i,Y)
#ifndef TWOD
                  + lindef_x.z * ORT(p,i,Z)
#endif
                                                 ;
      tmport[1] =   lindef_y.x * ORT(p,i,X) + lindef_y.y * ORT(p,i,Y)
#ifndef TWOD
                  + lindef_y.z * ORT(p,i,Z)
#endif
                                                 ;
#ifndef TWOD
      tmport[2] =   lindef_z.x * ORT(p,i,X) + lindef_z.y * ORT(p,i,Y)
                  + lindef_z.z * ORT(p,i,Z) ;
#endif

      ORT(p,i,X) += tmport[0];
      ORT(p,i,Y) += tmport[1];
#ifndef TWOD
      ORT(p,i,Z) += tmport[2];
#endif
    }
  }

  /* transform first box vector */
  tmpbox[0] = SPROD(lindef_x,box_x) ;
  tmpbox[1] = SPROD(lindef_y,box_x) ;
#ifndef TWOD
  tmpbox[2] = SPROD(lindef_z,box_x) ;
#endif

  box_x.x += tmpbox[0];
  box_x.y += tmpbox[1];
#ifndef TWOD
  box_x.z += tmpbox[2];
#endif
  
  /* transform second box vector */
  tmpbox[0] = SPROD(lindef_x,box_y) ;
  tmpbox[1] = SPROD(lindef_y,box_y) ;
#ifndef TWOD
  tmpbox[2] = SPROD(lindef_z,box_y) ;
#endif

  box_y.x += tmpbox[0];
  box_y.y += tmpbox[1];
#ifndef TWOD
  box_y.z += tmpbox[2];
#endif

  /* transform third box vector */
#ifndef TWOD
  tmpbox[0] = SPROD(lindef_x,box_z) ;
  tmpbox[1] = SPROD(lindef_y,box_z) ;
  tmpbox[2] = SPROD(lindef_z,box_z) ;

  box_z.x += tmpbox[0];
  box_z.y += tmpbox[1];
  box_z.z += tmpbox[2];
#endif
 
  /* apply box changes */
  make_box();
 
} /* lin_deform */


#endif /* HOMDEF */


#ifdef DEFORM

/*****************************************************************************
*
* deform_sample()
*
*****************************************************************************/

void deform_sample(void) 
{
  int k;
  /* loop over all atoms */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    int sort;
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      sort = VSORTE(p,i);
      /* move particles with virtual types  */
      ORT(p,i,X) += deform_size * (deform_shift + sort)->x;
      ORT(p,i,Y) += deform_size * (deform_shift + sort)->y;
#ifndef TWOD
      ORT(p,i,Z) += deform_size * (deform_shift + sort)->z;
#endif
    }
  }
}

#endif /* DEFORM */

