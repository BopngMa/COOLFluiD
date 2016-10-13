#include "FiniteVolumeCUDA/FVMCC_ComputeSourceRhsJacobCell.hh"
#include "Framework/MeshData.hh"
#include "Framework/BlockAccumulatorBaseCUDA.hh"
#include "Framework/CellConn.hh"
#include "Config/ConfigOptionPtr.hh"
#include "Common/CUDA/CudaDeviceManager.hh"
#include "Common/CUDA/CFVec.hh"
#include "Common/CUDA/CudaTimer.hh"
#include "FiniteVolume/CellData.hh"

#include "FiniteVolumeCUDA/FiniteVolumeCUDA.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/VarSetListT.hh"
#include "FiniteVolume/LaxFriedFlux.hh"
#include "FiniteVolume/LeastSquareP1PolyRec2D.hh"
#include "FiniteVolume/LeastSquareP1PolyRec3D.hh"
#include "FiniteVolume/BarthJesp.hh"
#include "MHD/MHD2DProjectionConsT.hh"
#include "MHD/MHD3DProjectionConsT.hh"
#include "MHD/MHD2DProjectionPrimT.hh"
#include "MHD/MHD3DProjectionPrimT.hh"
#include "MHD/MHDProjectionPrimToConsT.hh"
#include "FiniteVolumeMHD/LaxFriedFluxTanaka.hh"
#include "MHD/MHD2DProjectionVarSet.hh"
#include "MHD/MHD3DProjectionVarSet.hh"

#include "MultiFluidMHD/MultiFluidMHDVarSet.hh"
#include "MultiFluidMHD/EulerMFMHD2DHalfConsT.hh"
#include "MultiFluidMHD/EulerMFMHD2DHalfRhoiViTiT.hh"
#include "MultiFluidMHD/EulerMFMHD2DHalfRhoiViTiToConsT.hh"
#include "MultiFluidMHD/EulerMFMHD2DHalfConsToRhoiViTiT.hh"
#include "FiniteVolumeMultiFluidMHD/AUSMPlusUpFluxMultiFluid.hh"
#include "FiniteVolumeMultiFluidMHD/AUSMFluxMultiFluid.hh"
#include "FiniteVolumeMultiFluidMHD/DriftWaves2DHalfTwoFluid.hh"

#include "Maxwell/Maxwell2DProjectionVarSet.hh"
#include "Maxwell/Maxwell2DProjectionConsT.hh"
#include "FiniteVolumeMaxwell/StegerWarmingMaxwellProjection2D.hh"




//////////////////////////////////////////////////////////////////////////////

