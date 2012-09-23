// Declarations of classes that implements a scan
// or a collection of scans within the EC project.
// 
// ECScanClasses.h
//
// Jesper Andersson, FMRIB Image Analysis Group
//
// Copyright (C) 2011 University of Oxford 
//

#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <time.h>
#include "newmat.h"
#ifndef EXPOSE_TREACHEROUS
#define EXPOSE_TREACHEROUS           // To allow us to use .set_sform etc
#endif
#include "newimage/newimageall.h"
#include "miscmaths/miscmaths.h"
#include "warpfns/warpfns.h"
#include "topup/topup_file_io.h"
#include "topup/displacement_vector.h"
#include "EddyHelperClasses.h"
#include "EddyUtils.h"
#include "ECScanClasses.h"

using namespace EDDY;

NEWIMAGE::volume4D<float> ECScan::field_for_scan_to_model_transform(// Input
								    boost::shared_ptr<const NEWIMAGE::volume<float> > susc,
								    // Output
								    NEWIMAGE::volume<float>                           *omask,
								    NEWIMAGE::volume<float>                           *jac) const
{
  // Get RB matrix and EC field
  NEWMAT::Matrix iR = this->InverseMovementMatrix(); 
  NEWIMAGE::volume<float> eb = this->ECField();
  // Transform EC field using RB
  NEWIMAGE::volume<float> tot = this->GetIma(); tot = 0.0;
  NEWIMAGE::volume<char> mask1(tot.xsize(),tot.ysize(),tot.zsize()); 
  NEWIMAGE::copybasicproperties(tot,mask1); mask1 = 1;  // mask1 defines where the transformed EC map is valid
  NEWIMAGE::affine_transform(eb,iR,tot,mask1);          // Defined in warpfns.h
  if (omask) {
    *omask = EddyUtils::ConvertMaskToFloat(mask1);
    EddyUtils::SetTrilinearInterp(*omask);
  }
  // Add transformed EC and susc
  if (susc) tot += *susc;
  // Convert Hz-map to displacement field
  NEWIMAGE::volume4D<float> dfield = FieldUtils::Hz2VoxelDisplacements(tot,this->GetAcqPara());
  // Get Jacobian of tot map
  if (jac) *jac = FieldUtils::GetJacobian(dfield,this->GetAcqPara());
  // Transform dfield from voxels to mm
  dfield = FieldUtils::Voxel2MMDisplacements(dfield);

  return(dfield);
}

NEWIMAGE::volume4D<float> ECScan::field_for_model_to_scan_transform(// Input
								    boost::shared_ptr<const NEWIMAGE::volume<float> > susc,
								    // Output
								    NEWIMAGE::volume<float>                           *omask,
								    NEWIMAGE::volume<float>                           *jac) const
{
  // Get RB matrix and EC field
  NEWIMAGE::volume<float> tot = this->ECField();
  NEWIMAGE::volume<char> mask1(this->GetIma().xsize(),this->GetIma().ysize(),this->GetIma().zsize()); 
  NEWIMAGE::copybasicproperties(this->GetIma(),mask1); mask1 = 1;  // mask1 defines where the transformed susc map is valid
  if (susc) {
    NEWMAT::Matrix R = this->ForwardMovementMatrix();
    NEWIMAGE::volume<float> tsusc = *susc; tsusc = 0.0;
    NEWIMAGE::affine_transform(*susc,R,tsusc,mask1); // Defined in warpfns.h
    tot += tsusc;
  }
  // Convert HZ-map to displacement field
  NEWIMAGE::volume4D<float> dfield = FieldUtils::Hz2VoxelDisplacements(tot,this->GetAcqPara());
  // Invert Total displacement field
  bool own_mask = false;
  if (!omask) { omask = new NEWIMAGE::volume<float>(mask1.xsize(),mask1.ysize(),mask1.zsize()); own_mask=true; }
  *omask = 1.0;  // omask defines where the inverted total map is valid
  NEWIMAGE::volume4D<float> idfield = FieldUtils::InvertDisplacementField(dfield,this->GetAcqPara(),EddyUtils::ConvertMaskToFloat(mask1),*omask);
  EddyUtils::SetTrilinearInterp(*omask);
  if (own_mask) delete omask;
  // Get Jacobian of inverted tot map
  if (jac) *jac = FieldUtils::GetJacobian(idfield,this->GetAcqPara());
  // Transform idfield from voxels to mm
  idfield = FieldUtils::Voxel2MMDisplacements(idfield);

  return(idfield);
}

