
/*  pulseseq_test.cc  //for generation of the pulse sequences, for now epi!

    Ivana Drobnjak and Mark Jenkinson, FMRIB Image Analysis Group

    Copyright (C) 2005 University of Oxford  */

/*  Part of FSL - FMRIB's Software Library
    http://www.fmrib.ox.ac.uk/fsl
    fsl@fmrib.ox.ac.uk
    
    Developed at FMRIB (Oxford Centre for Functional Magnetic Resonance
    Imaging of the Brain), Department of Clinical Neurology, Oxford
    University, Oxford, UK
    
    
    LICENCE
    
    FMRIB Software Library, Release 3.3 (c) 2006, The University of
    Oxford (the "Software")
    
    The Software remains the property of the University of Oxford ("the
    University").
    
    The Software is distributed "AS IS" under this Licence solely for
    non-commercial use in the hope that it will be useful, but in order
    that the University as a charitable foundation protects its assets for
    the benefit of its educational and research purposes, the University
    makes clear that no condition is made or to be implied, nor is any
    warranty given or to be implied, as to the accuracy of the Software,
    or that it will be suitable for any particular purpose or for use
    under any specific conditions. Furthermore, the University disclaims
    all responsibility for the use which is made of the Software. It
    further disclaims any liability for the outcomes arising from using
    the Software.
    
    The Licensee agrees to indemnify the University and hold the
    University harmless from and against any and all claims, damages and
    liabilities asserted by third parties (including claims for
    negligence) which arise directly or indirectly from the use of the
    Software or the sale of any products based on the Software.
    
    No part of the Software may be reproduced, modified, transmitted or
    transferred in any form or by any means, electronic or mechanical,
    without the express permission of the University. The permission of
    the University is not required if the said reproduction, modification,
    transmission or transference is done without financial return, the
    conditions of this Licence are imposed upon the receiver of the
    product, and all original and amended source code is included in any
    transmitted product. You may be held legally responsible for any
    copyright infringement that is caused or encouraged by your failure to
    abide by these terms and conditions.
    
    You are not permitted under this Licence to use this Software
    commercially. Use for which any financial return is received shall be
    defined as commercial use, and includes (1) integration of all or part
    of the source code or the Software into a product for sale or license
    by or on behalf of Licensee to third parties or (2) use of the
    Software or any derivative of it for research with the final aim of
    developing software products for sale or license to a third party or
    (3) use of the Software or any derivative of it for research with the
    final aim of developing non-software products for sale or license to a
    third party, or (4) use of the Software to provide any service to an
    external organisation for which payment is received. If you are
    interested in using the Software commercially, please contact Isis
    Innovation Limited ("Isis"), the technology transfer company of the
    University, to negotiate a licence. Contact details are:
    innovation@isis.ox.ac.uk quoting reference DE/1112. */

#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>

#include "libprob.h"
#include "newmatap.h"
#include "newmatio.h"
#include "newimage/newimageall.h"
#include "utils/options.h"
//#include "possumfns.h"

#define _GNU_SOURCE 1
#define POSIX_SOURCE 1

using namespace NEWIMAGE;
using namespace NEWMAT;
using namespace MISCMATHS;
using namespace Utilities;
using namespace std;

const double gammabar=42.58*1e06;//(in Hz/T)

string title="pulse (Version 2.0)\nCopyright(c) 2003, University of Oxford (Ivana Drobnjak and Mark Jenkinson)";
string examples="pulse";

Option<bool>   verbose(string("-v,--verbose"), false, 
		  string("switch on diagnostic messages"), 
		  false, no_argument);
Option<bool>   help(string("-h,--help"), false,
		  string("display this message"),
		  false, no_argument);

Option<string> opt_object(string("-i,--inp"), string(""),
		  string("4D brain, resolution can be any."),
		  true, requires_argument);

//INPUT pulse sequence properties 
Option<string> opt_seq(string("--seq"), string("epi"),
		  string("default=epi (epi OR ge OR none)"),
		  false, requires_argument);
Option<float>  opt_angle(string("--angle"),90,
		  string("default=90 (flip angle in degrees)"),
	          false,requires_argument);
Option<int>    opt_numvol(string("--numvol"), 1,
	          string("default=1 (number of volumes)"),
	          false, requires_argument);
Option<int>    opt_numslc(string("--numslc"),1,
	          string("default=1 (number of slices)"),
	          false,requires_argument);
Option<string> opt_slcdir(string("--slcdir"), string("z-"),
		  string("default=z- (x+,x-, y+,y- or z+,or z- slice direction/orientation)"),
		  false, requires_argument);
Option<string> opt_phasedir(string("--phasedir"), string("y"),
		  string("default=y (x,y,or z phase encode direction"),
		  false, requires_argument);
Option<string> opt_readdir(string("--readdir"), string("x"),
		  string("default=x (x,y, or z read encode direction"),
		  false, requires_argument);
Option<float>  opt_slcthk(string("--slcthk"),0.006,
	          string("default=0.006m (slice thickness)"),
	          false,requires_argument);
Option<float>  opt_gap(string("--gap"),0,
	          string("default=0m (gap between the slices in m)"),
	          false,requires_argument);
Option<int>    opt_Nx(string("--nx"), 64,
		  string("default=64 (resolution in x of the output image)"),
		  false,requires_argument);
Option<int>    opt_Ny(string("--ny"), 64,
		  string("default=64 (resolution in y of the output image)"),
		  false,requires_argument);
Option<float>  opt_dx(string("--dx"),0.004,
	          string("default=0.004m (image voxel x-dimension)"),
	          false,requires_argument);
Option<float>  opt_dy(string("--dy"),0.004,
	          string("default=0.004m (image voxel y-dimension) "),
	          false,requires_argument);
Option<float>  opt_BWrec(string("--bw"),100000,
	          string("default=100000Hz ( receiving bandwidth) "),
	          false,requires_argument);
Option<float>  opt_TE(string("--te"),0.03,
	          string("default=0.03s (the time from the first RF to the first echo (in epi center of the k-space, in GE it is the center of the first line of the k-space)"),
	          false,requires_argument);
Option<float>  opt_TR(string("--tr"),3,
	          string("default=3s (the time between the two RF pulses applied on the same part of the object (in epi the acquisition time for the whole k-space in GE time for the first line)"),
	          false,requires_argument);
