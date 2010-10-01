#ifndef _VITAPORT_H_INCLUDED
#define _VITAPORT_H_INCLUDED

enum vitaport_filetypes {
 VITAPORT1_FILE=0,
 VITAPORT2_FILE,
 VITAPORT2RFILE
};
/* These are guesses, I've seen that 4 is also unsigned */
enum VITAPORT_DATYPES {
 VP_DATYPE_SIGNED=0,
 VP_DATYPE_UNSIGNED=5,
 VP_DATYPE_MARKER=130
};
enum VITAPORT_DASIZES {
 VP_DATATYPE_BYTE=0,
 VP_DATATYPE_SHORT
};

#define HDTYP_VITAPORT1 2
#define HDTYP_VITAPORT2 6
#define HDTYP_VITAPORT2P1 7
/* All Vitaport sampling frequencies are divided from this rate: */
#define VITAPORT_CLOCKRATE 76800.0
#define VITAPORT_SYNCMAGIC 0x4afc

#define VP_MARKERCHANNEL_NAME "MARKER"
#define VP_CHANNELNAME_LENGTH 6
struct vitaport_idiotic_multiplemarkertablenames {
 char *markertable_name;
 long idiotically_fixed_length;
};
extern struct vitaport_idiotic_multiplemarkertablenames markertable_names[];
#define VP_CHANNELTABLE_NAME "CHSETN"
#define VP_GLBMRKTABLE_NAME "GLBMRK"
#define VP_TABLEVAR_LENGTH 6

struct vitaport_fileheader {
 unsigned short Sync;
 unsigned short hdlen;
 unsigned short chnoffs;
 unsigned char hdtyp;
 unsigned char knum;
};
struct vitaportI_fileheader {
 unsigned short blckslh;
 unsigned short bytes;
};
struct vitaportII_fileheader {
 unsigned long dlen;
 unsigned char Hour;
 unsigned char Minute;
 unsigned char Second;
 unsigned char rstimeflag;
 unsigned char Day;
 unsigned char Month;
 unsigned char Year;
 unsigned char Sysvers;
 unsigned short scnrate;
};
struct vitaportII_fileext {
 unsigned short chdlen;
 unsigned short swvers;
 unsigned short hwvers;
 unsigned long recorder;
 unsigned short timer1;
 unsigned short sysflg12;
};

struct vitaport_channelheader {
 char kname[VP_CHANNELNAME_LENGTH];
 char kunit[4];
 unsigned char datype;
 unsigned char dasize;
 unsigned char scanfc;
 unsigned char fltmsk;
 unsigned char stofac;
 unsigned char resvd1;
 unsigned short mulfac;
 unsigned short offset;
 unsigned short divfac;
 unsigned short resvd2;
};
struct vitaportII_channelheader {
 unsigned short mvalue;
 unsigned char cflags;
 unsigned char hwhp;
 unsigned short hwampl;
 unsigned short hwlp;
 unsigned short nres1;
 unsigned short nres2;
 unsigned short nres3;
 unsigned short nres4;
};
struct vitaportIIrchannelheader {
 unsigned long doffs;
 unsigned long dlen;
 unsigned short mval;
 unsigned char chflg;
 unsigned char hpass;
 unsigned short ampl;
 unsigned short tpass;
};

extern struct_member sm_vitaport_fileheader[];
extern struct_member sm_vitaportI_fileheader[];
extern struct_member sm_vitaportII_fileheader[];
extern struct_member sm_vitaportII_fileext[];
extern struct_member sm_vitaport_channelheader[];
extern struct_member sm_vitaportII_channelheader[];
extern struct_member sm_vitaportIIrchannelheader[];
extern struct_member_description smd_vitaport_fileheader[];
extern struct_member_description smd_vitaportI_fileheader[];
extern struct_member_description smd_vitaportII_fileheader[];
extern struct_member_description smd_vitaportII_fileext[];
extern struct_member_description smd_vitaport_channelheader[];
extern struct_member_description smd_vitaportII_channelheader[];
extern struct_member_description smd_vitaportIIrchannelheader[];

/* The date and time values are stored one digit per nibble; here are
 * functions to convert bytes from and to this representation */
#define convert_fromhex(s)	(*(unsigned char *)(s)=((*(unsigned char *)(s)>>4)*10+(*(unsigned char *)(s)&0xf)))
#define convert_tohex(s)	(*(unsigned char *)(s)=(((*(unsigned char *)(s)/10)<<4)+(*(unsigned char *)(s)%10)))

#endif