NEWIMAGE::volume<float> ECScan::GetUnwarpedOriginalIma(// Input
						       boost::shared_ptr<const NEWIMAGE::volume<float> >   susc,
						       // Output
						       NEWIMAGE::volume<float>&                            omask) const
{
  return(transform_to_model_space(this->GetOriginalIma(),susc,omask)); 
}
NEWIMAGE::volume<float> ECScan::GetUnwarpedOriginalIma(// Input
						       boost::shared_ptr<const NEWIMAGE::volume<float> >   susc) const
{ 
  NEWIMAGE::volume<float> skrutt = this->GetOriginalIma(); skrutt = 0.0;
  return(transform_to_model_space(this->GetOriginalIma(),susc,skrutt)); 
}

NEWIMAGE::volume<float> ECScan::GetUnwarpedIma(// Input
					       boost::shared_ptr<const NEWIMAGE::volume<float> >   susc,
					       // Output
					       NEWIMAGE::volume<float>&                            omask) const
{
  return(transform_to_model_space(this->GetIma(),susc,omask)); 
}
NEWIMAGE::volume<float> ECScan::GetUnwarpedIma(// Input
					       boost::shared_ptr<const NEWIMAGE::volume<float> >   susc) const
{ 
  NEWIMAGE::volume<float> skrutt = this->GetIma(); skrutt = 0.0;
  return(transform_to_model_space(this->GetIma(),susc,skrutt)); 
}

NEWMAT::ColumnVector ECScan::GetHz2mmVector() const
{
  NEWMAT::ColumnVector hz2mm(3);
  hz2mm(1) = _ima.xdim() * (_acqp.PhaseEncodeVector())(1) * _acqp.ReadOutTime();
  hz2mm(2) = _ima.ydim() * (_acqp.PhaseEncodeVector())(2) * _acqp.ReadOutTime();
  hz2mm(3) = _ima.zdim() * (_acqp.PhaseEncodeVector())(3) * _acqp.ReadOutTime();

  return(hz2mm);
}

void ECScan::SetFWHM(double fwhm)
{
  if (_fwhm==fwhm) return;
  if (_fwhm && !fwhm) { _sima.reinitialize(0,0,0); _fwhm = 0.0; return; }
  else { _sima = NEWIMAGE::smooth(_ima,fwhm/std::sqrt(8.0*std::log(2.0))); _fwhm = fwhm; return; }
}

void ECScan::ReplaceSlices(const NEWIMAGE::volume<float>&                     rep,
			   boost::shared_ptr<const NEWIMAGE::volume<float> >  susc,
			   const NEWIMAGE::volume<float>&                     inmask, 
			   const std::vector<unsigned int>&                   ol)
{
  // Transform prediction into observation space
  NEWIMAGE::volume<float> pios = EddyUtils::TransformModelToScanSpace(rep,*this,susc);
  // Transform binary mask into observation space
  NEWIMAGE::volume<float> mask = rep; mask = 0.0;
  NEWIMAGE::volume<float> bios = EddyUtils::transform_model_to_scan_space(inmask,*this,susc,false,mask,NULL,NULL);
  bios.binarise(0.9); // Value above (arbitrary) 0.9 implies valid voxels
  mask *= bios;       // Volume and input mask falls within FOV
  // Replace requested slices where the resampled rep is valid
  for (unsigned int ii=0; ii<ol.size(); ii++) {
    for (int j=0; j<_ima.ysize(); j++) {
      for (int i=0; i<_ima.xsize(); i++) {
	if (mask(i,j,ol[ii])) _ima(i,j,ol[ii]) = pios(i,j,ol[ii]);
      }
    }
  }
  // Re-apply smoothing
  if (_fwhm) _sima = NEWIMAGE::smooth(_ima,_fwhm/std::sqrt(8.0*std::log(2.0)));

  return;  
}

