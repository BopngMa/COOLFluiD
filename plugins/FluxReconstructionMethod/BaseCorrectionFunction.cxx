// Copyright (C) 2012 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "Framework/MethodStrategyProvider.hh"

#include "FluxReconstructionMethod/FluxReconstruction.hh"
#include "FluxReconstructionMethod/BaseCorrectionFunction.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace FluxReconstructionMethod {

//////////////////////////////////////////////////////////////////////////////

BaseCorrectionFunction::BaseCorrectionFunction(const std::string& name) :
  FluxReconstructionSolverStrategy(name)
{
  CFAUTOTRACE;
}

//////////////////////////////////////////////////////////////////////////////

BaseCorrectionFunction::~BaseCorrectionFunction()
{
  CFAUTOTRACE;
}

//////////////////////////////////////////////////////////////////////////////

void BaseCorrectionFunction::setup()
{
  CFAUTOTRACE;
  
  FluxReconstructionSolverStrategy::setup();
}

//////////////////////////////////////////////////////////////////////////////

void BaseCorrectionFunction::unsetup()
{
  CFAUTOTRACE;
  
  FluxReconstructionSolverStrategy::unsetup();
}

//////////////////////////////////////////////////////////////////////////////

  }  // namespace FluxReconstructionMethod

}  // namespace COOLFluiD