Option<float>  opt_TRslc(string("--trslc"),0.12,
	          string("default=0.12s (the time that takes for one slice, in EPI def is TR/25 , in GE it is TR=TRslc)"),
	          false,requires_argument);
Option<float>  opt_maxG(string("--maxG"),0.055,
	          string("default=0.055 T/m (maximum gradient strength) "),
	          false,requires_argument);
Option<float>  opt_risetime(string("--riset"),0.00022,
	          string("default=0.00022s (time it takes for the gradient to reach its max value) "),
	          false,requires_argument);
Option<float>  opt_zstart(string("--zstart"),0,
	          string("default=0m (the lowest position in the slice direction in m)"),
	          false,requires_argument);

//OUTPUT matrix
Option<string> opt_pulse(string("-o,--out"), string(""),
		  string("pulse sequence matrix"),
		  true, requires_argument);

Option<bool> opt_kcoord(string("-k,--kcoord"),false,
		  string("default=no (saving k-space coordinates)"),
		  false, no_argument);


int nonoptarg;

/////////////////////////////////////////////////////////////////////////////////////////////////////
Matrix partialepisequence(const int n,const RowVector zc,const int ns,const double ddz,const string slicedir, const string phasedir, const string readdir,const int resX,const int resY){
 // Input parameters: n = number of volumes, ns = number of slices (per volume)
 //   ddz = slice thickness (m) , zc = coordinate of slice centre (m)
      //EPISEQUENCE MATRIX INPUTFILE
 //(1)=time in s,(2)=rf angle,(3)=rf frequency bandwidth df(Hz),(4)=rf center frequency fc(Hz),(5)=readout 1/0 (6)=x gradient(T/m),(7)=y gradient(T/m),(8)=z gradient(T/m)
 /////////////////////////////////
 //SLICE DIRECTION
 ////////////////////////////////
 int aa=6;
 int bb=7;
 int cc=8;
 int bhelp=0;
 int simdir=1;
 if (slicedir=="z-" || slicedir=="z+") cc=8;
 if (slicedir=="y-" || slicedir=="y+") cc=7;
 if (slicedir=="x-" || slicedir=="x+") cc=6;
 
 if (phasedir=="z") bb=8;
 if (phasedir=="y") bb=7;
 if (phasedir=="x") bb=6;

 if (readdir=="z") aa=8;
 if (readdir=="y") aa=7;
 if (readdir=="x") aa=6;

 if (slicedir=="x-" || slicedir=="y-" || slicedir=="z-") {
   bhelp=ns+1;
   simdir=-1;
 }
 /////////////////////////////
 //INPUT PARAMETERS
 ////////////////////////////
 float angle=opt_angle.value();
 float angle_rad=angle*M_PI/180;
 double bw=(double) (opt_BWrec.value());//BW
 double TE=(double) (opt_TE.value());
 double TR=(double) (opt_TR.value());
 double TRslc=(double) (opt_TRslc.value());
 if (TRslc*ns>TR){
   cout<<"WARNING:TR>=TRslc*numslc and your values TR="<<TR<<"; TRslc="<<TRslc<<"; numslc="<<ns<<" do not satisfy this."<<endl;
   exit(EXIT_FAILURE);
 }
 double rt=(double) (opt_risetime.value());
 double maxG=(double) (opt_maxG.value());
 double dx=(double) (opt_dx.value());
 double dy=(double) (opt_dy.value());
 int extra=resY-resX/2;//number of lines till we get to the center of the k-space
 if (resY<resX/2){
   cout<<"WARNING: resY<resX/2"<<endl;
   exit(EXIT_FAILURE);
 }
 double tana=maxG/rt;
 ///////////////////////////
 //SLICE SELECTION - works for all slice select direction. Gz is slice select gradient NOT Gz gradient.
 ///////////////////////////
 double Gz=7.128*1e-03;//slice select gradient during rf excitation (in T/m)
 double dtz=Gz/tana;
 double rft=4*0.001;
 double dtz1=sqrt(Gz*(dtz+rft)*2/tana);
 double Gz1=dtz1*tana/2;
 double TA=rft/2+dtz+dtz1;//when all rf stuff ends
 double df=ddz*gammabar*Gz;//frequency width (in m*Hz/T*T/m=Hz),
 //cout<<"frequency width     "<<df<<endl;
 double t=0;//time (in ms)
 //double offset=0;//for now (in m)
 int step=1;//row number in the matrix
 RowVector fc=zc*gammabar*Gz;
 cout<<"Vector of centers of slices is (m) "<<zc<<endl;
 cout<<"Vector of corresponding frequences is (Hz) "<<fc<<endl;//vector of centers of frequences (Hz)
 ///////////////////////////
 //READ OUT
 ///////////////////////////
 double dtx=1/bw;//sampling time along the readout direction
 double dkx=1/(resX*dx);//dimensions of sampling distances in the k-space
 double dky=1/(resY*dy);
 double kx,ky,kz;//for calculating the positions in the k-space by summing the areas.
 double Gx=dkx/(gammabar*dtx); //coming from dtx*gammabar*Gx=dkx
 double dt=Gx/tana;
 double dty=sqrt(4*dky/(gammabar*tana));
 double Gy=dty*tana/2;
 double dtx1=sqrt(Gx*(dt+resX*dtx)*2/tana);
 double Gx1=dtx1*tana/2;
 double dty1=sqrt(extra)*dty;
 double Gy1=dty1*tana/2;
 double TEl=extra*(2*dt+(resX-1)*dtx)+(dt+resX/2*dtx);//TE is on the (resX/2+1,resY/2+1)point in the k-space
 double TEr=(resY/2-1)*(2*dt+(resX-1)*dtx)+(dt+(resX/2-1)*dtx);
 double TD=TE-TEl;
 double TC=TD-dtx1;
 double TB=TC-dty1;
 double TF=TE+TEr;
 double tcrush=100*rt;
 double TG=TF+2*rt+tcrush;//100rt is how much is needed to make ggx ggy and ggz become 100 times larger which will reduce the signal 100 times -----  still in process of testing dec 20
 cout<<"Times in the acquisition of one slice starting from the RF pulse (in s)"<<endl;
 cout<<"the slice selection gradient is done TA="<<TA<<endl;
 cout<<"the phase encode gradient starts dephasing TB="<<TB<<endl;
 cout<<"the phase encode gradient stops dephasing, and the read-out gradient starts dephasing TC="<<TC<<endl;
 cout<<"the read-out gradient stops dephasing,begining of the read-out period TD="<<TD<<endl;
 cout<<"echo time TE="<<TE<<endl;
 cout<<"end of the read-out period and begining of the crushers TF="<<TF<<endl;
 cout<<"end of the crushers TG="<<TG<<endl;
 cout<<"end of the acquisition of one slice TRslc="<<TRslc<<endl;
 if (TD<0 || TB<0 || TA<0 || TC<0 || TF>TRslc || TG>TRslc){
   cout<<"WARNING:TE is not long enough to accomodate for the resX, resY, and the BW"<<endl; 
   exit(EXIT_FAILURE);
 }
 cout.precision(20);
 ////////////////////////////
 //MAIN LOOP
 ///////////////////////////
 int nreadp=n*ns*resX*resY;
 int readstep=0;
 Matrix M=zeros(n*ns*resX*resY*2,8);//main epi matrix
 Matrix coord(2,nreadp); //matrix for the kspace coordinates, 1st raw for kx and 2nd for ky
 for (int a=1;a<=n;a++){
   //cout<<"--Volume number----"<<a<<"-------"<<endl;
   for (int b=1;b<=ns;b++){
     kx=0;ky=0;kz=0;
     ////////////////slice selection //have to work more on this, quite primitive at the moment
     //cout<<"--Slice number----"<<b<<"-------"<<endl;
     //cout<<"Time at the begining of slice acquisition: "<<t<<endl;
     t+=dtz;step=step+1;M(step,1)=t;M(step,cc)=Gz; //kz=kz+M(step,cc)*dtz/2;//TC
     t+=rft/2;step=step+1;M(step,1)=t;M(step,2)=angle_rad;M(step,3)=df;M(step,4)=fc(bhelp+simdir*b);M(step,cc)=Gz;//kz=kz+M(step,cc)*rft/2;//TRF
     double TRF=t;
     //cout<<"The time when the RF pulse happenes for the VolNum "<<a<<" and the SlcNum "<<b<<" is TRF="<<TRF<<endl;
     t+=rft/2;step=step+1;M(step,1)=t;M(step,cc)=Gz; kz=kz+M(step,cc)*rft/2;
     t+=dtz;step=step+1;M(step,1)=t; kz=kz+M(step-1,cc)*dtz/2;
     t+=dtz1/2;step=step+1;M(step,1)=t;M(step,cc)=-Gz1; kz=kz+M(step,cc)*dtz1/4;
     t+=dtz1/2;step=step+1;M(step,1)=t; kz=kz+M(step-1,cc)*dtz1/4;//TA
     //cout<<"End of slice selection and dephasing TA= "<<t<<endl;
     //cout<<"kz= "<<kz<<endl;
     ////////////////dephase
     t=TRF+TB;step=step+1;M(step,1)=t;//TB
     //cout<<"Begining of the Gy gradient TB= "<<t<<endl;
     t+=dty1/2;step=step+1;M(step,1)=t;M(step,bb)=-Gy1;ky=ky+M(step,bb)*dty1/4;
     t+=dty1/2;step=step+1;M(step,1)=t;  ky=ky+M(step-1,bb)*dty1/4;//TC
     //cout<<"End of Gy and begining of Gx, TC= "<<t<<endl;
     t+=dtx1/2;step=step+1;M(step,1)=t;M(step,aa)=-Gx1; kx=kx+M(step,aa)*dtx1/4;
     t+=dtx1/2;step=step+1;M(step,1)=t; kx=kx+M(step-1,aa)*dtx1/4;//TD
     //cout<<"Begining of the read-out TD= "<<t<<endl;
     ////////////////readout
     for (int c=1;c<=resY/2+extra;c++){
       if (c==1){
	 t+=dt;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=Gx;kx=kx+M(step,aa)*dt/2; readstep=readstep+1;
	 //  cout<<"The first point in the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; 
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }      
       else { 
         t+=dty/2;step=step+1;M(step,1)=t;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*dty*tana/2;ky=ky+dty*M(step-1,bb)/4;kx=kx+M(step,aa)*dty/4;
         t+=dt-dty/2;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*Gx; kx=kx+(M(step,aa)+M(step-1,aa))*(dt-dty/2)/2;readstep=readstep+1;
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }
       for (int d=1;d<=resX-1;d++){
         t+=dtx;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*Gx;kx=kx+M(step,aa)*dtx;readstep=readstep+1;
         //if (c==(extra+1) && d==(resX/2)){
	 // cout<<"Time at the center of the k-space is TE= "<<t<<"Expected time is TE= "<<TE<<endl;
	 // cout<<"The center of the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; //TE
	 //}
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }
       if (c==resY/2+extra){
         t+=dt;step=step+1;M(step,1)=t;kx=kx+M(step-1,aa)*dt/2;//TF
       }
       else {
         t+=dt-dty/2;step=step+1;M(step,1)=t;M(step,aa)=MISCMATHS::pow(-1.0f, (double) c+1)*dty*tana/2;kx=kx+(M(step-1,aa)+M(step,aa))*(dt-dty/2)/2;
         t+=dty/2;step=step+1;M(step,1)=t;M(step,bb)=Gy;ky=ky+dty*M(step,bb)/4;kx=kx+M(step-1,aa)*dty/4;//TG
       }
     }
     /////////////////Crushers
     t+=rt;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=tcrush;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=rt;step=step+1;M(step,1)=t;
     t=(double) (TRslc*b+(a-1)*TR);step=step+1;M(step,1)=t;
   }
   t=(double) (TR*a);step=step+1;M(step,1)=t;
 }

 if (opt_kcoord.set()) write_binary_matrix(coord,"kcoord_"+opt_pulse.value());
 
 M=M.Rows(1,step);
 return M;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
Matrix episequence(const int n,const RowVector zc,const int ns,const double ddz,const string slicedir, const string phasedir, const string readdir,const int resX,const int resY){
 // Input parameters: n = number of volumes, ns = number of slices (per volume)
 //   ddz = slice thickness (m) , zc = coordinate of slice centre (m)
      //EPISEQUENCE MATRIX INPUTFILE
 //(1)=time in s,(2)=rf angle,(3)=rf frequency bandwidth df(Hz),(4)=rf center frequency fc(Hz),(5)=readout 1/0 (6)=x gradient(T/m),(7)=y gradient(T/m),(8)=z gradient(T/m)
 /////////////////////////////////
 //SLICE DIRECTION
 ////////////////////////////////
 int aa=6;
 int bb=7;
 int cc=8;
 int bhelp=0;
 int simdir=1;
 if (slicedir=="z-" || slicedir=="z+") cc=8;
 if (slicedir=="y-" || slicedir=="y+") cc=7;
 if (slicedir=="x-" || slicedir=="x+") cc=6;
 
 if (phasedir=="z") bb=8;
 if (phasedir=="y") bb=7;
 if (phasedir=="x") bb=6;

 if (readdir=="z") aa=8;
 if (readdir=="y") aa=7;
 if (readdir=="x") aa=6;

 if (slicedir=="x-" || slicedir=="y-" || slicedir=="z-") {
   bhelp=ns+1;
   simdir=-1;
 }
 /////////////////////////////
 //INPUT PARAMETERS
 ////////////////////////////
 float angle=opt_angle.value();
 float angle_rad=angle*M_PI/180;
 double bw=(double) (opt_BWrec.value());//BW
 double TE=(double) (opt_TE.value());
 double TR=(double) (opt_TR.value());
 double TRslc=(double) (opt_TRslc.value());
 if (TRslc*ns>TR){
   cout<<"WARNING:TR>=TRslc*numslc and your values TR="<<TR<<"; TRslc="<<TRslc<<"; numslc="<<ns<<" do not satisfy this."<<endl;
   exit(EXIT_FAILURE);
 }
 double rt=(double) (opt_risetime.value());
 double maxG=(double) (opt_maxG.value());
 double dx=(double) (opt_dx.value());
 double dy=(double) (opt_dy.value());
 double tana=maxG/rt;
 ///////////////////////////
 //SLICE SELECTION - works for all slice select direction. Gz is slice select gradient NOT Gz gradient.
 ///////////////////////////
 double Gz=7.128*1e-03;//slice select gradient during rf excitation (in T/m)
 double dtz=Gz/tana;
 double rft=4*0.001;
 double dtz1=sqrt(Gz*(dtz+rft)*2/tana);
 double Gz1=dtz1*tana/2;
 double TA=rft/2+dtz+dtz1;//when all rf stuff ends
 double df=ddz*gammabar*Gz;//frequency width (in m*Hz/T*T/m=Hz),
 //cout<<"frequency width     "<<df<<endl;
 double t=0;//time (in ms)
 //double offset=0;//for now (in m)
 int step=1;//row number in the matrix
 RowVector fc=zc*gammabar*Gz;
 cout<<"Vector of centers of slices is (m) "<<zc<<endl;
 cout<<"Vector of corresponding frequences is (Hz) "<<fc<<endl;//vector of centers of frequences (Hz)
 ///////////////////////////
 //READ OUT
 ///////////////////////////
 double dtx=1/bw;//sampling time along the readout direction
 double dkx=1/(resX*dx);//dimensions of sampling distances in the k-space
 double dky=1/(resY*dy);
 double kx,ky,kz;//for calculating the positions in the k-space by summing the areas.
 double Gx=dkx/(gammabar*dtx); //coming from dtx*gammabar*Gx=dkx
 double dt=Gx/tana;
 double dty=sqrt(4*dky/(gammabar*tana));
 double Gy=dty*tana/2;
 double dtx1=sqrt(Gx*(dt+resX*dtx)*2/tana);
 double Gx1=dtx1*tana/2;
 double dty1=sqrt(resY/2)*dty;
 double Gy1=dty1*tana/2;
 double TEl=resY/2*(2*dt+(resX-1)*dtx)+(dt+resX/2*dtx);//TE is on the (resX/2+1,resY/2+1)point in the k-space
 double TEr=(resY/2-1)*(2*dt+(resX-1)*dtx)+(dt+(resX/2-1)*dtx);
 double TD=TE-TEl;
 double TC=TD-dtx1;
 double TB=TC-dty1;
 double TF=TE+TEr;
 double tcrush=100*rt;
 double TG=TF+2*rt+tcrush;//100rt is how much is needed to make ggx ggy and ggz become 100 times larger which will reduce the signal 100 times -----  still in process of testing dec 20
 cout<<"Times in the acquisition of one slice starting from the RF pulse (in s)"<<endl;
 cout<<"the slice selection gradient is done TA="<<TA<<endl;
 cout<<"the phase encode gradient starts dephasing TB="<<TB<<endl;
 cout<<"the phase encode gradient stops dephasing, and the read-out gradient starts dephasing TC="<<TC<<endl;
 cout<<"the read-out gradient stops dephasing,begining of the read-out period TD="<<TD<<endl;
 cout<<"echo time TE="<<TE<<endl;
 cout<<"end of the read-out period and begining of the crushers TF="<<TF<<endl;
 cout<<"end of the crushers TG="<<TG<<endl;
 cout<<"end of the acquisition of one slice TRslc="<<TRslc<<endl;
 if (TD<0 || TB<0 || TA<0 || TC<0 || TF>TRslc || TG>TRslc){
   cout<<"WARNING:TE is not long enough to accomodate for the resX, resY, and the BW"<<endl; 
   exit(EXIT_FAILURE);
 }
 cout.precision(20);
 ////////////////////////////
 //MAIN LOOP
 ///////////////////////////
 int nreadp=n*ns*resX*resY;
 int readstep=0;
 Matrix M=zeros(n*ns*resX*resY*2,8);//main epi matrix
 Matrix coord(2,nreadp); //matrix for the kspace coordinates, 1st raw for kx and 2nd for ky
 for (int a=1;a<=n;a++){
   //cout<<"--Volume number----"<<a<<"-------"<<endl;
   for (int b=1;b<=ns;b++){
     kx=0;ky=0;kz=0;
     ////////////////slice selection //have to work more on this, quite primitive at the moment
     //cout<<"--Slice number----"<<b<<"-------"<<endl;
     //cout<<"Time at the begining of slice acquisition: "<<t<<endl;
     t+=dtz;step=step+1;M(step,1)=t;M(step,cc)=Gz; //kz=kz+M(step,cc)*dtz/2;//TC
     t+=rft/2;step=step+1;M(step,1)=t;M(step,2)=angle_rad;M(step,3)=df;M(step,4)=fc(bhelp+simdir*b);M(step,cc)=Gz;//kz=kz+M(step,cc)*rft/2;//TRF
     double TRF=t;
     //cout<<"The time when the RF pulse happenes for the VolNum "<<a<<" and the SlcNum "<<b<<" is TRF="<<TRF<<endl;
     t+=rft/2;step=step+1;M(step,1)=t;M(step,cc)=Gz; kz=kz+M(step,cc)*rft/2;
     t+=dtz;step=step+1;M(step,1)=t; kz=kz+M(step-1,cc)*dtz/2;
     t+=dtz1/2;step=step+1;M(step,1)=t;M(step,cc)=-Gz1; kz=kz+M(step,cc)*dtz1/4;
     t+=dtz1/2;step=step+1;M(step,1)=t; kz=kz+M(step-1,cc)*dtz1/4;//TA
     //cout<<"End of slice selection and dephasing TA= "<<t<<endl;
     //cout<<"kz= "<<kz<<endl;
     ////////////////dephase
     t=TRF+TB;step=step+1;M(step,1)=t;//TB
     //cout<<"Begining of the Gy gradient TB= "<<t<<endl;
     t+=dty1/2;step=step+1;M(step,1)=t;M(step,bb)=-Gy1;ky=ky+M(step,bb)*dty1/4;
     t+=dty1/2;step=step+1;M(step,1)=t;  ky=ky+M(step-1,bb)*dty1/4;//TC
     //cout<<"End of Gy and begining of Gx, TC= "<<t<<endl;
     t+=dtx1/2;step=step+1;M(step,1)=t;M(step,aa)=-Gx1; kx=kx+M(step,aa)*dtx1/4;
     t+=dtx1/2;step=step+1;M(step,1)=t; kx=kx+M(step-1,aa)*dtx1/4;//TD
     //cout<<"Begining of the read-out TD= "<<t<<endl;
     ////////////////readout
     for (int c=1;c<=resY;c++){
       if (c==1){
	 t+=dt;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=Gx;kx=kx+M(step,aa)*dt/2; readstep=readstep+1;
	 //  cout<<"The first point in the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; 
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }      
       else { 
         t+=dty/2;step=step+1;M(step,1)=t;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*dty*tana/2;ky=ky+dty*M(step-1,bb)/4;kx=kx+M(step,aa)*dty/4;
         t+=dt-dty/2;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*Gx; kx=kx+(M(step,aa)+M(step-1,aa))*(dt-dty/2)/2;readstep=readstep+1;
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }
       for (int d=1;d<=resX-1;d++){
         t+=dtx;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=MISCMATHS::pow(-1.0f,(double) c+1)*Gx;kx=kx+M(step,aa)*dtx;readstep=readstep+1;
         //if (c==(resY/2+1) && d==(resX/2)){
           //cout<<"Time at the center of the k-space is TE= "<<t<<"Expected time is TE= "<<TE<<endl;
           //cout<<"The center of the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; //TE
	 //}
         coord(1,readstep)=kx*gammabar;
         coord(2,readstep)=ky*gammabar;
       }
       if (c==resY){
         t+=dt;step=step+1;M(step,1)=t;kx=kx+M(step-1,aa)*dt/2;//TF
       }
       else {
         t+=dt-dty/2;step=step+1;M(step,1)=t;M(step,aa)=MISCMATHS::pow(-1.0f, (double) c+1)*dty*tana/2;kx=kx+(M(step-1,aa)+M(step,aa))*(dt-dty/2)/2;
         t+=dty/2;step=step+1;M(step,1)=t;M(step,bb)=Gy;ky=ky+dty*M(step,bb)/4;kx=kx+M(step-1,aa)*dty/4;//TG
       }
     }
     /////////////////Crushers
     t+=rt;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=tcrush;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=rt;step=step+1;M(step,1)=t;
     t=(double) (TRslc*b+(a-1)*TR);step=step+1;M(step,1)=t;
   }
   t=(double) (TR*a);step=step+1;M(step,1)=t;
 }

 if (opt_kcoord.set()) write_binary_matrix(coord,"kcoord_"+opt_pulse.value());
 
 M=M.Rows(1,step);
 return M;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
Matrix gradecho(const int n,const RowVector zc,const int ns,const double ddz,const string slicedir, const string phasedir, const string readdir, const int resX,const int resY){
 // Input parameters: n = number of volumes, ns = number of slices (per volume)
 //   ddz = slice thickness (m) , zc = coordinate of slice centre (m)
      //EPISEQUENCE MATRIX INPUTFILE
 //(1)=time in s,(2)=rf angle,(3)=rf frequency bandwidth df(Hz),(4)=rf center frequency fc(Hz),(5)=readout 1/0 (6)=x gradient(T/m),(7)=y gradient(T/m),(8)=z gradient(T/m)
 /////////////////////////////////
 //SLICE DIRECTION
 ////////////////////////////////
 int aa=6;
 int bb=7;
 int cc=8;
 int bhelp=0;
 int simdir=1;
 if (slicedir=="z-" || slicedir=="z+") cc=8;
 if (slicedir=="y-" || slicedir=="y+") cc=7;
 if (slicedir=="x-" || slicedir=="x+") cc=6;
 
 if (phasedir=="z") bb=8;
 if (phasedir=="y") bb=7;
 if (phasedir=="x") bb=6;

 if (readdir=="z") aa=8;
 if (readdir=="y") aa=7;
 if (readdir=="x") aa=6;

 if (slicedir=="x-" || slicedir=="y-" || slicedir=="z-"){
   bhelp=ns+1; 
   simdir=1;
 }
 /////////////////////////////
 //INPUT PARAMETERS
 ////////////////////////////
 float angle=opt_angle.value();
 float angle_rad=angle*M_PI/180;
 float bw=opt_BWrec.value();//BW
 float TE=opt_TE.value();
 float TR=opt_TR.value();
 float TRline=opt_TRslc.value();
 if (TRline*resY*ns>TR){
   cout<<"TR and TRline and numslc do not agree"<<endl;//in general in gradecho TR is the time between the RF pulses so TRline is what actually is being considered to be TR. I am just trying to make it here consistant with the episeq generator and am calleing themlike this. TR here is the time that takes for generation of one volume!!
   exit(EXIT_FAILURE);
 }
 float rt=opt_risetime.value();
 float maxG=opt_maxG.value();
 float dx=opt_dx.value();
 float dy=opt_dy.value();
 double tana=maxG/rt;
 ///////////////////////////
 //SLICE SELECTION
 ///////////////////////////
 double Gz=7.128*1e-03;//z gradient during rf excitation (in T/m)
 double dtz=0.040731429*0.001;
 double rft=4*0.001;
 double dtz1=sqrt(Gz*(dtz+rft/2)/tana);
 double Gz1=dtz1*tana/2;
 double TA=rft/2+dtz+dtz1;//when all rf stuff ends
 double df=ddz*gammabar*Gz;//frequency width (in m*Hz/T*T/m=Hz),
 //cout<<"frequency width     "<<df<<endl;
 double t=0;//time (in ms)
 //double offset=0;//for now (in m)
 int step=1;//row number in the matrix
 RowVector fc=zc*gammabar*Gz;
 cout<<"Vector of centers of slices is (m) "<<zc<<endl;
 cout<<"Vector of corresponding frequences is (Hz) "<<fc<<endl;//vector of centers of frequences (Hz)
 ///////////////////////////
 //READ OUT
 ///////////////////////////
 double dtx=1/bw;//sampling time along the readout direction
 double dkx=1/(resX*dx);//dimensions of sampling distances in the k-space
 double dky=1/(resY*dy);
 double kx,ky;//for calculating the positions in the k-space by summing the areas.
 double Gx=dkx/(gammabar*dtx); //coming from dtx*gammabar*Gx=dkx
 double dt=Gx/tana;
 double dty=sqrt(4*dky/(gammabar*tana));
 //double Gy=dty*tana/2;
 double dtx1=sqrt(Gx*(dt+resX*dtx)*2/tana);
 double Gx1=dtx1*tana/2;
 double TEl=dt+resX/2*dtx;//TE is on the (resX/2+1,resY/2+1)point in the k-space
 double TEr=dt+(resX/2-1)*dtx;
 double dty1=sqrt(resY/2)*dty;//calculate dty1 and Gy1
 double Gy1=dty1*tana/2;
 double TD=TE-TEl;
 double TC=TD-dtx1;
 double TB=TC-dty1;
 double TF=TE+TEr;
 double tcrush=100*rt;
 double TG=TF+2*rt+tcrush;//100rt is how much is needed to make ggx ggy and ggz become 100 times larger which will reduce the signal 100 times -----  still in process of testing dec 20
 cout<<"Times in the acquisition of one slice starting from the RF pulse (in s)"<<endl;
 cout<<"the slice selection gradient is done TA="<<TA<<endl;
 cout<<"the phase encode gradient starts dephasing TB="<<TB<<endl;
 cout<<"the phase encode gradient stops dephasing, and the read-out gradient starts dephasing TC="<<TC<<endl;
 cout<<"the read-out gradient stops dephasing,begining of the read-out period TD="<<TD<<endl;
 cout<<"echo time TE="<<TE<<endl;
 cout<<"end of the read-out period and begining of the crushers TF="<<TF<<endl;
 cout<<"end of the crushers TG="<<TG<<endl;
 cout<<"end of the acquisition of one slice TRslc="<<TRline<<endl;
 if (TD<0 || TB<0 || TA<0 || TC<0 || TF>TRline || TG>TRline){
  cout<<"WARNING:TE is not long enough to accomodate for the resX, resY, and the BW"<<endl;
  exit(EXIT_FAILURE);
 }

 ////////////////////////////
 //MAIN LOOP
 ///////////////////////////
 int nreadp=n*ns*resX*resY;
 int readstep=0;
 Matrix M=zeros(n*ns*resX*resY*2,8);//main matrix: 
 Matrix coord(2,nreadp); //matrix for the kspace coordinates, 1st raw for kx and 2nd for ky
 for (int a=1;a<=n;a++){
   for (int b=1;b<=ns;b++){
     for (int c=1;c<=resY;c++){
       kx=0;ky=0;
       ////////////////////////////////////////
       //making matrix to save the coordinates
       ////////////////////////////////////////
       ////////////////slice selection //have to work more on this, quite primitive at the moment
       t=t+dtz;step=step+1;M(step,1)=t;M(step,cc)=Gz;
       t=t+rft/2;step=step+1;M(step,1)=t;M(step,2)=angle_rad;M(step,3)=df;M(step,4)=fc(bhelp + simdir*b);M(step,cc)=Gz;//TRF
       float TRF=t;//cout<<"The time when the RF pulse happenes for the VolNum "<<a<<" and the SlcNum "<<b<<" is TRF="<<TRF<<endl;
       t=t+rft/2;step=step+1;M(step,1)=t;M(step,cc)=Gz;
       t=t+dtz;step=step+1;M(step,1)=t;
       t=t+dtz1/2;step=step+1;M(step,1)=t;M(step,cc)=-Gz1;
       t=t+dtz1/2;step=step+1;M(step,1)=t;//TA
       ////////////////dephase
       dty1=sqrt(fabs(((float)(resY))/2-c+1))*dty;//calculate dty1 and Gy1
       Gy1=sign(resY/2-c+1)*dty1*tana/2;
       TB=TC-dty1;
       t=TRF+TB;step=step+1;M(step,1)=t;//TB
       t=t+dty1/2;step=step+1;M(step,1)=t;M(step,bb)=-Gy1;ky=ky+M(step,bb)*dty1/4;
       t=t+dty1/2;step=step+1;M(step,1)=t;  ky=ky+M(step-1,bb)*dty1/4;//TC
       t=t+dtx1/2;step=step+1;M(step,1)=t;M(step,aa)=-Gx1; kx=kx+M(step,aa)*dtx1/4;
       t=t+dtx1/2;step=step+1;M(step,1)=t; kx=kx+M(step-1,aa)*dtx1/4;//TD
       ////////////////readout
       t=t+dt;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=Gx;kx=kx+Gx*dt/2; readstep=readstep+1;
       //cout<<"The first point in the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; 
       coord(1,readstep)=kx*gammabar;
       coord(2,readstep)=ky*gammabar;
       for (int d=1;d<=resX-1;d++){
         t=t+dtx;step=step+1;M(step,1)=t;M(step,5)=1;M(step,aa)=Gx;kx=kx+M(step,aa)*dtx;readstep=readstep+1;
	 //if (c==1)cout<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl;
         //if (d==1)cout<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl;
         //if (c==(resY/2+1) && d==(resX/2))cout<<"The center of the k-space for the VolNum "<<a<<" and the SlcNum "<<b<<" is (kx,ky)="<<"("<<kx*gammabar<<","<<ky*gammabar<<")"<<endl; //TE
	 coord(1,readstep)=kx*gammabar;
  	 coord(2,readstep)=ky*gammabar;
       }
       t=t+dt;step=step+1;M(step,1)=t;kx=kx+Gx*dt/2;//TF
     /////////////////Crushers
     t+=rt;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=tcrush;step=step+1;M(step,1)=t;M(step,aa)=maxG;M(step,bb)=maxG;M(step,cc)=maxG;
     t+=rt;step=step+1;M(step,1)=t;
     t=TRline*c+TRline*resY*(b-1)+TR*(a-1);step=step+1;M(step,1)=t;
     }
   t=TRline*resY*b+TR*(a-1);step=step+1;M(step,1)=t;step=step+1;M(step,1)=t;
   }
 t=TR*a;step=step+1;M(step,1)=t;step=step+1;M(step,1)=t;
 }
 //if (opt_kcoord.set()) write_binary_matrix(coord,"kcoord_"+opt_pulse.value() );
 //Matrix M1(step,8);
 M=M.Rows(1,step);
 return M;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int compute_volume(){
  cout<<"Generating the pulse sequence..."<<endl; 
  //////////////////////////////////////////////////////////////////////////
  // READ IN THE OBJECT (BRAIN)                                           //
  //////////////////////////////////////////////////////////////////////////
  volume4D<double> phantom;//consists of gry,wht,csf,fat,mus,con,gli,skn (in that order)
  read_volume4D(phantom,opt_object.value());
  int Nx=phantom.xsize();double xdim=phantom.xdim()*1e-03; 
  int Ny=phantom.ysize();double ydim=phantom.ydim()*1e-03;
  int Nz=phantom.zsize();double zdim=phantom.zdim()*1e-03;
  int Nzz=Nz;double zzdim=zdim;
  print_volume_info(phantom,"object");
  //////////////////////////////////////////////////////////////////
  RowVector posx(Nx); 
  RowVector posy(Ny); 
  RowVector posz(Nz);
  /////////////////////////////////////////////////////////////////////////////
  // SET UP COORDINATE SYSTEM WITH THE CENTER IN THE CENTER OF THE OBJECT    //
  /////////////////////////////////////////////////////////////////////////////
  double xxx=-(Nx-1)/2.0;
  for (int m=1;m<=Nx;m++){
    posx(m)=xxx*xdim;
    xxx=xxx+1;
  }
  double yyy=-(Ny-1)/2.0;
  for (int m=1;m<=Ny;m++){
    posy(m)=yyy*ydim;
    yyy=yyy+1;
  }
  double zzz=-(Nz-1)/2.0;
  for (int m=1;m<=Nz;m++){
    posz(m)=zzz*zdim;
    zzz=zzz+1;
  }
  write_ascii_matrix(posx,opt_pulse.value()+".posx");
  write_ascii_matrix(posy,opt_pulse.value()+".posy");
  write_ascii_matrix(posz,opt_pulse.value()+".posz");
  
  ////////////////////////////////////////////////////////////////////////
  // PULSE SEQUENCE                                                     //
  ////////////////////////////////////////////////////////////////////////
  int ns=opt_numslc.value();
  int n= opt_numvol.value();
  double slcthk=opt_slcthk.value();//slcthk (m)
  int resX=opt_Nx.value();
  int resY=opt_Ny.value();
  double gap=opt_gap.value();
  Matrix pulse;
  //////////////////////////////////////////////////////////////////////////
  // SET UP THE VECTOR OF CENTRES OF SLICES                               //
  //////////////////////////////////////////////////////////////////////////
  string slcdir=opt_slcdir.value();
  string phasedir=opt_phasedir.value();
  string readdir=opt_readdir.value();
  if (slcdir==phasedir || slcdir==readdir || readdir==phasedir){
   cout<<"WARNING: The same gradients used for different directions in the k-space!!"<<endl;
   exit(EXIT_FAILURE);
  }
  int phasedir_int=2;
  int readdir_int=3;
  if (phasedir=="x") phasedir_int=3;
  if (phasedir=="y") phasedir_int=2;
  if (phasedir=="z") phasedir_int=1;
  if (readdir=="x") readdir_int=3;
  if (readdir=="y") readdir_int=2;
  if (readdir=="z") readdir_int=1;

  RowVector poszz;
  int slcdir_int=1;
  if (slcdir=="z+"){
    Nzz=Nz;
    zzdim=zdim;
    poszz=posz;
    slcdir_int=1;
  }
  if (slcdir=="z-"){
    Nzz=Nz;
    zzdim=zdim;
    poszz=posz;
    slcdir_int=-1;
  }
  if (slcdir=="y+"){
    Nzz=Ny;
    zzdim=ydim;
    poszz=posy;
    slcdir_int=2;
  }
  if (slcdir=="y-"){
    Nzz=Ny;
    zzdim=ydim;
    poszz=posy;
    slcdir_int=-2;
  }
  if (slcdir=="x+"){
    Nzz=Nx;
    zzdim=xdim;
    poszz=posx;
    slcdir_int=3;
  }
  if (slcdir=="x-"){
    Nzz=Nx;
    zzdim=xdim;
    poszz=posx;
    slcdir_int=-3;
  }
  double zstar=(double)(opt_zstart.value());
  double nvox=(double)(1/zzdim);//number of voxels per 1m
  int zstart=(int)(zstar*nvox+0.0001);//0.0001 is just a fix so that it rounds it properly
  cout<<"Number of voxels per 1m in the input object is "<<nvox<<endl;
  RowVector zc(ns);//the vector of centers of slices
  double ss=poszz(zstart+1)-zzdim/2+slcthk/2;
  cout<<"poszz"<<poszz<<endl;
  if (ss>poszz(Nzz)){ 
      cout<<"WARNING: the center of your slice excides the size of the object, i.e. ss>poszz(Nzz)"<<endl;
      cout.precision(10);
      cout<<"ss= "<<ss<<"; poszz(Nzz)= "<<poszz(Nzz)<<endl;
  }
  for (int m=1;m<=ns;m++){
    zc(m)=ss;
    cout<<"Center of slices"<<ss<<"; Slice thickness "<<slcthk<<"; Gap "<<gap<<endl;
    ss=ss+slcthk+gap;
  }
  int seqnum=0;
  if (opt_seq.value()=="epi") {
    seqnum=1;
    pulse=episequence(n,zc,ns,slcthk,slcdir,phasedir,readdir,resX,resY);
    write_binary_matrix(pulse,opt_pulse.value());
  }
  if (opt_seq.value()=="ge"){
    seqnum=2;
    pulse=gradecho(n,zc,ns,slcthk,slcdir,phasedir,readdir,resX,resY);
    write_binary_matrix(pulse,opt_pulse.value());
  } 
  if (opt_seq.value()=="partial"){
    seqnum=3;
    pulse=partialepisequence(n,zc,ns,slcthk,slcdir,phasedir,readdir,resX,resY);
    write_binary_matrix(pulse,opt_pulse.value());
  }
  RowVector pulseinfo(20);
  pulseinfo(1)=seqnum;//0 is for none (i.e. if pulse seq is off the scanner or smth), 1 is for epi made by pulse.cc, 2 is for ge made by pulse.cc
  pulseinfo(2)=opt_TE.value();
  pulseinfo(3)=opt_TR.value();
  pulseinfo(4)=opt_TRslc.value();
  pulseinfo(5)=opt_Nx.value();
  pulseinfo(6)=opt_Ny.value();
  pulseinfo(7)=opt_dx.value();
  pulseinfo(8)=opt_dy.value();
  pulseinfo(9)=opt_maxG.value();
  pulseinfo(10)=opt_risetime.value();
  pulseinfo(11)=opt_BWrec.value();
  pulseinfo(12)=opt_numvol.value();
  pulseinfo(13)=opt_numslc.value();
  pulseinfo(14)=opt_slcthk.value();
  pulseinfo(15)=slcdir_int;
  pulseinfo(16)=opt_gap.value();
  pulseinfo(17)=opt_zstart.value();
  pulseinfo(18)=opt_angle.value();
  pulseinfo(19)=phasedir_int;
  pulseinfo(20)=readdir_int;
  write_ascii_matrix(pulseinfo,opt_pulse.value()+".info");
  string filename=opt_pulse.value()+".readme";
  ofstream pulsetext(filename.c_str());
    if (!pulsetext) { 
      cerr << "Could not open file " << filename << " for writing" << endl;
      return -1;
    }
  pulsetext.setf(ios::scientific | ios::showpos); 
  pulsetext << "SeqType = " << opt_seq.value() << endl;
  pulsetext << "TE = " << opt_TE.value() <<"(s)"<< endl;
  pulsetext << "TR = " << opt_TR.value() <<"(s)"<< endl;
  pulsetext << "TRslc = " << opt_TRslc.value() <<"(s)"<< endl;
  pulsetext << "Nread = " << opt_Nx.value() << endl;
  pulsetext << "Nphase = " << opt_Ny.value() <<endl;
  pulsetext << "dread = " << opt_dx.value() <<"(m)"<<endl;
  pulsetext << "dphase = " << opt_dy.value() <<"(m)"<<endl;
  pulsetext << "maxG = " << opt_maxG.value() <<"(T/m)"<<endl;
  pulsetext << "RiseT = " << opt_risetime.value() <<"(s)"<<endl;
  pulsetext << "BWrec = " << opt_BWrec.value() <<"(Hz)"<<endl;
  pulsetext << "Nvol = " << opt_numvol.value() <<endl;
  pulsetext << "Nslc = " << opt_numslc.value() <<endl;
  pulsetext << "SlcThk = " << opt_slcthk.value() <<"(m)"<<endl;
  pulsetext << "SlcDir = " << opt_slcdir.value() <<endl;
  pulsetext << "Gap = " << opt_gap.value() <<"(m)"<<endl;
  pulsetext << "zstart = " << opt_zstart.value() <<"(m)"<<endl;
  pulsetext << "FlipAngle = " << opt_angle.value() << "(degrees)"<< endl;
  pulsetext << "PhaseDir = " << opt_phasedir.value() <<endl;
  pulsetext << "ReadDir = " << opt_readdir.value() <<endl;
  pulsetext << endl;
  pulsetext.close();
 return 0;
}

int main (int argc, char *argv[]){

  Tracer tr("main");
  OptionParser options(title, examples);

  try {
    options.add(opt_seq);
    options.add(opt_object);
    options.add(opt_pulse);
    options.add(opt_angle);
    options.add(opt_TE);
    options.add(opt_TR);
    options.add(opt_TRslc);
    options.add(opt_Nx);
    options.add(opt_Ny);
    options.add(opt_dx);
    options.add(opt_dy);
    options.add(opt_maxG);
    options.add(opt_risetime);
    options.add(opt_BWrec);
    options.add(opt_numvol);
    options.add(opt_numslc);
    options.add(opt_slcthk);
    options.add(opt_gap);
    options.add(opt_zstart);
    options.add(opt_slcdir);
    options.add(opt_phasedir);
    options.add(opt_readdir);
    options.add(verbose);
    options.add(help);
    options.add(opt_kcoord);
    
    nonoptarg = options.parse_command_line(argc, argv);

    // line below stops the program if there are less than 2 non-optional args
    //   or the help was requested or a compulsory option was not set
    if ( (help.value()) || (!options.check_compulsory_arguments(true)) )
      {
	options.usage();
	exit(EXIT_FAILURE);
      }
    
  }  catch(X_OptionError& e) {
    options.usage();
    cerr << endl << e.what() << endl;
    exit(EXIT_FAILURE);
  } catch(std::exception &e) {
    cerr << e.what() << endl;
  } 

  // Call the local functions

  compute_volume();
  return 0;
}