NEWIMAGE::volume<float> ECScan::motion_correct(const NEWIMAGE::volume<float>&  inima,
					       NEWIMAGE::volume<float>         *omask) const
{
  // Transform image using inverse RB
  NEWMAT::Matrix iR = InverseMovementMatrix();
  NEWIMAGE::volume<float> ovol = inima; ovol = 0.0;
  NEWIMAGE::volume<char> mask(ovol.xsize(),ovol.ysize(),ovol.zsize()); 
  NEWIMAGE::copybasicproperties(inima,mask); mask = 1;
  NEWIMAGE::affine_transform(inima,iR,ovol,mask);
  *omask = EddyUtils::ConvertMaskToFloat(mask);
  EddyUtils::SetTrilinearInterp(*omask);
  return(ovol);
}

NEWIMAGE::volume<float> ECScan::transform_to_model_space(// Input
							 const NEWIMAGE::volume<float>&                    inima,
							 boost::shared_ptr<const NEWIMAGE::volume<float> > susc,
							 // Output
							 NEWIMAGE::volume<float>&                          omask) const
{
  // Get total field from scan
  NEWIMAGE::volume<float> jac;
  NEWIMAGE::volume4D<float> dfield = FieldForScanToModelTransform(susc,omask,jac);
  // Transform image using inverse RB, dfield and Jacobian
  NEWMAT::Matrix iR = InverseMovementMatrix();
  NEWIMAGE::volume<float> ovol = GetIma(); ovol = 0.0;
  NEWIMAGE::volume<char> mask2(ovol.xsize(),ovol.ysize(),ovol.zsize()); 
  NEWIMAGE::copybasicproperties(inima,mask2); mask2 = 1;
  NEWIMAGE::general_transform(inima,iR,dfield,ovol,mask2);
  // Combine all masks
  omask *= EddyUtils::ConvertMaskToFloat(mask2);
  EddyUtils::SetTrilinearInterp(omask);
  return(jac*ovol);
}

