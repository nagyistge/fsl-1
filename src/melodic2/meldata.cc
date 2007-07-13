/*  MELODIC - Multivariate exploratory linear optimized decomposition into 
              independent components
    
    meldata.cc - data handler / container class

    Christian F. Beckmann, FMRIB Image Analysis Group
    
    Copyright (C) 1999-2004 University of Oxford */

// {{{  includes/namespaces

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

#include "newimage/newimageall.h"
#include "meloptions.h"
#include "meldata.h"
#include "melodic.h" 
#include "utils/log.h"
#include <time.h>
#include "miscmaths/miscprob.h"
#include "melhlprfns.h" 

using namespace Utilities;
using namespace NEWIMAGE;

// }}}
 
namespace Melodic{
// {{{ Setup

  Matrix MelodicData::process_file(string fname, int numfiles)
  {
    volume4D<float> RawData;

    //read data
    message("Reading data file " << fname << "  ... ");
    read_volume4D(RawData,fname);
    message(" done" << endl);
    del_vols(RawData,opts.dummy.value());
    
    Mean += meanvol(RawData)/numfiles;
	
    //reshape
    Matrix tmpData;
    tmpData = RawData.matrix(Mask);
    
    //estimate smoothness
    if(Resels == 0)
      Resels = est_resels(RawData,Mask);
        
    //update mask
    if(opts.update_mask.value()){
      message("  Excluding voxels with constant value ...");
      update_mask(Mask, tmpData);
      message(" done" << endl);
    }

    //convert to percent BOLD signal change
    if(opts.pbsc.value()){
      message("  Converting data to percent BOLD signal change ...");
      Matrix meanimg = convert_to_pbsc(tmpData);
      meanR = meanimg.Row(1);
      message(" done" << endl);
    }
    else{
      message(string("  Removing mean image ..."));
      meanR = mean(tmpData);
      tmpData = remmean(tmpData);
      message(" done" << endl);
    }

    //convert to power spectra
    if(opts.pspec.value()){
      message("  Converting data to powerspectra ...");
      tmpData = calc_FFT(tmpData);
      message(" done" << endl);
    }
	
    meanC = mean(tmpData,2);

    //switch dimension in case temporal ICA is required
    if(opts.temporal.value()){
      message(string("  Switching dimensions for temporal ICA") << endl);
      tmpData = tmpData.t();
      Matrix tmp;
      tmp = meanC;
      meanC = meanR.t();
      meanR = tmp.t();
      message("  Data size : " << Data.Nrows() << " x " << Data.Ncols() <<endl);
    }
      
    //variance - normalisation
    if(opts.varnorm.value()){
      message("  Normalising by voxel-wise variance ..."); 
      stdDev = varnorm(tmpData,tmpData.Nrows(),3.1);
      stdDevi = pow(stdDev,-1); 
      message(" done" << endl);
    }

    return tmpData;
  }

  Matrix MelodicData::expand_mix()
  {
    Matrix out;
    out = expand_dimred(mixMatrix);
    return out;
  }

  Matrix MelodicData::expand_dimred(const Matrix& Mat)
  {
    int first, last;
    first = 1;
    last = DWM.at(0).Ncols();
    Matrix tmp = DWM.at(0) * Mat.Rows(first,last);
    for(unsigned int ctr = 1; ctr < DWM.size(); ctr++){
      first = last + 1;
      last += DWM.at(ctr).Ncols();
      tmp &= DWM.at(ctr) * Mat.Rows(first, last);
    }
    return tmp;
  }

  Matrix MelodicData::reduce_dimred(const Matrix& Mat)
  {
    int first, last;
    first = 1;
    last = WM.at(0).Ncols();
    Matrix tmp = WM.at(0) * Mat.Rows(first,last);
    for(unsigned int ctr = 1; ctr < WM.size(); ctr++){
      first = last + 1;
      last += WM.at(ctr).Ncols();
      tmp &= WM.at(ctr) * Mat.Rows(first, last);
    }
    return tmp;
  }