using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Config;
using namespace COOLFluiD::Physics::MHD;
using namespace COOLFluiD::Physics::Maxwell;
using namespace COOLFluiD::Physics::MultiFluidMHD;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace FiniteVolume {

//////////////////////////////////////////////////////////////////////////////



//Provider for AUSMPlusUpFlux with Source
#define FVMCC_MULTIFLUIDMHD_RHS_JACOB_PROV_AUSMPLUSUP_SOURCE(__dim__,__half__,__svars__,__uvars__,__sourceterm__,__nbBThreads__,__providerName__) \
MethodCommandProvider<FVMCC_ComputeSourceRhsJacobCell<AUSMPlusUpFluxMultiFluid<MultiFluidMHDVarSet<Maxwell##__dim__##ProjectionVarSet> >, \
			              VarSetListT<EulerMFMHD##__dim__##__half__##__svars__##T, EulerMFMHD##__dim__##__half__##__uvars__##T>, \
				      DriftWaves2DHalfTwoFluid<MultiFluidMHDVarSet<Maxwell##__dim__##ProjectionVarSet> >, \
				      LeastSquareP1PolyRec##__dim__ , BarthJesp, __nbBThreads__>, \
		      CellCenterFVMData, FiniteVolumeCUDAModule>	\
fvmcc_RhsJacobMultiFluidMHDAUSMPlusUp##__dim__##__half__##__svars__##__uvars__##__sourceterm__##__nbBThreads__##Provider(__providerName__);

// 48 block threads (default)
FVMCC_MULTIFLUIDMHD_RHS_JACOB_PROV_AUSMPLUSUP_SOURCE(2D,Half,Cons,RhoiViTi,DriftWaves2DHalfTwoFluid,48,"CellNumJacobAUSMPlusUpEulerMFMHD2DHalfRhoiViTiDriftWavesTwoFluid")
//SOURCE HARD CODED
#undef FVMCC_MULTIFLUIDMHD_RHS_JACOB_PROV_AUSMPLUSUP_SOURCE

//////////////////////////////////////////////////////////////////////////////

template <typename PHYS>
HOST_DEVICE inline void setState(CFreal* state, CFreal* statePtr, 
				 CFreal* node, CFreal* nodePtr)
{
  // copy the state node data to shared memory
  for (CFuint i = 0; i < PHYS::DIM; ++i) {node[i] = nodePtr[i];}
  // copy the state data to shared memory
  for (CFuint i = 0; i < PHYS::NBEQS; ++i) {state[i] = statePtr[i];} 
}
      
//////////////////////////////////////////////////////////////////////////////
      
template <typename PHYS>
HOST_DEVICE inline void setFaceNormal(FluxData<PHYS>* fd, CFreal* normal)
{  
  CudaEnv::CFVecSlice<CFreal,PHYS::DIM> n(normal);
  const CFreal area = n.norm2();
  fd->setFaceArea(area);
  const CFreal ovArea = 1./area;
  CudaEnv::CFVecSlice<CFreal,PHYS::DIM> un(fd->getUnitNormal());
  for (CFuint i = 0; i < PHYS::DIM; ++i) {
    un[i] = n[i]*ovArea;
  }
}
      
//////////////////////////////////////////////////////////////////////////////

template <typename PHYS, typename PTR>
HOST_DEVICE void setFluxData(const CFuint f, const CFint stype, 
			     const CFuint stateID, const CFuint cellID, 
			     KernelData<CFreal>* kd, FluxData<PHYS>* fd,
			     PTR cellFaces)
{  
  fd->setStateID(RIGHT, stateID);
  CFreal* statePtrR = (stype > 0) ? &kd->states[stateID*PHYS::NBEQS] : &kd->ghostStates[stateID*PHYS::NBEQS];  
  CFreal* nodePtrR = (stype > 0) ? &kd->centerNodes[stateID*PHYS::DIM] : &kd->ghostNodes[stateID*PHYS::DIM];  
  setState<PHYS>(fd->getState(RIGHT), statePtrR, fd->getNode(RIGHT), nodePtrR);
  
  fd->setIsBFace(stype < 0);
  fd->setStateID(LEFT, cellID);
  const CFuint faceID = cellFaces[f*kd->nbCells + cellID];
  fd->setIsOutward(kd->isOutward[faceID] == cellID);
  
  CFreal* statePtrL = &kd->states[cellID*PHYS::NBEQS];
  CFreal* nodePtrL = &kd->centerNodes[cellID*PHYS::DIM];
  setState<PHYS>(fd->getState(LEFT), statePtrL, fd->getNode(LEFT), nodePtrL);
  setFaceNormal<PHYS>(fd, &kd->normals[faceID*PHYS::DIM]);
}

//////////////////////////////////////////////////////////////////////////////

template <typename T, CFuint SIZE>
void print(const std::string& name, T* array) 
{
  std::cout << name << " = \t";
  for (CFuint i = 0; i < SIZE; ++i) {
    std::cout.precision(10); std::cout << array[i] << " ";
  }
  std::cout << "\n";
}
      
//////////////////////////////////////////////////////////////////////////////

template <typename T, CFuint SIZE>
void printArray(T* array) 
{
  for (CFuint i = 0; i < SIZE; ++i) {
    std::cout << array[i] << " ";
  }
  std::cout << "\n";
}

//////////////////////////////////////////////////////////////////////////////

template <typename MODEL>
HOST_DEVICE void computeFaceCentroid(const CellData::Itr* cell, const CFuint faceIdx, 
				     const CFreal* nodes, CFreal* midFaceCoord)
{  
  CudaEnv::CFVecSlice<CFreal, MODEL::DIM> coord(midFaceCoord);
  coord = 0.;
  const CFuint nbFaceNodes = cell->getNbFaceNodes(faceIdx);
  const CFreal ovNbFaceNodes = 1./(static_cast<CFreal>(nbFaceNodes));
  for (CFuint n = 0; n < nbFaceNodes; ++n) {
    const CFuint cellNodeID = cell->getNodeID(faceIdx, n);
    const CFuint nodeID = cell->getNodeID(faceIdx,n);
    const CFreal* faceNode = &nodes[nodeID*MODEL::DIM];
    for (CFuint d = 0; d < MODEL::DIM; ++d) {
      coord[d] += faceNode[d];
    }
  }
  coord *= ovNbFaceNodes;
}

//////////////////////////////////////////////////////////////////////////////

template <typename PHYS, typename POLYREC>
__global__ void computeGradientsKernel(typename POLYREC::BASE::template DeviceConfigOptions<NOTYPE>* dcor,
				       const CFuint nbCells,
				       CFreal* states, 
				       CFreal* nodes,
				       CFreal* centerNodes,
				       CFreal* ghostStates,
				       CFreal* ghostNodes,
				       CFreal* uX,
				       CFreal* uY,
				       CFreal* uZ,
				       CFreal* limiter,
				       CFreal* updateCoeff, 
				       CFreal* rhs,
				       CFreal* normals,
				       CFint* isOutward,
				       const CFuint* cellInfo,
				       const CFuint* cellStencil,
				       const CFuint* cellFaces,
				       const CFuint* cellNodes,
				       const CFint*  neighborTypes,
				       const Framework::CellConn* cellConn)
{    
  // each thread takes care of computing the gradient for one single cell
  const int cellID = threadIdx.x + blockIdx.x*blockDim.x;
  
  if (cellID < nbCells) {    
    KernelData<CFreal> kd (nbCells, states, nodes, centerNodes, ghostStates, ghostNodes, updateCoeff, 
			   rhs, normals, uX, uY, uZ, isOutward);
    
    // compute and store cell gradients at once 
    POLYREC polyRec(dcor);
    CellData cells(nbCells, cellInfo, cellStencil, cellFaces, cellNodes, neighborTypes, cellConn);
    CellData::Itr cell = cells.getItr(cellID);
    polyRec.computeGradients(&states[cellID*PHYS::NBEQS], &centerNodes[cellID*PHYS::DIM], &kd, &cell);
  }
}
      
//////////////////////////////////////////////////////////////////////////////

template <typename PHYS, typename POLYREC, typename LIMITER>
__global__ void computeLimiterKernel(typename LIMITER::BASE::template DeviceConfigOptions<NOTYPE>* dcol,
				     typename POLYREC::BASE::template DeviceConfigOptions<NOTYPE>* dcor,
				     const CFuint nbCells,
				     CFreal* states, 
				     CFreal* nodes,
				     CFreal* centerNodes,
				     CFreal* ghostStates,
				     CFreal* ghostNodes,
				     CFreal* uX,
				     CFreal* uY,
				     CFreal* uZ,
				     CFreal* limiter,
				     CFreal* updateCoeff, 
				     CFreal* rhs,
				     CFreal* normals,
				     CFint* isOutward,
				     const CFuint* cellInfo,
				     const CFuint* cellStencil,
				     const CFuint* cellFaces,
				     const CFuint* cellNodes,
				     const CFint*  neighborTypes,
				     const Framework::CellConn* cellConn)
{    
  // each thread takes care of computing the gradient for one single cell
  const int cellID = threadIdx.x + blockIdx.x*blockDim.x;
 
  if (cellID < nbCells) {    
    // compute all cell quadrature points at once (size of this array is overestimated)
    CFreal midFaceCoord[PHYS::DIM*PHYS::DIM*2];
    
    CellData cells(nbCells, cellInfo, cellStencil, cellFaces, cellNodes, neighborTypes, cellConn);
    CellData::Itr cell = cells.getItr(cellID);
    const CFuint nbFacesInCell = cell.getNbFacesInCell();
    for (CFuint f = 0; f < nbFacesInCell; ++f) { 
      computeFaceCentroid<PHYS>(&cell, f, nodes, &midFaceCoord[f*PHYS::DIM]);
    }
    
    // compute cell-based limiter at once
    KernelData<CFreal> kd (nbCells, states, nodes, centerNodes, ghostStates, ghostNodes, updateCoeff, 
			   rhs, normals, uX, uY, uZ, isOutward);
    LIMITER limt(dcol);
    
    if (dcor->currRes > dcor->limitRes && (dcor->limitIter > 0 && dcor->currIter < dcor->limitIter)) {	
      limt.limit(&kd, &cell, &midFaceCoord[0], &limiter[cellID*PHYS::NBEQS]);
    }
    else {
      if (!dcor->freezeLimiter) {
	// historical modification of the limiter
	CudaEnv::CFVec<CFreal,PHYS::NBEQS> tmpLimiter;
	limt.limit(&kd, &cell, &midFaceCoord[0], &tmpLimiter[0]);
	CFuint currID = cellID*PHYS::NBEQS;
	for (CFuint iVar = 0; iVar < PHYS::NBEQS; ++iVar, ++currID) {
	  limiter[currID] = min(tmpLimiter[iVar],limiter[currID]);
	}
      }
    }
  }
}
  
//////////////////////////////////////////////////////////////////////////////

template <typename SCHEME, typename POLYREC, typename LIMITER>
__global__ void computeFluxJacobianKernel(typename SCHEME::BASE::template DeviceConfigOptions<NOTYPE>* dcof,
					  typename POLYREC::BASE::template DeviceConfigOptions<NOTYPE>* dcor,
					  typename LIMITER::BASE::template DeviceConfigOptions<NOTYPE>* dcol,
					  typename NumericalJacobian::DeviceConfigOptions<typename SCHEME::MODEL>* dcon,
					  typename SCHEME::MODEL::PTERM::template DeviceConfigOptions<NOTYPE>* dcop,
					  const CFuint nbCells,
					  const CFuint startCellID,
					  CFreal* states, 
					  CFreal* nodes,
					  CFreal* centerNodes,
					  CFreal* ghostStates,
					  CFreal* ghostNodes,
					  CFreal* blockJacob,
					  CFuint* blockStart,
					  CFreal* uX,
					  CFreal* uY,
					  CFreal* uZ,
					  CFreal* limiter,
					  CFreal* updateCoeff, 
					  CFreal* rhs,
					  CFreal* normals,
					  CFint* isOutward,
					  const CFuint* cellInfo,
					  const CFuint* cellStencil,
					  const CFuint* cellFaces,
					  const CFuint* cellNodes,
					  const CFint* neighborTypes,
					  const Framework::CellConn* cellConn)
{  
  typedef typename SCHEME::MODEL PHYS;

  // each thread takes care of computing the gradient for one single cell
  const int cellID = threadIdx.x + blockIdx.x*blockDim.x + startCellID;
  //printf("Inside computeFluxJacobianKernel \n \n");
  if (cellID < nbCells) {
    
    // compute and store cell gradients at once 
    POLYREC polyRec(dcor);
    SCHEME  fluxScheme(dcof);
    LIMITER limt(dcol);
    NumericalJacobian::DeviceFunc<PHYS> numJacob(dcon);
    KernelData<CFreal> kd (nbCells, states, nodes, centerNodes, ghostStates, 
			   ghostNodes, updateCoeff, rhs, normals, uX, uY, uZ, isOutward);
    
    // compute all cell quadrature points at once (array size can be overestimated in 3D)
    const CFuint MAX_NB_FACES = PHYS::DIM*2;
    CFreal midFaceCoord[PHYS::DIM*MAX_NB_FACES];
    CudaEnv::CFVec<CFreal,PHYS::NBEQS> fluxDiff;
    CudaEnv::CFVec<CFreal,PHYS::NBEQS> resBkp;
    FluxData<PHYS> currFd; currFd.initialize();
    typename SCHEME::MODEL pmodel(dcop);
    
    // reset the rhs and update coefficients to 0
    CudaEnv::CFVecSlice<CFreal,PHYS::NBEQS> res(&rhs[cellID*PHYS::NBEQS]);
    res = 0.;
    updateCoeff[cellID] = 0.;
    
    CellData cells(nbCells, cellInfo, cellStencil, cellFaces, cellNodes, neighborTypes, cellConn);
    CellData::Itr cell = cells.getItr(cellID);
    const CFuint nbFacesInCell = cell.getNbActiveFacesInCell();
    const CFuint nbRows = nbFacesInCell + 1;
    const CFuint bStartCellID = blockStart[cellID];
    
    // this block accumulator represents a column block (nbFaces+1 x 1)
    BlockAccumulatorBaseCUDA acc(nbRows, 1, PHYS::NBEQS, &blockJacob[bStartCellID]);
    acc.reset();
    
    // compute the face flux and flux numerical jacobian within the same loop
    for (CFuint f = 0; f < nbFacesInCell; ++f) { 
      const CFint stype = cell.getNeighborType(f);
      
      if (stype != 0) { // skip all partition faces
	const CFuint stateID = cell.getNeighborID(f);
	setFluxData(f, stype, stateID, cellID, &kd, &currFd, cellFaces);
	
	// compute face quadrature points (face centroids)
	CFreal* faceCenters = &midFaceCoord[f*PHYS::DIM];
	computeFaceCentroid<PHYS>(&cell, f, nodes, faceCenters);
	
	// extrapolate solution on quadrature points on both sides of the face
	polyRec.extrapolateOnFace(&currFd, faceCenters, uX, uY, uZ, limiter);
	fluxScheme(&currFd, &pmodel); // compute the convective flux across the face
	
	for (CFuint iEq = 0; iEq < PHYS::NBEQS; ++iEq) {
	  const CFreal value = currFd.getResidual()[iEq];
	  res[iEq]   -= value;  // update the residual 
          //printf("resFlux [%d] \t %f \n", iEq, res[iEq]);
	  resBkp[iEq] = value;  // backup the current face-based residual
	}
	
	// update the update coefficient
	updateCoeff[cellID] += currFd.getUpdateCoeff();
		
	// only contribution from internal faces is computed here  
	if (stype > 0) { 	  
	  currFd.setIsPerturb(true);
	  // flux jacobian computation
	  for (CFuint iVar = 0; iVar < PHYS::NBEQS; ++iVar) {
	    // here we perturb the current variable for the left cell state
	    numJacob.perturb(iVar, &currFd.getState(LEFT)[iVar]);
	    
	    // extrapolate solution on quadrature points on both sides of the face
	    const CFreal rstateBkpL = currFd.getRstate(LEFT)[iVar];
	    polyRec.extrapolateOnFace(iVar, &currFd, faceCenters, uX, uY, uZ, limiter);
	    fluxScheme(&currFd, &pmodel); // compute the convective flux across the face
	    
	    // compute the numerical jacobian of the flux
	    CudaEnv::CFVecSlice<CFreal,PHYS::NBEQS> resPert(currFd.getResidual());
	    numJacob.computeDerivative(&resBkp, &resPert, &fluxDiff);
	    
	    // contribution to the row corresponding of the current cell
	    // this subblock gets all contributions from all face cells
	    acc.addValues(0, 0, iVar, &fluxDiff[0]);
	    
	    // contribution to row corresponding to the f+1 cell: 
	    // this is the flux jacobian contribution for the neighbor cells
	    // due to the currently perturbed cell state and is opposite in sign
	    // because the outward normal for neighbors is inward for the current cell
	    fluxDiff *= -1.0;
	    acc.addValues(f+1, 0, iVar, &fluxDiff[0]); 
	    
	    // restore perturbed states
	    currFd.getRstate(LEFT)[iVar] = rstateBkpL;
	    numJacob.restore(&currFd.getState(LEFT)[iVar]);
	  }
	  
	  currFd.setIsPerturb(false);
	}
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////////////

template <typename SOURCE>
__global__ void computeSource(    CFreal* volume,
                                  typename SOURCE::BASE::template DeviceConfigOptions<NOTYPE>* dcos,
				  typename SOURCE::MODEL::PTERM::template DeviceConfigOptions<NOTYPE>* dcop,
                                  typename NumericalJacobian::DeviceConfigOptions<typename SOURCE::MODEL>* dcon,
				  const CFuint nbCells,
 				  const CFuint startCellID,
				  CFreal* states, 
				  CFreal* centerNodes,
				  CFreal* ghostStates,
				  CFreal* ghostNodes,
				  CFreal* blockJacob,
			          CFuint* blockStart,
				  CFreal* uX,
				  CFreal* uY,
				  CFreal* uZ,
				  CFreal* limiter,
				  CFreal* updateCoeff, 
				  CFreal* rhs,
				  CFreal* normals,
				  CFint* isOutward,
				  const CFuint* cellInfo,
				  const CFuint* cellStencil,
				  const CFuint* cellFaces,
				  const CFuint* cellNodes,
				  const CFint*  neighborTypes,
				  const Framework::CellConn* cellConn,
				  CFreal ResFactor, bool IsAxisymmetric)
{
  const int cellID = threadIdx.x + blockIdx.x*blockDim.x + startCellID;

  if (cellID < nbCells) { 

    const CFuint nbEqs = SOURCE::MODEL::NBEQS;
    NumericalJacobian::DeviceFunc<typename SOURCE::MODEL> numJacob(dcon);

    const CFuint bStartCellID = blockStart[cellID];

    //Block accumulator where the source jacobian will be stored
    BlockAccumulatorBaseCUDA acc(1, 1, SOURCE::MODEL::NBEQS, &blockJacob[bStartCellID]);

    CFreal invR = 1.0;
    if (IsAxisymmetric) {     
      printf("IsAxisymmetric not implemented \n");
      //invR /= abs(currCell->getState(0)->getCoordinates()[YY]); //It just need the y-component (easy addition)
    }

    CFreal factor = invR*volume[cellID]*ResFactor;

    //Arrays needed for the source jacobian
    CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> SourceDiff;
    CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> sourceBkp;
    CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> source;
    source = 0.0;
    sourceBkp = 0.0;    

    SOURCE Source(dcos);
    typename SOURCE::MODEL pmodel(dcop);

    
    CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> stateBkp;
    CudaEnv::CFVecSlice<CFreal,SOURCE::MODEL::NBEQS> state(&states[cellID*SOURCE::MODEL::NBEQS]);
    Source(&state[0], &pmodel, &source[0]);   //Source term computation
    source *= factor;    

    CudaEnv::CFVecSlice<CFreal,SOURCE::MODEL::NBEQS> res(&rhs[cellID*SOURCE::MODEL::NBEQS]);
    for (CFuint iEq = 0; iEq < nbEqs; ++iEq) {  //Add source term to the RHS and create backup for the derivatives
       const CFreal value = source[iEq]; 
       res[iEq] += value;  
       sourceBkp[iEq] = value;
       stateBkp[iEq] = state[iEq];
    }
   
    /////////////////////////////////////////////

    //Source Jacobian computation
    for (CFuint iVar = 0; iVar < SOURCE::MODEL::NBEQS; ++iVar) {
      // here we perturb the current variable for the state
      numJacob.perturb(iVar, &state[iVar]);
	    
      //Computation of the source with the perturbed state
      Source(&state[0], &pmodel, &source[0]);
	    
      //Compute the numerical derivative
      source *= factor;
      SourceDiff = 0.0; 
      numJacob.computeDerivative(&sourceBkp, &source, &SourceDiff);
      SourceDiff *= factor;
       
//      printf("sourceBkp %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \n",
// sourceBkp[0],sourceBkp[1],sourceBkp[2],sourceBkp[3],sourceBkp[4],sourceBkp[5],sourceBkp[6],sourceBkp[7],sourceBkp[8],sourceBkp[9],
// sourceBkp[10],sourceBkp[11],sourceBkp[12],sourceBkp[13],sourceBkp[14],sourceBkp[15]);


      acc.addValues(0, 0, iVar, &SourceDiff[0]);   //Add values to the block accumulator

      // restore perturbed states
      state[iVar] = stateBkp[iVar];
    }
   
  //////////////////////////////

  
  }
}
//////////////////////////////////////////////////////////





  
template <typename SCHEME, typename SOURCE, typename POLYREC, typename LIMITER>
void computeFluxSourceJacobianCPU(typename SCHEME::BASE::template DeviceConfigOptions<NOTYPE>* dcof,
			    typename POLYREC::BASE::template DeviceConfigOptions<NOTYPE>* dcor,
			    typename LIMITER::BASE::template DeviceConfigOptions<NOTYPE>* dcol,
			    typename NumericalJacobian::DeviceConfigOptions<typename SCHEME::MODEL>* dcon,
			    typename SCHEME::MODEL::PTERM::template DeviceConfigOptions<NOTYPE>* dcop,
                            typename SOURCE::BASE::template DeviceConfigOptions<NOTYPE>* dcos,
			    const CFuint nbCells,
			    CFreal* states, 
                            CFreal* volumes,
			    CFreal* nodes,
			    CFreal* centerNodes,
			    CFreal* ghostStates,
			    CFreal* ghostNodes, 
			    CFreal* blockJacob,
			    CFuint* blockStart,
			    CFreal* uX,
			    CFreal* uY,
			    CFreal* uZ,
			    CFreal* limiter,
			    CFreal* updateCoeff, 
			    CFreal* rhs,
			    CFreal* normals,
			    CFint* isOutward,
			    const CFuint* cellInfo,
			    const CFuint* cellStencil,
			    const CFuint* cellFaces,
			    const CFuint* cellNodes,
			    const CFint* neighborTypes,
			    const Framework::CellConn* cellConn,
                            CFreal ResFactor, bool IsAxisymmetric)
{ 
  using namespace std;
  
  typedef typename SCHEME::MODEL PHYS;
  
  CudaEnv::CudaTimer& timer = CudaEnv::CudaTimer::getInstance();
  timer.start();
  
  FluxData<PHYS> fd; fd.initialize();
  FluxData<PHYS>* currFd = &fd;
  cf_assert(currFd != CFNULL);
  SCHEME fluxScheme(dcof);
  POLYREC polyRec(dcor);
  LIMITER limt(dcol);
  
  CellData cells(nbCells, cellInfo, cellStencil, cellFaces, cellNodes, neighborTypes, cellConn);
  KernelData<CFreal> kd(nbCells, states, nodes, centerNodes, ghostStates, ghostNodes, updateCoeff, 
			rhs, normals, uX, uY, uZ, isOutward);
  
  const CFuint MAX_NB_FACES = PHYS::DIM*2;
  CFreal midFaceCoord[PHYS::DIM*MAX_NB_FACES];
  CudaEnv::CFVec<CFreal,PHYS::NBEQS> fluxDiff;
  CudaEnv::CFVec<CFreal,PHYS::NBEQS> resBkp;
  CudaEnv::CFVec<CFreal,PHYS::NBEQS> tmpLimiter;
  NumericalJacobian::DeviceFunc<PHYS> numJacob(dcon);
  PHYS pmodel(dcop);
  

  CudaEnv::CFVec<CFreal,PHYS::NBEQS> source;
  CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> SourceDiff;
  CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> sourceBkp;
  SOURCE Source(dcos);



  // compute the cell-based gradients
  for (CFuint cellID = 0; cellID < nbCells; ++cellID) {
    CellData::Itr cell = cells.getItr(cellID);
    polyRec.computeGradients(&states[cellID*PHYS::NBEQS], &centerNodes[cellID*PHYS::DIM], &kd, &cell);
  }
  
  // printGradients<PHYS::NBEQS>(uX, uY, uZ, nbCells);
  CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::computeFluxJacobianCPU() => computing gradients took " << timer.elapsed() << " s\n");
  timer.start();
  
  // compute the cell based limiter
  for (CellData::Itr cell = cells.begin(); cell <= cells.end(); ++cell) {
    // compute all cell quadrature points at once (size of this array is overestimated)
    const CFuint nbFacesInCell = cell.getNbFacesInCell();
    for (CFuint f = 0; f < nbFacesInCell; ++f) { 
      computeFaceCentroid<PHYS>(&cell, f, nodes, &midFaceCoord[f*PHYS::DIM]);
    }
    
    const CFuint cellID = cell.getCellID();
    if (dcor->currRes > dcor->limitRes && (dcor->limitIter > 0 && dcor->currIter < dcor->limitIter)) {	
      // compute cell-based limiter
      limt.limit(&kd, &cell, &midFaceCoord[0], &limiter[cellID*PHYS::NBEQS]);
    }
    else {
      if (!dcor->freezeLimiter) {
	// historical modification of the limiter
	limt.limit(&kd, &cell, &midFaceCoord[0], &tmpLimiter[0]);
	CFuint currID = cellID*PHYS::NBEQS;
	for (CFuint iVar = 0; iVar < PHYS::NBEQS; ++iVar, ++currID) {
	  limiter[currID] = min(tmpLimiter[iVar],limiter[currID]);
	}
      }
    }
  }
  
  // printLimiter<PHYS::NBEQS>(limiter, nbCells);
  
  CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::computeFluxJacobianCPU() => computing limiter took " << timer.elapsed() << " s\n");
  timer.start();
  
  // compute the fluxes and the jacobian
  for (CellData::Itr cell = cells.begin(); cell <= cells.end(); ++cell) {
    // reset the rhs and update coefficients to 0
    const CFuint cellID = cell.getCellID();
    CudaEnv::CFVecSlice<CFreal,PHYS::NBEQS> res(&rhs[cellID*PHYS::NBEQS]);
    res = 0.;
    updateCoeff[cellID] = 0.;
    
    const CFuint nbFacesInCell = cell.getNbActiveFacesInCell();
    const CFuint nbRows = nbFacesInCell + 1;
    const CFuint bStartCellID = blockStart[cellID];
        
    // this block accumulator represents a column block (nbFaces+1 x 1)
    BlockAccumulatorBaseCUDA acc(nbRows, 1, PHYS::NBEQS, &blockJacob[bStartCellID]);
    acc.reset();
    
    



    for (CFuint f = 0; f < nbFacesInCell; ++f) { 
      const CFint stype = cell.getNeighborType(f);
      if (stype != 0) { // skip all partition faces
	const CFuint stateID =  cell.getNeighborID(f);
	setFluxData(f, stype, stateID, cellID, &kd, currFd, cellFaces);
	
	// compute face quadrature points (centroid)
	CFreal* faceCenters = &midFaceCoord[f*PHYS::DIM];
	computeFaceCentroid<PHYS>(&cell, f, nodes, faceCenters);
	
	// extrapolate solution on quadrature points on both sides of the face
	polyRec.extrapolateOnFace(currFd, faceCenters, uX, uY, uZ, limiter);
		

	
	
	fluxScheme(currFd, &pmodel); // compute the convective flux across the face
		

	for (CFuint iEq = 0; iEq < PHYS::NBEQS; ++iEq) {
	  const CFreal value = currFd->getResidual()[iEq];
	  res[iEq]   -= value;  // update the residual 
	  resBkp[iEq] = value;  // backup the current face-based residual
	}
	
	// update the update coefficient
	updateCoeff[cellID] += currFd->getUpdateCoeff();
	
        /////////////////////////////////////////////////
 



	// only contribution from internal faces is computed here  
	if (stype > 0) { 
	  currFd->setIsPerturb(true);
	  
	  // flux jacobian computation
	  for (CFuint iVar = 0; iVar < PHYS::NBEQS; ++iVar) {
	    // here we perturb the current variable for the left cell state
	    numJacob.perturb(iVar, &currFd->getState(LEFT)[iVar]);
	    
	    // extrapolate solution on quadrature points on both sides of the face
	    const CFreal rstateBkpL = currFd->getRstate(LEFT)[iVar];
	    polyRec.extrapolateOnFace(iVar, currFd, faceCenters, uX, uY, uZ, limiter);
	    fluxScheme(currFd, &pmodel); // compute the convective flux across the face
	    
	    // compute the numerical jacobian of the flux
	    CudaEnv::CFVecSlice<CFreal,PHYS::NBEQS> resPert(currFd->getResidual());
	    numJacob.computeDerivative(&resBkp, &resPert, &fluxDiff);
	    
	    
//	    printf("fluxDiff %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \n",
// fluxDiff[0],fluxDiff[1],fluxDiff[2],fluxDiff[3],fluxDiff[4],fluxDiff[5],fluxDiff[6],fluxDiff[7],fluxDiff[8],fluxDiff[9],
// fluxDiff[10],fluxDiff[11],fluxDiff[12],fluxDiff[13],fluxDiff[14],fluxDiff[15]);

	    // flux is computed with the outward normal, so the sign is correct here
	    // contribution to the row corresponding of the current cell
	    // this subblock gets all contributions from all face cells
	    acc.addValues(0, 0, iVar, &fluxDiff[0]);
	    
	    // contribution to row corresponding to the f+1 cell: 
	    // this is the flux jacobian contribution for the neighbor cells
	    // due to the currently perturbed cell state and is opposite in sign
	    // because the outward normal for neighbors is inward for the current cell
	    fluxDiff *= -1.0;

	    acc.addValues(f+1, 0, iVar, &fluxDiff[0]);   
	    
	    // restore perturbed states
	    currFd->getRstate(LEFT)[iVar] = rstateBkpL;
	    numJacob.restore(&currFd->getState(LEFT)[iVar]);
	  }
	  	  
	  currFd->setIsPerturb(false);
	}
      }
    }

    
    //Source computation
    source = 0.;
    
 
    //CellData cells(nbCells, cellInfo, cellStencil, cellFaces, cellNodes, neighborTypes, cellConn);
    //CellData::Itr cell = cells.getItr(cellID);
    CudaEnv::CFVec<CFreal,SOURCE::MODEL::NBEQS> stateBkp;
    CudaEnv::CFVecSlice<CFreal,PHYS::NBEQS> state(&states[cellID*PHYS::NBEQS]);
    Source(&state[0], &pmodel, &source[0]);

    CFreal invR = 1.0;
    if (IsAxisymmetric) {     //Is this used?
      //invR /= abs(currCell->getState(0)->getCoordinates()[YY]);  //This can be done using cellConn or states? idk
    }
    CFreal factor = invR*volumes[cellID]*ResFactor;     

    source *= factor;
    for (CFuint iEq = 0; iEq < PHYS::NBEQS; ++iEq) { 
      res[iEq] += source[iEq];   
      //printf("source [%d] \t %e \n", iEq, source[iEq]);
       sourceBkp[iEq] = source[iEq];
       stateBkp[iEq] = state[iEq];
    }
    
    /////////////////////////////////////////////

    //Source Jacobian computation
    for (CFuint iVar = 0; iVar < SOURCE::MODEL::NBEQS; ++iVar) {
      // here we perturb the current variable for the state
      numJacob.perturb(iVar, &state[iVar]);
	    
      //Computation of the source with the perturbed state
      Source(&state[0], &pmodel, &source[0]);
	    
      //Compute the numerical derivative
      source *= factor;
      SourceDiff = 0.0; 
      numJacob.computeDerivative(&sourceBkp, &source, &SourceDiff);
	    
      // contribution to the row corresponding of the current cell
      // this subblock gets all contributions from all face cells
      SourceDiff *= factor;  //Not sure if I should do this.. I think that it is needed.
       
//      printf("SourceDiff %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \t %e \n",
// SourceDiff[0],SourceDiff[1],SourceDiff[2],SourceDiff[3],SourceDiff[4],SourceDiff[5],SourceDiff[6],SourceDiff[7],SourceDiff[8],SourceDiff[9],
// SourceDiff[10],SourceDiff[11],SourceDiff[12],SourceDiff[13],SourceDiff[14],SourceDiff[15]);


      acc.addValues(0, 0, iVar, &SourceDiff[0]);   //From FVMCC_ComputeRhsJacob.cxx

      // restore perturbed states
      state = stateBkp;
      numJacob.restore(&state[iVar]);
    }

  //////////////////////////////


  } 
  
  CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::computeFluxJacobianCPU()  took " << timer.elapsed() << " s\n");
}

//////////////////////////////////////////////////////////////////////////////

template <typename SCHEME, typename PHYSICS, typename SOURCE, typename POLYREC, typename LIMITER, CFuint NB_BLOCK_THREADS>
void FVMCC_ComputeSourceRhsJacobCell<SCHEME,PHYSICS,SOURCE,POLYREC,LIMITER,NB_BLOCK_THREADS>::execute()
{
  using namespace std;
  using namespace COOLFluiD::Framework;
  using namespace COOLFluiD::Common;
  
  CFTRACEBEGIN;
  
  CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() START\n");
  
  initializeComputationRHS();
  
  const CFuint nbCells = this->socket_states.getDataHandle().size();
  cf_assert(nbCells > 0);
  DataHandle<CFreal> updateCoeff = this->socket_updateCoeff.getDataHandle();
  DataHandle<CFreal> rhs = this->socket_rhs.getDataHandle();
  DataHandle<CFreal> normals = this->socket_normals.getDataHandle();
  DataHandle<CFint> isOutward = this->socket_isOutward.getDataHandle();  
  
  SafePtr<SCHEME> lf = this->getMethodData().getFluxSplitter().template d_castTo<SCHEME>();
  SafePtr<POLYREC> pr = this->getMethodData().getPolyReconstructor().template d_castTo<POLYREC>();
  SafePtr<LIMITER> lm = this->getMethodData().getLimiter().template d_castTo<LIMITER>();
  SafePtr<typename PHYSICS::PTERM> phys = PhysicalModelStack::getActive()->getImplementor()->
    getConvectiveTerm().d_castTo<typename PHYSICS::PTERM>();
  

  
  //Added for Source 
  
  SelfRegistPtr<SOURCE> ls1  = (*this->getMethodData().getSourceTermComputer())[0].template d_castTo<SOURCE>(); //Only valid if there is only one source term!!
  SafePtr<SOURCE> ls = ls1.getPtr();
  typedef typename SOURCE::template DeviceFunc<GPU, PHYSICS> SourceTerm; 



  typedef typename SCHEME::template  DeviceFunc<GPU, PHYSICS> FluxScheme;  
  typedef typename POLYREC::template DeviceFunc<PHYSICS> PolyRec;  
  typedef typename LIMITER::template DeviceFunc<PHYSICS> Limiter;  
  
  CudaEnv::CudaTimer& timer = CudaEnv::CudaTimer::getInstance();


  if (this->m_onGPU) {
    
    timer.start();
    // copy of data that change at every iteration
    this->socket_states.getDataHandle().getGlobalArray()->put(); 
    this->m_ghostStates.put();
    this->socket_volumes.getDataHandle().getLocalArray()->put(); 

    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => CPU-->GPU data transfer took " << timer.elapsed() << " s\n");
    timer.start();
    
    ConfigOptionPtr<POLYREC, NOTYPE, GPU> dcor(pr);
    ConfigOptionPtr<LIMITER, NOTYPE, GPU> dcol(lm);
    ConfigOptionPtr<SCHEME,  NOTYPE, GPU> dcof(lf);
    ConfigOptionPtr<typename PHYSICS::PTERM, NOTYPE, GPU> dcop(phys);
    //Added for Source    
    ConfigOptionPtr<SOURCE, NOTYPE, GPU> dcos(ls);
    const CFuint blocksPerGrid = CudaEnv::CudaDeviceManager::getInstance().getBlocksPerGrid(nbCells);
    const CFuint nThreads = CudaEnv::CudaDeviceManager::getInstance().getNThreads();
    
    CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() <blocksPerGrid, nThreads> = <" 
	  <<  blocksPerGrid << "," << nThreads << ">\n");
    
    //dim3 blocks(this->m_nbBlocksPerGridX, this->m_nbBlocksPerGridY);
    
    timer.start();
    
    // compute the cell-based gradients
    computeGradientsKernel<PHYSICS, PolyRec> <<<blocksPerGrid,nThreads>>> 
      (dcor.getPtr(),
       nbCells,
       this->socket_states.getDataHandle().getGlobalArray()->ptrDev(), 
       this->socket_nodes.getDataHandle().getGlobalArray()->ptrDev(),
       this->m_centerNodes.ptrDev(), 
       this->m_ghostStates.ptrDev(),
       this->m_ghostNodes.ptrDev(),
       this->socket_uX.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_uY.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_uZ.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_limiter.getDataHandle().getLocalArray()->ptrDev(),
       updateCoeff.getLocalArray()->ptrDev(), 
       rhs.getLocalArray()->ptrDev(),
       normals.getLocalArray()->ptrDev(),
       isOutward.getLocalArray()->ptrDev(),
       this->m_cellInfo.ptrDev(),
       this->m_cellStencil.ptrDev(),
       this->m_cellFaces->getPtr()->ptrDev(),
       this->m_cellNodes->getPtr()->ptrDev(),
       this->m_neighborTypes.ptrDev(),
       this->m_cellConn.ptrDev());
    
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => computeGradientsKernel took " << timer.elapsed() << " s\n");
    
    timer.start();
    
    // compute the limiter in each cell
    computeLimiterKernel<PHYSICS, PolyRec, Limiter> <<<blocksPerGrid,nThreads>>> 
      (dcol.getPtr(), 
       dcor.getPtr(), 
       nbCells,
       this->socket_states.getDataHandle().getGlobalArray()->ptrDev(), 
       this->socket_nodes.getDataHandle().getGlobalArray()->ptrDev(),
       this->m_centerNodes.ptrDev(), 
       this->m_ghostStates.ptrDev(),
       this->m_ghostNodes.ptrDev(),
       this->socket_uX.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_uY.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_uZ.getDataHandle().getLocalArray()->ptrDev(),
       this->socket_limiter.getDataHandle().getLocalArray()->ptrDev(),
       updateCoeff.getLocalArray()->ptrDev(), 
       rhs.getLocalArray()->ptrDev(),
       normals.getLocalArray()->ptrDev(),
       isOutward.getLocalArray()->ptrDev(),
       this->m_cellInfo.ptrDev(),
       this->m_cellStencil.ptrDev(),
       this->m_cellFaces->getPtr()->ptrDev(),
       this->m_cellNodes->getPtr()->ptrDev(),
       this->m_neighborTypes.ptrDev(),
       this->m_cellConn.ptrDev());
    
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => computeLimiterKernel took " << timer.elapsed() << " s\n");
    
    timer.start();
    // compute the flux jacobian in each cell
    CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() => Configuring method \n");
    ConfigOptionPtr<NumericalJacobian, PHYSICS, GPU> dcon
      (&this->getMethodData().getNumericalJacobian());
    CFreal startCellID = 0;
    CFreal FluxTime = 0;
    CFreal SourceTime = 0;
    CFreal UpdateSystemTime = 0;
    
    CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() => End of Configuring method \n");
    for (CFuint s = 0; s < m_nbCellsInKernel.size(); ++s) {
      CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() => loop " << s << " of " << m_nbCellsInKernel.size() << "\n");
      computeFluxJacobianKernel<FluxScheme, PolyRec, Limiter> <<<m_nbKernelBlocks,nThreads>>> 
	(dcof.getPtr(),
	 dcor.getPtr(),
	 dcol.getPtr(),
	 dcon.getPtr(),
	 dcop.getPtr(),
	 nbCells,
	 startCellID,
	 this->socket_states.getDataHandle().getGlobalArray()->ptrDev(), 
	 this->socket_nodes.getDataHandle().getGlobalArray()->ptrDev(),
	 this->m_centerNodes.ptrDev(), 
	 this->m_ghostStates.ptrDev(),
	 this->m_ghostNodes.ptrDev(),
	 m_blockJacobians.ptrDev(), 
	 m_blockStart.ptrDev(),
	 this->socket_uX.getDataHandle().getLocalArray()->ptrDev(),
	 this->socket_uY.getDataHandle().getLocalArray()->ptrDev(),
	 this->socket_uZ.getDataHandle().getLocalArray()->ptrDev(),
	 this->socket_limiter.getDataHandle().getLocalArray()->ptrDev(),
	 updateCoeff.getLocalArray()->ptrDev(), 
	 rhs.getLocalArray()->ptrDev(),
	 normals.getLocalArray()->ptrDev(),
	 isOutward.getLocalArray()->ptrDev(),
	 this->m_cellInfo.ptrDev(),
	 this->m_cellStencil.ptrDev(),
	 this->m_cellFaces->getPtr()->ptrDev(),
	 this->m_cellNodes->getPtr()->ptrDev(),
	 this->m_neighborTypes.ptrDev(),
	 this->m_cellConn.ptrDev());
      
      FluxTime += timer.elapsed();
      timer.start();

      CFreal ResFactor = this->getMethodData().getResFactor(); //Default = 1
      bool IsAxisymmetric = this->getMethodData().isAxisymmetric(); //Default = false
      computeSource<SourceTerm> <<<blocksPerGrid,nThreads>>>
        (this->socket_volumes.getDataHandle().getLocalArray()->ptrDev(),
         dcos.getPtr(),
         dcop.getPtr(),
         dcon.getPtr(),
         nbCells,
         startCellID,
         this->socket_states.getDataHandle().getGlobalArray()->ptrDev(), 
         this->m_centerNodes.ptrDev(), 
         this->m_ghostStates.ptrDev(),
         this->m_ghostNodes.ptrDev(),
	 m_blockJacobians.ptrDev(), 
	 m_blockStart.ptrDev(),
         this->socket_uX.getDataHandle().getLocalArray()->ptrDev(),
         this->socket_uY.getDataHandle().getLocalArray()->ptrDev(),
         this->socket_uZ.getDataHandle().getLocalArray()->ptrDev(),
         this->socket_limiter.getDataHandle().getLocalArray()->ptrDev(),
         updateCoeff.getLocalArray()->ptrDev(), 
         rhs.getLocalArray()->ptrDev(),
         normals.getLocalArray()->ptrDev(),
         isOutward.getLocalArray()->ptrDev(),
         this->m_cellInfo.ptrDev(),
         this->m_cellStencil.ptrDev(),
         this->m_cellFaces->getPtr()->ptrDev(),
         this->m_cellNodes->getPtr()->ptrDev(),
         this->m_neighborTypes.ptrDev(),
         this->m_cellConn.ptrDev(),
         ResFactor, 
         IsAxisymmetric);



      SourceTime += timer.elapsed();      
      timer.start();
      m_blockJacobians.get();
      // update the portion of system matrix computed by this kernel
      updateSystemMatrix(s);
      startCellID += m_nbCellsInKernel[s];
      UpdateSystemTime += timer.elapsed();
      timer.start();
    }
    
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => computeFluxJacobianKernel took " << FluxTime << " s\n");
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => computeSourceTerm took " << SourceTime << "\n");
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => updateSystemMatrix took " << UpdateSystemTime << "\n");

    rhs.getLocalArray()->get();
    updateCoeff.getLocalArray()->get();
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => GPU-->CPU data transfer took " << timer.elapsed() << " s\n");
  }
  else {
    ConfigOptionPtr<SCHEME>  dcof(lf);
    ConfigOptionPtr<POLYREC> dcor(pr);
    ConfigOptionPtr<LIMITER> dcol(lm);
    ConfigOptionPtr<NumericalJacobian, PHYSICS> dcon(&this->getMethodData().getNumericalJacobian());
    ConfigOptionPtr<typename PHYSICS::PTERM> dcop(phys);
    ConfigOptionPtr<SOURCE> dcos(ls);
    
    CFreal ResFactor = this->getMethodData().getResFactor(); //Default = 1
    bool IsAxisymmetric = this->getMethodData().isAxisymmetric(); //Default = false
    
    computeFluxSourceJacobianCPU<FluxScheme, SourceTerm, PolyRec, Limiter>
      (dcof.getPtr(),
       dcor.getPtr(),
       dcol.getPtr(),
       dcon.getPtr(),
       dcop.getPtr(),
       dcos.getPtr(),
       nbCells,
       this->socket_states.getDataHandle().getGlobalArray()->ptr(), 
       this->socket_volumes.getDataHandle().getLocalArray()->ptr(),
       this->socket_nodes.getDataHandle().getGlobalArray()->ptr(),
       this->m_centerNodes.ptr(), 
       this->m_ghostStates.ptr(),
       this->m_ghostNodes.ptr(),
       m_blockJacobians.ptr(), 
       m_blockStart.ptr(),
       this->socket_uX.getDataHandle().getLocalArray()->ptr(),
       this->socket_uY.getDataHandle().getLocalArray()->ptr(),
       this->socket_uZ.getDataHandle().getLocalArray()->ptr(),
       this->socket_limiter.getDataHandle().getLocalArray()->ptr(),
       updateCoeff.getLocalArray()->ptr(), 
       rhs.getLocalArray()->ptr(),
       normals.getLocalArray()->ptr(),
       isOutward.getLocalArray()->ptr(),
       this->m_cellInfo.ptr(),
       this->m_cellStencil.ptr(),
       this->m_cellFaces->getPtr()->ptr(),
       this->m_cellNodes->getPtr()->ptr(),
       this->m_neighborTypes.ptr(),
       this->m_cellConn.ptr(),
       ResFactor, IsAxisymmetric);
    timer.start();
    // update the system matrix
    updateSystemMatrix(0);
    CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => updateSystemMatrix took " << timer.elapsed() << "\n");
  }
  
  timer.start();
  // compute flux jacobians on boundaries
  executeBC();
  CFLog(NOTICE, "FVMCC_ComputeSourceRhsJacobCell::execute() => executeBC() took " << timer.elapsed() << " s\n");

  finalizeComputationRHS();
  
  CFLog(VERBOSE, "FVMCC_ComputeSourceRhsJacobCell::execute() END\n");
  
 /* for (int i = 0; i < updateCoeff.size(); ++i) {
    // for (int i = 0; i < 10000; ++i) {
    std::cout << "updateCoeff[" << i << "] = " << updateCoeff[i]  << std::endl;
    std::cout << "rhs[" << i << "] = ";
    for (int j = 0; j < 9; ++j) {
      std::cout << rhs[i*9+j] << " ";
    }
    std::cout << std::endl;
  }*/
  
  // abort();
  
  CFTRACEEND;
}

//////////////////////////////////////////////////////////////////////////////

template <CFuint NBEQS>
void printGradients(CFreal* uX, CFreal* uY, CFreal* uZ, CFuint nbCells)
{  
  CFuint idxr = 0;
  for (CFuint cellID = 0; cellID < nbCells; ++cellID) {
    for (CFuint i = 0; i < NBEQS; ++i, ++idxr) {
      std::cout << "cellID["<< cellID << "], "<< i << " => UX (";
      std::cout.precision(12); std::cout << uX[idxr] << ", " << uY[idxr] << ", " << uZ[idxr] << ")\n";
    }
  } 
}

//////////////////////////////////////////////////////////////////////////////

template <CFuint NBEQS>
void printLimiter(CFreal* limiter, CFuint nbCells)
{ 
  CFuint idxl = 0;
  for (CFuint cellID = 0; cellID < nbCells; ++cellID) {
    std::cout << "cellID["<< cellID << "] => LIM (";
    for (CFuint i = 0; i < NBEQS; ++i, ++idxl) {
      std::cout.precision(12); std::cout << limiter[idxl] << " ";
    }
    std::cout << ")\n";
  }
}
 
//////////////////////////////////////////////////////////////////////////////

   } // namespace FiniteVolume
    
  } // namespace Numerics

} // namespace COOLFluiD