ECScanManager::ECScanManager(const std::string&               imafname,
			     const std::string&               maskfname,
			     const std::string&               acqpfname,
			     const std::string&               topupfname,
			     const std::string&               bvecsfname,
			     const std::string&               bvalsfname,
			     EDDY::ECModel                    ecmodel,
			     const std::vector<unsigned int>& indicies,
			     const std::vector<unsigned int>& sess,
			     unsigned int                     no_of_sessions) : _has_topup(false)
{
  // Set number of sessions that data was acquired in
  _nsess = no_of_sessions;
  // Read acquisition parameters file
  TOPUP::TopupDatafileReader tdfr(acqpfname);
  boost::shared_ptr<TOPUP::TopupFileReader> tfrp;
  // Read output from topup
  if (topupfname != string("")) {
    tfrp = boost::shared_ptr<TOPUP::TopupFileReader>(new TOPUP::TopupFileReader(topupfname));
    _topup_field = boost::shared_ptr<NEWIMAGE::volume<float> >(new NEWIMAGE::volume<float>(tfrp->FieldAsVolume()));
    EddyUtils::SetSplineInterp(*_topup_field);
    _topup_field->forcesplinecoefcalculation(); // N.B. neccessary if OpenMP is to work
    _has_topup = true;
  }
  // Read bvecs and bvals
  NEWMAT::Matrix bvecsM = MISCMATHS::read_ascii_matrix(bvecsfname);
  if (bvecsM.Nrows()>bvecsM.Ncols()) bvecsM = bvecsM.t();
  NEWMAT::Matrix bvalsM = MISCMATHS::read_ascii_matrix(bvalsfname);
  if (bvalsM.Nrows()>bvalsM.Ncols()) bvalsM = bvalsM.t();
  // Read mask
  NEWIMAGE::read_volume(_mask,maskfname);
  EddyUtils::SetTrilinearInterp(_mask);
  // Go through and read all image volumes, spliting b0s and dwis
  NEWIMAGE::volume4D<float> all;
  NEWIMAGE::read_volume4D(all,imafname);
  _sf = 100.0 / mean_of_first_b0(all,_mask,bvecsM,bvalsM);
  _fi.resize(all.tsize());
  for (int s=0; s<all.tsize(); s++) {
    EDDY::AcqPara acqp(tdfr.PhaseEncodeVector(indicies[s]),tdfr.ReadOutTime(indicies[s]));
    EDDY::DiffPara dp(bvecsM.Column(s+1),bvalsM(1,s+1));
    if (EddyUtils::IsDiffusionWeighted(dp)) {
      boost::shared_ptr<EDDY::ScanECModel> ecp;
      switch (ecmodel) {
      case EDDY::Linear:
	ecp = boost::shared_ptr<EDDY::ScanECModel>(new LinearScanECModel(_has_topup));
	break;
      case EDDY::Quadratic:
	ecp = boost::shared_ptr<EDDY::ScanECModel>(new QuadraticScanECModel(_has_topup));
	break;
      case EDDY::Cubic:
	ecp = boost::shared_ptr<EDDY::ScanECModel>(new CubicScanECModel(_has_topup));
	break;
      default:
        throw EddyException("ECScanManager::ECScanManager: Invalid EC model");
      }
      if (_has_topup) {
	NEWMAT::Matrix fwd_matrix = TOPUP::MovePar2Matrix(tfrp->MovePar(indicies[s]),all[s]);
	NEWMAT::ColumnVector bwd_mp = TOPUP::Matrix2MovePar(fwd_matrix.i(),all[s]);
	ecp->SetParams(bwd_mp,EDDY::MOVEMENT);
      }
      NEWIMAGE::volume<float> tmp = all[s]*_sf; EddyUtils::SetSplineInterp(tmp);
      _scans.push_back(ECScan(tmp,acqp,dp,ecp,sess[s]-1));
      _fi[s].first = 0; _fi[s].second = _scans.size() - 1;
    }
    else {
      boost::shared_ptr<EDDY::ScanECModel> ecp = boost::shared_ptr<EDDY::ScanECModel>(new MovementScanECModel());    
      if (_has_topup) {
	NEWMAT::Matrix fwd_matrix = TOPUP::MovePar2Matrix(tfrp->MovePar(indicies[s]),all[s]);
	NEWMAT::ColumnVector bwd_mp = TOPUP::Matrix2MovePar(fwd_matrix.i(),all[s]);
	ecp->SetParams(bwd_mp,EDDY::MOVEMENT);
      }
      NEWIMAGE::volume<float> tmp = all[s]*_sf; EddyUtils::SetSplineInterp(tmp);
      _b0scans.push_back(ECScan(tmp,acqp,dp,ecp,sess[s]-1));
      _fi[s].first = 1; _fi[s].second = _b0scans.size() - 1;
    }
  }
}

unsigned int ECScanManager::NScans(ScanType st) const
{
  unsigned int rval = 0;
  switch (st) {
  case ANY: rval = _scans.size()+_b0scans.size(); break;
  case DWI: rval = _scans.size(); break;
  case B0: rval = _b0scans.size(); break;
  default: break;
  } 
  return(rval);
}

unsigned int ECScanManager::NLSRPairs(ScanType st) const
{
  unsigned int rval = 0;
  if (!CanDoLSRResampling()) throw;
  else rval = NScans(st)/2;
  return(rval);
}