  void MelodicData::set_TSmode()
  {
    message("Calculating T- and S-modes " << endl << endl);
    Matrix tmp, tmpT, tmpS, tmpT2, tmpS2;
    tmp = expand_dimred(mixMatrix);
    tmpT = zeros(tmp.Nrows()/numfiles, tmp.Ncols());
    tmpS = zeros(numfiles, tmp.Ncols());
    krfact(tmp,tmpT,tmpS);
    for(int ctr = 1; ctr <= tmp.Ncols(); ctr++){
      tmpT2 << tmpT.Column(ctr);
      tmpS2 << tmpS.Column(ctr);
      add_Tmodes(tmpT2);
      add_Smodes(tmpS2);
    }
  }

  void MelodicData::setup()
  { 
    setup_misc();
    numfiles = (int)opts.inputfname.value().size();
    if((numfiles > 1) && (opts.approach.value()==string("defl") || opts.approach.value()==string("symm")))
      opts.approach.set_T("tica");

    Matrix alldat, tmpData;
    alldat = process_file(opts.inputfname.value().at(0), numfiles) / numfiles;
    for(int ctr = 1; ctr < numfiles; ctr++){
      tmpData = process_file(opts.inputfname.value().at(ctr), numfiles);
      alldat += tmpData / numfiles;
    }

    message(endl << "Initial data size : "<<alldat.Nrows()<<" x "<<alldat.Ncols()<<endl<<endl);

    //estimate model order
    ColumnVector PPCA;
    RowVector AdjEV, PercEV;
    Matrix Corr, tmpE;
    int order;

    if(opts.pca_dim.value() == 0){
      order = ppca_dim(alldat, RXweight, PPCA, AdjEV, PercEV, Corr, pcaE, pcaD, Resels, opts.pca_est.value());	  
      calc_white(pcaE, pcaD, order, whiteMatrix, dewhiteMatrix);
   
      opts.pca_dim.set_T(order);
    }
    else{
      order = opts.pca_dim.value();
      std_pca(tmpData, RXweight, Corr, pcaE, pcaD);
      calc_white(pcaE, pcaD, order, whiteMatrix, dewhiteMatrix);
    }  
      
    if(numfiles < 2){
      Data = alldat;
      Matrix tmp = Identity(Data.Nrows());
      DWM.push_back(tmp);
      WM.push_back(tmp);
    } else {
      for(int ctr = 0; ctr < numfiles; ctr++){
	tmpData = process_file(opts.inputfname.value().at(ctr), numfiles);
	
	//  whiten (separate / joint) 
	if(!opts.joined_whiten.value()){	  
      	  std_pca(tmpData, RXweight, Corr, pcaE, pcaD);
	  calc_white(pcaE, pcaD, order, whiteMatrix, dewhiteMatrix);
	}
	tmpData = whiteMatrix * tmpData;
	DWM.push_back(dewhiteMatrix);
	WM.push_back(whiteMatrix);

	//concatenate Data
	if(Data.Storage() == 0)
	  Data = tmpData;
	else
	  Data &= tmpData;
      }
    }

    message("  Data size : "<<Data.Nrows()<<" x "<<Data.Ncols()<<endl);
    
    /*    {//remove row mean
      if(opts.temporal.value()){
	message(string("Removing mean image ... "));
      }else{
	message(string("Removing mean time course ... "));
	}*/
      meanC=mean(Data,2);
      /*  Data=remmean(Data,2); 
      message("done"<<endl);
      }*/
        
    //save the mean & mask
    save_volume(Mask,logger.appendDir("mask"));
    save_volume(Mean,logger.appendDir("mean"));
    
  } // void setup()
// }}}

