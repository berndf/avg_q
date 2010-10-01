/* * * * * * * * * * * * * rawdata.c * * * * * * * * * * * * * */

/* <time> */
/* pb 12-01-1994 09:06; correct compression error */
/* pb 08-10-1993 10:26; structure of trials allows > 32k points per channel */
/* pb 17-08-1993 11:00; 386 version */
/* pb 11-05-1993 10:55; allocate space for trial data according to required size */
/* pb 03-05-1993 18:23; new calls */
/* pb 26-03-1993 10:08; MAXdataperchannel */
/* pb 04-09-1992 16:02; C7.0 */
/* pb 20-08-1992 15:49; trial base size = 62 */
/*  25-03-1992 11:37; add strdot */
/* pb 13-12-1990 17:18; npoints computed here */
/* pb Jun 11 13:19 1990; Path defined by environment searched in RawOpen */
/* pb Mar 08 10:10 1990; headb.nexttrial defined in RawCreate */
/*  Jan 15 15:05 1990; exit if header string sized too small */
/*  Dec 08 11:42 1989; add pointer to addresses for RawOpen and RawCreate */
/*  Dec 08 09:42 1989; simplify RawClose */
/*  May 13 19:02 1989; add checks for array sizes */
/*  Jan 25 10:37 1989; header array and headerstring must be allocated in main program */
#include <stdio.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#ifdef WIN32
#define snprintf _snprintf
#endif
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include "rawdatan.h"

#ifndef LITTLE_ENDIAN
#include "Intel_compat.h"
#endif

#define QUIT 1
#define CONTINUE 0

#define RALLOCDATA 1
#define RALLOCARRAY 2
#define RALLOCALL 3

#define RADDREXIST 1
#define RADDRNOEXIST 0

#define ALLOC 1
#define NOALLOC 0

/* For error processing */
#include <transform.h>
extern struct external_methods_struct *kn_emethods;

static int Roldtrialbsize;         /* set to 1 if header base array[4] is zero*/
static int Rbsize;                 /* set to 60 (old) or 62 (new) base size*/
static int Rpackverify=0;          /* verify packing if set to 1*/
static int Rnpoints;

#define RAWMESS_SIZE 120
static char RAWmess[RAWMESS_SIZE];

/* function declarations  */

static void Rtrialfree(struct Rtrial *Rtr,int flag);

static int Packdata(struct Rtrial *trialb, short int *trialdata, short int *trdata);
static int Unpackdata(struct Rtrial *trialb, short int *trialdata, short int *trdata);
static int CompressChan(short int *newarray, short int *charray, int npoints);
static void DoCompression(short int *newarray, short int *charray);
static void Compare(short int *charray, short int *testarray);
static int UndoCompression(short int *newarray, short int *charray);
static int TwoPowerN(int n);
static int AnalyseDiffs(short int *charray, int n, int npoints, short int *nsubsid);
static void icopychan(short int *charray,short int *iarray,int channel,int ndat,int nkan,int dir);
static void ConvertOldTrialBase(struct Rtrial *Rtr, int direction);

#define Rperror(s) Rmessage(s, QUIT);

/*---------------------------------------------------------------------*/
void * Rdataalloc(void *ptr,size_t n,char *msg)
/*---------------------------------------------------------------------*/
{/* allocate memory for trial*/
    ptr = (short int *)realloc(ptr,n);
    if(ptr == NULL)
        {
        strcpy(RAWmess,"Unable to allocate memory for ");
        strcat(RAWmess,msg);
        Rperror(RAWmess);
        }
    return(ptr);
}

/*---------------------------------------------------------------------*/
static void Rtrialfree(struct Rtrial *Rtr,int flag)
/*---------------------------------------------------------------------*/
{
    if(flag & RALLOCARRAY)
        free(Rtr->parray);
    if(flag & RALLOCDATA)
        free(Rtr->pdata);
}

/*---------------------------------------------------------------------*/
static void Rfreehead(struct Rheader *Rhd)
/*---------------------------------------------------------------------*/
{
 /* Take care to allocate the string passed to RCreateHead
  * and to call RGetHead always with qalloc=ALLOC! */
    free(Rhd->pstring);
    free(Rhd->parray);
    free(Rhd->paddr);
}

/*---------------------------------------------------------------------*/
void RClose(struct Rheader *Rhd,struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    if(fclose(Rhd->filehandle))
        Rperror("Failed to close raw data file");
    Rfreehead(Rhd);
    Rtrialfree(Rtr,RALLOCALL);
}