std::vector<unsigned int> ECScanManager::GetDwi2GlobalIndexMapping() const
{
  std::vector<unsigned int> i2i(_scans.size());
  for (unsigned int i=0; i<_fi.size(); i++) {
    if (!_fi[i].first) i2i[_fi[i].second] = i;
  }
  return(i2i);
}

std::pair<unsigned int,unsigned int> ECScanManager::GetLSRPair(unsigned int i, ScanType st) const
{
  std::pair<unsigned int,unsigned int> rval(0,0);
  if (!CanDoLSRResampling()) throw;
  else if (i >= NLSRPairs(st)) throw;
  else {
    rval.first = i; rval.second = NLSRPairs(st) + i;
  }  
  return(rval);
}

bool ECScanManager::CanDoLSRResampling() const
{
  // Do first pass to find where we go from one 
  // blip direction to the next.
  NEWMAT::ColumnVector blip1 = Scan(0,ANY).GetAcqPara().PhaseEncodeVector();
  unsigned int n_dir1 = 1;
  for (; n_dir1<NScans(ANY); n_dir1++) {
    if (Scan(n_dir1,ANY).GetAcqPara().PhaseEncodeVector() != blip1) break;
  }
  if (n_dir1 != NScans(ANY)/2) return(false);
  // Do second pass to ensure they are divided up 
  // into pairs with matching acquisition parameters
  for (unsigned int i=0; i<n_dir1; i++) {
    if (!EddyUtils::AreMatchingPair(Scan(i,ANY),Scan(i+n_dir1,ANY))) return(false);
  }
  return(true);
}

NEWIMAGE::volume<float> ECScanManager::LSRResamplePair(// Input
						       unsigned int              i, 
						       unsigned int              j, 
						       ScanType                  st,
						       // Output
						       NEWIMAGE::volume<float>&  omask) const
{
  if (!EddyUtils::AreMatchingPair(Scan(i,st),Scan(j,st))) throw EddyException("ECScanManager::LSRResamplePair:: Mismatched pair");
  // Resample both images using rigid body parameters
  NEWIMAGE::volume<float> imask, jmask;
  NEWIMAGE::volume<float> imai = Scan(i,st).GetMotionCorrectedOriginalIma(imask);
  NEWIMAGE::volume<float> imaj = Scan(j,st).GetMotionCorrectedOriginalIma(jmask);
  NEWIMAGE::volume<float> mask = imask*jmask;
  omask.reinitialize(mask.xsize(),mask.ysize(),mask.zsize());
  omask = 1.0;
  NEWIMAGE::volume<float> ovol(imai.xsize(),imai.ysize(),imai.zsize());
  NEWIMAGE::copybasicproperties(imai,ovol);
  // Get fields
  NEWIMAGE::volume4D<float> fieldi = Scan(i,st).FieldForScanToModelTransform(GetSuscHzOffResField());
  NEWIMAGE::volume4D<float> fieldj = Scan(j,st).FieldForScanToModelTransform(GetSuscHzOffResField());
  // Check what direction phase-encode is in
  bool pex = false;
  unsigned int sz = fieldi[0].ysize();
  if (Scan(i,st).GetAcqPara().PhaseEncodeVector()(1)) { pex = true; sz = fieldi[0].xsize(); }
  TOPUP::DispVec dvi(sz), dvj(sz);
  NEWMAT::Matrix StS = dvi.GetS_Matrix(false);
  StS = StS.t()*StS;
  NEWMAT::ColumnVector zeros;
  if (pex) zeros.ReSize(imai.xsize()); else zeros.ReSize(imai.ysize());
  zeros = 0.0;
  for (int k=0; k<imai.zsize(); k++) {
    // Loop over all colums/rows
    for (int ij=0; ij<((pex) ? imai.ysize() : imai.xsize()); ij++) {
      bool row_col_is_ok = true;
      if (pex) {
        if (!dvi.RowIsAlright(mask,k,ij)) row_col_is_ok = false;
	else {
	  dvi.SetFromRow(fieldi[0],k,ij);
	  dvj.SetFromRow(fieldj[0],k,ij);
	}
      }
      else {
        if (!dvi.ColumnIsAlright(mask,k,ij)) row_col_is_ok = false;
	else {
	  dvi.SetFromColumn(fieldi[0],k,ij);
	  dvj.SetFromColumn(fieldj[0],k,ij);
	}
      }
      if (row_col_is_ok) {
	NEWMAT::Matrix K = dvi.GetK_Matrix(1.0) | dvj.GetK_Matrix(1.0);
	NEWMAT::Matrix KtK = K.t()*K + 0.01*StS;
	NEWMAT::ColumnVector y;
        if (pex) y = imai.ExtractRow(ij,k) | imaj.ExtractRow(ij,k);
	else y = imai.ExtractColumn(ij,k) | imaj.ExtractColumn(ij,k);
	NEWMAT::ColumnVector Kty = K.t()*y;
	NEWMAT::ColumnVector y_hat = KtK.i()*Kty;
	if (pex) ovol.SetRow(ij,k,y_hat); else ovol.SetColumn(ij,k,y_hat);
      }
      else {
	if (pex) omask.SetRow(ij,k,zeros); else omask.SetColumn(ij,k,zeros);
      }
    }
  }
  return(ovol);
}