  void MelodicData::setup_misc()
  {

    //initialize Mean
    read_volume(Mean,opts.inputfname.value().at(0),tempInfo);

    //save first image
    tmpnam(Mean_fname); // generate a tmp name
    save_volume(Mean,Mean_fname);    

    //create mask
    create_mask(Mask);

    // clean /tmp
    char callRMstr[1000];
    ostrstream osc(callRMstr,1000);
    osc  << "rm " << string(Mean_fname) <<"*  " << '\0';
    system(callRMstr);
 
    if(!samesize(Mean,Mask)){
      cerr << "ERROR:: mask and data have different dimensions  \n\n";
      exit(2);
    }

    //reset mean
    Mean *= 0;
     
    //set up weighting
    if(opts.segment.value().length()>0){
      create_RXweight();
    }

    //seed the random number generator
    double tmptime = time(NULL);
    srand((unsigned int) tmptime);
  }



// {{{ Save
  void MelodicData::save()
  {   

    //check for temporal ICA
    if(opts.temporal.value()){
      message(string("temporal ICA: transform back the data ... "));
      Matrix tmpIC = mixMatrix.t();
      mixMatrix=IC.t();
      IC=tmpIC;

      unmixMatrix=pinv(mixMatrix);
      Data = Data.t();
      tmpIC = meanC;
      meanC = meanR.t();
      meanR = tmpIC.t();
      //  whiteMatrix = whiteMatrix.t;
      //  dewhiteMatrix = dewhiteMatrix.t();
      message(string("done") << endl);
      opts.temporal.set_T(false); // Do not switch again!
    }
 
    message(endl << "Writing results to : " << endl);

    //Output IC	
    if((IC.Storage()>0)&&(opts.output_origIC.value())&&(after_mm==false))
      save4D(IC,opts.outputfname.value() + "_oIC");
      

    //Output IC -- adjusted for noise	
      if(IC.Storage()>0){
	volume4D<float> tempVol;	
   
	//Matrix ICadjust;
	if(after_mm){
	  save4D(IC,opts.outputfname.value() + "_IC");
	  // ICadjust = IC;
	}	else{
	  stdNoisei = pow(stdev(Data - mixMatrix * IC)*std::sqrt((float)(Data.Nrows()-1))/
			  std::sqrt((float)(Data.Nrows()-IC.Nrows())),-1);
	  
	  ColumnVector diagvals;
	  diagvals=pow(diag(unmixMatrix*unmixMatrix.t()),-0.5);
	  save4D(SP(IC,diagvals*stdNoisei),opts.outputfname.value() + "_IC");
	}

	//	tempVol.setmatrix(ICadjust,Mask);
	//strncpy(tempInfo.header.hist.aux_file,"render3",24);
	//save_volume4D(tempVol,logger.appendDir(opts.outputfname.value() 
	//				       + "_IC"),tempInfo);
	//message("  " << logger.appendDir(opts.outputfname.value() + "_IC") <<endl);
	
	if(opts.output_origIC.value())
	  save4D(stdNoisei,string("Noise_stddev_inv"));
      }

      //Output T- & S-modes
      save_Tmodes();
      save_Smodes();

    //Output mixMatrix
    if(mixMatrix.Storage()>0){
      saveascii(mixMatrix, opts.outputfname.value() + "_mix");
      mixFFT=calc_FFT(mixMatrix, opts.logPower.value());
      saveascii(mixFFT,opts.outputfname.value() + "_FTmix");      
    }

    //Output ICstats
    if(ICstats.Storage()>0)
      saveascii(ICstats,opts.outputfname.value() + "_ICstats"); 
      

    //Output unmixMatrix
    if(opts.output_unmix.value() && unmixMatrix.Storage()>0)
      saveascii(unmixMatrix,opts.outputfname.value() + "_unmix");

    //Output Mask
    message("  "<< logger.appendDir("mask") <<endl);

    //Output mean
    if(opts.output_mean.value() && meanC.Storage()>0 && meanR.Storage()>0){
      saveascii(meanR,opts.outputfname.value() + "_meanR");
      saveascii(meanC,opts.outputfname.value() + "_meanC");
    }

    //Output white
    if(opts.output_white.value() && whiteMatrix.Storage()>0&&
       dewhiteMatrix.Storage()>0){
      saveascii(whiteMatrix,opts.outputfname.value() + "_white");
      saveascii(dewhiteMatrix,opts.outputfname.value() + "_dewhite");
      Matrix tmp;
      tmp=calc_FFT(dewhiteMatrix, opts.logPower.value());
      saveascii(tmp,opts.outputfname.value() + "_FTdewhite");
    }

    //Output PCA
    if(opts.output_pca.value() && pcaD.Storage()>0&&pcaE.Storage()>0){
      saveascii(pcaE,opts.outputfname.value() + "_pcaE");
      saveascii((Matrix) diag(pcaD),opts.outputfname.value() + "_pcaD");
      
      Matrix PCAmaps;
      if(whiteMatrix.Ncols()==Data.Ncols()){
	PCAmaps = dewhiteMatrix.t();
      }else
	PCAmaps = whiteMatrix * Data;

      save4D(PCAmaps,opts.outputfname.value() + "_pca");
     
    }
  } //void save()
// }}}

// {{{ remove_components  
  int MelodicData::remove_components()
  {  
    message("Reading " << opts.filtermix.value() << endl) 
    mixMatrix = read_ascii_matrix(opts.filtermix.value());
    if (mixMatrix.Storage()<=0) {
      cerr <<" Please specify the mixing matrix correctly" << endl;
      exit(2);
    }
    
    unmixMatrix = pinv(mixMatrix);
    IC = unmixMatrix * Data;

    string tmpstr;
    tmpstr = opts.filter.value();

    Matrix noiseMix;
    Matrix noiseIC;

    int ctr=0;    
    char *p;
    char t[1024];
    const char *discard = ", [];{(})abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ~!@#$%^&*_-=+|\':><./?";
    
    message("Filtering the data...");
    strcpy(t, tmpstr.c_str());
    p=strtok(t,discard);
    ctr = atoi(p);
    if(ctr>0 && ctr<=mixMatrix.Ncols()){
      message(" "<< ctr );
      noiseMix = mixMatrix.Column(ctr);
      noiseIC  = IC.Row(ctr).t();    
    }else{
      cerr << endl<< "component number "<<ctr<<" does not exist" << endl;
    }
    
    do{
      p=strtok(NULL,discard);
      if(p){
	ctr = atoi(p);
	
        if(ctr>0 && ctr<=mixMatrix.Ncols()){
	  message(" "<<ctr);
	  noiseMix |= mixMatrix.Column(ctr);
	  noiseIC  |= IC.Row(ctr).t();
	}
	else{
	  cerr << endl<< "component number "<<ctr<<" does not exist" << endl;
	}
      }
    }while(p);
    message(endl);
    Matrix newData;
    newData = Data - noiseMix * noiseIC.t();

    //cerr << newData.Nrows() << " x " << newData.Ncols() << endl;
    //cerr << meanC.Nrows() << " x " << meanC.Ncols() << endl;
    //cerr << meanR.Nrows() << " x " << meanR.Ncols() << endl;
    newData = newData + meanC*ones(1,newData.Ncols());
    newData = newData + ones(newData.Nrows(),1)*meanR;
    
    volume4D<float> tmp;
    read_volume4D(tmp,opts.inputfname.value().at(0)); 
    tmp.setmatrix(newData,Mask);
    save_volume4D(tmp,logger.appendDir(opts.outputfname.value() + "_ICAfiltered")); 
   
    return 0;
  } // int remove_components()
// }}}
// {{{ create_RXweight
  void MelodicData::create_RXweight()
  {
    message("Reading the weights for the covariance R_X from file "<< opts.segment.value() << endl);
  
    volume4D<float> tmpRX;
    read_volume4D(tmpRX,opts.segment.value());
    RXweight = tmpRX.matrix(Mask);
  } 

