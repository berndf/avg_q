/*
 * Copyright (C) 1994,1995,1997,1998,2004,2010,2011,2013-2015 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
/*
 * Spline gridder: Gridder using surface splines to interpolate the data
 * input given by a 3-d array of irregular input points z(x, y).
 * The program determines the convex hull of the input point cloud and
 * outputs zero for each grid point outside the point cloud.
 * The output is suitable as input for gnuplot (parametric splot).
 *						-- Bernd Feige 3.03.1994
 */

#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include <float.h>
#include <array.h>

GLOBAL DATATYPE xmean, ymean;

/*{{{}}}*/
/*{{{  is_inside(array *index, array *inpoints, DATATYPE x, DATATYPE y) {*/
LOCAL int
is_inside(array *index, array *inpoints, DATATYPE x, DATATYPE y) {
 array x1, x2, x3;
 DATATYPE x3data[2], d, x3dist, hold, xminline=0, yminline=0;
 DATATYPE dist, mindist= -1, xline, yline;

 /*{{{  Prepare x3: x3 is an array that holds x and y*/
 x3data[0]=x;
 x3data[1]=y;
 x3.nr_of_elements=2;
 x3.nr_of_vectors=x3.element_skip=1;
 x3.start=x3data;
 array_setreadwrite(&x3);
 array_reset(&x3);
 /*}}}  */
 /*{{{  x3dist:=distance between x3 and (xmean, ymean)*/
 hold=x-xmean;
 x3dist=hold*hold;
 hold=y-ymean;
 x3dist+=hold*hold;
 /*}}}  */

 /*{{{  Select qhull segment at minimum distance from x*/
 array_reset(inpoints);
 array_reset(index);
 do {
  x1=x2= *inpoints;
  x1.current_vector=array_scan(index);
  x2.current_vector=array_scan(index);
  array_setto_vector(&x1); array_setto_vector(&x2);
  x1.nr_of_elements=x2.nr_of_elements=2;

  d=array_parameter_linedist(&x1, &x2, &x3);

  /* Let the end points participate in the vote as well */
  if (d>1) d=1;
  if (d<0) d=0;
  xline=array_scan(&x1)*(1.0-d)+array_scan(&x2)*d;
  yline=array_scan(&x1)*(1.0-d)+array_scan(&x2)*d;
  hold=xline-array_scan(&x3);
  dist=hold*hold;
  hold=yline-array_scan(&x3);
  dist+=hold*hold;
  
  if (mindist<=0 || dist<mindist) {
   mindist=dist;
   xminline=xline;
   yminline=yline;
  }
 } while (index->message!=ARRAY_ENDOFSCAN);
 /*}}}  */

 /*{{{  dist:=(line-mean)^2*/
 hold=xminline-xmean;
 dist=hold*hold;
 hold=yminline-ymean;
 dist+=hold*hold;
 /*}}}  */

 return (dist>x3dist);
}
/*}}}  */

