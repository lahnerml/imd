/******************************************************************************
*
* van-Hove-Correlation and mean square displacement
*
* $Revision$
* $Date$
*
******************************************************************************/

/* ToDo:                                                          */
/* correl_alloc() von correl_init() separieren                    */
/*                                                                */

#include "imd.h"

/******************************************************************************
 Globale Parameter in globals.h:
 GS[][][]     Histogramm-Array fuer Selbstkorrelation
 msqd         mittlere quadratische Verschiebung (mean square displacement)
 inv_dr       Kehrwert der Schrittweite fuer Histogramm GS[i][r][t]
 ncorr_rmax   Dimension des Korrelations-Histogramms GS[i][r][t] im r-Bereich
 ncorr_tmax   Dimension des Korrelations-Histogramms GS[i][r][t] im t-Bereich
 correl_start Startzeitpunkt der Korrelation (bzgl. Start der ges. Simulation)
 correl_stop  Stopzeitpunkt der Korrelation (bzgl. Start der ges. Simulation)
 correl_int   Intervall fuer Wiederholung der Korrelation
 correl_ts    Sampling-Zeitintervall f�r Korrelation
 num_sort[]   Anzahl der Atome f�r jede Sorte
******************************************************************************/

/*****************************************************************************
* Praeprozessor-Flags:
* CORRELATE   Selbstkorrelation und MSQD berechnen
* MSQD        ausschliesslich MSQD berechnen
*****************************************************************************/

typedef integer* intptr;

/******************************************************************************
*
* Allocate histogram arrays and setup parameters for correlation
*
******************************************************************************/

void init_correl(int ncorr_rmax, int ncorr_tmax)
{
  int i,j;
#ifdef CORRELATE
  real diagonal;
  char *filename;

  GS = calloc(ntypes,sizeof(intptr));
  if (GS == NULL) {
    error("Cannot allocate histogram for correlation\n");
  };

  for (i=0; i<ntypes; i++) {
    GS[i]=calloc(ncorr_tmax,sizeof(intptr));
    if (GS[i] == NULL) {
      error("Cannot allocate histogram for correlation\n");
    }
  }

  for (i=0; i<ntypes; i++) {
    for (j=0; j<ncorr_tmax; j++) {
      GS[i][j]=(integer *)calloc(ncorr_rmax,sizeof(integer));
      if (GS[i][j] == NULL) {
        error("Cannot allocate histogram for correlation\n");
      }
    }
  }
#ifdef TWOD
  diagonal = sqrt(SQR(box_x.x+box_y.x)+SQR(box_x.y+box_y.y));
#else
  diagonal = sqrt(SQR(box_x.x+box_y.x+box_z.x)+SQR(box_x.y+box_y.y+box_z.y)+
                  SQR(box_x.z+box_y.z+box_z.z));
#endif
  /* Limit histogram size in r domain to ncorr_rmax entries */
  inv_dr = (real)ncorr_rmax/diagonal;
  if ((filename = malloc(256))==NULL) {
    error("cannot malloc namebuffer\n");
  }
  for (i=0; i<ntypes; i++) {
    sprintf(filename,"%s.corr.%u",outfilename,(unsigned)i);
    unlink(filename);
  }
  free(filename);
#endif /* CORRELATE */

  /* Allocate msqd arrays */
  msqd=malloc(ntypes*sizeof(real));
#ifdef MPI
  msqd_global=malloc(ntypes*sizeof(real));
#else
  msqd_global=msqd;
#endif

}

/******************************************************************************
*
* Correlation routine for VHCF and MSQD
*
* for step == ref_step store positions to refpos, otherwise correlate
* seqnum gives the sequence number for the correlation output file
*
******************************************************************************/

