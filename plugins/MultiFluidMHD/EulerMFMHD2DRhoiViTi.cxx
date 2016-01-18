#include "Environment/ObjectProvider.hh"
#include "Common/StringOps.hh"
#include "MultiFluidMHD/MultiFluidMHD.hh"
#include "MultiFluidMHD/EulerMFMHD2DRhoiViTi.hh"
#include "Framework/PhysicalConsts.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace MultiFluidMHD {

//////////////////////////////////////////////////////////////////////////////

Environment::ObjectProvider<EulerMFMHD2DRhoiViTi, ConvectiveVarSet, MultiFluidMHDModule, 1>
mfMHD2DRhoiViTiProvider("EulerMFMHD2DRhoiViTi");

//////////////////////////////////////////////////////////////////////////////

EulerMFMHD2DRhoiViTi::EulerMFMHD2DRhoiViTi(Common::SafePtr<BaseTerm> term) :
  MultiFluidMHDVarSet<Maxwell2DProjectionVarSet>(term),
  _rightEv(),
  _leftEv(),
  _m_i()
{    
  const CFuint endEM = 8;
  const CFuint nbSpecies    = getModel()->getNbScalarVars(0);
  const CFuint nbMomentum   = getModel()->getNbScalarVars(1);
  const CFuint nbEnergyEqs  = getModel()->getNbScalarVars(2);
  const CFuint totalNbEqs = nbSpecies + nbMomentum + nbEnergyEqs + 8;   
  
  vector<std::string> names(totalNbEqs);
  
  names[0] = "Bx";
  names[1] = "By";
  names[2] = "Bz";
  names[3] = "Ex";
  names[4] = "Ey";
  names[5] = "Ez";
  names[6] = "Psi";
  names[7] = "Phi";
  
  
  for (CFuint ie = 0; ie < nbSpecies; ++ie) {
    names[endEM + ie] = "rho" + StringOps::to_str(ie);
  }

  for (CFuint ie = 0; ie < nbSpecies; ++ie) {
    names[endEM + nbSpecies + 2*ie]     = "U" + StringOps::to_str(ie);
    names[endEM + nbSpecies + 2*ie + 1] = "V" + StringOps::to_str(ie);
  } 
  
  for (CFuint ie = 0; ie < nbSpecies; ++ie) {
    names[endEM + nbSpecies + nbMomentum + ie] = "T" + StringOps::to_str(ie);
  } 
  
  setVarNames(names);
}

//////////////////////////////////////////////////////////////////////////////

