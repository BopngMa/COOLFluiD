#ifndef COOLFluiD_Numerics_FiniteVolume_LeastSquareP1PolyRec2DPeriodic_hh
#define COOLFluiD_Numerics_FiniteVolume_LeastSquareP1PolyRec2DPeriodic_hh

//////////////////////////////////////////////////////////////////////////////

#include "FiniteVolume/LeastSquareP1PolyRec2D.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace FiniteVolume {

//////////////////////////////////////////////////////////////////////////////

/**
 * This class implements a least square polynomial reconstructor in 2D for FVM
 * with a fix to handle Periodic BCs in parallel simulations
 *
 * @author Andrea Lani
 */
class LeastSquareP1PolyRec2DPeriodic : public LeastSquareP1PolyRec2D {
public:
  /**
   * Defines the Config Option's of this class
   * @param options a OptionList where to add the Option's
   */
  static void defineConfigOptions(Config::OptionList& options);
  
  /**
   * Constructor
   */
  LeastSquareP1PolyRec2DPeriodic(const std::string& name);

  /**
   * Default destructor
   */
  virtual ~LeastSquareP1PolyRec2DPeriodic();
  
  /**
   * Returns the DataSocket's that this numerical strategy needs as sinks
   * @return a vector of SafePtr with the DataSockets
   */
  virtual std::vector<Common::SafePtr<Framework::BaseDataSocketSink> >
  needsSockets();
  
  /**
   * Compute the gradients
   */
  virtual void computeGradients();
  
  /**
   * Set up the private data
   */
  virtual void setup();

protected:

  /**
   * Extrapolate the solution in the face quadrature points
   */
  virtual void extrapolateImpl(Framework::GeometricEntity* const face);

  /**
   * Extrapolate the solution in the face quadrature points
   */
  virtual void extrapolateImpl(Framework::GeometricEntity* const face,
			       CFuint iVar, CFuint leftOrRight);
  
protected:
  
  /// array of flags indicating the periodic faces
  std::vector<bool> m_isPeriodicFace;
  
  /// names of the TRSs that are associated to periodic BCs
  std::vector<std::string> m_periodicTRS;
  
}; // end of class LeastSquareP1PolyRec2DPeriodic

//////////////////////////////////////////////////////////////////////////////
      
    } // namespace FiniteVolume

  } // namespace Numerics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Numerics_FiniteVolume_LeastSquareP1PolyRec2DPeriodic_hh