void correlate(int step, int ref_step, unsigned seqnum)
{
  integer k,i;

#if defined(MPI) && defined(CORRELATE)
  int *global_corr;
#endif

  if (step == ref_step) { /* initialize reference positions */
    /* loop over all cells */
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (k=0; k<ncells; ++k) {
      int i;
      cell *p;
      p = cell_array + CELLS(k);
      for (i=0; i<p->n*DIM; ++i) {
        p->refpos[i] = p->ort[i];
      }
    }
  } 
  else { /* calculate displacement relative to reference position */
#ifdef CORRELATE
    integer it;
    integer idr;

    /* for ncorr_tmax=1 averaging on the fly not possible */
    if (ncorr_tmax==1) it=0;
    else it = (int)(step-ref_step) % ncorr_tmax; /* cyclical correlation */

#ifdef MPI
    global_corr = calloc(ncorr_rmax,sizeof(integer));
    if (global_corr==NULL) {
      error("allocation of buffers failed\n");
    }
#endif /* MPI */
#endif /* CORRELATE */

    for (i=0; i<ntypes; i++) msqd[i] = (real)0;

    /* loop over all cells */
    for (k=0; k<ncells; ++k) {

      int i;
      cell *p;
      p = cell_array + CELLS(k);

      for (i = 0; i < p->n; ++i) {

        vektor dist;
        real dr,drsq;

        /* calculate distance between atom i at t=tau (ort) and t=0 (refpos) */
        dist.x = p->ort X(i) - p->refpos X(i);
        dist.y = p->ort Y(i) - p->refpos Y(i);
#ifndef TWOD
        dist.z = p->ort Z(i) - p->refpos Z(i);
#endif

        reduce_displacement(&dist);

        drsq  = dist.x * dist.x;
        drsq += dist.y * dist.y;
#ifndef TWOD
        drsq += dist.z * dist.z;
#endif

        /* mean square displacement with PBC applied */
        msqd[p->sorte[i]] += drsq;

#ifdef CORRELATE
        dr  = sqrt(drsq); /* correlate displacement with PBC applied */
        idr = (int)(dr*inv_dr);
        GS[p->sorte[i]][it][idr]++; /* calculate histogram for self part */
#endif
      }
    }

#ifdef CORRELATE
#ifdef MPI
    for (i=0; i<ntypes; i++) {
      MPI_Reduce(GS[i][it], global_corr, ncorr_rmax, 
                 INTEGER, MPI_SUM, 0, cpugrid);
      if (0==myid) memcpy(GS[i][it],global_corr,sizeof(integer)*ncorr_rmax);
    }
#endif
    /* GS[i][it][idr] contains local self correlation within one CPU */
    if (0==myid) write_add_corr(it,step,seqnum);
#endif

#ifdef MSQD
#ifdef MPI
    MPI_Reduce(msqd,msqd_global,ntypes,REAL,MPI_SUM,0,cpugrid);
#endif
    if (0==myid) write_msqd(step);
#endif

#ifdef CORRELATE
    /* set GS to zero before starting a new correlation run again */
    if (((correl_int != 0) && (((step-ref_step) % correl_int) == 0))
       || (ncorr_tmax == 1)) {
      for (i=0; i<ntypes; i++) {
        for (it=0; it<ncorr_tmax; it++) {
          memset(GS[i][it],0,sizeof(integer)*ncorr_rmax);
        }
      }
    }
#endif
  }
} /* correlate */

/******************************************************************************
*
* van-Hove-Korrelationen in Datei schreiben (anfuegen)
*
******************************************************************************/