/*---------------------------------------------------------------------*/
void ROpen(char const *name,struct Rheader *Rhd,struct Rtrial *Rtr, char *mode)
/*---------------------------------------------------------------------*/
{
    unsigned m;

    Rhd->filehandle = fopen(name,mode);
    if(Rhd->filehandle == NULL)
        {
        snprintf(RAWmess, RAWMESS_SIZE, "Open failed on raw data file %s",name);
        Rperror(RAWmess);
        }
    RGetHead(Rhd,ALLOC);

    Rhd->addressflag = RADDREXIST;

    if(fseek(Rhd->filehandle,Rhd->nexttrial,SEEK_SET) == -1L)
        Rperror("Failed to position start of addresses during open");

/* allocate memory for addresses*/
    if(Rhd->maxtrials < Rhd->trials)
        Rmessage("No. of trials exceeds maximum! (change maxtrials!)",QUIT);
    Rhd->paddr = (int *) malloc((Rhd->maxtrials + 1)*sizeof(int));
    if(Rhd->paddr == NULL)
        Rperror("Failed to allocate memory for addresses during open");

/* read addresses*/
    if(Rhd->addressflag)
        {
        m = fread((void *)Rhd->paddr, sizeof(int), Rhd->trials + 1, Rhd->filehandle);
        if(m == -1)
            {
            snprintf(RAWmess, RAWMESS_SIZE, "Read failed on addresses %s",name);
            Rperror(RAWmess);
            }
#    ifndef LITTLE_ENDIAN
	{int *pdata=Rhd->paddr, *p_end=Rhd->paddr+(Rhd->trials+1);
	 while (pdata<p_end) Intel_int(pdata++);
	}
#    endif
        }
    if((Rhd->headarray[4] & 0x7f) != TRIALBSIZE)
        Roldtrialbsize = 1;
    else
        Roldtrialbsize = 0;
    if(Rhd->headarray[4] & 0x0080)
        Rhd->packedflag = 1;
    else
        Rhd->packedflag = 0;
    Rtr->pdata = NULL;
    Rtr->parray = NULL;
/* allocate memory for trial information, depending on flag*/
/*    Rtrialalloc(Rhd,Rtr,flag);*/
    Rtr->paddr = Rhd->paddr;  /* trial addr points to address array*/
    Rtr->filehandle = Rhd->filehandle;
    Rtr->packedflag = Rhd->packedflag;
}

/*---------------------------------------------------------------------*/
void RGetHead(struct Rheader *Rhd,int qalloc)
/*---------------------------------------------------------------------*/
{
    int m;
/* Read header base */
    read_struct((char *)Rhd, sm_Rheader, Rhd->filehandle);
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rhd, sm_Rheader);
#   endif
/* Read header string */
    if(qalloc)
        {   /* allocate memory for string*/
        Rhd->pstring = (char *) malloc(Rhd->headstringsize);
        if(Rhd->pstring==NULL)
            Rperror("Could not allocate memory for header string");
        }

    m = fread((void *)Rhd->pstring, 1, Rhd->headstringsize, Rhd->filehandle);
    if(m == -1)
        Rperror("Header string read failed ");

/* Read header array */
    Rhd->arraysize =
        Rhd->headsize - Rhd->headstringsize - HEADBSIZE;
    if(qalloc)
        {
        Rhd->parray = (short int *)malloc(Rhd->arraysize);
        if(Rhd->parray==NULL)
            Rperror("Could not allocate memory for header array");
        }
    m = fread((void *)Rhd->parray, 1, Rhd->arraysize, Rhd->filehandle);
    if(m == -1)
        Rperror("Header array read failed ");
#   ifndef LITTLE_ENDIAN
    {short *pdata=Rhd->parray, *p_end=Rhd->parray+Rhd->arraysize/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif

    if((Rhd->headarray[4] & 0x7f) != TRIALBSIZE)
        Roldtrialbsize = 1;
    else
        Roldtrialbsize = 0;
}

/*---------------------------------------------------------------------*/
void RCreateHead(struct Rheader *Rhd,unsigned maxtr,unsigned maxch,
    unsigned conds,int subject,int flags,char *string)
