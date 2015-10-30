#ifndef COOLFluiD_Physics_MultiFluidMHD_EulerMFMHD2DHalfRhoiViTiToCons_hh
#define COOLFluiD_Physics_MultiFluidMHD_EulerMFMHD2DHalfRhoiViTiToCons_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/MultiScalarVarSetBase.hh"
#include "Framework/VarSetTransformer.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace MultiFluidMHD {

      class EulerMFMHDTerm;

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represents a transformer of variables from primitive
 * [rhoi vi ui wi Ti] to conservative variables
 * AAL: This one is used when you select RhoiViTi as vars
 *
 * @author Alejandro Alvarez
 *
 */
class EulerMFMHD2DHalfRhoiViTiToCons : public Framework::VarSetTransformer {
public:

  typedef Framework::MultiScalarTerm<EulerMFMHDTerm> PTERM;
 
  
  /**
   * Default constructor without arguments
   */
  EulerMFMHD2DHalfRhoiViTiToCons(Common::SafePtr<Framework::PhysicalModelImpl> model);

  /**
   * Default destructor
   */
  ~EulerMFMHD2DHalfRhoiViTiToCons();
  
  /**
   * Transform a state into another one
   */
  void transform(const Framework::State& state, Framework::State& result);
  
  /**
   * Transform a state into another one from reference precomputed
   * values (physical data)associated to the given state
   */
  void transformFromRef(const RealVector& data, Framework::State& result);
  
private:

  /// acquaintance of the PhysicalModel
  Common::SafePtr<EulerMFMHDTerm> _model;

  /// Array with the particle mass of the species
  RealVector			   _m_i;  
}; // end of class EulerMFMHD2DHalfRhoiViTiToCons

//////////////////////////////////////////////////////////////////////////////

    } // namespace MultiFluidMHD

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Physics_MultiFluidMHD_EulerMFMHD2DHalfRhoiViTiToCons_hh