void ECScanManager::SetParameters(const NEWMAT::Matrix& pM, ScanType st)
{
  if (pM.Nrows() != int(NScans(st))) throw EddyException("ECScanManager::SetParameters: Mismatch between parameter matrix and # of scans");
  for (unsigned int i=0; i<NScans(st); i++) {
    int ncol = Scan(i,st).NParam();
    Scan(i,st).SetParams(pM.SubMatrix(i+1,i+1,1,ncol).t());
  } 
}

const ECScan& ECScanManager::Scan(unsigned int indx, ScanType st) const
{
  if (!index_kosher(indx,st))  throw EddyException("ECScanManager::Scan: index out of range"); 
  if (st == DWI) return(_scans[indx]);
  else if (st == B0) return(_b0scans[indx]); 
  else { // ANY
    if (!_fi[indx].first) return(_scans[_fi[indx].second]);
    else return(_b0scans[_fi[indx].second]);
  }   
}

ECScan& ECScanManager::Scan(unsigned int indx, ScanType st)
{
  if (!index_kosher(indx,st))  throw EddyException("ECScanManager::Scan: index out of range"); 
  if (st == DWI) return(_scans[indx]);
  else if (st == B0) return(_b0scans[indx]); 
  else { // ANY
    if (!_fi[indx].first) return(_scans[_fi[indx].second]);
    else return(_b0scans[_fi[indx].second]);
  }   
}

NEWIMAGE::volume<float> ECScanManager::GetUnwarpedOrigScan(unsigned int              indx,
							   NEWIMAGE::volume<float>&  omask,
							   ScanType                  st) const 
{
  if (!index_kosher(indx,st))  throw EddyException("ECScanManager::GetUnwarpedOrigScan: index out of range");
  return(Scan(indx,st).GetUnwarpedOriginalIma(_topup_field,omask)); 
}

NEWIMAGE::volume<float> ECScanManager::GetUnwarpedScan(unsigned int              indx,
						       NEWIMAGE::volume<float>&  omask,
						       ScanType                  st) const 
{
  if (!index_kosher(indx,st))  throw EddyException("ECScanManager::GetUnwarpedScan: index out of range");
  return(Scan(indx,st).GetUnwarpedIma(_topup_field,omask)); 
}