/*---------------------------------------------------------------------*/
{
    int stlen;
    stlen = strlen(string);
    memset((char *)Rhd,0,60);   /* initialize to 0*/
    Rhd->maxtrials = maxtr;
    Rhd->maxchannels = maxch;
    Rhd->conditions = conds;
    Rhd->subject = subject;
    Rhd->headarray[4] = flags;
    Rhd->headarray[5] = 1000;   /* æs units are standard these days*/
    time(&Rhd->starttime);      /* set current time*/
    time(&Rhd->endtime);        /* set current time*/
    Rhd->headstringsize = stlen+1;
    Rhd->headsize = HEADBSIZE + stlen+1 + 12*maxch;
    Rhd->nexttrial = Rhd->headsize;
    if(flags & 128)
        Rhd->packedflag = 1;
    Rhd->arraysize = 12*maxch;
    Rhd->pstring = string;      /* point to the headerstring*/
}

/*---------------------------------------------------------------------*/
void RCreate(char const *name,struct Rheader *Rhd,struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
/* Create file */
    Rhd->filehandle = fopen(name,"w+b");
    Rhd->addressflag = RADDRNOEXIST;
    if(Rhd->filehandle == NULL)
        {
        snprintf(RAWmess, RAWMESS_SIZE, "Create failed on raw data file %s",name);
        Rperror(RAWmess);
        }
/* Allocate and write empty header array */
    Rhd->arraysize = Rhd->headsize - Rhd->headstringsize - HEADBSIZE;
    Rhd->parray = (short int *)malloc(Rhd->arraysize);
    if(Rhd->parray==NULL)
        Rperror("Could not allocate memory for header array");
    memset(Rhd->parray,0,Rhd->arraysize);

/* allocate memory for addresses*/
    Rhd->paddr = (int *) malloc((Rhd->maxtrials + 1)*sizeof(int));
    if(Rhd->paddr == NULL)
        Rperror("Failed to allocate memory for addresses during create");

    Rhd->nexttrial = Rhd->headsize;

    Rhd->headarray[4] &= 0xff80;
    if(Roldtrialbsize == 0)
        Rhd->headarray[4] += TRIALBSIZE;
    if(Rhd->headarray[4] & 0x0080)
        Rhd->packedflag = 1;
    else
        Rhd->packedflag = 0;
    RStoreHead(Rhd);
    Rtr->pdata = NULL;
    Rtr->parray = NULL;
/* allocate memory for trial information, depending on flag*/
/*    Rtrialalloc(Rhd,Rtr,flag);*/
    Rtr->paddr = Rhd->paddr;  /* trial addr points to address array*/
    Rtr->filehandle = Rhd->filehandle;
    Rtr->packedflag = Rhd->packedflag;
}

/*---------------------------------------------------------------------*/
void RStoreHead(struct Rheader *Rhd)
/*---------------------------------------------------------------------*/
{
    int m;
    if(fseek(Rhd->filehandle,0L,SEEK_SET)==-1L)       /* rewind file */
        Rperror("Failed to position start of file during storehead");

/* Write header base structure */
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rhd, sm_Rheader);
#   endif
    write_struct((char *)Rhd, sm_Rheader, Rhd->filehandle);
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rhd, sm_Rheader);
#   endif

/* Write header string */
    m = fwrite((void *)Rhd->pstring, 1, Rhd->headstringsize, Rhd->filehandle);
    if(m == -1)
        Rperror("Header string write failed ");