EulerMFMHD2DRhoiViTi::~EulerMFMHD2DRhoiViTi()
{
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::setup()
{
  MultiFluidMHDVarSet<Maxwell2DProjectionVarSet>::setup();
  
  setConstJacob();
  _m_i.resize(3);
}
      
//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::setConstJacob()
{
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computeProjectedJacobian(const RealVector& normal,
						   RealMatrix& jacob)
{
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computeJacobians()
{
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computeEigenValuesVectors(RealMatrix& rightEv,
					    RealMatrix& leftEv,
					    RealVector& eValues,
					    const RealVector& normal)
{
}


//////////////////////////////////////////////////////////////////////////////

CFuint EulerMFMHD2DRhoiViTi::getBlockSeparator() const
{
  return PhysicalModelStack::getActive()->getNbEq();
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::splitJacobian(RealMatrix& jacobPlus,
					RealMatrix& jacobMin,
					RealVector& eValues,
					const RealVector& normal)
{
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computePhysicalData(const State& state, RealVector& data)
{
  
  const CFuint nbSpecies    = getModel()->getNbScalarVars(0);
  const CFuint nbMomentum   = getModel()->getNbScalarVars(1);
  const CFuint nbEnergyEqs  = getModel()->getNbScalarVars(2);
  const CFuint endEM = 8;
  const CFuint firstSpecies = getModel()->getFirstScalarVar(0);  
  const CFuint firstVelocity = getModel()->getFirstScalarVar(1);   
  const CFuint firstTemperature = getModel()->getFirstScalarVar(2); 
  
  const CFreal m_e = getModel()->getMolecularMass3();
  const CFreal m_n = getModel()->getMolecularMass2();
  const CFreal m_p = getModel()->getMolecularMass1(); 
  
  //set the molar masses of the species (should be changed in the future)
  _m_i[0] = m_e;
  _m_i[1] = m_n;
  _m_i[2] = m_p;
    
  data[PTERM::BX] = state[0];
  data[PTERM::BY] = state[1]; 
  data[PTERM::BZ] = state[2];  
  data[PTERM::EX] = state[3];  
  data[PTERM::EY] = state[4];  
  data[PTERM::EZ] = state[5];  
  data[PTERM::PSI] = state[6];  
  data[PTERM::PHI] = state[7];  
    
   
  const bool isLeake = getModel()->isLeake();

  if (isLeake){
    
    //Total density
    CFreal rho = 0.0;
    CFreal rho_i = state[endEM];
    CFreal rho_n = state[endEM + 1];
    
    rho = (1 + m_e/m_p)*rho_i + rho_n;  // rho_Total = rho_i + rho_e + rho_n = (1 + m_e/m_i)rho_i + rho_n
    data[PTERM::RHO] = rho;
    const CFreal ovRho = 1./rho;
    
    cf_assert(data[PTERM::RHO] > 0.);
    
    //Partial densities
    data[firstSpecies] =  rho_i*ovRho;
    data[firstSpecies + 1] =  rho_n*ovRho;
    
    //Velocities
    const CFreal u_i = state[endEM + nbSpecies];	//Ions x-velocity
    const CFreal v_i = state[endEM + nbSpecies + 1];	//Ions y-velocity
    data[firstVelocity] = u_i;
    data[firstVelocity + 1] = v_i;    
    
    const CFreal u_n = state[endEM + nbSpecies + 2];	//Neutrals x-velocity
    const CFreal v_n = state[endEM + nbSpecies + 3];	//Neutrals y-velocity
    data[firstVelocity + 2] = u_n;
    data[firstVelocity + 3] = v_n;    
    
    //Energy Variables: Ti, pi, ai, Hi
    const CFuint firstTemperature = getModel()->getFirstScalarVar(2);
    const CFreal gamma = getModel()->getGamma();	// gamma = 5/3
    const CFreal K_B = PhysicalConsts::Boltzmann();     // Boltzmann constant
    
    //plasma
    const CFreal V2_i = u_i*u_i + v_i*v_i;
    const CFreal T_i = state[endEM + nbSpecies + 2*nbSpecies];       
  
    const CFreal R_i = K_B/m_p;				// ions gas constant
    const CFreal R_p = 2.*R_i;				// Plasma gas constant (ions + electrons)
    const CFreal Cv_p = R_p/(gamma-1.);
    const CFreal Cp_p = gamma*Cv_p;
    
    data[firstTemperature] = T_i;			//Temperature
    data[firstTemperature + 1] = T_i*R_p*rho_i;		//pressure (pe + pi)
    data[firstTemperature + 2] = sqrt(gamma*R_p*T_i);	//sound speed (pe + pi)
    data[firstTemperature + 3] = 0.5*V2_i + Cp_p*T_i;	//total enthaply of species (pe + pi) 
    
    //neutrals
    const CFreal V2_n = u_n*u_n + v_n*v_n;
    const CFreal T_n = state[endEM + nbSpecies + 2*nbSpecies + 1];       
  
    const CFreal R_n  = K_B/m_n;             // neutrals gas constant
    const CFreal Cv_n = R_n/(gamma-1.);      // Cv for neutrals 
    const CFreal Cp_n = gamma*Cv_n;          // Cp for neutrals
    
    data[firstTemperature + 4] = T_n;				//Temperature
    data[firstTemperature + 4 + 1] = T_n*R_n*rho_n;		//pressure
    data[firstTemperature + 4 + 2] = sqrt(gamma*R_n*T_n);	//sound speed
    data[firstTemperature + 4 + 3] = 0.5*V2_n + Cp_n*T_n;	//total enthalpy of species i 
    
  }
  
  else {

    //set the total density
    CFreal rho = 0.0;
    for (CFuint ie = 0; ie < nbSpecies; ++ie) {
      rho += state[endEM + ie];
    }

    data[PTERM::RHO] = rho;
    const CFreal ovRho = 1./rho;
    
    cf_assert(data[PTERM::RHO] > 0.);
    
    //set the energy parameters
    const CFreal gamma = getModel()->getGamma();
    const CFreal K_gas = PhysicalConsts::Boltzmann();
    const CFreal m_e = getModel()->getMolecularMass3();
    const CFreal m_n = getModel()->getMolecularMass2();
    const CFreal m_p = getModel()->getMolecularMass1(); 
    
    //set the molar masses of the species (should be changed in the future)
    _m_i[0] = m_e;
    _m_i[1] = m_n;
    _m_i[2] = m_p;
    
    
    //set the species mass fraction
    for (CFuint ie = 0; ie < nbSpecies; ++ie) {
      const CFreal rhoi = state[endEM + ie];
      data[firstSpecies + ie] = rhoi*ovRho;
      //set the species velocities in 2D
      const CFreal ui = state[endEM + nbSpecies + 2*ie];
      const CFreal vi = state[endEM + nbSpecies + 2*ie + 1];
      data[firstVelocity + 2*ie] = ui;
      data[firstVelocity + 2*ie + 1] = vi;

      //set the  energy physical data  
      const CFuint firstTemperature = getModel()->getFirstScalarVar(2);
      
      
      const CFreal mi = _m_i[ie];    
      const CFreal V2 = ui*ui + vi*vi;
      const CFreal Ti = state[endEM + nbSpecies + 2*nbSpecies + ie];       
    
      const CFreal c_p = (gamma/(gamma-1))*(K_gas/mi);
      const CFreal R_gas = K_gas/mi;
      const CFreal c_v = c_p - R_gas;
      
      data[firstTemperature + 4*ie] = Ti;//Temperature
      data[firstTemperature + 4*ie + 1] = Ti*R_gas*rhoi;//pressure
      data[firstTemperature + 4*ie + 2] = sqrt(gamma*R_gas*Ti);//sound speed
      data[firstTemperature + 4*ie + 3] = 0.5*V2 + c_p*Ti;//total enthaply of species i   
    }
  }
  CFLog(DEBUG_MAX,"EulerMFMHD2DRhoiViTi::computePhysicalData" << "\n");
  for (CFuint ie = 0; ie < firstTemperature + 4*nbSpecies; ++ie) {
    CFLog(DEBUG_MAX,"data["<< ie <<"] = "<< data[ie] << "\n");
  }
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computeStateFromPhysicalData(const RealVector& data,
					       State& state)
{
  
  const CFuint nbSpecies    = getModel()->getNbScalarVars(0);
  const CFuint endEM = 8;  
  
  state[0] = data[PTERM::BX];
  state[1] = data[PTERM::BY]; 
  state[2] = data[PTERM::BZ];  
  state[3] = data[PTERM::EX];  
  state[4] = data[PTERM::EY];  
  state[5] = data[PTERM::EZ];  
  state[6] = data[PTERM::PSI];  
  state[7] = data[PTERM::PHI];  
  
  const CFreal rho = data[PTERM::RHO];
  
  //set the species densities
  const CFuint firstSpecies = getModel()->getFirstScalarVar(0);
  for (CFuint ie = 0; ie < nbSpecies; ++ie) {
    state[endEM + ie] = data[firstSpecies + ie]*rho; //rhoi = Rho*yi
  }  
  
  //set the species velocities in 2D 
  const CFuint firstVelocity = getModel()->getFirstScalarVar(1);  
  for (CFuint ie = 0; ie < nbSpecies; ++ie) {
    state[endEM + nbSpecies + 2*ie] = data[firstVelocity + 2*ie];
    state[endEM + nbSpecies + 2*ie + 1] = data[firstVelocity + 2*ie + 1];    
  } 
  //set the temperatures
  const CFuint firstTemperature = getModel()->getFirstScalarVar(2);
  for (CFuint ie = 0; ie < nbSpecies; ++ie) {   
    state[endEM + nbSpecies + 2*nbSpecies + ie] = data[firstTemperature + 4*ie];
  }  
  CFLog(DEBUG_MAX,"EulerMFMHD2DRhoiViTi::computeStateFromPhysicalData" << "\n");
  for (CFuint ie = 0; ie < endEM + 4*nbSpecies; ++ie) {
     CFLog(DEBUG_MAX,"state["<< ie <<"] = "<< state[ie] << "\n");
  }
}

//////////////////////////////////////////////////////////////////////////////

CFreal EulerMFMHD2DRhoiViTi::getSpeed(const State& state) const
{
  throw Common::NotImplementedException
      (FromHere(), "EulerMFMHD2DRhoiViTi::getSpeed() not implemented");
 //   return 0.;
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::setDimensionalValues(const State& state,
                                       RealVector& result)
{
  const RealVector& refData = getModel()->getReferencePhysicalData();
  const CFuint nbSpecies    = getModel()->getNbScalarVars(0);
  const CFuint nbMomentum   = getModel()->getNbScalarVars(1);
  const CFuint nbEnergyEqs  = getModel()->getNbScalarVars(2);
  const CFuint totalNbEquations = nbSpecies + nbMomentum + nbEnergyEqs + 8; 
  
  for (CFuint i = 0; i < totalNbEquations; ++i) {
    result[i] = state[i]*refData[i];   
  }
}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::setAdimensionalValues(const State& state,
                                       RealVector& result)
{
  const RealVector& refData = getModel()->getReferencePhysicalData();
  const CFuint nbSpecies    = getModel()->getNbScalarVars(0);
  const CFuint nbMomentum   = getModel()->getNbScalarVars(1);
  const CFuint nbEnergyEqs  = getModel()->getNbScalarVars(2);
  const CFuint totalNbEquations = nbSpecies + nbMomentum + nbEnergyEqs + 8; 
  
  for (CFuint i = 0; i < totalNbEquations; ++i) {
    result[i] = state[i]/refData[i];   
  }  

}

//////////////////////////////////////////////////////////////////////////////

void EulerMFMHD2DRhoiViTi::computePerturbedPhysicalData(const Framework::State& state,
					       const RealVector& pdataBkp,
					       RealVector& pdata,
					       CFuint iVar)
{
  throw Common::NotImplementedException
      (FromHere(), "EulerMFMHD2DRhoiViTi::computePerturbedPhysicalData() not implemented");
}

//////////////////////////////////////////////////////////////////////////////

bool EulerMFMHD2DRhoiViTi::isValid(const RealVector& data)
{
 //  bool correct = true;
//   enum index {RHO, RHOU,RHOV,RHOE};

//   const CFreal rho = data[RHO];
//   const CFreal ovRho = 1./rho;
//   const CFreal u = data[RHOU]*ovRho;
//   const CFreal v = data[RHOV]*ovRho;
//   const CFreal V2 = u*u + v*v;

//   const CFreal gamma = getModel()->getGamma();
//   const CFreal gammaMinus1 = gamma - 1.;
//   /// Next call returns R/M, in dimensional units.
//   const CFreal R = getModel()->getR();

//   const CFreal p = gammaMinus1*(data[RHOE] - 0.5*rho*V2);

//   const CFreal T = p*ovRho/R;
  
//   const CFreal a2 = gamma*p*ovRho;

//   if( ( p < 0.) || ( T < 0.) || ( a2 < 0.) ){
//   return correct = false;
//   }

//   return correct;
  return true;
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace MultiFluidMHD

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
