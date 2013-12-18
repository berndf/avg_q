// Copyright (C) 1996,1997,2001 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

/*{{{}}}*/
#include "ConvexMesh.hh"
extern "C" {
 #include <optimize.h>
 #define FTOL 1e-4
}
#define ALLOWED_TYPICAL_LENGTH 8

/*{{{  do_spherefit(optimize_struct *ostructp)*/
LOCAL OPT_DTYPE
do_spherefit(optimize_struct *ostructp) {
 TPoints* tp=(TPoints*)ostructp->fixed_args;
 OPT_DTYPE *opts=(OPT_DTYPE *)ostructp->opt_args, sq=0.0, hold;
 Point c=Point(opts[0], opts[1], opts[2]);

 while (tp!=(TPoints*)Empty) {
  hold=(*tp - c).abs()-opts[3];
  sq+=hold*hold;	/* Accumulate the squared deviations from r */
  tp=(TPoints*)tp->next();
 }

 return sq;
}
/*}}}  */

ConvexMesh::ConvexMesh() {
 tp=(TPoints*)0;
 tr=(Triangles*)0;
 boundary=(Boundary*)0;
}
ConvexMesh::~ConvexMesh() {
 delete tp;
 delete tr;
 delete boundary;
}
ConvexMesh::ConvexMesh(TPoints *ttp) {
 const float typical_dd=ttp->typical_dd();
 tp=(TPoints*)ttp->first();

 fit_sphere();

 // Evaluated right to left!
 //tr=new Triangles(tp->nearest(), tp->nearest(), tp->nearest());
 tr=new Triangles();
 tr->p1=tp->nearest();
 tr->p2=tp->nearest();
 tr->p3=tp->nearest();
 boundary=new Boundary(tr);

 while (TRUE) {
  Boundary *nonconvex=boundary;
  // This flag indicates whether at least one nonconvex point was found
  Bool boundary_is_convex=TRUE;

  do {
   nonconvex=boundary;
   boundary_is_convex=TRUE;
   while ((nonconvex=nonconvex->non_convex_point(tr))!=(Boundary*)Empty) {
    Point* previous_p=((Boundary*)nonconvex->ringprevious())->point();
    Point* next_p=((Boundary*)nonconvex->ringnext())->point();

    Triangles* tr1;
    // Check a triangle made up of three successive boundary points
    if (previous_p->dd(next_p)<=ALLOWED_TYPICAL_LENGTH*typical_dd
     && (tr1=check_triangle(next_p, nonconvex->point(), previous_p))!=(Triangle*)Empty) {
     tr=(Triangles*)tr->addmember(tr1);
     boundary_is_convex=FALSE;
    } else {
     nonconvex=(Boundary*)nonconvex->next();
     continue;
    }

    // Remove point from boundary
    Boundary* nextstart=(Boundary*)nonconvex->next();
    if (boundary==nonconvex) boundary=(Boundary*)boundary->next();
    delete nonconvex;
    nonconvex=nextstart;
   }
  } while (!boundary_is_convex);

  Triangles* ntr=nearest_to_boundary();
  if (ntr==(Triangles*)Empty) break;
  tr=(Triangles*)tr->addmember(ntr);
 }
 Revise();
}
void ConvexMesh::display() {
 tr->display(GREEN);
 boundary->display(CYAN);
 cmov(origin.x, origin.y, origin.z);
 color(YELLOW);
 charstr("o");
}
void ConvexMesh::fit_sphere() {
 OPT_DTYPE sphere_startparameters[4];
 OPT_DTYPE sphere_parameters[4], residual;	/* 3x center, 1x radius */
 optimize_struct ostruct;
 array xi, id;
 int iter;

 // Set start values
 origin=tp->CenterOfGravity();
 radius=(*tp-origin).abs();

 sphere_startparameters[0]=origin.x;
 sphere_startparameters[1]=origin.y;
 sphere_startparameters[2]=origin.z;
 sphere_startparameters[3]=radius;

 /*{{{  Fit a sphere to the sensor array*/
 ostruct.function=&do_spherefit;
 ostruct.fixed_args=(void*)tp;
 ostruct.num_opt_args=4;
 ostruct.opt_args=sphere_parameters;

 /*{{{  Initialize xi*/
 xi.nr_of_elements=xi.nr_of_vectors=4;
 xi.element_skip=1;
 array_reset(&xi);
 id=xi;
 array_setto_identity(&id);
 if (array_allocate(&xi)==NULL) {
  //ERREXIT(tinfo->emethods, "dip_fit_init: Error allocating xi memory\n");
 }
 array_copy(&id, &xi);
 /*}}}  */

 residual=opt_powell(&ostruct, sphere_startparameters, &xi, FTOL, &iter);
 array_free(&xi);
 /*}}}  */

 if (radius==0 || sphere_parameters[3]/radius>100.0) {
  // This will take effect if the channel assembly is flat. The radius
  // will be so large that the angular distance calculations cannot any
  // longer be carried out reliably. We move the origin closer to the assembly
  // to get it to work again.
  Point fitted_origin=Point(sphere_parameters[0], sphere_parameters[1], sphere_parameters[2]);
  origin=origin+(fitted_origin-origin)/100.0;
  radius=sphere_parameters[3]/100;
 } else {
  origin.x=sphere_parameters[0];
  origin.y=sphere_parameters[1];
  origin.z=sphere_parameters[2];
  radius=sphere_parameters[3];
 }
}
Triangles* ConvexMesh::check_triangle(Point* p1, Point* p2, Point* p3) {
 Triangles* ntr=new Triangles(p1,p2,p3);
 // The triangle should not be too flat
 if (ntr->max_angle(FALSE)>M_PI*0.9) {
  delete ntr;
  ntr=(Triangles*)Empty;
 }
 return ntr;
}
Triangles* ConvexMesh::nearest_to_boundary() {
 float mindd= -1;
 TPoints* minpoint=(TPoints*)Empty;
 Boundary* minbpoint=(Boundary*)Empty;
 Boundary* inboundary=(Boundary*)boundary->first();
 Triangles* ntr=(Triangles*)Empty;

 while (inboundary!=(Boundary*)Empty) {
  float thisdd;
  // The problem in using the `distance to a line' when searching the optimal 
  // boundary section to which an external point should be added is that the
  // minimum distance to the end points of the line is returned if the  
  // projection does not fall between the end points. It follows that the
  // distance to the two segments containing a specified edge can be identical,
  // with no clue as to which segment should be preferred. Thus, we do not any
  // longer use the distance to a line but simply the distance to a point
  // in the middle of the segment.
  Point midpoint=(*inboundary->point()+(TPoints*)((Boundary*)inboundary->ringnext())->point())/2.0;
  TPoints* candidate=tp->angular_nearest(&origin, &midpoint, FALSE, &thisdd);
  if (mindd<0 || thisdd<mindd) {
   minpoint=candidate;
   minbpoint=inboundary;
   mindd=thisdd;
  }
  inboundary=(Boundary*)inboundary->next();
 }
 if (minpoint!=(TPoints*)Empty) {
  ntr=new Triangles(minbpoint->point(), minpoint, ((Boundary*)minbpoint->ringnext())->point());
  Boundary* newbpoint=new Boundary(minpoint);
  minpoint->mark_inside();
  minbpoint->addmember(newbpoint);
 }
 return ntr;
}
#undef DEBUG_REVISE
//#define DEBUG_REVISE 1
void ConvexMesh::Revise() {
 Triangles* const first_tr=(Triangles*)tr->first();
 Triangles* in_triangles;
 Bool mended_at_least_one;
#ifdef DEBUG_REVISE
 Bool do_display=TRUE;
#endif
 // The layout is: (Observe that the orientation of the 's' triangle is
 // reversed!)
 //          p3
 //        /   \
 //  p1=p1s --- p2=p2s
 //       \    /
 //        p3s
 // With the offending angle(s) at the p3 position(s).

 do {
  in_triangles=first_tr;
  mended_at_least_one=FALSE;
#ifdef DEBUG_REVISE
  int dev; 
  short val;
  if (do_display) {
   color(BLACK);
   clear();
   display();
   swapbuffers();
   dev=qread(&val);
   if (dev==KEYBD && val=='q') do_display=FALSE;
  }
#endif
  while (in_triangles!=(Triangles*)Empty) {
   Triangle tr1= *in_triangles;
   // The convention enforced by max_angle(reorder=TRUE) is to have the line 
   // opposing the maximum angle as the line between p1 and p2
   float tr1angle=tr1.max_angle(TRUE);
   // Now we look for a triangle that also includes the line p1-p2
   // The substitution is done if the sum of the max_angles of the two new
   // triangles is less than of the two old triangles
   Triangles* adjacent_tr=first_tr->find(tr1.p1, tr1.p2);
   if (adjacent_tr==in_triangles) {
    adjacent_tr=((Triangles*)in_triangles->next())->find(tr1.p1, tr1.p2);
   }
   if (adjacent_tr!=(Triangles*)Empty) {
    // The adjacent triangle is reordered so that p1s==p1 and p2s==p2
    // The triangle (p1s,p2s,p3s) has the INVERSE orientation from normal!
    float tr2angle= adjacent_tr->max_angle(FALSE);
    Point *p1s, *p2s, *p3s;
    if (tr1.p1==adjacent_tr->p1) {
     p1s=adjacent_tr->p1;
     p2s=adjacent_tr->p3;
     p3s=adjacent_tr->p2;
    } else if (tr1.p1==adjacent_tr->p2) {
     p1s=adjacent_tr->p2;
     p2s=adjacent_tr->p1;
     p3s=adjacent_tr->p3;
    } else {
     p1s=adjacent_tr->p3;
     p2s=adjacent_tr->p2;
     p3s=adjacent_tr->p1;
    }
    Triangle ntr1(tr1.p1,p3s,tr1.p3);
    Triangle ntr2(p2s, tr1.p3, p3s);
    /* Don't take these triangles if their common boundary is not convex,
     * ie if the normals of the two new triangles would be in opposite directions: */
    if (ntr1.normal()*ntr2.normal()>0
        && ntr1.max_angle(FALSE)+ntr2.max_angle(FALSE)<tr1angle+tr2angle) {
     mended_at_least_one=TRUE;
#ifdef DEBUG_REVISE
     /* Show the two triangles to change: */
     if (do_display) {
      ((Triangle *)in_triangles)->display(RED);
      ((Triangle *)adjacent_tr)->display(YELLOW);
      swapbuffers();
      dev=qread(&val);
      if (dev==KEYBD && val=='q') do_display=FALSE;
     }
#endif
     in_triangles->p1=ntr1.p1; in_triangles->p2=ntr1.p2; in_triangles->p3=ntr1.p3;
     adjacent_tr->p1=ntr2.p1; adjacent_tr->p2=ntr2.p2; adjacent_tr->p3=ntr2.p3;
     break;
    }
   }
   in_triangles=(Triangles*)in_triangles->next();
  }
 } while (mended_at_least_one);
}
void ConvexMesh::print() {
 tp->write_to_file(stdout);
 printf("\n");
 boundary->write_to_file(stdout);
 printf("\n");
 tr->print();
}