#   ifndef LITTLE_ENDIAN
    {short *pdata=Rhd->parray, *p_end=Rhd->parray+Rhd->arraysize/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    m = fwrite((void *)Rhd->parray, 1, Rhd->arraysize, Rhd->filehandle);
#   ifndef LITTLE_ENDIAN
    {short *pdata=Rhd->parray, *p_end=Rhd->parray+Rhd->arraysize/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    if(m == -1)
        Rperror("Header array write failed ");
}

/*---------------------------------------------------------------------*/
void RCopyTrialBase(struct Rtrial *Rtrtarg, struct Rtrial *Rtrsource)
/*---------------------------------------------------------------------*/
{
    memcpy((char *)Rtrtarg,(char *)Rtrsource,(char *)&Rtrtarg->npoints-(char *)Rtrtarg);
}

/*---------------------------------------------------------------------*/
void RAppendTrial(int trial,struct Rheader *Rhd,struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int addrf;
    addrf = Rhd->addressflag;
    Rhd->addressflag = 0;
    RStoreTrial(trial,Rhd,Rtr);
    Rhd->addressflag = addrf;
}

/*---------------------------------------------------------------------*/
int RStoreTrial(int trial,struct Rheader *Rhd,struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int m, trdat;
    char *trdata;
    int newsize;
    if(Rhd->headarray[4] & 0x0080)/* if compress bit set*/
        {                           /* compress data*/
        trdata = (char *) malloc((size_t)(Rtr->ndat*sizeof(short int)));
        if(trdata == NULL)
            {
            Rmessage("Unable to allocate memory for compression",QUIT);
            }
        newsize = Packdata(Rtr,Rtr->pdata, (short int *)trdata);
        Rtr->trialsize += (newsize - Rtr->ndat)*2;
        trdat = 2*newsize;
        }
    else                            /* no data compression*/
        {
        trdata = (char *)Rtr->pdata;
        trdat = Rtr->trialsize - Rtr->trialhead;
        }
/* If we are acquiring data, fill in addresses, last and next trial pointers,
   and the header base */
    if(Rhd->addressflag == 0)     /* address taken from header base */
        {
        if(Rhd->maxtrials < Rhd->trials)
            Rmessage("No. of trials exceeds maximum! (change maxtrials!)",QUIT);
        if(trial == 1) Rtr->lasttrial = 0;
        else Rtr->lasttrial = Rhd->paddr[trial-2];
        Rhd->paddr[trial-1] = Rhd->nexttrial;
        Rhd->nexttrial += Rtr->trialsize; /* header base updated */
        Rhd->paddr[trial] = Rhd->nexttrial;   /* addresses updated */
                                               /* no. of trials updated */
        if(trial != (int)(++(Rhd->trials)))
            {
            trial = Rhd->trials;
            Rtr->trialno = trial;
            snprintf(RAWmess, RAWMESS_SIZE, "Warning: trial no. doesn't match header info!"
                   "  - inserting number from header: trial %i",trial);
            Rmessage(RAWmess,CONTINUE);
            }
        Rtr->nexttrial = Rhd->nexttrial;
        Rtr->paddr[trial] = Rhd->paddr[trial];
        }
/* Write the header */
    RStoreTrialHead(trial,Rtr);
/* Write the data */
#   ifndef LITTLE_ENDIAN
    {short *pdata=(short *)trdata, *p_end=(short *)(trdata+trdat);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    m = fwrite((void *)trdata, 1, trdat, Rhd->filehandle);
#   ifndef LITTLE_ENDIAN
    {short *pdata=(short *)trdata, *p_end=(short *)(trdata+trdat);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    if(m == -1)
        Rperror("Failed to write trial data! ");

    if(Rhd->packedflag)
        {
        if(Rpackverify)
            {
            }
        free(trdata);
        }
    return(trdat/2);    /* return no. of data words*/
}

/*---------------------------------------------------------------------*/
void RStoreTrialHead(int trial, struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int m, trhead;
    trhead = (int)Rtr->trialhead;
    RStoreTrialBase(trial,Rtr);
#   ifndef LITTLE_ENDIAN
    {short *pdata=Rtr->parray, *p_end=Rtr->parray+(trhead-Rbsize)/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    m = fwrite((void *)Rtr->parray, 1, trhead - Rbsize, Rtr->filehandle);
#   ifndef LITTLE_ENDIAN
    {short *pdata=Rtr->parray, *p_end=Rtr->parray+(trhead-Rbsize)/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
    if(m == -1)
        Rperror("Failed to write trial array! ");
}

/*---------------------------------------------------------------------*/
static void ConvertOldTrialBase(struct Rtrial *Rtr, int direction)
/*---------------------------------------------------------------------*/
{ /* trialb.ndat was short integer in old version*/
  /* direction = 0 -> convert to old, otherwise convert from old version*/
    int i;
    char *trialbarray=(char *)Rtr;

    if(direction)   /* convert from old to new version*/
        {
        for(i=61; i>=12; i--)
            trialbarray[i] = trialbarray[i-2];
        trialbarray[12] = trialbarray[13] = 0;
        }
    else            /* convert from new to old version*/
        {
        for(i=12; i<60; i++)
            trialbarray[i] = trialbarray[i+2];
        }
}

/*---------------------------------------------------------------------*/
void RStoreTrialBase(int trial, struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int trialaddress;
    trialaddress = Rtr->paddr[trial-1];
    if(fseek(Rtr->filehandle,trialaddress,SEEK_SET) == -1L) /* position file */
        Rperror("Failed to position start of trial during store");

    Rbsize = TRIALBSIZE;
    if(Roldtrialbsize)
        {
        Rbsize -= 2;
        ConvertOldTrialBase(Rtr, 0);
        }

#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rtr, sm_Rtrial);
#   endif
    write_struct((char *)Rtr, sm_Rtrial, Rtr->filehandle);
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rtr, sm_Rtrial);
#   endif

    if(Roldtrialbsize)
        ConvertOldTrialBase(Rtr, 1);/* restore trial base*/
}

/*---------------------------------------------------------------------*/
void RGetTrial(int trial, struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int m, trdat;
    char *trdata;
    int nextaddr, thisaddr;
/* Read the header */
    RGetTrialHead(trial,Rtr);

/* allocate memory for trial information, depending on flag*/
    Rtr->pdata = (short int *)Rdataalloc(Rtr->pdata,(size_t)(2*Rtr->ndat),"trial data");

    if(Rtr->packedflag)  /* if packed, get size of data set from the difference*/
        {           /*  between the next and this address*/
        nextaddr = Rtr->paddr[trial];
        thisaddr = Rtr->paddr[trial-1];
        trdat = (int)(nextaddr - thisaddr);
        trdat -= (int)Rtr->trialhead;
        trdata = (char *)malloc(trdat);
        if(trdata == NULL)
            Rmessage("Unable to allocate memory for compression",QUIT);
        }
    else
        {
        trdat = (int)(Rtr->ndat*2);
        trdata = (char *)Rtr->pdata;
        }
/* Read the data */
    m = fread((void *)trdata, 1, trdat, Rtr->filehandle);
    if(m == -1)
        Rperror("Failed to read trial data! ");
#   ifndef LITTLE_ENDIAN
    {short *pdata=(short *)trdata, *p_end=(short *)(trdata+trdat);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif

    if(Rtr->packedflag)      /* if packed, unpack*/
        {
        Unpackdata(Rtr, Rtr->pdata, (short int *)trdata);
        Rtr->trialsize = Rtr->ndat*2 + Rtr->trialhead;
        free(trdata);
        }
}

/*---------------------------------------------------------------------*/
void RGetTrialHead(int trial, struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    int m, ntrarray;
    RGetTrialBase(trial,Rtr);
/* allocate memory for trial array*/
    Rtr->parray = (short int *)Rdataalloc(Rtr->parray,12*Rtr->nkan,"trial array");

    if((ntrarray = (int)(Rtr->trialhead - Rbsize)) > (int)(MAXtrarray*2))
        {
        snprintf(RAWmess, RAWMESS_SIZE, "Trial array too large for program! (bytes: %i %i)",
            ntrarray,MAXtrarray*2);
        Rmessage(RAWmess,QUIT);
        }
    m = fread((void *)Rtr->parray, 1, ntrarray, Rtr->filehandle);
    if(m == -1)
        Rperror("Failed to read trial array! ");
#   ifndef LITTLE_ENDIAN
    {short *pdata=Rtr->parray, *p_end=Rtr->parray+ntrarray/sizeof(short);
     while (pdata<p_end) Intel_short(pdata++);
    }
#   endif
}

/*---------------------------------------------------------------------*/
void RGetTrialBase(int trial, struct Rtrial *Rtr)
/*---------------------------------------------------------------------*/
{
    if(fseek(Rtr->filehandle,Rtr->paddr[trial-1],SEEK_SET) == -1L) /* position file */
        {
        snprintf(RAWmess, RAWMESS_SIZE, "Failed to position start of trial during get"
            " - trial %i, address: %i\n",trial,Rtr->paddr[trial-1]);
        Rperror(RAWmess);
        }
    Rbsize = TRIALBSIZE;	/* Will be used as offset within file */
    if(Roldtrialbsize)
        Rbsize -= 2;
    read_struct((char *)Rtr, sm_Rtrial, Rtr->filehandle);
#   ifndef LITTLE_ENDIAN
    change_byteorder((char *)Rtr, sm_Rtrial);
#   endif

    if(Roldtrialbsize)
        ConvertOldTrialBase(Rtr,1);
    if(Rtr->nkan != 0)
        Rtr->npoints = (Rtr->ndat/Rtr->nkan);
    else
        Rtr->npoints = 0;
    Rnpoints = Rtr->npoints;
    if(Rtr->npoints > MAXdataperchannel)
        {
        snprintf(RAWmess, RAWMESS_SIZE,
            "\nToo many data per channel (%i > %i)\n",Rtr->npoints,MAXdataperchannel);
        Rmessage(RAWmess,QUIT);
        }
}

/*---------------------------------------------------------------------*/
void RUpdate(struct Rheader *Rhd)
/*---------------------------------------------------------------------*/
/* Update file header and addresses for the current no. of trials */
{
    int m;
    Rhd->headarray[4] &= 0xff80;
    if(Roldtrialbsize == 0)
        Rhd->headarray[4] += TRIALBSIZE;
    time(&Rhd->endtime);        /* insert time now*/
    RStoreHead(Rhd);
    if(fseek(Rhd->filehandle,Rhd->nexttrial,SEEK_SET) == -1L)
        Rperror("Failed to position start of addresses during update");

#   ifndef LITTLE_ENDIAN
    {int *pdata=Rhd->paddr, *p_end=Rhd->paddr+(Rhd->trials+1);
     while (pdata<p_end) Intel_int(pdata++);
    }
#   endif
    m = fwrite((void *)Rhd->paddr, sizeof(int), Rhd->trials + 1, Rhd->filehandle);
#   ifndef LITTLE_ENDIAN
    {int *pdata=Rhd->paddr, *p_end=Rhd->paddr+(Rhd->trials+1);
     while (pdata<p_end) Intel_int(pdata++);
    }
#   endif
    if(m == -1)
        Rperror("Failed to write addresses! ");
}

/*--------------------------------------------------------------------*/
static int Packdata(struct Rtrial *trialb, short int *trialdata, short int *trdata)
/*--------------------------------------------------------------------*/
{
    short int k, *newarray, nwords, newsize=0, *charray;
    newarray = trdata;              /* point to start of array*/
    charray = (short int *) malloc(2*Rnpoints);
    if(charray == NULL)
        Rmessage("Unable to allocate memory for compression",QUIT);

    for(k=0; k<(short int)trialb->nkan; k++)    /* for each channel*/
        {
        icopychan(charray,trialdata,k,(short int)trialb->ndat,trialb->nkan,1);
/*        printf("Channel %i ",k);*/
        nwords = CompressChan(newarray,charray,Rnpoints);
        newarray += nwords;         /* point to next channel segment*/
        newsize += nwords;
        }
    free(charray);
    return(newsize);
}

#pragma optimize("",off)
/*--------------------------------------------------------------------*/
static int Unpackdata(struct Rtrial *trialb, short int *trialdata, short int *trdata)
/*--------------------------------------------------------------------*/
{
    short int k, *newarray, nwords, newsize=0, *charray;
    newarray = trdata;              /* point to start of array*/
    charray = (short int *) malloc(2*Rnpoints);
    if(charray == NULL)
        Rmessage("Unable to allocate memory for compression",QUIT);

    for(k=0; k<(short int)trialb->nkan; k++)    /* for each channel*/
        {
/*        printf("Channel %i ",k);*/
        if(UndoCompression(newarray, charray))
            trialb->marker[4] = 1;  /* this trial wasn't decompressed properly*/
        nwords = newarray[0];
/*        Compare(charray,testarray);*/
        newarray += nwords;         /* point to next channel segment*/
        newsize += nwords;
        icopychan(charray,trialdata,k,(short int)trialb->ndat,trialb->nkan,0);
        }
    free(charray);
    return(newsize);
}

/*--------------------------------------------------------------------*/
static int CompressChan(short int *newarray, short int *charray, int npoints)
/*--------------------------------------------------------------------*/
{ /* array converted into the following:*/
  /*  word 1: no. of words*/
  /*  word 2: no. of bits per data word (nbits)*/
  /*  word 3: no. of words in subsidiary array (nsubsid)*/
  /*  words 4-nsubsid+3: no. of successive words without a starter*/
  /*  words nsubsid+4 onwards:*/
  /*    nsubsid groups consisting of*/
  /*      1. a starting level (16-bit)*/
  /*      2. successive differences as groups of bits of length nbits*/
    short int n, nwords, nwmin=0, ncomp, nsubsid, nsubsidbest, *testarray,
        bitmax=15, iwasthere=0;
    nwmin = npoints;
/* find no. of bits that give maximum compression*/
/*    printf(" analyze");*/
    for(n=3; n<=bitmax; n++)
        {
        nwords = AnalyseDiffs(charray,n,npoints,&nsubsid);
/*        printf(".");*/
        if(nwords < nwmin && nwords > 0)
            {
            nwmin = nwords;
            ncomp = n;
            nsubsidbest = nsubsid;
            iwasthere=1;
            }
        else
            {
            if(iwasthere)   /* if search monotonic, this is first increase*/
                break;      /*  after minimum*/
            }
        }
    if(nwmin == 0)
        Rmessage("Unable to compress data! (nwmin=0)",QUIT);

/*    printf("nbits: %i; nwords: %i\n",ncomp,nwmin);*/
    newarray[0] = nwmin;
    newarray[1] = ncomp;
    newarray[2] = nsubsidbest;
    memset(&newarray[3],0,nwmin*2);
/* do the compression for this no. of bits*/
/*    printf("compress\r");*/
    DoCompression(newarray, charray);
    if(Rpackverify)
        {
        testarray = (short int*) malloc((size_t)(npoints*sizeof(short)));
        if(testarray == NULL)
            Rmessage("Unable to allocate memory for verification!",QUIT);

        UndoCompression(newarray, testarray);
        Compare(charray,testarray);
        free(testarray);
        }
    return(nwmin);
}
#pragma optimize("",on)

/*--------------------------------------------------------------------*/
static void Compare(short int *charray, short int *testarray)
/*--------------------------------------------------------------------*/
{
    int i;
    for(i=0; i<Rnpoints; i++)
        {
        if(charray[i] != testarray[i])
            {
            snprintf(RAWmess, RAWMESS_SIZE, "arrays different at %i: %i %i!\n",
                i,charray[i],testarray[i]);
            Rmessage(RAWmess,QUIT);
            }
        }
}

/*--------------------------------------------------------------------*/
static void DoCompression(short int *newarray, short int *charray)
/*--------------------------------------------------------------------*/
{
    short int i, currentpos, diff, maxdiff, nsubsid=0, subsidcnt=0, subsidwords;
    short int *subsidarray, *dataarray, nbit, leftbit, mask, wd, rem;
    int lword=0, lval;

    nbit = newarray[1];
    dataarray = newarray + 3 + newarray[2]; /* point to start of data*/
    subsidarray = newarray+3;               /* point to subsid. array*/
    rem = leftbit = 32 - nbit;              /* base no. to shift left*/

    maxdiff = TwoPowerN(nbit);              /* 2**n*/
    mask = maxdiff-1;                       /* n-1 bits set to 1*/
    maxdiff >>= 1;                          /* 2**(n-1)*/

    dataarray[0] = charray[0];                  /* store first word*/
    for(i=1, currentpos=1; i<Rnpoints; i++)
        {
        diff = charray[i] - charray[i-1];
        if(diff >= maxdiff || (-diff) >= maxdiff) /* limit exceeded*/
            {
            subsidarray[nsubsid] = subsidcnt;   /* save no. of compressed wds.*/
            nsubsid++;
            if(leftbit-rem > 16)   /* need to raise counter if previous*/
                currentpos++;       /* word isn't full*/
            dataarray[currentpos] = charray[i];  /* store whole word*/
            currentpos++;
            subsidwords = (short)(((int)subsidcnt*nbit)/16);
            if((int)subsidwords*16 < (int)subsidcnt*nbit) subsidwords++;
            subsidcnt = 0;
            lword = 0;
            rem = leftbit;          /* avoid extra increment of currentpos*/
/*            nwords += subsidwords + 1;  // no. of compressed words + 1*/
            }
        else    /* here to put next n-bit word into place*/
            {
            wd = (short)(((int)subsidcnt*nbit)/16);
            rem = (short)(((int)subsidcnt*nbit)%16);
            lval = diff & mask;          /* next value to store*/
            lval <<= (leftbit-rem);      /* shift word left*/
            lword |= lval;               /* new set of bits in place*/
            dataarray[currentpos] = (int)(lword >> 16);

            if (leftbit-rem <= 16)        /* leftmost 16 bits full?*/
                {
                currentpos++;
                dataarray[currentpos] = (short int)(lword & 0xffff);
                lword <<= 16;            /* shift left*/
                if(leftbit-rem != 16) rem -= nbit;
                }

            subsidcnt++;    /* count no. of compressed words*/
            }
        }
    subsidarray[nsubsid] = subsidcnt;   /* save no. of compressed wds.*/
/* currentpos should equal the no. of words saved in newarray[0]!*/
/* nsubsid should equal newarray[2]*/
}

/*--------------------------------------------------------------------*/
static int UndoCompression(short int *newarray, short int *charray)
/*--------------------------------------------------------------------*/
{
    short int *dataarray, *subsidarray, nbit, nsubsid=0, subsidcnt=0;
    short int i, s, rightbit, mask, maxdiff, currentpos, wd, rem;
    int lword, lval;
    maxdiff = TwoPowerN(newarray[1]);       /* 2**n*/
    mask = maxdiff-1;                       /* n-1 bits set*/
    nbit = newarray[1];
    if(nbit > 15)                           /* arose from an error!*/
        {
        Rmessage(" bad channel set to zero!",CONTINUE);
        memset(charray,0,Rnpoints*2);
        return(1);
        }
    dataarray = newarray + 3 + newarray[2]; /* point to start of data*/
    subsidarray = newarray+3;               /* point to subsid. array*/
    rem = rightbit = 32 - nbit;             /* base no. to shift right*/
    i = currentpos = 0;
    do
        {
        charray[i] = dataarray[currentpos];
        currentpos++;
        i++;
        subsidcnt = subsidarray[nsubsid];
        for(s=0; s<subsidcnt; s++)          /* unpack subsid array*/
            {
            wd = (short)(((int)s*nbit)/16);
            rem = (short)(((int)s*nbit)%16);
            lword = (int)(dataarray[currentpos]) << 16;
            lword += (unsigned short)dataarray[currentpos+1];
            lval = lword >> (rightbit-rem);
            if(rightbit-rem <= 16)
                {
                currentpos++;
                if(rightbit-rem != 16) rem -= nbit;
                }
            lval &= mask;
            if((lval & mask) != (lval & (mask >> 1)))/* negative value*/
                lval -= maxdiff;
            charray[i] = (short int)lval;
            charray[i] += charray[i-1];
            i++;
            }
        if(rightbit-rem > 16)
            currentpos++;
        rem = rightbit;
        nsubsid++;
        } while (i < Rnpoints);
    return(0);
}

/*--------------------------------------------------------------------*/
static int TwoPowerN(int n)
/*--------------------------------------------------------------------*/
{/* return 2 to the power of n (n>=1)*/
    int power=2, i;
    for(i=1; i<n; i++)
        power *= 2;
    return(power);
}

/*--------------------------------------------------------------------*/
static int AnalyseDiffs(short int *charray, int n, int Rnpoints, short int *nsubsid)
/*--------------------------------------------------------------------*/
{ /* output the no. of words that would result from each word length*/
    short int i, nwords = 3, diff, maxdiff;
    int subsidcnt=0, subsidwords;
    maxdiff = TwoPowerN(n-1);                /* 2**n*/
    *nsubsid = 1;
    for(i=0; i<Rnpoints-1; i++)
        {
        diff = charray[i+1] - charray[i];
        if(diff >= maxdiff || (-diff) >= maxdiff) /* limit exceeded*/
            {
            (*nsubsid)++;
            subsidwords = (subsidcnt*n)/16;
            if(subsidwords*16 < subsidcnt*n) subsidwords++;
            subsidcnt = 0;
            nwords += (short int)subsidwords + 1;  /* no. of compressed words + 1*/
            }
        else
            {
            subsidcnt++;    /* count no. of compressed words*/
            }
        }
    subsidwords = (subsidcnt*n)/16;
    if(subsidwords*16 < subsidcnt*n) subsidwords++;
    subsidcnt = 0;
    nwords += (short int)subsidwords + 1;  /* no. of compressed words + 1*/
    nwords += (*nsubsid);      /* to account for subsidiary array*/
    return(nwords);
}

/*--------------------------------------------------------------------*/
static void icopychan(short int *charray,short int *iarray,int channel,int ndat,int nkan,int dir)
/*--------------------------------------------------------------------*/
/* put data for one channel into one-dimensional array, or reverse */
{
    int i, j;
    if(dir)
        for(i=channel,j=0; i<ndat; i+=nkan,j++)
            charray[j] = iarray[i];
    else
        for(i=channel,j=0; i<ndat; i+=nkan,j++)
            iarray[i] = charray[j];
}

/*--------------------------------------------------------------------*/
void Rmessage(char *str, int type)
/*--------------------------------------------------------------------*/
{
 if(type == QUIT) {
  ERREXIT1(kn_emethods, "kn.rawdatan: %s\n", MSGPARM(str));
 } else {
  TRACEMS1(kn_emethods, 0, "kn.rawdatan: %s\n", MSGPARM(str));
 }
}
