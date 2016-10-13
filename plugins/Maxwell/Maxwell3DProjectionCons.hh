#ifndef COOLFluiD_Physics_Maxwell_Maxwell3DProjectionCons_hh
#define COOLFluiD_Physics_Maxwell_Maxwell3DProjectionCons_hh

//////////////////////////////////////////////////////////////////////////////

#include "Maxwell3DProjectionVarSet.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace Maxwell {

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represents a Maxwell physical model 2D for projection scheme
 * for conservative variables
 *
 * @author Alejandro Alvarez Laguna
 * @author Andrea Lani
 */
class Maxwell3DProjectionCons : public Maxwell3DProjectionVarSet {
public: // function

  /**
   * Constructor
   * @see MaxwellProjection
   */
  Maxwell3DProjectionCons(Common::SafePtr<Framework::BaseTerm> term);

  /**
   * Default destructor
   */
  ~Maxwell3DProjectionCons();

  /**
   * Set up the private data and give the maximum size of states physical
   * data to store
   */
  virtual void setup();

  /**
   * Get extra variable names
   */
  std::vector<std::string> getExtraVarNames() const;

  /**
   * Gets the block separator for this variable set
   */
  CFuint getBlockSeparator() const;

  /**
   * Set the jacobians
   */
  virtual void computeJacobians();
  
  /**
   * Set the jacobian matrix
   */
  virtual void computeProjectedJacobian(const RealVector& normal, RealMatrix& jacob);

  /**
   * Split the jacobian
   */
  void splitJacobian(RealMatrix& jacobPlus,
		     RealMatrix& jacobMin,
		     RealVector& eValues,
		     const RealVector& normal);

  /**
   * Set the matrix of the right eigenvectors and the matrix of the eigenvalues
   */
  void computeEigenValuesVectors(RealMatrix& rightEv,
                             RealMatrix& leftEv,
                             RealVector& eValues,
                             const RealVector& normal);
  /**
   * Give dimensional values to the adimensional state variables
   */
  void setDimensionalValues(const Framework::State& state, RealVector& result);
  
  /**
   * Give adimensional values to the dimensional state variables
   */
  void setAdimensionalValues(const Framework::State& state, RealVector& result);
  
  /**
   * Set the PhysicalData corresponding to the given State
   * @see MaxwellModel
   */
  void computePhysicalData(const Framework::State& state,
			   RealVector& data);
  
  /**
   * Set a State starting from the given PhysicalData
   * @see MaxwellModel
   */
  void computeStateFromPhysicalData(const RealVector& data,
			       Framework::State& state);
 

  /// Compute the perturbed physical data
  virtual void computePerturbedPhysicalData(const Framework::State& state,
					    const RealVector& pdataBkp,
					    RealVector& pdata,
					    CFuint iVar);
  
  /**
   * Returns true if the state fed doesn't have unphysical values
   * Overrides the standard definition in ConvectiveVarSet.
   */
  bool isValid(const RealVector& data);
  
protected:
  
  /**
   * Set the constant part (independent form the solution) of the
   * jacobians
   */
  void setConstJacob();

private: // data
  
  /// temporary matrix of right eigenvalues
  RealMatrix                       _rightEv;
  
  /// temporary matrix of left eigenvalues
  RealMatrix                       _leftEv;
 
}; // end of class Maxwell3DProjectionCons

//////////////////////////////////////////////////////////////////////////////


    } // namespace Maxwell

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Physics_Maxwell_Maxwell3DProjectionCons_hh
