//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "ts/tsMesh.h"
#include "ts/tsMeshIntrinsics.h"
#include "ts/arch/tsMeshIntrinsics.arch.h"

void (*zero_vert_normal_bulk)(const dsize_t count, U8 * __restrict const outPtr, const dsize_t outStride) = NULL;
void (*m_matF_x_BatchedVertWeightList)(const MatrixF &mat, const dsize_t count, const TSSkinMesh::BatchData::BatchedVertWeight * __restrict batch, U8 * const __restrict outPtr, const dsize_t outStride) = NULL;

//------------------------------------------------------------------------------
// Default C++ Implementations (pretty slow)
//------------------------------------------------------------------------------

void zero_vert_normal_bulk_C(const dsize_t count, U8 * __restrict const outPtr, const dsize_t outStride)
{
   register char *outData = reinterpret_cast<char *>(outPtr);

   // TODO: Try prefetch w/ ptr de-reference

   for(register int i = 0; i < count; i++)
   {
      TSMesh::__TSMeshVertexBase *outElem = reinterpret_cast<TSMesh::__TSMeshVertexBase *>(outData);
      outElem->_vert.zero();
      outElem->_normal.zero();
      outData += outStride;
   }
}

//------------------------------------------------------------------------------

void m_matF_x_BatchedVertWeightList_C(const MatrixF &mat, 
                                    const dsize_t count,
                                    const TSSkinMesh::BatchData::BatchedVertWeight * __restrict batch,
                                    U8 * const __restrict outPtr,
                                    const dsize_t outStride)
{
   const register MatrixF m = mat;

   register Point3F tempPt;
   register Point3F tempNrm;

   for(register int i = 0; i < count; i++)
   {
      const TSSkinMesh::BatchData::BatchedVertWeight &inElem = batch[i];

      TSMesh::__TSMeshVertexBase *outElem = reinterpret_cast<TSMesh::__TSMeshVertexBase *>(outPtr + inElem.vidx * outStride);

      m.mulP( inElem.vert, &tempPt );
      m.mulV( inElem.normal, &tempNrm );

      outElem->_vert += ( tempPt * inElem.weight );
      outElem->_normal += ( tempNrm * inElem.weight );
   }
}

//------------------------------------------------------------------------------
// Automatic initializer
//------------------------------------------------------------------------------

class _TSMeshIntrinsics_REG
{
public:
   _TSMeshIntrinsics_REG()
   {
      // Assign defaults (C++ versions)
      zero_vert_normal_bulk = zero_vert_normal_bulk_C;
      m_matF_x_BatchedVertWeightList = m_matF_x_BatchedVertWeightList_C;

#if defined(TORQUE_OS_XENON)
      zero_vert_normal_bulk = zero_vert_normal_bulk_X360;
      m_matF_x_BatchedVertWeightList = m_matF_x_BatchedVertWeightList_X360;
#else
      // Register for this signal
      Platform::SystemInfoReady.notify(this, &_TSMeshIntrinsics_REG::setSkinImplementation);
#endif
   }

   // Find the best implementation for the current CPU
   void setSkinImplementation()
   {
      if(Platform::SystemInfo.processor.properties & CPU_PROP_SSE)
      {
#if defined(TORQUE_CPU_X86)
         
         zero_vert_normal_bulk = zero_vert_normal_bulk_SSE;
         m_matF_x_BatchedVertWeightList = m_matF_x_BatchedVertWeightList_SSE;

#if (_MSC_VER >= 1500)
         // I don't have SS4 so I can't test this...it *should* be right, or close
         if(false /*Platform::SystemInfo.processor.properties & CPU_PROP_SSE4*/)
            m_matF_x_BatchedVertWeightList = m_matF_x_BatchedVertWeightList_SSE4;
#endif
#endif
      }
      else if(Platform::SystemInfo.processor.properties & CPU_PROP_ALTIVEC)
      {
#if !defined(TORQUE_OS_XENON) && defined(TORQUE_CPU_PPC)
         zero_vert_normal_bulk = zero_vert_normal_bulk_gccvec;
         m_matF_x_BatchedVertWeightList = m_matF_x_BatchedVertWeightList_gccvec;
#endif
      }
   }
};
static _TSMeshIntrinsics_REG _sTSMeshIntrinsicsReg;