int main(int argc, char **argv) {
 FILE *fp;
 surfspline_desc sspline;
 int err, npoints, itempart=0;
 int binary_output=FALSE, matlab_output=FALSE, mtv_output=FALSE, interpolate_only=FALSE;
 float fbuf, external_value=0.0;
 DATATYPE f, x, y, xmin, xmax, ymin, ymax, xstep, ystep;
 array index;
 pid_t pid;
 char outname[L_tmpnam], inname[L_tmpnam], **inargs, *infile;

 /*{{{  Read command line args*/
 for (inargs=argv+1; inargs-argv<argc && **inargs=='-'; inargs++) {
  switch(inargs[0][1]) {
   case 'B':
    binary_output=TRUE;
    break;
   case 'I':
    if (inargs[0][2]!='\0') {
     itempart=atoi(*inargs+2);
    }
    break;
   case 'i':
    interpolate_only=TRUE;
    if (inargs[0][2]!='\0') {
     external_value=atof(*inargs+2);
    }
    break;
   case 'M':
    matlab_output=TRUE;
    break;
   case 'm':
    mtv_output=TRUE;
    break;
   default:
    fprintf(stderr, "%s: Ignoring unknown option %s\n", argv[0], *inargs);
    continue;
  }
 }
 if (argc-(inargs-argv)!=3) {
  fprintf(stderr, "Usage: %s array_filename degree npoints\n"
          "Options:\n"
          " -Iitem: Use item number item as z-axis (2+item'th column); default: 0\n"
          " -i[value]: Interpolate only; set external values to value (default: 0.0)\n"
          " -B: Binary output (gnuplot floats)\n"
          " -M: Matlab output (x, y vectors and z matrix)\n"
          " -m: Plotmtv output\n"
  	  , argv[0]);
  return -1;
 }
 infile= *inargs++;
 if ((fp=fopen(infile, "r"))==NULL) {
  fprintf(stderr, "Can't open %s\n", infile);
  return -2;
 }
 array_undump(fp, &sspline.inpoints);
 fclose(fp);
 if (sspline.inpoints.message==ARRAY_ERROR) {
  fprintf(stderr, "Error in array_undump\n");
  return -3;
 }
 if (sspline.inpoints.nr_of_elements<3+itempart) {
  fprintf(stderr, "Not enough columns in array %s\n", *(inargs-1));
  return -3;
 }
 sspline.degree=atoi(*inargs++);;
 npoints=atoi(*inargs++);
 /*}}}  */

 /*{{{  Get the index of qhull point pairs by running qhull*/
 tmpnam(outname); tmpnam(inname);
 if ((pid=fork())==0) {	/* I'm the child */
#define LINEBUF_SIZE 128
  char linebuf[LINEBUF_SIZE];
  array tmp_array;

  /*{{{  Dump xy positions to tmp file inname*/
  tmp_array.nr_of_elements=2;
  tmp_array.nr_of_vectors=sspline.inpoints.nr_of_vectors;
  tmp_array.element_skip=1;
  if (array_allocate(&tmp_array)==NULL) {
   fprintf(stderr, "Error allocating tmp_array\n");
   return -4;
  }
  array_reset(&sspline.inpoints);
  do {
   array_write(&tmp_array, array_scan(&sspline.inpoints));
   array_write(&tmp_array, array_scan(&sspline.inpoints));
   array_nextvector(&sspline.inpoints);
  } while (tmp_array.message!=ARRAY_ENDOFSCAN);
  if ((fp=fopen(inname, "w"))==NULL) {
   fprintf(stderr, "Can't open %s\n", inname);
   return -5;
  }
  array_dump(fp, &tmp_array, ARRAY_ASCII);
  fclose(fp);
  array_free(&tmp_array);
  /*}}}  */

  snprintf(linebuf, LINEBUF_SIZE, "qhull C0 i b <%s >%s", inname, outname);
  execl("/bin/sh", "sh", "-c", linebuf, 0);
#undef LINEBUF_SIZE
 } else {
  wait(NULL);
 }
 unlink(inname);
 if ((fp=fopen(outname, "r"))==NULL) {
  fprintf(stderr, "Can't open %s\n", outname);
  return -6;
 }
 array_undump(fp, &index);
 fclose(fp);
 unlink(outname);
 /*}}}  */

 /*{{{  Find min, max and mean coordinates*/
 array_transpose(&sspline.inpoints);
 array_reset(&sspline.inpoints);
 xmin=array_min(&sspline.inpoints);
 ymin=array_min(&sspline.inpoints);
 array_reset(&sspline.inpoints);
 xmax=array_max(&sspline.inpoints);
 ymax=array_max(&sspline.inpoints);
 array_reset(&sspline.inpoints);
 xmean=array_mean(&sspline.inpoints);
 ymean=array_mean(&sspline.inpoints);
 xstep=(xmax-xmin)/npoints;
 ystep=(ymax-ymin)/npoints;
 array_transpose(&sspline.inpoints);
 array_reset(&sspline.inpoints);
 /*}}}  */

 /*{{{  Copy itempart to 3rd location if necessary*/
 /* Note: For this behavior it is essential that array_surfspline
  * will accept vectors of any size >=3 and only process the first three
  * elements ! */
 if (itempart>0) {
  DATATYPE hold;
  do {
   sspline.inpoints.current_element=2+itempart;
   hold=READ_ELEMENT(&sspline.inpoints);
   sspline.inpoints.current_element=2;
   WRITE_ELEMENT(&sspline.inpoints, hold);
   array_nextvector(&sspline.inpoints);
  } while (sspline.inpoints.message!=ARRAY_ENDOFSCAN);
 }
 /*}}}  */

 /*{{{  Do surface spline*/
 if ((err=array_surfspline(&sspline))!=0) {
  fprintf(stderr, "Error %d in array_surfspline\n", err);
  return err;
 }
 xmin-=xstep; xmax+=2*xstep;
 ymin-=ystep; ymax+=2*ystep;

 if (binary_output) {
  /*{{{  Gnuplot binary output*/
  int n_xval=(xmax-xmin)/xstep+1, n;	/* Number of x values */

  fbuf=n_xval;
  fwrite(&fbuf, sizeof(float), 1, stdout);
  for (fbuf=xmin, n=0; n<n_xval; fbuf+=xstep, n++) {	/* x values */
   fwrite(&fbuf, sizeof(float), 1, stdout);
  }
  for (y=ymin; y<=ymax; y+=ystep) {
   fbuf=y; fwrite(&fbuf, sizeof(float), 1, stdout);
   for (x=xmin, n=0; n<n_xval; x+=xstep, n++) {
    if (interpolate_only && !is_inside(&index, &sspline.inpoints, x, y)) {
     f=external_value;
    } else {
     f=array_fsurfspline(&sspline, x, y);
    }
    fbuf=f; fwrite(&fbuf, sizeof(float), 1, stdout);
   }
  }
  /*}}}  */
 } else if (matlab_output) {
  /*{{{  Matlab output*/
  array xm, ym, zm;
  xm.nr_of_elements=ym.nr_of_elements=1;
  xm.nr_of_vectors=zm.nr_of_elements=(xmax-xmin)/xstep+1;
  ym.nr_of_vectors=zm.nr_of_vectors=(ymax-ymin)/ystep+1;
  xm.element_skip=ym.element_skip=zm.element_skip=1;
  array_allocate(&xm); array_allocate(&ym); array_allocate(&zm);
  if (xm.message==ARRAY_ERROR || ym.message==ARRAY_ERROR || zm.message==ARRAY_ERROR) {
   fprintf(stderr, "Error allocating output arrays\n");
   return -7;
  }
  x=xmin; do {
   array_write(&xm, x);
   x+=xstep;
  } while (xm.message!=ARRAY_ENDOFSCAN);
  y=ymin; do {
   array_write(&ym, y);
   x=xmin; do {
    if (interpolate_only && !is_inside(&index, &sspline.inpoints, x, y)) {
     f=external_value;
    } else {
     f=array_fsurfspline(&sspline, x, y);
    }
    array_write(&zm, f);
    x+=xstep;
   } while (zm.message==ARRAY_CONTINUE);
   y+=ystep;
  } while (zm.message!=ARRAY_ENDOFSCAN);
  array_dump(stdout, &xm, ARRAY_MATLAB);
  array_dump(stdout, &ym, ARRAY_MATLAB);
  array_dump(stdout, &zm, ARRAY_MATLAB);
  array_free(&xm); array_free(&ym); array_free(&zm);
  /*}}}  */
 } else if (mtv_output) {
  /*{{{  Plotmtv output*/
  array zm;
  DATATYPE zmin=FLT_MAX, zmax= -FLT_MAX;
  zm.nr_of_elements=(xmax-xmin)/xstep+1;
  zm.nr_of_vectors=(ymax-ymin)/ystep+1;
  zm.element_skip=1;
  array_allocate(&zm);
  if (zm.message==ARRAY_ERROR) {
   fprintf(stderr, "Error allocating output arrays\n");
   return -7;
  }
  y=ymin; do {
   x=xmin; do {
    if (interpolate_only && !is_inside(&index, &sspline.inpoints, x, y)) {
     f=external_value;
    } else {
     f=array_fsurfspline(&sspline, x, y);
    }
    array_write(&zm, f);
    if (f<zmin) zmin=f;
    if (f>zmax) zmax=f;
    x+=xstep;
   } while (zm.message==ARRAY_CONTINUE);
   y+=ystep;
  } while (zm.message!=ARRAY_ENDOFSCAN);
  /*{{{  Print file header*/
  printf(
  "# Output of Spline_Gridder (C) Bernd Feige 1995\n\n"
  "$ DATA=CONTOUR\n\n"
  "%% toplabel   = \"Spline_Gridder output\"\n"
  "%% subtitle   = \"File: %s\"\n\n"
  "%% interp     = 0\n"
  "%% contfill   = on\n"
  "%% meshplot   = off\n\n"
  "%% xmin = %g  xmax = %g\n"
  "%% ymin = %g  ymax = %g\n"
  "%% zmin = %g  zmax = %g\n"
  "%% nx   = %d\n"
  "%% ny   = %d\n"
  , infile, 
  xmin, xmax, 
  ymin, ymax, 
  zmin, zmax, 
  zm.nr_of_elements, 
  zm.nr_of_vectors);
  /*}}}  */
  array_dump(stdout, &zm, ARRAY_MATLAB);
  array_free(&zm);
  /*}}}  */
 } else {
  /*{{{  Gnuplot output*/
  for (x=xmin; x<=xmax; x+=xstep) {
   for (y=ymin; y<=ymax; y+=ystep) {
    if (interpolate_only && !is_inside(&index, &sspline.inpoints, x, y)) {
     f=external_value;
    } else {
     f=array_fsurfspline(&sspline, x, y);
    }
    printf("%g %g %g\n", x, y, f);
   }
   printf("\n");
  }
  /*}}}  */
 }
 /*}}}  */

 return 0;
}
