#ifndef COOLFluiD_FluxReconstructionMethod_TVBLimiterEuler2D_hh
#define COOLFluiD_FluxReconstructionMethod_TVBLimiterEuler2D_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/DataSocketSink.hh"

#include "FluxReconstructionMethod/FluxReconstructionSolverData.hh"
#include "FluxReconstructionNavierStokes/TVBLimiterEuler.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {
    namespace NavierStokes {
      class Euler2DVarSet;
    }
  }

  namespace FluxReconstructionMethod {

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represent a command that applies an elementwise TVB limiter to the solution for 2D Euler,
 * taking into account the requirement of pressure positivty (pressure is limited instead of rhoE)
 *
 * @author Kris Van den Abeele
 *
 */
class TVBLimiterEuler2D : public TVBLimiterEuler {
public:

  /**
   * Constructor.
   */
  explicit TVBLimiterEuler2D(const std::string& name);

  /**
   * Destructor.
   */
  virtual ~TVBLimiterEuler2D();

  /**
   * Setup private data and data of the aggregated classes
   * in this command before processing phase
   */
  virtual void setup();

  /**
   * Unsetup private data
   */
  virtual void unsetup();

  /**
   * Configures the command.
   */
  virtual void configure ( Config::ConfigArgs& args );

private: // functions

  /// compute the pressure from the conserved variables
  void computePressFromConsVar(const RealVector& consVar, CFreal& press);

  /// compute the rhoE from pressure and the other conserved variables
  void computeRhoEFromPressAndOtherConsVar(RealVector& consVar, const CFreal& press);

protected: // data

  /// physical model (in conservative variables)
  Common::SafePtr<Physics::NavierStokes::Euler2DVarSet> m_eulerVarSet;

  /// heat capacity ratio minus one
  CFreal m_gammaMinusOne;

}; // class TVBLimiterEuler2D

//////////////////////////////////////////////////////////////////////////////

  } // namespace FluxReconstructionMethod

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_FluxReconstructionMethod_TVBLimiterEuler2D_hh
