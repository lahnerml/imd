/******************************************************************************
*
* imd_geom_3d.c -- Domain decomposition routines for the imd package
*
******************************************************************************/

/******************************************************************************
* $RCSfile$
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

/* To determine the cell into which a given particle belongs, we
   have to transform the cartesian coordinates of the particle into
   the coordinate system that is spanned by the vectors of the box
   edges. This yields coordinates in the interval [0..1] that are
   multiplied by global_cell_dim to get the cells index.

   Here we calculate the transformation matrix. */

void make_box( void )
{
  real det;

  /* Determinant */
  det = box_x.y * box_y.z * box_z.x +
        box_z.z * box_y.x * box_z.y +
	box_x.x * box_y.y * box_z.z -
        box_x.z * box_y.y * box_z.x -
        box_x.x * box_y.z * box_z.y -
	box_x.y * box_y.x * box_z.z;
  if ((0==myid) && (0==det)) error("Box Edges are parallel.");

  /* transposed inverse box */
  tbox_x.x = ( box_y.y * box_z.z - box_y.z * box_z.y ) / det;
  tbox_y.x = ( box_x.z * box_z.y - box_x.y * box_z.z ) / det;
  tbox_z.x = ( box_x.y * box_y.z - box_x.z * box_y.y ) / det;

  tbox_x.y = ( box_y.z * box_z.x - box_y.x * box_z.z ) / det;
  tbox_y.y = ( box_x.x * box_z.z - box_x.z * box_z.x ) / det;
  tbox_z.y = ( box_x.z * box_y.x - box_x.x * box_y.z ) / det;

  tbox_x.z = ( box_y.x * box_z.y - box_y.y * box_z.x ) / det;
  tbox_y.z = ( box_x.y * box_z.x - box_x.x * box_z.y ) / det;
  tbox_z.z = ( box_x.x * box_y.y - box_x.y * box_y.x ) / det;

}


/* Calculate smallest possible cell (i.e. the height==cutoff) */
/* This needs a better way to do operations on 3d vectors */

ivektor maximal_cell_dim( void ) 
{
  real    s1, s2, r2_cut, r2_cut2;
  vektor  hx,hy,hz;
  ivektor max_cell_dim;
  int     i,j;

  /* Height x */
  s1 = SPROD(box_x,box_y)/SPROD(box_y,box_y);
  s2 = SPROD(box_x,box_z)/SPROD(box_z,box_z);
  
  hx.x = box_x.x - s1 * box_y.x - s2 * box_z.x;
  hx.y = box_x.y - s1 * box_y.y - s2 * box_z.y;
  hx.z = box_x.z - s1 * box_y.z - s2 * box_z.z;
  
  /* Height y */
  s1 = SPROD(box_y,box_x)/SPROD(box_x,box_x);
  s2 = SPROD(box_y,box_z)/SPROD(box_z,box_z);

  hy.x = box_y.x - s1 * box_x.x - s2 * box_z.x;
  hy.y = box_y.y - s1 * box_x.y - s2 * box_z.y;
  hy.z = box_y.z - s1 * box_x.z - s2 * box_z.z;
  
  /* Height z */
  s1 = SPROD(box_z,box_x)/SPROD(box_x,box_x);
  s2 = SPROD(box_z,box_y)/SPROD(box_y,box_y);
  
  hz.x = box_z.x - s1 * box_x.x - s2 * box_y.x;
  hz.y = box_z.y - s1 * box_x.y - s2 * box_y.y;
  hz.z = box_z.z - s1 * box_x.z - s2 * box_y.z;
  
  /* Scaling factors box/cell */
#ifdef EAM2  
  /* the tables are in r2 */
  /* get the biggest r_cut of eam2_phi_r_end */
  r2_cut=0.0;
  for(i=0;i<ntypes;i++)
    for(j=0;j<ntypes;j++)
      r2_cut = MAX( r2_cut, *PTR_2D(eam2_phi_r_end,i,j,ntypes,ntypes) );

  /* get the biggest r_cut of eam2_r_end */
  r2_cut2=0.0;
  for(i=0;i<ntypes;i++)
    for(j=0;j<ntypes;j++)
      r2_cut2 = MAX( r2_cut2, *PTR_2D(eam2_r_end,i,j,ntypes,ntypes) );

  /* take the biggest one as actual cut-off */
  r2_cut2 = MAX(r2_cut2,r2_cut);
  printf("The actual cut-off is %lf (cut-off of core-core Potential: %lf)\n",
         r2_cut2, r2_cut);
  cellsz = MAX(cellsz, r2_cut2);
#endif

  max_cell_dim.x = (int) ( 1.0 / sqrt( cellsz / SPROD(hx,hx) ));
  max_cell_dim.y = (int) ( 1.0 / sqrt( cellsz / SPROD(hy,hy) ));
  max_cell_dim.z = (int) ( 1.0 / sqrt( cellsz / SPROD(hz,hz) ));
  return(max_cell_dim);

}


