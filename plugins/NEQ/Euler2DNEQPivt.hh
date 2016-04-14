#ifndef COOLFluiD_Physics_NEQ_Euler2DNEQPivt_hh
#define COOLFluiD_Physics_NEQ_Euler2DNEQPivt_hh

//////////////////////////////////////////////////////////////////////////////

#include "NavierStokes/MultiScalarVarSet.hh"
#include "NavierStokes/Euler2DVarSet.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {
  
  namespace Framework {
    class PhysicalChemicalLibrary;
  }
  
  namespace Physics {

    namespace NEQ {

//////////////////////////////////////////////////////////////////////////////

  /**
   * This class represents a Euler physical model 2D for primitive
   * variables with chemical NEQ
   *
   * @author Andrea Lani
   */
class Euler2DNEQPivt : 
	public NavierStokes::MultiScalarVarSet<NavierStokes::Euler2DVarSet> {
  
public: // classes
  
  /**
   * Constructor
   * @see Euler2D
   */
  Euler2DNEQPivt(Common::SafePtr<Framework::BaseTerm> term);

  /**
   * Default destructor
   */
  virtual ~Euler2DNEQPivt();

  /**
   * Set up the private data and give the maximum size of states physical
   * data to store
   */
  virtual void setup();

  /**
   * Get extra variable names
   */
  virtual std::vector<std::string> getExtraVarNames() const;

  /**
   * Gets the block separator for this variable set
   */
  virtual CFuint getBlockSeparator() const;

  /**
   * Split the jacobian
   */
  virtual void splitJacobian(RealMatrix& jacobPlus,
			  RealMatrix& jacobMin,
			  RealVector& eValues,
			  const RealVector& normal);
  /**
   * Set the matrix of the right eigenvectors and the matrix of the eigenvalues
   */
  virtual void computeEigenValuesVectors(RealMatrix& rightEv,
				     RealMatrix& leftEv,
				     RealVector& eValues,
				     const RealVector& normal);
  
  /**
   * Get the speed
   */
  virtual CFreal getSpeed(const Framework::State& state) const;

  /**
   * Give dimensional values to the adimensional state variables
   */
  virtual void setDimensionalValues(const Framework::State& state,
				    RealVector& result);

  /**
   * Give adimensional values to the dimensional state variables
   */
  virtual void setAdimensionalValues(const Framework::State& state,
				     RealVector& result);

  /**
   * Set other adimensional values for useful physical quantities
   */
  virtual void setDimensionalValuesPlusExtraValues
  (const Framework::State& state, RealVector& result,
   RealVector& extra);
  
  /// Compute the perturbed physical data
  virtual void computePerturbedPhysicalData(const Framework::State& state,
					    const RealVector& pdataBkp,
					    RealVector& pdata,
					    CFuint iVar);
  
  /**
   * Set the PhysicalData corresponding to the given State
   * @see EulerPhysicalModel
   */
  virtual void computePhysicalData(const Framework::State& state,
				   RealVector& data);
  
  /**
   * Set a State starting from the given PhysicalData
   * @see EulerPhysicalModel
   */
  virtual void computeStateFromPhysicalData(const RealVector& data,
					    Framework::State& state);
  
  /// Set the IDs corresponding to the velocity components in a State
  virtual void setStateVelocityIDs (std::vector<CFuint>& velIDs);
  
  /**
   * Compute the pressure derivative
   */
  virtual void computePressureDerivatives(const Framework::State& state, RealVector& dp);
  
  /**
   * Checks validity of data
   * @pre data is assumed to be of number of equations size
   */
  virtual bool isValid(const RealVector& data);
  
protected:
  
  /**
   * Set all thermodynamic quantities in the physical data array
   */
  virtual void setThermodynamics(CFreal rho,
				 const Framework::State& state,
				 RealVector& data);
  
  /**
   * Get the ID of the temperature given the number of species
   */
  CFuint getTempID(CFuint nbSpecies) const
  {
    return nbSpecies + 2;
  }
   
  /// get the electron temperature
  virtual CFreal getTe(const Framework::State& state); 
  
protected:
  
  /// thermodynamic library
  Common::SafePtr<Framework::PhysicalChemicalLibrary> _library;
  
  /// array to store density, enthalpy and energy
  RealVector _dhe;
  
  /// array to store the mass fractions of elements
  RealVector _ye;
  
  /// array to store rho_i
  RealVector _rhoi;
  
  /// array to store Rgas/molar mass for each species
  RealVector _Rspecies;
    
}; // end of class Euler2DNEQPivt

//////////////////////////////////////////////////////////////////////////////

    } // namespace NEQ

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Physics_NEQ_Euler2DNEQPivt_hh