  void MelodicData::est_smoothness(){
    if(Resels == 0){
      string SM_path = opts.binpath + "smoothest";
      string Mask_fname = logger.appendDir("mask");

      if(opts.segment.value().length()>0){
	Mask_fname =  opts.segment.value();
      } 

      // Setup external call to smoothest:
      char callSMOOTHESTstr[1000];
      ostrstream osc(callSMOOTHESTstr,1000);
      osc  << SM_path << " -d " << data_dim()
	   << " -r " << opts.inputfname.value().at(0) << " -m " 
	   << Mask_fname << " > " << logger.appendDir("smoothest") << '\0';
      
      message("  Calling Smoothest: " << callSMOOTHESTstr << endl);
      system(callSMOOTHESTstr);

      //read back the results
      ifstream in;
      string str;
      Resels = 1.0;
      
      in.open(logger.appendDir("smoothest").c_str(), ios::in);
      if(in>0){
	for(int ctr=1; ctr<7; ctr++){ in >> str;}
	in.close();
	if(str!="nan"){
	  Resels = atof(str.c_str());
	}
      }
    }
  }

  unsigned long MelodicData::standardise(volume<float>& mask, 
			    volume4D<float>& R)
  {
    unsigned long count = 0;
    int M=R.tsize();
    
    for (int z=mask.minz(); z<=mask.maxz(); z++) {
      for (int y=mask.miny(); y<=mask.maxy(); y++) {
	for (int x=mask.minx(); x<=mask.maxx(); x++) {
	  
	  if( mask(x,y,z) > 0.5) {
	    
	    count ++;
	  
	    if( M > 2 ) {
	      
	      // For each voxel 
	      //    calculate mean and standard deviation...
	      double Sx = 0.0, SSx = 0.0;
	      
	      for ( int t = 0; t < M; t++ ) {
		float R_it = R(x,y,z,t);
		
		Sx += R_it;
		SSx += (R_it)*(R_it);
	      }
	      
	      float mean = Sx / M;
	      float sdsq = (SSx - ((Sx)*(Sx) / M)) / (M - 1) ;
	      
	      if (sdsq<=0) {
		// trap for differences between mask and invalid data
		mask(x,y,z)=0;
		count--;
	      } else {
	      //    ... and use them to standardise to N(0, 1).
		for ( unsigned short t = 0; t < M; t++ ) {
		  R(x,y,z,t) = (R(x,y,z,t) - mean) / sqrt(sdsq);
		}
	      } 
	    }
	  }
	}
      }
    }  
    return count;
  }

