#ifndef COOLFluiD_Physics_MultiFluidMHD_Euler2DMFMHDConsToRhoiViTiInRhoiViTi_hh
#define COOLFluiD_Physics_MultiFluidMHD_Euler2DMFMHDConsToRhoiViTiInRhoiViTi_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/VarSetMatrixTransformer.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace MultiFluidMHD {

      class EulerMFMHDTerm;

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represents a transformer of variables from conservative
 * to RhoiViTi [rhoi ui vi Ti] starting from primitive variables
 *
 * Actually it is not implemented because it is not used
 *
 * @author Alejandro Alvarez
 *
 */
class Euler2DMFMHDConsToRhoiViTiInRhoiViTi : public Framework::VarSetMatrixTransformer {
public:

  /**
   * Default constructor without arguments
   */
  Euler2DMFMHDConsToRhoiViTiInRhoiViTi(Common::SafePtr<Framework::PhysicalModelImpl> model);

  /**
   * Default destructor
   */
  ~Euler2DMFMHDConsToRhoiViTiInRhoiViTi();

  /**
   * Set the transformation matrix from a given state
   */
  void setMatrix(const RealVector& state);

private:

  /**
   * Set the flag telling if the transformation is an identity one
   * @pre this method must be called during set up
   */
  bool getIsIdentityTransformation() const
  {
    return false;
  }

private: //data

  /// acquaintance of the PhysicalModel
  Common::SafePtr<EulerMFMHDTerm> _model;

}; // end of class Euler2DMFMHDConsToRhoiViTiInRhoiViTi

//////////////////////////////////////////////////////////////////////////////

    } // namespace MultiFluidMHD

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Physics_MultiFluidMHD_Euler2DMFMHDConsToRhoiViTiInRhoiViTi_hh