/* init_cells determines the size of the cells. The cells generated by
   scaling of the box. The scaling is done such that the distance
   between two of the cell's faces (==height) equals the cutoff.  
   An integer number of cells must fit into the box. If the cell
   calculated by the scaling described above is too small, it is
   enlarged accordingly.

   There must be at least three cells in each direction.  */

void init_cells( void )

{
  int i, j, k, l;
  vektor hx, hy, hz; 
  real tmp;
  vektor cell_scale;
  ivektor next_cell_dim, cell_dim_old;
  ivektor cellmin_old, cellmax_old, cellc;
  cell *p, *cell_array_old; 
  real s1, s2, r2_cut, r2_cut2;

#ifdef NPT
  if (0 == myid) {
    if (ensemble == ENS_NPT_ISO) {
      printf("actual_shrink=%f limit_shrink=%f limit_growth=%f\n",
              actual_shrink.x, limit_shrink.x, limit_growth.x );
    }
    else if (ensemble == ENS_NPT_AXIAL) {
      printf("actual_shrink.x=%f limit_shrink.x=%f limit_growth.x=%f\n",
              actual_shrink.x,   limit_shrink.x,   limit_growth.x );
      printf("actual_shrink.y=%f limit_shrink.y=%f limit_growth.y=%f\n",
              actual_shrink.y,   limit_shrink.y,   limit_growth.y );
      printf("actual_shrink.z=%f limit_shrink.z=%f limit_growth.z=%f\n",
              actual_shrink.z,   limit_shrink.z,   limit_growth.z );
    }
  }
#endif

  /* Calculate smallest possible cell (i.e. the height==cutoff) */
  /* This needs a better way to do operations on 3d vectors */

  /* Height x */
  s1 = SPROD(box_x,box_y)/SPROD(box_y,box_y);
  s2 = SPROD(box_x,box_z)/SPROD(box_z,box_z);
  
  hx.x = box_x.x - s1 * box_y.x - s2 * box_z.x;
  hx.y = box_x.y - s1 * box_y.y - s2 * box_z.y;
  hx.z = box_x.z - s1 * box_y.z - s2 * box_z.z;
  
  /* Height y */
  s1 = SPROD(box_y,box_x)/SPROD(box_x,box_x);
  s2 = SPROD(box_y,box_z)/SPROD(box_z,box_z);

  hy.x = box_y.x - s1 * box_x.x - s2 * box_z.x;
  hy.y = box_y.y - s1 * box_x.y - s2 * box_z.y;
  hy.z = box_y.z - s1 * box_x.z - s2 * box_z.z;
  
  /* Height z */
  s1 = SPROD(box_z,box_x)/SPROD(box_x,box_x);
  s2 = SPROD(box_z,box_y)/SPROD(box_y,box_y);
  
  hz.x = box_z.x - s1 * box_x.x - s2 * box_y.x;
  hz.y = box_z.y - s1 * box_x.y - s2 * box_y.y;
  hz.z = box_z.z - s1 * box_x.z - s2 * box_y.z;
  
  /* Scaling factors box/cell */
#ifdef EAM2  
  /* the tables are in r2 */
  /* get the biggest r_cut of eam2_phi_r_end */
  r2_cut=0.0;
  for(i=0;i<ntypes;i++)
    for(j=0;j<ntypes;j++)
      r2_cut = MAX( r2_cut, *PTR_2D(eam2_phi_r_end,i,j,ntypes,ntypes) );

  /* get the biggest r_cut of eam2_r_end */
  r2_cut2=0.0;
  for(i=0;i<ntypes;i++)
    for(j=0;j<ntypes;j++)
      r2_cut2 = MAX( r2_cut2, *PTR_2D(eam2_r_end,i,j,ntypes,ntypes) );

  /* take the biggest one as actual cut-off */
  r2_cut2 = MAX(r2_cut2,r2_cut);
  printf("The actual cut-off is %lf (cut-off of core-core Potential: %lf)\n",
         r2_cut2, r2_cut);
  cellsz = MAX(cellsz, r2_cut2);
#endif

  cell_scale.x = sqrt( cellsz / SPROD(hx,hx) );
  cell_scale.y = sqrt( cellsz / SPROD(hy,hy) );
  cell_scale.z = sqrt( cellsz / SPROD(hz,hz) );

#ifdef NPT
  /* the NEXT cell array for a GROWING system; 
     needed to determine when to recompute the cell division */
  if ((ensemble == ENS_NPT_ISO) || (ensemble == ENS_NPT_AXIAL)) {
    next_cell_dim.x = (int) ( (1.0 + cell_size_tolerance) / cell_scale.x );
    next_cell_dim.y = (int) ( (1.0 + cell_size_tolerance) / cell_scale.y );
    next_cell_dim.z = (int) ( (1.0 + cell_size_tolerance) / cell_scale.z );
    cell_scale.x   /= (1.0 - cell_size_tolerance);
    cell_scale.y   /= (1.0 - cell_size_tolerance);
    cell_scale.z   /= (1.0 - cell_size_tolerance);
  }
#endif
  /* set up the CURRENT cell array dimensions */
  global_cell_dim.x = (int) ( 1.0 / cell_scale.x );
  global_cell_dim.y = (int) ( 1.0 / cell_scale.y );
  global_cell_dim.z = (int) ( 1.0 / cell_scale.z );

  if (0 == myid )
  printf("Minimal cell size: \n\t ( %f %f %f ) \n\t ( %f %f %f ) \n\t ( %f %f %f )\n",
    box_x.x * cell_scale.x, box_x.y * cell_scale.x, box_x.z * cell_scale.x,
    box_y.x * cell_scale.y, box_y.y * cell_scale.y, box_y.z * cell_scale.y,
    box_z.x * cell_scale.z, box_z.y * cell_scale.z, box_z.z * cell_scale.z);

#ifdef MPI
  /* cpu_dim must be a divisor of global_cell_dim */
  if (0 != (global_cell_dim.x % cpu_dim.x))
     global_cell_dim.x = ((int)(global_cell_dim.x/cpu_dim.x))*cpu_dim.x;
  if (0 != (global_cell_dim.y % cpu_dim.y))
     global_cell_dim.y = ((int)(global_cell_dim.y/cpu_dim.y))*cpu_dim.y;
  if (0 != (global_cell_dim.z % cpu_dim.z))
     global_cell_dim.z = ((int)(global_cell_dim.z/cpu_dim.z))*cpu_dim.z;
#ifdef NPT
  /* cpu_dim must be a divisor of next_cell_dim */
  if ((ensemble == ENS_NPT_ISO) || (ensemble == ENS_NPT_AXIAL)) {
    if (0 != (next_cell_dim.x % cpu_dim.x))
       next_cell_dim.x = ((int)(next_cell_dim.x/cpu_dim.x))*cpu_dim.x;
    if (0 != (next_cell_dim.y % cpu_dim.y))
       next_cell_dim.y = ((int)(next_cell_dim.y/cpu_dim.y))*cpu_dim.y;
    if (0 != (next_cell_dim.z % cpu_dim.z))
       next_cell_dim.z = ((int)(next_cell_dim.z/cpu_dim.z))*cpu_dim.z;
  }
#endif
#endif

  /* Check if cell array is large enough */
  if ( 0 == myid ) {
#ifdef MPI
    if (global_cell_dim.x < cpu_dim.x) error("global_cell_dim.x < cpu_dim.x");
    if (global_cell_dim.y < cpu_dim.y) error("global_cell_dim.y < cpu_dim.y");
    if (global_cell_dim.z < cpu_dim.z) error("global_cell_dim.z < cpu_dim.z");
#endif
    if (global_cell_dim.x < 2) error("global_cell_dim.x < 2");
    if (global_cell_dim.y < 2) error("global_cell_dim.y < 2");
    if (global_cell_dim.z < 2) error("global_cell_dim.z < 2");
  }

#ifdef NPT

  /* if system grows, the next cell division should have more cells */
  if ((ensemble == ENS_NPT_ISO) || (ensemble == ENS_NPT_AXIAL)) {  
#ifdef MPI
    if (next_cell_dim.x == global_cell_dim.x) next_cell_dim.x += cpu_dim.x;
    if (next_cell_dim.y == global_cell_dim.y) next_cell_dim.y += cpu_dim.y;
    if (next_cell_dim.z == global_cell_dim.z) next_cell_dim.z += cpu_dim.z;
#else
    if (next_cell_dim.x == global_cell_dim.x) next_cell_dim.x += 1;
    if (next_cell_dim.y == global_cell_dim.y) next_cell_dim.y += 1;
    if (next_cell_dim.z == global_cell_dim.z) next_cell_dim.z += 1;
#endif
  }

  if (ensemble == ENS_NPT_ISO) {

    /* factor by which a cell can grow before a change of
       the cell division becomes worthwhile */
    /* getting more cells in at least one direction is enough */
    limit_growth.x = cell_scale.x * next_cell_dim.x;
    tmp            = cell_scale.y * next_cell_dim.y;
    limit_growth.x = MIN( limit_growth.x, tmp );
    tmp            = cell_scale.z * next_cell_dim.z;
    limit_growth.x = MIN( limit_growth.x, tmp );
    /* factor by which a cell can safely shrink */
    limit_shrink.x = cell_scale.x * global_cell_dim.x;
    tmp            = cell_scale.y * global_cell_dim.y;
    limit_shrink.x = MAX( limit_shrink.x, tmp );
    tmp            = cell_scale.z * global_cell_dim.z;
    limit_shrink.x = MAX( limit_shrink.x, tmp );
    limit_shrink.x = limit_shrink.x * (1.0 - cell_size_tolerance);

  }
  else if (ensemble == ENS_NPT_AXIAL) {

    /* factors by which a cell can grow before a change of
       the cell division becomes worthwhile */
    limit_growth.x = cell_scale.x * next_cell_dim.x;
    limit_growth.y = cell_scale.y * next_cell_dim.y;
    limit_growth.z = cell_scale.z * next_cell_dim.z;
    /* factor by which a cell can safely shrink */
    limit_shrink.x = cell_scale.x*global_cell_dim.x*(1.0-cell_size_tolerance);
    limit_shrink.y = cell_scale.y*global_cell_dim.y*(1.0-cell_size_tolerance);
    limit_shrink.z = cell_scale.z*global_cell_dim.z*(1.0-cell_size_tolerance);
 
 }

#endif /* NPT */

  /* If an integer number of cells does not fit exactly into the box, the
     cells are enlarged accordingly */
  cell_scale.x = 1.0 / global_cell_dim.x;
  cell_scale.y = 1.0 / global_cell_dim.y;
  cell_scale.z = 1.0 / global_cell_dim.z;

  if (0 == myid )
  printf("Actual cell size: \n\t ( %f %f %f ) \n\t ( %f %f %f ) \n\t ( %f %f %f )\n",
    box_x.x * cell_scale.x, box_x.y * cell_scale.x, box_x.z * cell_scale.x,
    box_y.x * cell_scale.y, box_y.y * cell_scale.y, box_y.z * cell_scale.y,
    box_z.x * cell_scale.z, box_z.y * cell_scale.z, box_z.z * cell_scale.z);

  if (0==myid)
  printf("Global cell array dimensions: %d %d %d\n",
          global_cell_dim.x,global_cell_dim.y,global_cell_dim.z);

  /* keep a copy of cell_dim & Co., so that we can redistribute the atoms */
  cell_dim_old = cell_dim;
  cellmin_old  = cellmin;
  cellmax_old  = cellmax;

#ifdef MPI
  /* this test should be obsolete now */
  if (0==myid) {
     if ( 0 !=  global_cell_dim.x % cpu_dim.x ) 
        error("cpu_dim.x no divisor of global_cell_dim.x");
     if ( 0 !=  global_cell_dim.y % cpu_dim.y ) 
        error("cpu_dim.y no divisor of global_cell_dim.y");
     if ( 0 !=  global_cell_dim.z % cpu_dim.z ) 
        error("cpu_dim.z no divisor of global_cell_dim.z");
  }

  cell_dim.x = global_cell_dim.x / cpu_dim.x + 2;  
  cell_dim.y = global_cell_dim.y / cpu_dim.y + 2;
  cell_dim.z = global_cell_dim.z / cpu_dim.z + 2;

  cellmin.x = 1;
  cellmin.y = 1;
  cellmin.z = 1;

  cellmax.x = cell_dim.x - 1;
  cellmax.y = cell_dim.y - 1;
  cellmax.z = cell_dim.z - 1;
    
  if (0==myid) 
    printf("Local cell array dimensions (incl buffer): %d %d %d\n",
	   cell_dim.x,cell_dim.y,cell_dim.z);
#else

  cell_dim.x = global_cell_dim.x;
  cell_dim.y = global_cell_dim.y;
  cell_dim.z = global_cell_dim.z;

  cellmin.x = 0;
  cellmin.y = 0;
  cellmin.z = 0;

  cellmax.x = cell_dim.x;
  cellmax.y = cell_dim.y;
  cellmax.z = cell_dim.z;

  printf("Local cell array dimensions: %d %d %d\n",
	 cell_dim.x,cell_dim.y,cell_dim.z);
#endif

  /* get box transformation matrix */
  make_box();

  /* save old cell_array (if any), and allocate new one */
  cell_array_old = cell_array;
  cell_array = (cell *) malloc(
		     cell_dim.x * cell_dim.y * cell_dim.z * sizeof(cell));
  if ( 0 == myid )
  if (NULL == cell_array) error("Cannot allocate memory for cells");

  /* Initialize cells */
  for (i=0; i < cell_dim.x; ++i)
    for (j=0; j < cell_dim.y; ++j)
      for (k=0; k < cell_dim.z; ++k) {

	p = PTR_3D_V(cell_array, i, j, k, cell_dim);
	p->n_max=0;
#ifdef MPI
        /* don't alloc data space for buffer cells */
        if ((0!=i) && (0!=j) && (0!=k) &&
            (i != cell_dim.x-1) &&
            (j != cell_dim.y-1) &&
            (k != cell_dim.z-1))
#endif
            alloc_cell(p, initsz);
  }

  /* on the first invocation we have to set up the MPI process topology */
#ifdef MPI
  if (cell_array_old == NULL) setup_mpi_topology();
#endif

  /* redistribute atoms */
  if (cell_array_old != NULL) {
    for (j=cellmin_old.x; j < cellmax_old.x; j++)
      for (k=cellmin_old.y; k < cellmax_old.y; k++)
        for (l=cellmin_old.z; l < cellmax_old.z; l++) {
          p = PTR_3D_V(cell_array_old, j, k, l, cell_dim_old);
          for (i = p->n - 1; i >= 0; i--) {
#ifdef MPI
            cellc = local_cell_coord(p->ort X(i),p->ort Y(i),p->ort Z(i));
            /* strangly, some atoms get into buffer cells; 
               we push them back into the real cells, 
               so that we don't lose them  */
            if (cellc.x == 0) cellc.x++;
            if (cellc.y == 0) cellc.y++;
            if (cellc.z == 0) cellc.z++;
            if (cellc.x == cellmax.x) cellc.x--;
            if (cellc.y == cellmax.y) cellc.y--;
            if (cellc.z == cellmax.z) cellc.z--;
#else
            cellc = cell_coord(p->ort X(i),p->ort Y(i),p->ort Z(i));
#endif
            move_atom( cellc, p, i );
          }
          alloc_cell( p, 0 );  /* free old cell */
    }
    free(cell_array_old);
  }

#ifdef NPT
  if ((ensemble == ENS_NPT_ISO) || (ensemble == ENS_NPT_AXIAL)) {
    revise_cell_division = 0;
    cells_too_small      = 0;
    actual_shrink.x      = 1.0;
    actual_shrink.y      = 1.0;
    actual_shrink.z      = 1.0;
  }
  if (0 == myid) {
    if (ensemble == ENS_NPT_ISO) {
      printf("actual_shrink=%f limit_shrink=%f limit_growth=%f\n",
              actual_shrink.x, limit_shrink.x, limit_growth.x );
    }
    else if (ensemble == ENS_NPT_AXIAL) {
      printf("actual_shrink.x=%f limit_shrink.x=%f limit_growth.x=%f\n",
              actual_shrink.x,   limit_shrink.x,   limit_growth.x );
      printf("actual_shrink.y=%f limit_shrink.y=%f limit_growth.y=%f\n",
              actual_shrink.y,   limit_shrink.y,   limit_growth.y );
      printf("actual_shrink.z=%f limit_shrink.z=%f limit_growth.z=%f\n",
              actual_shrink.z,   limit_shrink.z,   limit_growth.z );
    }
  }
#endif /* NPT */

  make_cell_lists();

}