void ECScanManager::SeparateFieldOffsetFromMovement()
{
  if (HasFieldOffset()) {
    // Put everything that can be offset into offset vector
    NEWMAT::ColumnVector hz = hz_vector_with_everything();
    // Get design explaining offset as off-iso-centre + linear with time
    NEWMAT::Matrix X = offset_design_matrix();
    // Get explained component for reintroduction to offset
    NEWMAT::ColumnVector offset = (X*MISCMATHS::pinv(X))*hz;
    // Reintroduce explained component
    for (unsigned int i=0; i<NScans(DWI); i++) Scan(i,DWI).SetFieldOffset(offset(i+1));
    // Adjust movement component accordingly
    for (unsigned int i=0; i<NScans(DWI); i++) {
      NEWMAT::ColumnVector ymm = Scan(i,DWI).GetParams(EDDY::MOVEMENT);
      for (int j=1; j<=3; j++) {
	if (Scan(i,DWI).GetHz2mmVector()(j)) {
	  ymm(j) = (hz(i+1) - offset(i+1))*Scan(i,DWI).GetHz2mmVector()(j);
	}
      }
      Scan(i,DWI).SetParams(ymm,EDDY::MOVEMENT);
    }
  }
}

void ECScanManager::WriteParameterFile(const std::string& fname, ScanType st) const
{
  NEWMAT::Matrix  params;
  if (st == B0) params.ReSize(Scan(0,B0).NParam(),NScans(st));
  else params.ReSize(Scan(0,DWI).NParam(),NScans(st));
  int b0_np = Scan(0,B0).NParam();
  params = 0.0;
  for (unsigned int i=0; i<NScans(st); i++) {
    if (st==DWI || (st==ANY && IsDWI(i))) params.Column(i+1) = Scan(i,st).GetParams(EDDY::ALL);
    else params.SubMatrix(1,b0_np,i+1,i+1) = Scan(i,st).GetParams(EDDY::ALL);
  }
  MISCMATHS::write_ascii_matrix(fname,params.t());
}

void ECScanManager::WriteECFields(const std::string& fname, ScanType st) const
{
  NEWIMAGE::volume4D<float> ovol(Scan(0).GetIma().xsize(),Scan(0).GetIma().ysize(),Scan(0).GetIma().zsize(),NScans(st));
  NEWIMAGE::copybasicproperties(Scan(0).GetIma(),ovol);
  for (unsigned int i=0; i<NScans(st); i++) ovol[i] = GetScanHzECOffResField(i,st);
  NEWIMAGE::write_volume4D(ovol,fname);
}

NEWMAT::Matrix ECScanManager::offset_design_matrix() const
{
  // First build part where we model it as an offset for the linear parameters
  NEWMAT::Matrix offset_part;
  NEWMAT::RowVector test = Scan(0,DWI).GetLinearParameters();
  if (test.Ncols()) { // If there is a linear part of current model
    offset_part.ReSize(NScans(DWI),test.Ncols());
    for (unsigned int i=0; i<NScans(DWI); i++) {
      offset_part.Row(i+1) = Scan(i,DWI).GetLinearParameters();
    }
  }
  // Secondly add the linear (with time) part from gradient heating
  // Build one regressor per session to model within session heating.
  NEWMAT::Matrix linear_part;
  if (NoOfSessions()>1) linear_part.ReSize(NScans(DWI),NoOfSessions()+1); 
  else linear_part.ReSize(NScans(DWI),1); 
  linear_part = 0.0;
  for (unsigned int i=0; i<NScans(DWI); i++) {
    linear_part(i+1,Scan(i,DWI).Session()) = linear_part.Column(Scan(i,DWI).Session()).Maximum()+1;
  }
  // 3rd pass to put in linear regressors across all scans (residual heating between sessions)
  if (NoOfSessions()>1) { for (unsigned int i=0; i<NScans(DWI); i++) linear_part(i+1,NoOfSessions()+1) = double(i); }
  // Rescale them to make matrix better conditioned
  for (int i=0; i<linear_part.Ncols(); i++) {
    linear_part.Column(i+1) /= linear_part.Column(i+1).Maximum();
  }
  
  NEWMAT::Matrix design_matrix;
  if (test.Ncols()) design_matrix = offset_part | linear_part;
  else design_matrix = linear_part;

  MISCMATHS::write_ascii_matrix(string("design_matrix_for_debugging.txt"),design_matrix);

  return(design_matrix);
}

