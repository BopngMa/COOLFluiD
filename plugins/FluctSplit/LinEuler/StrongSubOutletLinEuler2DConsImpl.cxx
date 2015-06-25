#include <numeric>
#include <algorithm>

#include "StrongSubOutletLinEuler2DConsImpl.hh"
#include "FluctSplit/CreateBoundaryNodalNormals.hh"
#include "FluctSplit/InwardNormalsData.hh"
#include "FluctSplit/FluctSplitLinEuler.hh"
#include "Framework/MeshData.hh"
#include "FluctSplit/FluctuationSplitData.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/NamespaceSwitcher.hh"
#include "MathTools/MatrixInverter.hh"
#include "Environment/ObjectProvider.hh"
#include "FluctSplit/SpaceTime_Splitter.hh"
#include "Framework/CFL.hh"
#include "MathTools/RealMatrix.hh"
#include "Framework/SubSystemStatus.hh"
#include "Framework/LSSMatrix.hh"
#include "Framework/BlockAccumulator.hh"
#include <boost/concept_check.hpp>


//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::MathTools;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Physics::LinearizedEuler;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

    namespace FluctSplit {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<StrongSubOutletLinEuler2DConsImpl, FluctuationSplitData, FluctSplitLinEulerModule> strongSubOutletLinEuler2DConsImplProvider("StrongSubOutletLinEuler2DConsImpl");

//////////////////////////////////////////////////////////////////////////////

StrongSubOutletLinEuler2DConsImpl::StrongSubOutletLinEuler2DConsImpl(const std::string& name) :
  FluctuationSplitCom(name),
  socket_normals("normals"),
  socket_faceNeighCell("faceNeighCell"),
  socket_rhs("rhs"),
  socket_states("states"),
  socket_nodes("nodes"),
  socket_isUpdated("isUpdated"),
  socket_updateCoeff("updateCoeff"),
  socket_volumes("volumes"),
  socket_pastStates("pastStates"),
  socket_isBState("isBState"),
  socket_bStatesNeighbors("bStatesNeighbors"),  
  m_Statenormals(0),
  m_kPast(0),
  _varSet(),
  bndNod2Elm(0),
  m_adimNormal(),
  m_adimCharNormal(),
  m_nbEqs(),
  m_kPlus(0),
  m_k(0)
{
}

//////////////////////////////////////////////////////////////////////////////

StrongSubOutletLinEuler2DConsImpl::~StrongSubOutletLinEuler2DConsImpl()
{
  if (isSetup()) unsetup();
}

//////////////////////////////////////////////////////////////////////////////

std::vector<Common::SafePtr<BaseDataSocketSink> >
StrongSubOutletLinEuler2DConsImpl::needsSockets()
{

  std::vector<Common::SafePtr<BaseDataSocketSink> > result;

  result.push_back(&socket_normals);
  result.push_back(&socket_faceNeighCell);
  result.push_back(&socket_rhs);
  result.push_back(&socket_states);
  result.push_back(&socket_nodes);
  result.push_back(&socket_isUpdated);
  result.push_back(&socket_updateCoeff);
  result.push_back(&socket_volumes);
  result.push_back(&socket_bStatesNeighbors);
  result.push_back(&socket_pastStates);
  result.push_back(&socket_isBState);

  return result;
}

//////////////////////////////////////////////////////////////////////////////

void StrongSubOutletLinEuler2DConsImpl::setup()
{
  FluctuationSplitCom::setup();

  m_nbEqs = PhysicalModelStack::getActive()->getNbEq();

// create boundary nodal normals, which pointing outwards
  _bcNormals.resize(getTrsList().size());
  _varSet->setup();

  CreateBoundaryNodalNormals obj(getMethodData().getStdTrsGeoBuilder());
  obj.setDataSockets(socket_normals,socket_faceNeighCell);
  obj.create(getTrsList(), _bcNormals);


/// create the cell connectivity

  // find the inner trs
  vector< SafePtr<TopologicalRegionSet> > trs = MeshDataStack::getActive()->getTrsList();
  SafePtr<TopologicalRegionSet> innerTrs=0;
  for (CFuint i = 0; i < trs.size(); ++i) {
    if (trs[i]->hasTag("inner")) {
      innerTrs = trs[i];
      break;
    }
  }
  if (innerTrs==0) Common::NoSuchValueException(FromHere(),"Trs with tag 'inner' not found.");

  // set up numbers
  DataHandle< Node*,GLOBAL > innerNodes = socket_nodes.getDataHandle();
  const CFuint innerNbNodes = innerNodes.size();
  CFVec<CFint> bndNodeGlobal2Local(innerNbNodes);
  const CFuint innerNbGeos=innerTrs->getLocalNbGeoEnts();
  const Common::SafePtr< Common::ConnectivityTable<CFuint> > innerGeo2Node=innerTrs->getGeo2NodesConn();

  // set the boundary trs
  SafePtr<TopologicalRegionSet> bndTrs = getCurrentTRS();

  // getting nodes and preliminary settings
  Common::SafePtr< std::vector< CFuint > > bndNodes = bndTrs->getNodesInTrs();
  const CFuint bndNbNodes= bndTrs->getNbNodesInTrs();
  bndNod2Elm.resize(bndNbNodes);
  bndNodeGlobal2Local=-1;

  // cycle all the nodes in the TRS to set isBndNode flag
  CFuint bndCheckNbNode=0;
  for (std::vector< CFuint >::iterator itd = bndNodes->begin(); itd != bndNodes->end(); ++itd)
     bndNodeGlobal2Local[*itd]=bndCheckNbNode++;

  // build boundary node -> innercells elem connectivity
  for (CFuint i=0; i<innerNbGeos; i++) {
    const CFuint innerGeoLocalID=innerTrs->getLocalGeoID(i);
    const CFuint innerNbGeoNodes=innerGeo2Node->nbCols(i);
    for (CFuint j=0; j<innerNbGeoNodes; j++)
      if (bndNodeGlobal2Local[(*innerGeo2Node)(i,j)]!=-1)
        bndNod2Elm[bndNodeGlobal2Local[(*innerGeo2Node)(i,j)]].push_back(innerGeoLocalID);
  }

/// create the boundary normals for the nodes
  DataHandle < Framework::State*, Framework::GLOBAL > states = socket_states.getDataHandle();
  DataHandle<bool> isBState = socket_isBState.getDataHandle();
  DataHandle< InwardNormalsData*> m_normals = socket_normals.getDataHandle();

//   prepares to loop over cells by getting the GeometricEntityPool
  Common::SafePtr<GeometricEntityPool<StdTrsGeoBuilder> >
    geoBuilder = getMethodData().getStdTrsGeoBuilder();
  StdTrsGeoBuilder::GeoData& geoData = geoBuilder->getDataGE();
  geoData.trs = MeshDataStack::getActive()->getTrs("InnerCells");

  Common::SafePtr< vector<CFuint> > statesIdx = getCurrentTRS()->getStatesInTrs();

  bndNodNorm.resize(bndNbNodes);

  for(CFuint i=0; i<bndNbNodes; i++)
    bndNodNorm[i].resize(2);
// go through the nodes


  for (CFuint iState = 0; iState < statesIdx->size(); ++iState) {
    const CFuint stateID = (*statesIdx)[iState];

// get the boundary elements containing the state
    vector <CFuint> cells = bndNod2Elm[iState];
    CFuint nbElemsOfState = cells.size();

    RealVector NodalNormal(2);
    NodalNormal = 0.0;

// loop over the elements
    for (CFuint elem=0; elem<nbElemsOfState; ++elem) {

        // build the GeometricEntity
        CFuint cellID = cells[elem];
        geoData.idx = cellID;
        GeometricEntity& cell = *geoBuilder->buildGE();
        vector<State*> *const statesInCell = cell.getStates();
        const CFuint nbStatesInCell = statesInCell->size();

        CFuint NrBoundNodes = 0;

        for (CFuint elemState=0; elemState<nbStatesInCell; ++elemState) {
           State *const currState = (*statesInCell)[elemState];
           CFuint currStateID = currState->getLocalID();

           if(isBState[currStateID])
             NrBoundNodes+= 1;
        }

        if (NrBoundNodes==2) {
          for (CFuint elemState=0; elemState<nbStatesInCell; ++elemState) {
            State *const currState = (*statesInCell)[elemState];
            CFuint currStateID = currState->getLocalID();

            if(!isBState[currStateID]) {
               NodalNormal[0] += m_normals[cellID]->getNodalNormComp(elemState,0);
               NodalNormal[1] += m_normals[cellID]->getNodalNormComp(elemState,1);

            }
          }
        }

      geoBuilder->releaseGE();

      }

   CFreal Nlength = 0.0;
   for (CFuint dim=0; dim<2; dim++)
    Nlength += NodalNormal[dim]*NodalNormal[dim];
   Nlength = sqrt(Nlength);


   NodalNormal/=-Nlength;

   bndNodNorm[iState] = NodalNormal;

// cout << bndNodNorm[iState] << "\t" << states[stateID]->getCoordinates() <<"\n";

  }


/// for the computation of the distribution matrix

  getMethodData().getDistributionData().computeBetas = true;

  CFuint m_maxNbStatesInCell = MeshDataStack::getActive()->Statistics().getMaxNbStatesInCell();

  m_kPlus.resize(m_maxNbStatesInCell);
  m_k.resize(m_maxNbStatesInCell);
  m_kPast.resize(m_maxNbStatesInCell);
  m_Statenormals.resize(m_maxNbStatesInCell);

  for (CFuint i = 0; i < m_maxNbStatesInCell; ++i) {
    m_Statenormals[i] = new RealVector(2);
  }

  m_adimNormal.resize(2);
  m_adimCharNormal.resize(2);

}

//////////////////////////////////////////////////////////////////////////////

void StrongSubOutletLinEuler2DConsImpl::executeOnTrs()
{
  
  SafePtr<LinearSystemSolver> lss =
    getMethodData().getLinearSystemSolver()[0];

  SafePtr<LSSMatrix> jacobMatrix = lss->getMatrix();

  // this should be an intermediate lightweight assembly !!!
  // it is needed because here you SET values while elsewhere
  // you ADD values
  jacobMatrix->flushAssembly();
  
  vector<RealVector>* bcNormalsInTrs = &(_bcNormals[getCurrentTrsID()]);

  DataHandle<CFreal> m_volumes = socket_volumes.getDataHandle();
  DataHandle< InwardNormalsData*> m_normals = socket_normals.getDataHandle();

  DataHandle<CFreal> rhs = socket_rhs.getDataHandle();
  DataHandle < Framework::State*, Framework::GLOBAL > states = socket_states.getDataHandle();
  DataHandle<bool> isUpdated = socket_isUpdated.getDataHandle();
  DataHandle<CFreal> updateCoeff = socket_updateCoeff.getDataHandle();
  DataHandle<bool> isBState = socket_isBState.getDataHandle();
  DataHandle< std::valarray<Framework::State*> > bStatesNeighbors = socket_bStatesNeighbors.getDataHandle();
  DataHandle<State*> pastStatesStorage = socket_pastStates.getDataHandle();

  // prepares to loop over cells by getting the GeometricEntityPool
  Common::SafePtr<GeometricEntityPool<StdTrsGeoBuilder> >
    geoBuilder = getMethodData().getStdTrsGeoBuilder();
  StdTrsGeoBuilder::GeoData& geoData = geoBuilder->getDataGE();
  geoData.trs = MeshDataStack::getActive()->getTrs("InnerCells");

  Common::SafePtr< vector<CFuint> > statesIdx = getCurrentTRS()->getStatesInTrs();

  const CFreal dt = SubSystemStatusStack::getActive()->getDT();
  
  // block accumulator 1*1
  auto_ptr<BlockAccumulator> acc(lss->createBlockAccumulator(1, 1, m_nbEqs));

   RealMatrix _jacob; 
   _jacob.resize(m_nbEqs, m_nbEqs);
	
    for(CFuint ii=0; ii<m_nbEqs; ii++) {
	  for(CFuint jj=0; jj<m_nbEqs; jj++) {
	    _jacob(ii,jj) = 0.0;
	  }
	}
	
    const RealVector& linearData = _varSet->getModel()->getPhysicalData();
    const CFreal c     = linearData[LinEulerTerm::c];
    const CFreal oneoverc = 1./c;
  
// go through all the states involved in the boundary trs
   for (CFuint iState = 0; iState < statesIdx->size(); ++iState) {
    const CFuint stateID = (*statesIdx)[iState];
    
// see if the state is updated already
    if (!isUpdated[stateID]) {
      const CFreal updateCoeffValue = updateCoeff[stateID];

      CFreal charResTwo = 0.0;
      CFreal charResThree = 0.0;
      RealVector Res(m_nbEqs);
      RealVector charRes(m_nbEqs);
      for (CFuint iEq = 0; iEq < m_nbEqs; ++iEq) {
        Res[iEq] = 0.;
        charRes[iEq] = 0.;
      }

      if (std::abs(updateCoeffValue) > MathTools::MathConsts::CFrealEps()) {

      // get the boundary elements containing the state
      vector <CFuint> cells = bndNod2Elm[iState];
      CFuint nbElemsOfState = cells.size();

      RealVector bcNormalState =  bndNodNorm[iState];

      CFreal ncharx = (bcNormalState)[0];
      CFreal nchary = (bcNormalState)[1];
      
      CFuint NNodes = 3*nbElemsOfState;
      
      CFuint ijIDs[NNodes];
      
       vector <RealMatrix> jacobians;
       jacobians.resize(NNodes);
	
	for(CFuint ii=0; ii<NNodes; ii++) {
	  jacobians[ii].resize(m_nbEqs,m_nbEqs);
	  for(CFuint jj=0; jj<m_nbEqs; jj++)
	    for(CFuint kk=0; kk<m_nbEqs; kk++)
	    (jacobians[ii])(jj,kk) = 0.0;
	}

	CFreal Dii[3];
        CFreal D23[3];

	for(CFuint ii=0; ii<3; ii++) {	
	  Dii[ii]=0.;
	  D23[ii]=0.;
	}
	

    CFuint elemCount = 0;
      
//  loop over the elements in which the state is involved
    for (CFuint elem=0; elem<nbElemsOfState; ++elem) {
      
        // build the GeometricEntity
        CFuint cellID = cells[elem];
        geoData.idx = cellID;
        GeometricEntity& cell = *geoBuilder->buildGE();
        vector<State*> *const statesInCell = cell.getStates();
        const CFuint nbStatesInCell = statesInCell->size();

// compute the characteristic variables and the derivatives needed
        RealVector acoustics(nbStatesInCell);
        RealVector omega(nbStatesInCell);

        for (CFuint elemState=0; elemState<nbStatesInCell; ++elemState) {
          State *const currState = (*statesInCell)[elemState];

          acoustics[elemState]= 2.0*oneoverc*((*currState)[3]);
          omega[elemState]= 2.0*(((*currState)[1])*ncharx+((*currState)[2])*nchary);

        }

/********************************** Matrix distribution in characteristic ************************************/

        CFreal resCharElemTwo=0.;
        CFreal resCharElemThree=0.;

        RealVector faceLength(nbStatesInCell);

        for (CFuint ii = 0; ii < nbStatesInCell; ++ii) {
          (*m_Statenormals[ii])[0] = m_normals[cellID]->getNodalNormComp(ii,0);
          (*m_Statenormals[ii])[1] = m_normals[cellID]->getNodalNormComp(ii,1);
          faceLength[ii] = m_normals[cellID]->getAreaNode(ii);
        }
        CFreal Area = m_volumes[cellID];

        computeCharK(*statesInCell, m_kPlus, m_k, m_kPast, m_Statenormals, faceLength, Area);

//         compute the distribution coefficients betas in this cell
        CFuint boundaryState = 100;
        for (CFuint elemState=0; elemState<nbStatesInCell; ++elemState) {
          State *const currState = (*statesInCell)[elemState];
	  CFuint currID=currState->getLocalID();
	  ijIDs[elemCount*3+elemState] = currID;
          if(stateID == currID) {
            boundaryState = elemState;
          }
        }

        m_betas = distributeLDA(m_kPlus, m_betas, boundaryState);

        RealVector acoustic_past(nbStatesInCell);
        RealVector omega_past(nbStatesInCell);

        for (CFuint i = 0; i < nbStatesInCell; ++i) {
          State *const currState = (*statesInCell)[i];
          CFuint stateIDlocal = currState->getLocalID();
          State const currState_past = (*pastStatesStorage[stateIDlocal]);
          acoustic_past[i]= 2.0*oneoverc*((currState_past)[3]);
          omega_past[i]= 2.0*(((currState_past)[1])*ncharx+((currState_past)[2])*nchary);
        }
        
        for (CFuint ii = 0; ii < nbStatesInCell; ++ii) {
          resCharElemTwo += (m_k[ii])*(acoustics[ii])+ (m_kPast[ii])*(acoustic_past[ii]);
          resCharElemThree += (m_k[ii])*(omega[ii])+(m_kPast[ii])*(omega_past[ii]);
        }

// compute the distributed residual
        charResTwo += m_betas*resCharElemTwo;
        charResThree += m_betas*resCharElemThree;
	
        for (CFuint iStat = 0; iStat < nbStatesInCell; ++iStat) {
	  Dii[iStat] = m_betas*m_k[iStat];
// 	  D23[iStat] = m_betas*0.25*c/Area*(nchary*(*m_Statenormals[iStat])[0]-ncharx*(*m_Statenormals[iStat])[1]);
	  //this is just an approximation, but works fine
	  D23[iStat] = 0.0;
	}
	
        for (CFuint iJac = 0; iJac < nbStatesInCell; ++iJac) {
	   jacobians[elemCount*3+iJac](0,0) = Dii[iJac];
	   
	   jacobians[elemCount*3+iJac](1,1) = Dii[iJac]+ncharx*nchary*D23[iJac];
	   jacobians[elemCount*3+iJac](1,2) = nchary*nchary*D23[iJac];
	   jacobians[elemCount*3+iJac](1,3) = nchary*D23[iJac]*oneoverc;
	   
	   jacobians[elemCount*3+iJac](2,1) = -ncharx*ncharx*D23[iJac];
	   jacobians[elemCount*3+iJac](2,2) = Dii[iJac]-ncharx*nchary*D23[iJac];
	   jacobians[elemCount*3+iJac](2,3) = -ncharx*D23[iJac]*oneoverc;
	   
	   jacobians[elemCount*3+iJac](3,3) =  Dii[iJac];
	}

/****************** End of matrix distribution in characteristic *******************/

      elemCount += 1;

      geoBuilder->releaseGE();

     }  // end of looping over the elements


// this is entropy, good as it is.
      charRes[0] = -(rhs(stateID, 0, m_nbEqs) - rhs(stateID, 3, m_nbEqs)*oneoverc*oneoverc);

// this is vorticity in the direction where acoustic wave is propagating, so they are decpoupled

      charRes[1] = -(rhs(stateID, 1, m_nbEqs)*nchary - rhs(stateID, 2, m_nbEqs)*ncharx);
      charRes[2] = charResTwo;
      charRes[3] = charResThree;

// transform back to conservative
      Res[0] = (charRes[0]+0.5/c*(charRes[2]));
      Res[1] = (nchary*charRes[1]+0.5*ncharx*(charRes[3]));
      Res[2] = (-ncharx*charRes[1]+0.5*nchary*(charRes[3]));
      Res[3] = (0.5*c*(charRes[2]));

     for (CFuint iEq = 0; iEq < m_nbEqs; ++iEq) {
       rhs(stateID, iEq, m_nbEqs) = -Res[iEq];
     }
     
//merge all the contributions
       vector <RealMatrix> jacobian_merged;
       CFint reducedset = 3*nbElemsOfState-2*(nbElemsOfState-1);
       jacobian_merged.resize(reducedset);
       CFuint jacobianIDs[reducedset];
	
	for(CFuint ii=0; ii<reducedset; ii++) {
	  (jacobian_merged[ii]).resize(m_nbEqs,m_nbEqs);
	  for(CFuint jj=0; jj<m_nbEqs; jj++)
	    for(CFuint kk=0; kk<m_nbEqs; kk++)
	    (jacobian_merged[ii])(jj,kk) = 0.0;
	}


     CFuint setted = 1;
//set the first item     
     jacobian_merged[0]=jacobians[0];
     jacobianIDs[0]=ijIDs[0];
//merge contributions
     for (CFuint Inclnode = 1; Inclnode < NNodes; ++Inclnode) {
        const CFuint entryID = ijIDs[Inclnode];
        bool found = false;
	for(CFuint ii=0; ii<setted; ii++) {
	  if(entryID==jacobianIDs[ii]) {
	    found = true;
            jacobian_merged[ii]+=jacobians[Inclnode];
	  }
	}
        if(found==false) {
            jacobian_merged[setted]=jacobians[Inclnode];
	    jacobianIDs[setted]=ijIDs[Inclnode];
	    setted+=1;
	}
     }

     acc->setRowIndex(0, stateID);
   
     for (CFuint Inclnode = 0; Inclnode < reducedset; ++Inclnode) {
        const CFuint entryID = jacobianIDs[Inclnode];

// 	cout << "The connected elements: "<< ijIDs[Inclnode] << "\n";

        acc->setColIndex(0, entryID);

        _jacob =  jacobian_merged[Inclnode];

	for (CFuint iVar = 0; iVar < m_nbEqs; ++iVar) {
	  for (CFuint jVar = 0; jVar < m_nbEqs; ++jVar) {
	    acc->setValue(0,0, iVar, jVar,
			  _jacob(iVar,jVar));
          }
         }
         jacobMatrix->setValues(*acc);
       }
      }
    isUpdated[stateID] = true; // flagging is important!!!!!
    }
  }
  // this should be an intermediate lightweight assembly !!!
  // it is needed because here you SET values while elsewhere
  // you ADD values
  jacobMatrix->flushAssembly();
   
}

//////////////////////////////////////////////////////////////////////////////

void StrongSubOutletLinEuler2DConsImpl::configure ( Config::ConfigArgs& args )
{
  FluctuationSplitCom::configure(args);

  std::string name = getMethodData().getNamespace();
  Common::SafePtr<Namespace> nsp = NamespaceSwitcher::getInstance
    (SubSystemStatusStack::getCurrentName()).getNamespace(name);
  Common::SafePtr<PhysicalModel> physModel = PhysicalModelStack::getInstance().getEntryByNamespace(nsp);

  std::string varSetName = "LinEuler2DCons";
  _varSet.reset((Environment::Factory<ConvectiveVarSet>::getInstance().getProvider(varSetName)->
    create(physModel->getImplementor()->getConvectiveTerm())).d_castTo<Physics::LinearizedEuler::LinEuler2DCons>());

  cf_assert(_varSet.isNotNull());

}

//////////////////////////////////////////////////////////////////////////////

void StrongSubOutletLinEuler2DConsImpl::computeCharK(std::vector<Framework::State*>& states,   RealVector& _kPlus,   RealVector& _k, RealVector& _kPast, std::vector<RealVector*>& normal, RealVector& faceLength, CFreal& Area)
  {

  CFuint _nbStatesInCell = states.size();
  RealVector _nodeArea(states.size());
  RealVector _kspace = _k;

  const CFreal kCoeff = 1./2.;
  const CFreal tCoeff = SubSystemStatusStack::getActive()->getDT()/2.;

  const RealVector& linearData = _varSet->getModel()->getPhysicalData();
  const CFreal U0     = linearData[LinEulerTerm::U0];
  const CFreal V0     = linearData[LinEulerTerm::V0];

    for (CFuint iState = 0; iState < _nbStatesInCell; ++iState){
      _nodeArea[iState] = faceLength[iState];
      for (CFuint iDim = 0; iDim < 2; ++iDim) {
        m_adimNormal[iDim] = (*normal[iState])[iDim];
      }
      m_adimNormal *= 1./_nodeArea[iState];

    _kspace[iState] = U0*m_adimNormal[0]+V0*m_adimNormal[1];
    _kspace[iState] *= _nodeArea[iState] * kCoeff;

    _kPast[iState]  = _kspace[iState]*tCoeff - Area/3.;
    _k[iState] =_kspace[iState]*tCoeff + Area/3.;

//     _k[iState]  = _k[iState]*tCoeff + Area;
    _kPlus[iState] = max(0.,_k[iState]);

    }

}


//////////////////////////////////////////////////////////////////////////////

CFreal StrongSubOutletLinEuler2DConsImpl::distributeLDA(RealVector& m_kPlus, CFreal m_betas, CFuint boundarystate){

  m_sumKplus = m_kPlus[0];
  for (CFuint iState = 1; iState < 3; ++iState) {
    m_sumKplus  += m_kPlus[iState];
  }

  m_betas = (m_kPlus[boundarystate])/m_sumKplus;

  return m_betas;
}

//////////////////////////////////////////////////////////////////////////////


void StrongSubOutletLinEuler2DConsImpl::unsetup()
{
  
  
  
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace FluctSplit

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