#ifdef CORRELATE
void write_add_corr(int it, int steps, unsigned seqnum)
{
  int i,idr,imax;
  char *filename;
  FILE *fp;
  real t;
  real delta_r;
  char *buffer = NULL;

  delta_r = 1.0/inv_dr;

  if ((filename = malloc(256))==NULL) {
    error("Cannot malloc name buffer\n");
  };
  for (i=0; i<ntypes; i++) {
    sprintf(filename,"%s.corr%u.%u",outfilename,(unsigned)seqnum,(unsigned)i);
    /* gnuplot data file with x y z data for 3D plot */
    if ((correl_omode == 1) || (correl_omode == 2)) { /* for gnuplot */
      fp = fopen(filename,"a");
      if (fp == NULL) {
         error("Cannot open file\n");
      };
#if BUFSIZ < 4096
      buffer=malloc(4096);
      if (buffer != NULL) setvbuf(fp,buffer,_IOFBF,4096);
#endif
      t = steps*timestep;
      imax=0;
      for (idr=0; idr<ncorr_rmax; idr++) if (GS[i][it][idr]!=0) imax=idr;
      if (imax<ncorr_rmax) imax++;
      if (imax<ncorr_rmax) imax++;
      /* write t, r, G(r,t) for all r with nonzero G(r,t) */
      for (idr=0; idr<imax; idr++) {
        fprintf(fp,"%g %g %g\n",(double)t,(double)idr*delta_r,(double)GS[i][it][idr]/num_sort[i]);
      };
      if (correl_omode==2) putc('\n',fp);
      putc('\n',fp);
      fclose(fp);
    }
    else if (correl_omode == 3) { /* long correlation data files */
      sprintf(filename,"%s.corr.%u",outfilename,(unsigned)i);
      fp = fopen(filename,"a");
      if (fp == NULL) {
         error("Cannot open file\n");
      };
#if BUFSIZ < 4096
      buffer=malloc(4096);
      if (buffer != NULL) setvbuf(fp,buffer,_IOFBF,4096);
#endif
      t = steps*timestep;
      if (GS_rcut != 0.0) imax = (int)(GS_rcut/delta_r);
      else imax=ncorr_rmax;
      /* write t, r, G(r,t) for all r (produces very big files) */
      for (idr=0; idr<imax; idr++) {
        fprintf(fp,"%g %g %g\n",(double)t,(double)idr*delta_r,(double)GS[i][it][idr]/num_sort[i]);
      };
      fclose(fp);
    }
    else if (correl_omode == 4) { /* short correlation data files */
      fp = fopen(filename,"a");
      if (fp == NULL) {
         error("Cannot open file\n");
      };
#if BUFSIZ < 4096
      buffer=malloc(4096);
      if (buffer != NULL) setvbuf(fp,buffer,_IOFBF,4096);
#endif
      /* write header first */
      /* write number of values in each direction: ncorr_tmax, ncorr_rmax */
      fprintf(fp,"%u %u\n",ncorr_tmax,ncorr_rmax);
      /* write step sizes: timestep, delta-r */
      fprintf(fp,"%g %g\n",(double)timestep,(double)delta_r);

      /* look for last nonzero value in GS[][][] */
      imax=0;
      for (idr=0; idr<ncorr_rmax; idr++) if (GS[i][it][idr]!=0) imax=idr;
      if (imax<ncorr_rmax) imax++;
      if (imax<ncorr_rmax) imax++;

      /* for each line write number of values (imax) first */
      fprintf(fp,"%u\n",(unsigned)imax);
      /* and then write GS(r,t) for all r with nonzero GS(r,t) values */
      for (idr=0; idr<imax; idr++) {
        fprintf(fp,"%g\n",(double)GS[i][it][idr]/num_sort[i]);
      };
      fclose(fp);
    }
    else if (correl_omode == 5) { /* for gnuplot */
      sprintf(filename,"%s.corr%u.%u",outfilename,(unsigned)steps,(unsigned)i);
      fp = fopen(filename,"a");
      if (fp == NULL) {
         error("Cannot open file\n");
      };
#if BUFSIZ < 4096
      buffer=malloc(4096);
      if (buffer != NULL) setvbuf(fp,buffer,_IOFBF,4096);
#endif
      t = steps*timestep;
      if (GS_rcut != 0.0) imax = (int)(GS_rcut/delta_r);
      else {
        imax=0;
        for (idr=0; idr<ncorr_rmax; idr++) if (GS[i][it][idr]!=0) imax=idr;
        if (imax<ncorr_rmax) imax++; /* write two zeros at end */
        if (imax<ncorr_rmax) imax++;
      };
      /* write t, r, G(r,t) for all r with nonzero G(r,t) */
      for (idr=0; idr<imax; idr++) {
        fprintf(fp,"%g %g %g\n",(double)t,(double)idr*delta_r,(double)GS[i][it][idr]/num_sort[i]);
      };
      fclose(fp);
    }
  };
  free(filename);
} /* write_add_corr */
#endif /* CORRELATE */