NEWMAT::ColumnVector ECScanManager::hz_vector_with_everything() const
{
  NEWMAT::ColumnVector hz(NScans(DWI));
  for (unsigned int i=0; i<NScans(DWI); i++) {
    NEWMAT::Matrix X = Scan(i,DWI).GetHz2mmVector();
    NEWMAT::ColumnVector ymm = Scan(i,DWI).GetParams(EDDY::MOVEMENT);
    hz(i+1) = Scan(i,DWI).GetFieldOffset() + (MISCMATHS::pinv(X)*ymm.Rows(1,3)).AsScalar();
  }
  return(hz);
}

void ECScanManager::set_reference(unsigned int ref,
				  ScanType     st)
{
  if (ref >= NScans(st)) throw EddyException("ECScanManager::set_reference: ref index out of bounds");
  NEWMAT::Matrix Mr = Scan(ref,st).ForwardMovementMatrix();
  for (unsigned int i=0; i<NScans(st); i++) {
    NEWMAT::Matrix M = Scan(i,st).ForwardMovementMatrix();
    NEWMAT::ColumnVector new_mp = TOPUP::Matrix2MovePar(M*Mr.i(),Scan(i,st).GetIma());
    Scan(i,st).SetParams(new_mp,MOVEMENT);
  }
}

double ECScanManager::mean_of_first_b0(const NEWIMAGE::volume4D<float>&   vols,
				       const NEWIMAGE::volume<float>&     mask,
				       const NEWMAT::Matrix&              bvecs,
				       const NEWMAT::Matrix&              bvals) const
{
  double rval = 0.0;
  for (int s=0; s<vols.tsize(); s++) {
    EDDY::DiffPara  dp(bvecs.Column(s+1),bvals(1,s+1));
    if (EddyUtils::Isb0(dp)) {
      rval = vols[s].mean(mask);
      break;
    }
  }
  if (!rval) throw EddyException("ECScanManager::mean_of_first_b0: Zero mean");

  return(rval);
}

void ECScanManager::write_jac_registered_images(const std::string& fname, ScanType st)
{
  SetFWHM(0.0);
  NEWIMAGE::volume4D<float> ovol(Scan(0,st).GetIma().xsize(),Scan(0,st).GetIma().ysize(),Scan(0,st).GetIma().zsize(),NScans(st));
  NEWIMAGE::copybasicproperties(Scan(0,st).GetIma(),ovol);
  // # pragma omp parallel for shared(ovol)
  for (unsigned int i=0; i<NScans(st); i++) {
    NEWIMAGE::volume<float> tmp = GetUnwarpedScan(i,st) / ScaleFactor();
    // # pragma omp critical
    {
      ovol[i] = tmp;
    }
  }
  NEWIMAGE::write_volume4D(ovol,fname);    
}

void ECScanManager::write_lsr_registered_images(const std::string& fname, ScanType st)
{
  SetFWHM(0.0);
  NEWIMAGE::volume4D<float> ovol(Scan(0,st).GetIma().xsize(),Scan(0,st).GetIma().ysize(),Scan(0,st).GetIma().zsize(),NLSRPairs(st));
  NEWIMAGE::copybasicproperties(Scan(0,st).GetIma(),ovol);
  // # pragma omp parallel for shared(ovol)
  for (unsigned int i=0; i<NLSRPairs(st); i++) {
    std::pair<unsigned int,unsigned int> par = GetLSRPair(i,st);
    NEWIMAGE::volume<float> omask;
    NEWIMAGE::volume<float> tmp = LSRResamplePair(par.first,par.second,st,omask) / ScaleFactor();
    // # pragma omp critical
    {
      ovol[i] = tmp;
    }
  }
  NEWIMAGE::write_volume4D(ovol,fname);    
}
