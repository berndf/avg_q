/* --------------------------- rawdata.h --------------------------- */
/* <time> */
/* pb 17-08-1993 11:00; 386 version */
/* pb 11-05-1993 10:28; ndatbytes */
/*  03-05-1993 22:15; new structures and routines */
/* pb 26-03-1993 10:09; MAXdataperchannel */

#include <time.h>
#include <read_struct.h>

/* maximum sizes of arrays - must be defined in main program */
extern unsigned int MAXtrials;
extern unsigned int MAXdata;
extern unsigned int MAXdataperchannel;
extern unsigned int MAXtrarray;
extern unsigned int MAXhdstring;
extern unsigned int MAXhdarray; 

extern struct_member sm_Rtrial[];
extern struct_member sm_Rheader[];

/* header parameters:
The header consists of the header base structure, which contains the
  basic parameters of the file, followed by the header string, and
  an array of variable size containing further parameters */

#define HEADARRAY_SIZE 6
#define HEADBSIZE 40
struct Rheader
    {                       /* BEGINNING OF header base:  40 bytes  */
unsigned short  headsize;        /* size of header text + parameters  */
unsigned short  headstringsize;  /* size of header string  */
unsigned short  trials;          /* no. of trials  */
unsigned short  maxtrials;       /* maximum no. of trials  */
unsigned short  conditions;      /* maximum no. of conditions  */
unsigned short  maxchannels;     /* maximum no. of channels  */
int        nexttrial;       /* byte address of next trial  */
                            /* = byte address of trial address table  */
time_t     starttime;       /* time of recording data  */
time_t     endtime;         /* time data recording finished  */
short int  subject;         /* subject no.  */
short int  group;           /* group no.  */
short int  headarray[HEADARRAY_SIZE];    /* space for other values  */
                            /* END OF HEADER BASE*/
char       *pstring;        /* string*/
short int  *parray;         /* array*/
int        *paddr;          /* trial address list*/
FILE *filehandle;           /* file handle*/
short int  addressflag;     /* addresses from header base (0) or from addresses (1)*/
short int  packedflag;      /* true if packed*/
short int  arraysize;       /* size of header array (bytes)*/
    };

/* trial parameters:
Each trial consists of a trial base structure, followed by an array of
  variable size containing further parameters. */

#define T_CONDITION_SIZE 3
#define T_MARKER_SIZE 5
#define TRIALARRAY_SIZE 8
#define TRIALBSIZE 62
struct Rtrial
    {                       /* BEGINNING of trial base: 62 bytes  */
short int  trialno;         /* trial no.*/
short int  spare[2];
short int  nkan;            /* no. of channels  */
short int  nabt;            /* sampling interval in ms  */
/* CAUTION: The following was a short in the `old' format and 
 * ALIGNMENT PROBLEM: is supposed to be 2-byte aligned, but gcc pads to 4 */
int        ndat;            /* no. of data words  */
short int  condition[T_CONDITION_SIZE];    /* condition no. (3)  */
short int  marker[T_MARKER_SIZE];       /* trial markers. (5)  */
short int  trialarray[TRIALARRAY_SIZE];   /* space for other values  */
int        trialsize;       /* size of trial in bytes  */
int        trialhead;       /* size of trial header in bytes  */
int        lasttrial;       /* byte address of last trial  */
int        nexttrial;       /* byte address of next trial*/
                            /* END OF TRIAL BASE*/
      int  npoints;         /* points per channel*/
short int  *parray;         /* array*/
short int  *pdata;          /* data*/
int        *paddr;          /* pointer to address array*/
FILE  *filehandle;          /* file handle*/
short int  packedflag;      /* true if packed*/
    };

#define RALLOCDATA 1
#define RALLOCARRAY 2
#define RALLOCALL 3

#define RADDREXIST 1
#define RADDRNOEXIST 0

#define ALLOC 1
#define NOALLOC 0


/* function declarations */

void *Rdataalloc(void *ptr,size_t n,char *msg);
void RCopyTrialBase(struct Rtrial *Rtrtarg, struct Rtrial *Rtrsource);
void RAppendTrial(int trial,struct Rheader *Rhd,struct Rtrial *Rtr);
int RStoreTrial(int trial,struct Rheader *Rhd,struct Rtrial *Rtr);
void RStoreTrialHead(int trial, struct Rtrial *Rtr);
void RStoreTrialBase(int trial, struct Rtrial *Rtr);
void RGetTrial(int trial, struct Rtrial *Rtr);
void RGetTrialHead(int trial, struct Rtrial *Rtr);
void RGetTrialBase(int trial, struct Rtrial *Rtr);
void RUpdate(struct Rheader *Rhd);
void RClose(struct Rheader *Rhd,struct Rtrial *Rtr);
void ROpen(char const *name,struct Rheader *Rhd,struct Rtrial *Rtr,char *mode);
void RGetHead(struct Rheader *Rhd,int flag);
void RCreateHead(struct Rheader *Rhd,unsigned maxtr,unsigned maxch,
    unsigned conds,int subject,int flags,char *string);
void RCreate(char const *name,struct Rheader *Rhd,struct Rtrial *Rtr);
void RStoreHead(struct Rheader *Rhd);
void Rmessage(char *message,int flag);
void Rperror(char *str);

#define Sample() ((headb.headarray[5]==1000) ? (trialb.nabt/1000.) : trialb.nabt)
#define nint(X)  ((X)>=0.? (int)((X)+0.5):(int)((X)-0.5))
