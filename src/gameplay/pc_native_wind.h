/* Fractional powers of the authored coupled angle/velocity map. A restoring
 * segment is selected once per authored tick; an exact host-clock phase splits
 * intervals at its boundary. Contact stays projected until that boundary and
 * wrapping rebases the affine segment. The first partial interval completes
 * one authored map at the next boundary. Original/whole calls bypass this map.
 * This is simulation state, not pose interpolation. */
#ifndef TSFP_PC_NATIVE_WIND_H
#define TSFP_PC_NATIVE_WIND_H
#include <math.h>
#include <stdint.h>
typedef struct PcWindHistory {double anchor;int contact;double side;int prepared;double slope,offset,span,scale;} PcWindHistory;
typedef struct PcNativeWindAxis {float angle,velocity;} PcNativeWindAxis;
static inline int pc_native_wind_flow(double *a,double *v,double f,double d,double k,double slope,double offset,double t)
{
 double q=1+k*slope,radius=sqrt(q*d),x=(q+q*d)/(2*radius),ratio,center;
 if(!(q>0&&x>-1))return 0;
 if(fabs(x-1)<1e-12){ratio=t;center=1;}
 else if(x<1){double th=acos(x);ratio=sin(t*th)/sin(th);center=cos(t*th);}
 else{double th=acosh(x);ratio=sinh(t*th)/sinh(th);center=cosh(t*th);}
 double scale=pow(radius,t),eq=(q*d*f+k*offset)/(1-q),old=*a-eq;
 double na=scale*((center+ratio*(q/radius-x))*old+ratio*q*d/radius*(*v))+eq;
 double nv=scale*(ratio*(q-1)/radius*old+(center+ratio*(q*d/radius-x))*(*v));
 *a=na;*v=nv;return isfinite(na)&&isfinite(nv);
}
static inline int pc_native_wind_axis(float angle,float velocity,float forcing,float drag,float restoring,float limit,float h,uint32_t whole,PcNativeWindAxis *out,PcWindHistory *history,double wind_start_phase,double wind_exact_delta)
{
 (void)whole;
 if(!out||!history||!isfinite(angle)||!isfinite(velocity)||!isfinite(forcing)||!(drag>0)||!(restoring>0)||!(h>0&&h<1)||!isfinite(drag)||!isfinite(restoring)||!isfinite(limit)||limit<0||!(wind_start_phase>=0&&wind_start_phase<1)||!(wind_exact_delta>0&&wind_exact_delta<1))return 0;
 double a=angle,v=velocity-forcing*h,left=wind_exact_delta,phase=wind_start_phase;
 for(unsigned split=0;left>1e-12&&split<3;split++){
  double boundary=1-phase,part=fmin(left,boundary);
  if(!history->prepared){
   history->anchor=a;history->span=1;history->scale=1/boundary;history->contact=0;
   double f=forcing;
   v=fmax(-90,fmin(90,v+f))-f;
   double drift=a+drag*(v+f),fold=(double)0.9f;
   if(drift<90){history->slope=-1;history->offset=0;}
   else if(drift<180){history->slope=-fold;history->offset=90*fold-90;}
   else if(drift<270){history->slope=fold;history->offset=9-180*fold;}
   else{history->slope=-1;history->offset=360;}
   history->prepared=1;
  }
  if(!pc_native_wind_flow(&a,&v,forcing*history->span,drag,(double)0.011111111380159855f*restoring,history->slope,history->offset,part*history->scale))return 0;
  if(a>=360){a-=360;history->offset+=360*history->slope;history->anchor-=360;}if(a<0){a+=360;history->offset-=360*history->slope;history->anchor+=360;}
  if(limit!=0){
   if(a>limit&&a<180){a=limit;history->contact=1;history->side=limit;}
   if(a<360-limit&&a>=180){a=360-limit;history->contact=1;history->side=360-limit;}
   if(history->contact)a=history->side;
  }
  if(part>=boundary-1e-12){
   if(history->contact)v=remainder(a-history->anchor,360);
   history->anchor=a;history->contact=0;history->prepared=0;
  }
  left-=part;phase=0;
 }
 *out=(PcNativeWindAxis){(float)a,(float)v};return isfinite(out->angle)&&isfinite(out->velocity);
}
#endif