/******************************************************************************
*
*  make_cell_lists creates a list of indices of all inner cells
*  (only if MPI), and a list of all pairs of interacting cells.
*  These lists make it easy to loop over these cells and pairs.
*
******************************************************************************/

void make_cell_lists(void)
{
  int i,j,k,l,m,n,r,s,t,nn;
  ivektor ipbc, neigh;
  pair *P;

  /* initialize pairs before creating the first pair lists */
  if (nallcells==0) for (i=0; i<6; ++i) pairs[i] = NULL;

  nallcells = cell_dim.x * cell_dim.y * cell_dim.z;
#ifdef MPI
  ncells = (cell_dim.x-2) * (cell_dim.y-2) * (cell_dim.z-2);
  /* make list of inner cell indices */
  cells  = (integer*) realloc( cells, ncells * sizeof(integer) );
  l = 0;
  for (i=cellmin.x; i<cellmax.x; ++i)
    for (j=cellmin.y; j<cellmax.y; ++j)
      for (k=cellmin.z; k<cellmax.z; ++k) {
        cells[l] = i * cell_dim.y * cell_dim.z + j * cell_dim.z + k;
        l++;
      }
#else
  ncells = cell_dim.x * cell_dim.y * cell_dim.z;
#endif

#if !(defined(MPI) && defined(MONOLJ)) /* i.e., not world record */ 

  /* Make lists with pairs of interacting cells, taking account of 
     the boundary conditions. We distribute pairs on several lists 
     such that among the pairs in any list there is no cell that 
     occurs twice. This allows to update forces independently
     for the pairs from the same list.
  */

  nn = sizeof(pair) * (cell_dim.x * cell_dim.y * ((cell_dim.z+1)/2));
  pairs[0] = (pair *) realloc( pairs[0], nn * 2 );  npairs[0] = 0;
  pairs[1] = (pair *) realloc( pairs[1], nn * 2 );  npairs[1] = 0;
  nn = sizeof(pair) * (cell_dim.x * ((cell_dim.y+1)/2) * cell_dim.z);
  pairs[2] = (pair *) realloc( pairs[2], nn * 3 );  npairs[2] = 0;
  pairs[3] = (pair *) realloc( pairs[3], nn * 3 );  npairs[3] = 0;
  nn = sizeof(pair) * (((cell_dim.x+1)/2) * cell_dim.y * cell_dim.z);
  pairs[4] = (pair *) realloc( pairs[4], nn * 9 );  npairs[4] = 0;
  pairs[5] = (pair *) realloc( pairs[5], nn * 9 );  npairs[5] = 0;
  if ((pairs[0]==NULL) || (pairs[1]==NULL) || (pairs[2]==NULL) ||
      (pairs[3]==NULL) || (pairs[4]==NULL) || (pairs[5]==NULL)) 
      error("cannot allocate pair lists");

  /* for each cell */
  for (i=cellmin.x; i<cellmax.x; ++i)
    for (j=cellmin.y; j<cellmax.y; ++j)
      for (k=cellmin.z; k<cellmax.z; ++k)

	/* For half of the neighbours of this cell */
	for (l=0; l <= 1; ++l)
	  for (m=-l; m <= 1; ++m)
	    for (n=(l==0 ? -m  : -l ); n <= 1; ++n) { 

              /* array where to put the pairs */
              if (l==0) {
                if (m==0) { if (k % 2 == 0) nn=0; else nn=1; }
                else      { if (j % 2 == 0) nn=2; else nn=3; }
              } else      { if (i % 2 == 0) nn=4; else nn=5; }

#ifdef MPI
              r = i+l - 1 + my_coord.x * (cell_dim.x - 2);
              s = j+m - 1 + my_coord.y * (cell_dim.y - 2);
              t = k+n - 1 + my_coord.z * (cell_dim.z - 2);
#else
              r = i+l;
              s = j+m;
              t = k+n;
#endif

              /* Apply periodic boundaries */
              ipbc.x = 0;
              if (r<0) ipbc.x--; else if (r>global_cell_dim.x-1) ipbc.x++;

              ipbc.y = 0;
              if (s<0) ipbc.y--; else if (s>global_cell_dim.y-1) ipbc.y++;

              ipbc.z = 0;
              if (t<0) ipbc.z--; else if (t>global_cell_dim.z-1) ipbc.z++;

#ifdef MPI
              r = i+l;
              s = j+m;
              t = k+n;
#else
              if (r<0) r=cell_dim.x-1; 
              else if (r>cell_dim.x-1) r=0;

              if (s<0) s=cell_dim.y-1; 
              else if (s>cell_dim.y-1) s=0;

              if (t<0) t=cell_dim.z-1; 
              else if (t>cell_dim.z-1) t=0;
#endif

              if (((pbc_dirs.x==1) || (pbc_dirs.x==ipbc.x)) &&
                  ((pbc_dirs.y==1) || (pbc_dirs.y==ipbc.y)) &&
                  ((pbc_dirs.z==1) || (pbc_dirs.z==ipbc.z)))
              {
                /* add pair to list */
                P = pairs[nn] + npairs[nn];
                P->np = i*cell_dim.y*cell_dim.z + j*cell_dim.z + k;
                P->nq = r*cell_dim.y*cell_dim.z + s*cell_dim.z + t;
                P->ipbc[0] = ipbc.x;
                P->ipbc[1] = ipbc.y;
                P->ipbc[2] = ipbc.z;
                npairs[nn]++;
	      }
	    }

#ifdef MPI

  /* If we don't use actio=reactio accross cpus, we have to do
     the force loop also on the other half of the neighbours for the 
     cells on the surface of the CPU */

  for (i=0; i<6; ++i) npairs2[i] = npairs[i];

  /* for each cell */
  for (i=cellmin.x; i<cellmax.x; ++i)
    for (j=cellmin.y; j<cellmax.y; ++j)
      for (k=cellmin.z; k<cellmax.z; ++k)

	/* for the other half of the neighbours of this cell */
	for (l=0; l <= 1; ++l)
	  for (m=-l; m <= 1; ++m)
	    for (n=(l==0 ? -m  : -l ); n <= 1; ++n) { 

              neigh.x = i-l;
              neigh.y = j-m;
              neigh.z = k-n;

              /* if second cell is a buffer cell */
              if ((neigh.x == 0) || (neigh.x == cell_dim.x-1) || 
                  (neigh.y == 0) || (neigh.y == cell_dim.y-1) ||
                  (neigh.z == 0) || (neigh.z == cell_dim.z-1)) 
              {
                /* array where to put the pairs */
                if (l==0) {
                  if (m==0) { if (k % 2 == 0) nn=0; else nn=1; }
                  else      { if (j % 2 == 0) nn=2; else nn=3; }
                } else      { if (i % 2 == 0) nn=4; else nn=5; }

                /* Apply periodic boundaries */
                ipbc.x = 0; r = neigh.x - 1 + my_coord.x * (cell_dim.x - 2);
                if (r<0) ipbc.x--; else if (r>global_cell_dim.x-1) ipbc.x++;
                r = neigh.x;

                ipbc.y = 0; s = neigh.y - 1 + my_coord.y * (cell_dim.y - 2);
                if (s<0) ipbc.y--; else if (s>global_cell_dim.y-1) ipbc.y++;
                s = neigh.y;

                ipbc.z = 0; t = neigh.z - 1 + my_coord.z * (cell_dim.z - 2);
                if (t<0) ipbc.z--; else if (t>global_cell_dim.z-1) ipbc.z++;
                t = neigh.z;

                if (((pbc_dirs.x==1) || (pbc_dirs.x==ipbc.x)) &&
                    ((pbc_dirs.y==1) || (pbc_dirs.y==ipbc.y)) &&
                    ((pbc_dirs.z==1) || (pbc_dirs.z==ipbc.z)))
                {
                  /* add pair to list */
                  P = pairs[nn] + npairs2[nn];
                  P->np = i*cell_dim.y*cell_dim.z + j*cell_dim.z + k;
                  P->nq = r*cell_dim.y*cell_dim.z + s*cell_dim.z + t;
                  P->ipbc[0] = ipbc.x;
                  P->ipbc[1] = ipbc.y;
                  P->ipbc[2] = ipbc.z;
                  npairs2[nn]++;
	        }
	      }
	    }

#endif /* MPI */

#endif /* !(defined(MPI) && definded(MONOLJ)), i.e., not world record */

}


