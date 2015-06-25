#ifndef COOLFluiD_SendBuffer_hh
#define COOLFluiD_SendBuffer_hh

//////////////////////////////////////////////////////////////////////////////
#include "Common/MPI/MPIError.hh"
#include <vector>
#include "Common/COOLFluiD.hh"
#include "Common/PE.hh"
#include "LagrangianSolver/LagrangianSolverModule.hh"
#include <iostream>
//////////////////////////////////////////////////////////////////////////////


namespace COOLFluiD {

namespace LagrangianSolver{

template<typename T>
class SendBuffer
{
private:
    std::vector<T    > m_sendBuffer;
    std::vector<int>  m_sendCounts;
    std::vector<CFint> m_sendRanks;
    std::vector<T    > m_sendBufferOrdered;
    CFuint              m_nbProcesses;
    MPI_Comm            m_comm;
    MPI_Datatype        m_MPIdatatype;

public:
    SendBuffer();
    void reserve(CFuint nCells);
    bool sincronize(std::vector<T> &recvBuffer, bool isLastPhoton);
    void push_back(const T &a, const CFuint &rank);
    MPI_Datatype getMPIdatatype() const{ return m_MPIdatatype; }
    void setMPIdatatype(const MPI_Datatype MPIdatatype){ m_MPIdatatype = MPIdatatype; }
};

template<typename T>
SendBuffer<T>::SendBuffer():
  m_sendBuffer(),
  m_sendCounts(),
  m_sendBufferOrdered()
{
  const std::string nsp = Framework::MeshDataStack::getActive()->getPrimaryNamespace();
  
  m_comm = Common::PE::GetPE().GetCommunicator(nsp);
  m_nbProcesses= Common::PE::GetPE().GetProcessorCount(nsp);
  m_sendCounts.resize(m_nbProcesses);
}

template<typename T>
void SendBuffer<T>::reserve(CFuint nCells){
  try{
    m_sendBuffer.reserve(nCells);
    m_sendRanks.reserve(nCells);
    m_sendBufferOrdered.reserve(nCells); //is it necessary?
  }
  catch (std::bad_alloc& ba){
    std::cerr << "bad_alloc caught: " << ba.what() << " aborting, "<<'\n'; 
    abort();
  }

}

template<typename T>
void SendBuffer<T>::push_back( const T &a, const CFuint &rank){
    if(m_sendBuffer.capacity() == m_sendBuffer.size() ){
       CFLog(INFO,"Realocation! \n");
    }
    m_sendBuffer.push_back(a);
    m_sendRanks.push_back(rank);
    cf_assert(rank<m_nbProcesses);
    ++m_sendCounts[rank];
}

template<typename T>
bool SendBuffer<T>::sincronize( std::vector<T> &recvBuffer, bool isLastPhoton)
{
  //get the number of photons to send and the displacements
  if( m_nbProcesses <= 1){
    return isLastPhoton;
  } 

 
  CFuint nbPhotonsSend=0;
  std::vector<int> displacements(m_nbProcesses);
  for(CFuint i=0; i< m_nbProcesses ; ++i ){
    displacements[i] = nbPhotonsSend;
    nbPhotonsSend += m_sendCounts[i];
  }
  
  m_sendBufferOrdered.resize(nbPhotonsSend);
  
  //get the number of photons to receive
  std::vector<int> recvCounts(m_nbProcesses);
  std::vector<int> recvDisps(m_nbProcesses);
  MPI_Alltoall(&m_sendCounts[0], 1, Common::MPIStructDef::getMPIType(&m_sendCounts[0]), 
	       &recvCounts[0], 1, Common::MPIStructDef::getMPIType(&recvCounts[0]), m_comm);
  
  CFuint nbPhotonsRecv=0;
  for(CFuint i=0; i< m_nbProcesses ; ++i ){
    recvDisps[i]   = nbPhotonsRecv;
    nbPhotonsRecv += recvCounts[i];
  }
  
  recvBuffer.resize(nbPhotonsRecv);
  
  //copy and organize the data into the new buffer
  //TODO: let's look for a way to do it without an extra buffer!
  std::vector<int> tempDisps= displacements;
  int *tempDisp;
  for( CFuint i=0; i < m_sendBuffer.size(); ++i ){
    tempDisp= &(tempDisps[ m_sendRanks[i] ]);
    m_sendBufferOrdered[ *tempDisp ] = m_sendBuffer[i];
    ++ *tempDisp;
  }
  
  MPI_Alltoallv(&m_sendBufferOrdered[0], &m_sendCounts[0], &displacements[0],
		m_MPIdatatype, &recvBuffer[0], &recvCounts[0],
		&recvDisps[0], m_MPIdatatype, m_comm );
  
  //clear the sendbuffers
  m_sendBuffer.clear();
  m_sendRanks.clear();
  for(CFuint i=0; i<m_sendCounts.size(); ++i){
    m_sendCounts[i]=0;
  }
  
  //check finish condition (all buffers have zero size and all partitions have generated all photons)
  CFuint totalPhotonsRecv;
  nbPhotonsRecv += (isLastPhoton)? 0 : 1 ;
  
  MPI_Allreduce(&nbPhotonsRecv, &totalPhotonsRecv, 1, 
		Common::MPIStructDef::getMPIType(&nbPhotonsRecv), MPI_SUM, m_comm);
  return (totalPhotonsRecv == 0);
}  
  
}
  
}

#endif
