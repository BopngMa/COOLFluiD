#ifndef COOLFluiD_ShapeFunctions_SetQuadLagrangeP1FRP2StateCoord_hh
#define COOLFluiD_ShapeFunctions_SetQuadLagrangeP1FRP2StateCoord_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/SetElementStateCoord.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Framework { class Node; }
  namespace Framework { class State; }

  namespace FluxReconstructionMethod {


//////////////////////////////////////////////////////////////////////////////

/**
 * This class is a functor and offers an abstract interface
 * for setting the corresponding space coordinates (Framework::Node) in
 * the State's in a quadrangle with P1 geometrical and P2 solution
 * interpolation.
 *
 * @author Kris Van den Abeele
 */
class SetQuadLagrangeP1FRP2StateCoord : public Framework::SetElementStateCoord {

public:

  /**
   * Constructor
   */
  SetQuadLagrangeP1FRP2StateCoord() : Framework::SetElementStateCoord()
  {
  }

  /**
   * Destructor
   */
  ~SetQuadLagrangeP1FRP2StateCoord()
  {
  }

  /**
   * Overloading of the operator () to make this class act as a
   * functor
   * @param nodes   list of the nodes in the current element
   * @param states  list of the states in the current element
   */
  void operator() (const std::vector<Framework::Node*>& nodes,
                   std::vector<Framework::State*>& states);

  /**
   * Function allowing to update the StateCoord
   * @param nodes   list of the nodes in the current element
   * @param states  list of the states in the current element
   */
  void update(const std::vector<Framework::Node*>& nodes,
                            std::vector<Framework::State*>& states);

}; // end of class SetQuadLagrangeP1FRP2StateCoord

//////////////////////////////////////////////////////////////////////////////

  } // namespace FluxReconstructionMethod

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_ShapeFunctions_SetQuadLagrangeP1FRP2StateCoord_hh