/******************************************************************************
*
*  cell_coord computes the (global) cell coorinates of a position
*
******************************************************************************/

ivektor cell_coord(real x, real y, real z)
{
  ivektor coord;

  /* Map positions to boxes */
  coord.x = (int)(global_cell_dim.x * (x*tbox_x.x + y*tbox_x.y + z*tbox_x.z));
  coord.y = (int)(global_cell_dim.y * (x*tbox_y.x + y*tbox_y.y + z*tbox_y.z));
  coord.z = (int)(global_cell_dim.z * (x*tbox_z.x + y*tbox_z.y + z*tbox_z.z));

  /* rounding errors may put atoms slightly outside the simulation cell */
  /* in the case of no pbc they may even be far outside */
  if      (coord.x >= global_cell_dim.x) coord.x = global_cell_dim.x - 1;
  else if (coord.x < 0)                  coord.x = 0;
  if      (coord.y >= global_cell_dim.y) coord.y = global_cell_dim.y - 1;
  else if (coord.y < 0)                  coord.y = 0;
  if      (coord.z >= global_cell_dim.z) coord.z = global_cell_dim.z - 1;
  else if (coord.z < 0)                  coord.z = 0;

  return(coord);

}


/* map vektor back into simulation box */

vektor back_into_box(vektor pos)
{
  int i;

  if (pbc_dirs.x==1) {
    i = FLOOR(SPROD(pos,tbox_x));
    pos.x  -= i *  box_x.x;
    pos.y  -= i *  box_x.y;
    pos.z  -= i *  box_x.z;
  }

  if (pbc_dirs.y==1) {
    i = FLOOR(SPROD(pos,tbox_y));
    pos.x  -= i *  box_y.x;
    pos.y  -= i *  box_y.y;
    pos.z  -= i *  box_y.z;
  }

  if (pbc_dirs.z==1) {
    i = FLOOR(SPROD(pos,tbox_z));
    pos.x  -= i *  box_z.x;
    pos.y  -= i *  box_z.y;
    pos.z  -= i *  box_z.z;
  }

  return pos;

}

