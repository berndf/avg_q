/*****************************************************************************/
/*      module: NEUROHDR                     written by: Henning Nordholz    */
/*        date: 16-Aug-93                   last update:                     */
/*     project:                                                              */
/* description:                                                              */
/*                                                                           */
/*   NEUROSCAN file header                                                   */
/*                                                                           */
/*****************************************************************************/
#ifndef _NEUROHEAD_H_INCLUDED
#ifdef __cplusplus
extern "C"{
#endif

#ifndef BYTE
        #define BYTE char
#endif 
#ifndef UBYTE
        #define UBYTE unsigned char
#endif 

#ifndef WORD
        #define WORD short
#endif 

#ifndef UWORD
        #define UWORD unsigned short
#endif 

/*** OLD STRUCTURE FOR ELECTRODE TABLE ***/ 
typedef struct {
        char lab[10];
        float x_coord;
        float y_coord;
        float alpha_wt;
        float beta_wt;
}OLDELECTLOC;

/*** OLD STRUCTURE FOR ERP HEADER ***/ 
typedef struct{ 
        char   type;
        char   id[20];
        char   oper[20];
        char   doctor[20];
        char   referral[20];
        char   hospital[20]; 
        char   patient[20]; 
        short int    age;
        char   sex;
        char   hand;
        char   med[20];
        char   category[20];
        char   state[20]; 
        char   label[20];
        char   date[10];
        char   time[12];
        char   avgmode;
        char   review; 
        short unsigned    nsweeps;
        short unsigned    compsweeps;
        short unsigned    pnts;
        short int    nchannels;
        short int    update;
        char   domain;
        unsigned short int    rate;
        double scale;
        char   veegcorrect;
        float  veogtrig;
        short int    veogchnl;
        float  heogtrig; 
        short int    heogchnl; 
        char   baseline;
        float  offstart;
        float  offstop;
        char   reject;
        char   rejchnl1;
        char   rejchnl2;
        char   rejchnl3;
        char   rejchnl4;
        float  rejstart;
        float  rejstop;
        float  rejmin;
        float  rejmax;
        char   trigtype;
        float  trigval;
        char   trigchnl;
        float  trigisi;
        float  trigmin;
        float  trigmax;
        float  trigdur;
        char   dir;
        float  dispmin;
        float  dispmax;
        float  xmin;
        float  xmax;
        float  ymin;
        float  ymax;
        float  zmin;
        float  zmax;
        float  lowcut;
        float  highcut;
        char   common;
        char   savemode;
        char   manmode;
        char   ref[20];
        char   screen[80];
        char   seqfile[80];
        char   montage[80];
        char   heegcorrect;
        char   variance;
        short int    acceptcnt;
        short int    rejectcnt;
        char   reserved[74];
        OLDELECTLOC elect_tab[64]; 
}OLDSETUP;

/*** CURRENT VERSION 3.0 STRUCTURE FOR ELECTRODE TABLE ***/ 

typedef struct {                /* Electrode structure  ------------------- */
        char  lab[10];          /* Electrode label - last bye contains NULL */
        char  reference;        /* Reference electrode number               */
        char  skip;             /* Skip electrode flag ON=1 OFF=0           */
        char  reject;           /* Artifact reject flag                     */
        char  display;          /* Display flag for 'STACK' display         */
        char  bad;              /* Bad electrode flag                       */
        unsigned short int n;   /* Number of observations                   */
        char  avg_reference;    /* Average reference status                 */
        char  ClipAdd;          /* Automatically add to clipboard           */
        float x_coord;          /* X screen coord. for 'TOP' display        */
        float y_coord;          /* Y screen coord. for 'TOP' display        */
        float veog_wt;          /* VEOG correction weight                   */
        float veog_std;         /* VEOG std dev. for weight                 */
        float snr;              /* signal-to-noise statistic                */
        float heog_wt;          /* HEOG Correction weight                   */
        float heog_std;         /* HEOG Std dev. for weight                 */
        short int baseline;     /* Baseline correction value in raw ad units*/
        char  Filtered;         /* Toggel indicating file has be filtered   */
        char  Fsp;              /* Extra data                               */
        float aux1_wt;          /* AUX1 Correction weight                   */ 
        float aux1_std;         /* AUX1 Std dev. for weight                 */
        float sensitivity;      /* electrode sensitivity                    */
        char  Gain;             /* Amplifier gain                           */
        char  HiPass;           /* Hi Pass value                            */
        char  LoPass;           /* Lo Pass value                            */
        unsigned char Page;     /* Display page                             */
        unsigned char Size;     /* Electrode window display size            */
        unsigned char Impedance;/* Impedance test                           */
        unsigned char PhysicalChnl; /* Physical channel used                    */      
        char  Rectify;           /* Free space                               */      
        float calib;            /* Calibration factor                       */
}ELECTLOC;

/*** STRUCTURE FOR ERP HEADER ***/ 
typedef struct{ 
        char   rev[12];         /* Revision string                         */
        long   NextFile;        /* offset to next file                     */
        long   PrevFile;        /* offset to prev file                     */
        char   type;            /* File type AVG=0, EEG=1, etc.            */
        char   id[20];          /* Patient ID                              */
        char   oper[20];        /* Operator ID                             */
        char   doctor[20];      /* Doctor ID                               */
        char   referral[20];    /* Referral ID                             */
        char   hospital[20];    /* Hospital ID                             */
        char   patient[20];     /* Patient name                            */
        short  int age;         /* Patient Age                             */
        char   sex;             /* Patient Sex Male='M', Female='F'        */
        char   hand;            /* Handedness Mixed='M',Rt='R', lft='L'    */
        char   med[20];         /* Medications                             */
        char   category[20];    /* Classification                          */
        char   state[20];       /* Patient wakefulness                     */
        char   label[20];       /* Session label                           */
        char   date[10];        /* Session date string                     */
        char   time[12];        /* Session time strin                      */
        float  mean_age;        /* Mean age (Group files only)             */
        float  stdev;           /* Std dev of age (Group files only)       */
        short int n;            /* Number in group file                    */
        char   compfile[38];    /* Path and name of comparison file        */
        float  SpectWinComp;    /* Spectral window compensation factor     */
        float  MeanAccuracy;    /* Average respose accuracy                */
        float  MeanLatency;     /* Average response latency                */
        char   sortfile[46];    /* Path and name of sort file              */
        int    NumEvents;       /* Number of events in eventable */
        char   compoper;        /* Operation used in comparison            */
        char   avgmode;         /* Set during online averaging             */
        char   review;          /* Set during review of EEG data           */
        short unsigned nsweeps;      /* Number of expected sweeps               */
        short unsigned compsweeps;   /* Number of actual sweeps                 */ 
        short unsigned acceptcnt;    /* Number of accepted sweeps               */
        short unsigned rejectcnt;    /* Number of rejected sweeps               */
        short unsigned pnts;         /* Number of points per waveform           */
        short unsigned nchannels;    /* Number of active channels               */
        short unsigned avgupdate;    /* Frequency of average update             */
        char  domain;           /* Acquisition domain TIME=0, FREQ=1       */
        char  variance;         /* Variance data included flag             */
        unsigned short rate;    /* D-to-A rate                             */
        double scale;           /* scale factor for calibration            */
        char  veogcorrect;      /* VEOG corrected flag                     */
        char  heogcorrect;      /* HEOG corrected flag                     */
        char  aux1correct;      /* AUX1 corrected flag                     */
        char  aux2correct;      /* AUX2 corrected flag                     */
        float veogtrig;         /* VEOG trigger percentage                 */
        float heogtrig;         /* HEOG trigger percentage                 */
        float aux1trig;         /* AUX1 trigger percentage                 */
        float aux2trig;         /* AUX2 trigger percentage                 */
        short int heogchnl;     /* HEOG channel number                     */
        short int veogchnl;     /* VEOG channel number                     */
        short int aux1chnl;     /* AUX1 channel number                     */
        short int aux2chnl;     /* AUX2 channel number                     */
        char  veogdir;          /* VEOG trigger direction flag             */
        char  heogdir;          /* HEOG trigger direction flag             */
        char  aux1dir;          /* AUX1 trigger direction flag             */ 
        char  aux2dir;          /* AUX2 trigger direction flag             */
        short int veog_n;       /* Number of points per VEOG waveform      */
        short int heog_n;       /* Number of points per HEOG waveform      */
        short int aux1_n;       /* Number of points per AUX1 waveform      */
        short int aux2_n;       /* Number of points per AUX2 waveform      */
        short int veogmaxcnt;   /* Number of observations per point - VEOG */
        short int heogmaxcnt;   /* Number of observations per point - HEOG */
        short int aux1maxcnt;   /* Number of observations per point - AUX1 */
        short int aux2maxcnt;   /* Number of observations per point - AUX2 */
        char   veogmethod;      /* Method used to correct VEOG             */
        char   heogmethod;      /* Method used to correct HEOG             */
        char   aux1method;      /* Method used to correct AUX1             */
        char   aux2method;      /* Method used to correct AUX2             */
        float  AmpSensitivity;  /* External Amplifier gain                 */
        char   LowPass;         /* Toggle for Amp Low pass filter          */
        char   HighPass;        /* Toggle for Amp High pass filter         */
        char   Notch;           /* Toggle for Amp Notch state              */
        char   AutoClipAdd;     /* AutoAdd on clip                         */
        char   baseline;        /* Baseline correct flag                   */
        float  offstart;        /* Start point for baseline correction     */
        float  offstop;         /* Stop point for baseline correction      */
        char   reject;          /* Auto reject flag                        */
        float  rejstart;        /* Auto reject start point                 */
        float  rejstop;         /* Auto reject stop point                  */
        float  rejmin;          /* Auto reject minimum value               */
        float  rejmax;          /* Auto reject maximum value               */
        char   trigtype;        /* Trigger type                            */
        float  trigval;         /* Trigger value                           */
        char   trigchnl;        /* Trigger channel                         */
        short int trigmask;     /* Wait value for LPT port                 */
        float trigisi;          /* Interstimulus interval (INT trigger)    */
        float trigmin;          /* Min trigger out voltage (start of pulse)*/
        float trigmax;          /* Max trigger out voltage (during pulse)  */
        char  trigdir;          /* Duration of trigger out pulse           */
        char  Autoscale;        /* Autoscale on average                    */
        short int n2;           /* Number in group 2 (MANOVA)              */
        char  dir;              /* Negative display up or down             */
        float dispmin;          /* Display minimum (Yaxis)                 */
        float dispmax;          /* Display maximum (Yaxis)                 */
        float xmin;             /* X axis minimum (epoch start in sec)     */
        float xmax;             /* X axis maximum (epoch stop in sec)      */
        float AutoMin;          /* Autoscale minimum                       */
        float AutoMax;          /* Autoscale maximum                       */
        float zmin;             /* Z axis minimum - Not currently used     */
        float zmax;             /* Z axis maximum - Not currently used     */
        float lowcut;           /* Archival value - low cut on external amp*/ 
        float highcut;          /* Archival value - Hi cut on external amp */ 
        char  common;           /* Common mode rejection flag              */
        char  savemode;         /* Save mode EEG AVG or BOTH               */
        char  manmode;          /* Manual rejection of incomming data      */
        char  ref[10];          /* Label for reference electode            */
        char  Rectify;          /* Rectification on external channel       */
        float DisplayXmin;      /* Minimun for X-axis display              */
        float DisplayXmax;      /* Maximum for X-axis display              */
        char  phase;            /* flag for phase computation              */
        char  screen[16];       /* Screen overlay path name                */
        short int CalMode;      /* Calibration mode                        */
        short int CalMethod;    /* Calibration method                      */
        short int CalUpdate;    /* Calibration update rate                 */
        short int CalBaseline;  /* Baseline correction during cal          */
        short int CalSweeps;    /* Number of calibration sweeps            */           
        float CalAttenuator;    /* Attenuator value for calibration        */
        float CalPulseVolt;     /* Voltage for calibration pulse           */
        float CalPulseStart;    /* Start time for pulse                    */
        float CalPulseStop;     /* Stop time for pulse                     */  
        float CalFreq;          /* Sweep frequency                         */  
        char  taskfile[34];     /* Task file name                          */
        char  seqfile[34];      /* Sequence file path name                 */
        char  SpectMethod;      /* Spectral method */
        char  SpectScaling;     /* Scaling employed */
        char  SpectWindow;      /* Window employed */
        float SpectWinLength;   /* Length of window % */
        char  SpectOrder;       /* Order of Filter for Max Entropy method */
        char  NotchFilter;      /* Notch Filter in or out */
        short HeadGain;         /* Current head gain for SYNAMP */
        long  AdditionalFiles;  /* No of additional files */
        char  unused[5];        /* Free space */
        short  FspStopMethod;   /* FSP - Stoping mode                      */
        short  FspStopMode;     /* FSP - Stoping mode                      */
        float FspFValue;        /* FSP - F value to stop terminate         */
        short int FspPoint;     /* FSP - Single point location             */
        short int FspBlockSize; /* FSP - block size for averaging          */
        unsigned short FspP1;   /* FSP - Start of window                   */
        unsigned short FspP2;   /* FSP - Stop  of window                   */
        float FspAlpha;         /* FSP - Alpha value                       */
        float FspNoise;         /* FSP - Signal to ratio value             */
        short int FspV1;        /* FSP - degrees of freedom                */   
        char  montage[40];      /* Montage file path name                  */   
        char  EventFile[40];    /* Event file path name                    */   
        float fratio;           /* Correction factor for spectral array    */
        char  minor_rev;        /* Current minor revision                  */
        short int eegupdate;    /* How often incomming eeg is refreshed    */ 
        char   compressed;      /* Data compression flag                   */
        float  xscale;          /* X position for scale box - Not used     */
        float  yscale;          /* Y position for scale box - Not used     */
        float  xsize;           /* Waveform size X direction               */
        float  ysize;           /* Waveform size Y direction               */
        char   ACmode;          /* Set SYNAP into AC mode                  */
        unsigned char   CommonChnl;      /* Channel for common waveform             */
        char   Xtics;           /* Scale tool- 'tic' flag in X direction   */ 
        char   Xrange;          /* Scale tool- range (ms,sec,Hz) flag X dir*/ 
        char   Ytics;           /* Scale tool- 'tic' flag in Y direction   */ 
        char   Yrange;          /* Scale tool- range (uV, V) flag Y dir    */ 
        float  XScaleValue;     /* Scale tool- value for X dir             */
        float  XScaleInterval;  /* Scale tool- interval between tics X dir */
        float  YScaleValue;     /* Scale tool- value for Y dir             */
        float  YScaleInterval;  /* Scale tool- interval between tics Y dir */
        float  ScaleToolX1;     /* Scale tool- upper left hand screen pos  */
        float  ScaleToolY1;     /* Scale tool- upper left hand screen pos  */
        float  ScaleToolX2;     /* Scale tool- lower right hand screen pos */
        float  ScaleToolY2;     /* Scale tool- lower right hand screen pos */
        short int port;         /* Port address for external triggering    */
        long  NumSamples;       /* Number of samples in continous file     */
        char  FilterFlag;       /* Indicates that file has been filtered   */
        float LowCutoff;        /* Low frequency cutoff                    */
        short int LowPoles;     /* Number of poles                         */
        float HighCutoff;       /* High frequency cutoff                   */ 
        short int HighPoles;    /* High cutoff number of poles             */
        char  FilterType;       /* Bandpass=0 Notch=1 Highpass=2 Lowpass=3 */
        char  FilterDomain;     /* Frequency=0 Time=1                      */
        char  SnrFlag;          /* SNR computation flag                    */
        char  CoherenceFlag;    /* Coherence has been  computed            */
        char  ContinousType;    /* Method used to capture events in *.cnt  */ 
        long  EventTablePos;    /* Position of event table                 */ 
        float ContinousSeconds; /* Number of seconds to displayed per page */
        long  ChannelOffset;    /* Block size of one channel in SYNAMPS  */
        char  AutoCorrectFlag;  /* Autocorrect of DC values */
        unsigned char DCThreshold; /* Auto correct of DC level  */
}SETUP;

/* sweep header for epoched data */
typedef struct
{
    char    accept;     /* accept byte */
    short   type;       /* trial type */
    short   correct;    /* accuracy */
    float   rt;         /* reaction time */
    short   response;   /* response time */
    short   not_used;
}
NEUROSCAN_EPOCHED_SWEEP_HEAD;
/*************************************************************************
 * DEFINITIONS OF TAG FOR EEG TYPES (TEEG)
 *
 * This structure describes an tag type 0 in a continous file */
typedef enum
{
    TEEG_EVENT_TAB1=1,        /* Event table tag type */
    TEEG_EVENT_TAB2=2       
}
TEEG_TYPE;

typedef struct{
        char Teeg;
        long Size;
        union {
            void *Ptr;      /* Memory pointer */
            long Offset;    /* Relative file position */
                            /* 0 Means the data start immediately */
                            /* >0 Means the data starts at a relative offset */
                            /* from current position at the end of the tag */
        } p_o;
} TEEG;

/* ************************************************************************
 * DEFINITIONS OF EVENT TYPES
 * 
 * This structure describes an event type 1 in a continous file */

/* values 0xb=DC-Reset 0xc=Reject 0xd=Accept 0xe=Start/Stop */
enum NEUROSCAN_ACCEPTVALUES {
 NAV_DCRESET=0xb, NAV_REJECT, NAV_ACCEPT, NAV_STARTSTOP
};

typedef struct{
        UWORD StimType; /* range  0-65535 */
        UBYTE KeyBoard; /* range  0-11  corresponding to function keys +1 */
#ifdef We_Dont_Think_that_Bitfields_are_Nonportable
        UBYTE KeyPad:4; /* range  0-15  bit coded response pad */
        UBYTE Accept:4; /* values: see enum NEUROSCAN_ACCEPTVALUES */
#define Event_KeyPad_value(s) ((s).KeyPad)
#define Event_Accept_value(s) ((enum NEUROSCAN_ACCEPTVALUES)(s).Accept)
#define Event_KeyPad_set(s, x) ((s).KeyPad=(x)) 
#define Event_Accept_set(s, x) ((s).Accept=(x))
#else
	UBYTE KeyPadAccept;
#define Event_KeyPad_value(s) ((s).KeyPadAccept&0xf)
#define Event_Accept_value(s) ((enum NEUROSCAN_ACCEPTVALUES)(((s).KeyPadAccept>>4)&0xf))
#define Event_KeyPad_set(s, x) ((s).KeyPadAccept=((s).KeyPadAccept&0xf0)|(x)) 
#define Event_Accept_set(s, x) ((s).KeyPadAccept=((s).KeyPadAccept&0xf)|((x)<<4))
#endif
        long Offset;    /* file offset of event */
} EVENT1;

/* This structure describes an event type 2 in a continous file */
/* It  contains addition information regarding subject performance in  */
/* behavioral task */

typedef struct{
	/* This is the EVENT1 part: */
        UWORD StimType; /* range  0-65535 */
        UBYTE KeyBoard; /* range  0-11  corresponding to function keys +1 */
#ifdef We_Dont_Think_that_Bitfields_are_Nonportable
        UBYTE KeyPad:4; /* range  0-15  bit coded response pad */
        UBYTE Accept:4; /* values: see enum NEUROSCAN_ACCEPTVALUES */
#define Event_KeyPad_value(s) ((s).KeyPad)
#define Event_Accept_value(s) ((enum NEUROSCAN_ACCEPTVALUES)(s).Accept)
#define Event_KeyPad_set(s, x) ((s).KeyPad=(x)) 
#define Event_Accept_set(s, x) ((s).Accept=(x))
#else
	UBYTE KeyPadAccept;
#define Event_KeyPad_value(s) ((s).KeyPadAccept&0xf)
#define Event_Accept_value(s) ((enum NEUROSCAN_ACCEPTVALUES)(((s).KeyPadAccept>>4)&0xf))
#define Event_KeyPad_set(s, x) ((s).KeyPadAccept=((s).KeyPadAccept&0xf0)|(x)) 
#define Event_Accept_set(s, x) ((s).KeyPadAccept=((s).KeyPadAccept&0xf)|((x)<<4))
#endif
        long Offset;    /* file offset of event */

	/* This is additional: */
        EVENT1  Event1; 
        WORD    Type;
        WORD    Code;       
        float   Latency;
        BYTE    EpochEvent;
        BYTE    Accept;
        BYTE    Accuracy;
} EVENT2;

/* This was taken from Henning Nordholz's focread7.c */
enum NEUROSCAN_SAVEMODES {
 NSM_EEGF=0, NSM_EEG=1, NSM_BOTH=2, NSM_CONT=3, 
 NSM_LAST /* Tells how many types are defined */
};
#define NSM_AVGD NSM_EEGF;

enum NEUROSCAN_TYPES {
 NTY_AVERAGED=0, NTY_EPOCHED=1,
 NTY_LAST /* Tells how many types are defined */
};

/* Subtype definitions. This heuristic seems necessary because of the
 * sloppy way NeuroScan detects (and documents !) them.
 * NST_CONTINUOUS is the multiplexed format of the 100 and 330 kHz modules, 
 * NST_SYNAMPS the blocked continuous format of the SYNAMPS. */
enum NEUROSCAN_SUBTYPES {
 NST_CONT0=0, NST_CONTINUOUS, NST_DCMES, NST_SYNAMPS, NST_EPOCHS, NST_AVERAGE, 
 NST_LAST /* Tells how many types are defined */
};
#define NEUROSCAN_SUBTYPE(eegstrucp) ((eegstrucp)->savemode==NSM_CONT ? (enum NEUROSCAN_SUBTYPES)(NST_CONT0+(eegstrucp)->ContinousType) : ((eegstrucp)->type==NTY_AVERAGED ? NST_AVERAGE : NST_EPOCHS))
extern char *neuroscan_subtype_names[];
extern int neuroscan_accept_translation[];

/* Trafo in microvolts. channelstrucp is a pointer to the structure of 
 * the channel from which the word s or float f was taken. */
#define NEUROSCAN_CONVSHORT(channelstrucp, s) (((DATATYPE)((s)-(channelstrucp)->baseline))*(channelstrucp)->sensitivity*(channelstrucp)->calib/204.8)
#define NEUROSCAN_CONVFLOAT(channelstrucp, f) ((f)*(channelstrucp)->calib/(channelstrucp)->n)
#define NEUROSCAN_SHORTCONV(channelstrucp, d) (rint((d)*204.8/(channelstrucp)->calib/(channelstrucp)->sensitivity)+(channelstrucp)->baseline)
#define NEUROSCAN_FLOATCONV(channelstrucp, f) ((f)*(channelstrucp)->n/(channelstrucp)->calib)

extern struct_member sm_SETUP[], sm_ELECTLOC[], sm_NEUROSCAN_EPOCHED_SWEEP_HEAD[], sm_TEEG[], sm_EVENT1[], sm_EVENT2[];
extern struct_member_description smd_SETUP[], smd_ELECTLOC[], smd_NEUROSCAN_EPOCHED_SWEEP_HEAD[], smd_TEEG[], smd_EVENT1[], smd_EVENT2[];

#ifdef ORIGINAL_NEUROSCAN_MACROS
#define ADINDEX(CHNL,PNT) ((float)(ad_buff[((CHNL)+(int)erp.nchannels*(PNT))]-erp.elect_tab[(CHNL)].baseline))

/* Conversion for microvolts without channel-specific calibration */
#define GETADVAL(CHNL,PNT) ((ADINDEX(CHNL,PNT)/204.8)*erp.elect_tab[CHNL].sensitivity)

/* Complete conversion to microvolts */
#define GETADuV(CHNL,PNT)  (GETADVAL(CHNL,PNT)*erp.elect_tab[(CHNL)].calib)

// Conversion from microvolts back to AD raw values
#define PUTADVAL(CHNL,PNT) ((elect[CHNL]->v[PNT]*204.8)/erp.elect_tab[CHNL].sensitivity)

/* Conversion for scaled ad value  with calibration */
#define GETADRAW(CHNL,PNT) ((ADINDEX(CHNL,PNT))*erp.elect_tab[(CHNL)].calib)

/* Conversion for scaled int value  with baseline but no calibration */
#define GETADINT(CHNL,PNT) ((ADINDEX(CHNL,PNT))/204.8)

#define GETVAL(CHNL,PNT) ((elect[CHNL]->v[PNT]/(float)erp.elect_tab[CHNL].n)*erp.elect_tab[CHNL].calib)

#define GETRVAL(CHNL,PNT) ((result[CHNL]->v[PNT]/(float)rslterp.elect_tab[CHNL].n)*rslterp.elect_tab[CHNL].calib)

#define GETCVAL(CHNL,PNT) ((compelect[CHNL]->v[PNT]/(float)comperp.elect_tab[CHNL].n)*comperp.elect_tab[CHNL].calib)
#endif /* ORIGINAL_NEUROSCAN_MACROS */

#ifdef __cplusplus
}
#endif
#define _NEUROHEAD_H_INCLUDED
#endif