  float MelodicData::est_resels(volume4D<float> R, volume<float> mask)
  {
    message("  Estimating data smoothness ... ");
    unsigned long mask_volume = standardise(mask, R);

    int dof = R.tsize();
    unsigned long N = mask_volume;

    // MJ additions to make it cope with 2D images
    bool usez = true;
    if (R.zsize() <= 1) { usez = false; }

    enum {X = 0, Y, Z};
    float SSminus[3] = {0, 0, 0}, S2[3] = {0, 0, 0};

    int zstart=1;
    if (!usez) zstart=0;
    for ( unsigned short z = zstart; z < R.zsize() ; z++ )
      for ( unsigned short y = 1; y < R.ysize() ; y++ )
	for ( unsigned short x = 1; x < R.xsize() ; x++ )
	  // Sum over N
	  if( (mask(x, y, z)>0.5) &&
	      (mask(x-1, y, z)>0.5) && 
	      (mask(x, y-1, z)>0.5) && 
	      ( (!usez) || (mask(x, y, z-1)>0.5) ) ) {
	    
	    N++;
	  
	    for ( unsigned short t = 0; t < R.tsize(); t++ ) {
	      // Sum over M
	      SSminus[X] += R(x, y, z, t) * R(x-1, y, z, t);
	      SSminus[Y] += R(x, y, z, t) * R(x, y-1, z, t);
	      if (usez) SSminus[Z] += R(x, y, z, t) * R(x, y, z-1, t);

	      S2[X] += 0.5 * (R(x, y, z, t)*R(x, y, z, t) + R(x-1, y, z, t)*R(x-1, y, z, t));
	      S2[Y] += 0.5 * (R(x, y, z, t)*R(x, y, z, t) + R(x, y-1, z, t)*R(x, y-1, z, t));
	      if (usez) S2[Z] += 0.5 * (R(x, y, z, t)*R(x, y, z, t) + R(x, y, z-1, t)*R(x, y, z-1, t));
	    }
	  }

    float norm = 1.0/(float) N;
    float v = dof;	// v - degrees of freedom (nu)  
    if(R.tsize() > 1) {
      norm = (v - 2) / ((v - 1) * N * R.tsize());
    }
 
    // for extreme smoothness 
    if (SSminus[X]>=0.99999999*S2[X]) 
      SSminus[X]=0.99999*S2[X];  
    if (SSminus[Y]>=0.99999999*S2[Y]) 
      SSminus[Y]=0.99999*S2[Y];
    if (usez) 
      if (SSminus[Z]>=0.99999999*S2[Z]) 
	SSminus[Z]=0.99999*S2[Z];
    // Convert to sigma squared
    float sigmasq[3] = {0,0,0};
    sigmasq[X] = -1.0 / (4 * log(fabs(SSminus[X]/S2[X])));
    sigmasq[Y] = -1.0 / (4 * log(fabs(SSminus[Y]/S2[Y])));
    if (usez) { sigmasq[Z] = -1.0 / (4 * log(fabs(SSminus[Z]/S2[Z]))); }
    
    // Convert to full width half maximum
    float FWHM[3] = {0,0,0};
    FWHM[X] = sqrt(8 * log(2) * sigmasq[X]);
    FWHM[Y] = sqrt(8 * log(2) * sigmasq[Y]);
    if (usez) { FWHM[Z] = sqrt(8 * log(2) * sigmasq[Z]); }
    float resels = FWHM[X] * FWHM[Y];
    if (usez) resels *= FWHM[Z];

    message(" done " <<endl);
    return resels;
  }
// }}}
// {{{ create_mask 
  void MelodicData::create_mask(volume<float>& theMask)
  {
    if(opts.use_mask.value() && opts.maskfname.value().size()>0){   // mask provided 
      read_volume(theMask,opts.maskfname.value());
      message("Mask provided : " << opts.maskfname.value()<<endl);
    }
    else{
      if(opts.perf_bet.value() && opts.use_mask.value()){ //use BET
	message("Create mask ... ");
	// set up all strings
	string BET_outputfname = string(Mean_fname)+"_brain";

	string BET_path = opts.binpath + "bet";
	string BET_optarg = "-m -f 0.4"; // see man bet
	string Mask_fname = BET_outputfname+"_mask";

	// Setup external call to BET:

	char callBETstr[1000];
	ostrstream osc(callBETstr,1000);
	osc  << BET_path << " " << Mean_fname << " " 
	     << BET_outputfname << " " << BET_optarg << " > /dev/null " << '\0';
	
        message("  Calling BET: " << callBETstr << endl);
	system(callBETstr);
	
	// read back the Mask file   
	read_volume(theMask,Mask_fname);

	message("done" << endl);
      }  
      else{
	if(opts.use_mask.value()){   //just threshold the Mean
	  message("Create mask ... ");
	  float Mmin, Mmax, Mtmp;
	  Mmin = Mean.min(); Mmax = Mean.max();
	  theMask = binarise(Mean,Mmin + opts.threshold.value()* (Mmax-Mmin),Mmax);
          Mtmp = Mmin + opts.threshold.value()* (Mmax-Mmin);
	  message("done" << endl);
	}
	else{ //well, don't threshold then
	  theMask = Mean;
	  theMask = 1.0;
	}
      }
    }
    if(opts.remove_endslices.value()){ 
      // just in case mc introduced something nasty
      message("  Deleting end slices" << endl);
      for(int ctr1=theMask.miny(); ctr1<=theMask.maxy(); ctr1++){
	for(int ctr2=theMask.minx(); ctr2<=theMask.maxx(); ctr2++){   
	  theMask(ctr2,ctr1,Mask.minz()) = 0.0;
	  theMask(ctr2,ctr1,Mask.maxz()) = 0.0;
	}
      }
    }
  } //void create_mask()
// }}}
// {{{ Sort
  void MelodicData::sort()
  {
    int numComp = mixMatrix.Ncols(), numVox = IC.Ncols(), 
        numTime = mixMatrix.Nrows(), i,j;

    for(int ctr_i = 1; ctr_i <= numComp; ctr_i++){
      if(IC.Row(ctr_i).Sum()<0){
	flipres(ctr_i); };}
    //    cerr << "HERE2" << endl << endl;


    // re-order wrt standard deviation of IC maps
    message("Sorting IC maps" << endl);  
    Matrix tmpscales, tmpICrow, tmpMIXcol;
    tmpscales = stdev(IC,2);
    ICstats = tmpscales;

    double max_val, min_val = tmpscales.Minimum()-1;

    for(int ctr_i = 1; ctr_i <= numComp; ctr_i++){

      max_val = tmpscales.Maximum2(i,j);
      ICstats(ctr_i,1)=max_val;
  
      tmpICrow = IC.Row(ctr_i);
      tmpMIXcol = mixMatrix.Column(ctr_i);
      
      IC.SubMatrix(ctr_i,ctr_i,1,numVox) = IC.SubMatrix(i,i,1,numVox);
      mixMatrix.SubMatrix(1,numTime,ctr_i,ctr_i) = 
	mixMatrix.SubMatrix(1,numTime,i,i);
  
      IC.SubMatrix(i,i,1,numVox) = tmpICrow.SubMatrix(1,1,1,numVox);
      mixMatrix.SubMatrix(1,numTime,i,i) = tmpMIXcol.SubMatrix(1,numTime,1,1);
  
      tmpscales(i,1)=tmpscales(ctr_i,1);
      tmpscales(ctr_i,1)=min_val;
    }

    ICstats /= ICstats.Column(1).Sum();
    ICstats *= 100;
    
    if(EVP.Storage()>0){
      tmpscales = ICstats.Column(1).AsMatrix(ICstats.Nrows(),1) * EVP(1,numComp);
      ICstats |= tmpscales;
    }

    if(Data.Storage()>0&&stdDev.Storage()>0){
    //if(DataVN.Storage()>0&&stdDev.Storage()>0){
      //cerr << " ICstats " << ICstats << endl << endl;

      Matrix copeP(tmpscales), copeN(tmpscales);
      Matrix max_ICs(tmpscales), min_ICs(tmpscales);

      for(int ctr_i = 1; ctr_i <= numComp; ctr_i++){
	int i,j;
	max_ICs(ctr_i,1) = IC.Row(ctr_i).Maximum2(i,j);
	//cerr << " ICstats " << ICstats << endl << endl;

	//cerr << endl <<(pinv(mixMatrix)*DataVN.Column(j)) << endl;
	copeP(ctr_i,1) = std::abs((pinv(mixMatrix)*Data.Column(j)).Row(ctr_i).AsScalar()*stdDev(1,j)*100*(mixMatrix.Column(ctr_i).Maximum()-mixMatrix.Column(ctr_i).Minimum())/meanR(1,j));

	min_ICs(ctr_i,1) = IC.Row(ctr_i).Minimum2(i,j);
	copeN(ctr_i,1) = -1.0*std::abs((pinv(mixMatrix)*Data.Column(j)).Row(ctr_i).AsScalar()*stdDev(1,j)*100*(mixMatrix.Column(ctr_i).Maximum()-mixMatrix.Column(ctr_i).Minimum())/meanR(1,j));

      }
      ICstats |= copeP;
      ICstats |= copeN;
    }
    
    mixFFT=calc_FFT(mixMatrix, opts.logPower.value());
    unmixMatrix = pinv(mixMatrix);

    //if(ICstats.Storage()>0){cout << "ICstats: " << ICstats.Nrows() <<"x" << ICstats.Ncols() << endl;}else{cout << "ICstats empty " <<endl;}
  }
// }}}
// {{{ Status

