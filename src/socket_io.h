
/******************************************************************************
*
* IMD -- The ITAP Molecular Dynamics Program
*
* Copyright 1996-2001 Institute for Theoretical and Applied Physics,
* University of Stuttgart, D-70550 Stuttgart
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#define PROTOCOL_VERSION_MAJOR  0
#define PROTOCOL_VERSION_MINOR  1

#define VIS_INIT               10
#define VIS_INIT_ATOMS         15
#define VIS_WRITE_ATOMS        20
#define VIS_WRITE_DISTRIB      30
#define VIS_CHANGE_PARAMS      40
#define VIS_RESTART            50

#define VIS_QUIT               99 
#define VIS_WRITE_QUIT        100

#define VIS_PARAM_DEFORM        1


/* legacy stuff */
#define T_WRITE_DISTRIB         1
#define T_WRITE_PICTURE         2
#define T_WRITE_RAS             8
#define T_WRITE_CONF_SOCKET     9

#define ATOMS_FLAG_SIZE 6
typedef struct {
  integer sorte;
  integer ort;
  integer impuls;
  integer Ekin;
  integer Epot;
  integer nbanz;
} atoms_flag_t;

#define ATOMS_FILT_SIZE 7
typedef struct {
  float sorte;
  float x;
  float y;
  float z;
  float Ekin;
  float Epot;
  float nbanz;
} atoms_filt_t;

#define SOCK_BUF_AT_SIZE 131072
float sock_buf_at[SOCK_BUF_AT_SIZE];   /* buffer for sending atoms */
atoms_flag_t at_send_flags;            /* atoms send flags */
atoms_flag_t at_filt_flags;            /* atoms filter flags */
atoms_filt_t at_filt_max, at_filt_min; /* atoms filter minima and maxima */

integer atlen;                         /* number of data items per atom */
int soc=-1;                            /* socket file descriptor */
unsigned long varIP;                   /* IP number of visualization server */