  void MelodicData::status(const string &txt)
  {
    cout << "MelodicData Object " << txt << endl;
    if(Data.Storage()>0){cout << "Data: " << Data.Nrows() <<"x" << Data.Ncols() << endl;}else{cout << "Data empty " <<endl;}
    if(pcaE.Storage()>0){cout << "pcaE: " << pcaE.Nrows() <<"x" << pcaE.Ncols() << endl;}else{cout << "pcaE empty " <<endl;}
    if(pcaD.Storage()>0){cout << "pcaD: " << pcaD.Nrows() <<"x" << pcaD.Ncols() << endl;}else{cout << "pcaD empty " <<endl;}
    if(whiteMatrix.Storage()>0){cout << "white: " << whiteMatrix.Nrows() <<"x" << whiteMatrix.Ncols() << endl;}else{cout << "white empty " <<endl;}
    if(dewhiteMatrix.Storage()>0){cout << "dewhite: " << dewhiteMatrix.Nrows() <<"x" << dewhiteMatrix.Ncols() << endl;}else{cout << "dewhite empty " <<endl;}
    if(mixMatrix.Storage()>0){cout << "mix: " << mixMatrix.Nrows() <<"x" << mixMatrix.Ncols() << endl;}else{cout << "mix empty " <<endl;}
    if(unmixMatrix.Storage()>0){cout << "unmix: " << unmixMatrix.Nrows() <<"x" << unmixMatrix.Ncols() << endl;}else{cout << "unmix empty " <<endl;}
    if(IC.Storage()>0){cout << "IC: " << IC.Nrows() <<"x" << IC.Ncols() << endl;}else{cout << "IC empty " <<endl;}
    
  } //void status()
// }}}
}

