/*
Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <OpenColorSpace/OpenColorSpace.h>

#include "MatrixOps.h"
#include "MathUtils.h"

#include <cstring>
#include <sstream>

OCS_NAMESPACE_ENTER
{
    namespace
    {
        void ApplyMatrixNoAlpha(float* rgbaBuffer, long numPixels,
                                const float* mat44)
        {
            float r,g,b;
            
            for(long pixelIndex=0; pixelIndex<numPixels; ++pixelIndex)
            {
                r = rgbaBuffer[0];
                g = rgbaBuffer[1];
                b = rgbaBuffer[2];
                
                rgbaBuffer[0] = r*mat44[0] + g*mat44[1] + b*mat44[2];
                rgbaBuffer[1] = r*mat44[4] + g*mat44[5] + b*mat44[6];
                rgbaBuffer[2] = r*mat44[8] + g*mat44[9] + b*mat44[10];
                
                rgbaBuffer += 4;
            }
        }
        
        void ApplyOffsetNoAlpha(float* rgbaBuffer, long numPixels,
                                const float* offset4)
        {
            for(long pixelIndex=0; pixelIndex<numPixels; ++pixelIndex)
            {
                rgbaBuffer[0] += offset4[0];
                rgbaBuffer[1] += offset4[1];
                rgbaBuffer[2] += offset4[2];
                
                rgbaBuffer += 4;
            }
        }
    }
    
    
    namespace
    {
        class MatrixOffsetOp : public Op
        {
        public:
            MatrixOffsetOp(const float * m44,
                           const float * offset4,
                           TransformDirection direction);
            virtual ~MatrixOffsetOp();
            
            virtual void preRender();
            virtual void render(float* rgbaBuffer, long numPixels) const;
        
        private:
            float m_m44[16];
            float m_offset4[4];
            TransformDirection m_direction;
        };
        
        typedef SharedPtr<MatrixOffsetOp> MatrixOffsetOpRcPtr;
        
        // TODO: Optimize the sub-cases. (dispatch to different calls based on
        // matrix analysis
        
        MatrixOffsetOp::MatrixOffsetOp(const float * m44,
                                       const float * offset4,
                                       TransformDirection direction):
                                       Op(),
                                       m_direction(direction)
        {
            if(m_direction == TRANSFORM_DIR_UNKNOWN)
            {
                throw OCSException("Cannot apply MatrixOffsetOp op, unspecified transform direction.");
            }
            
            memcpy(m_m44, m44, 16*sizeof(float));
            memcpy(m_offset4, offset4, 4*sizeof(float));
        }
        
        MatrixOffsetOp::~MatrixOffsetOp()
        { }
        
        void MatrixOffsetOp::preRender()
        {
        
        }
        void MatrixOffsetOp::render(float* rgbaBuffer, long numPixels) const
        {
            if(!rgbaBuffer) return;
            
            if(m_direction == TRANSFORM_DIR_FORWARD)
            {
                if(!IsM44Identity(m_m44))
                {
                    ApplyMatrixNoAlpha(rgbaBuffer, numPixels, m_m44);
                }
                
                if(!IsVecEqualToZero(m_offset4, 4))
                {
                    ApplyOffsetNoAlpha(rgbaBuffer, numPixels, m_offset4);
                }
            }
            else if(m_direction == TRANSFORM_DIR_INVERSE)
            {
                if(!IsVecEqualToZero(m_offset4, 4))
                {
                    float invOffset4[4] = { -m_offset4[0],
                                            -m_offset4[1],
                                            -m_offset4[2],
                                            -m_offset4[3] };
                    ApplyOffsetNoAlpha(rgbaBuffer, numPixels, invOffset4);
                }
                
                if(!IsM44Identity(m_m44))
                {
                    // TODO : Cache mtx inverses
                    float invMtx44[16];
                    if(!GetM44Inverse(invMtx44, m_m44))
                    {
                        std::ostringstream os;
                        os << "Cannot apply MatrixOffsetOp op. ";
                        os << "Matrix inverse does not exist for m44 (";
                        for(int i=0; i<16; ++i) os << m_m44[i] << " ";
                        os << ").";
                        throw OCSException(os.str().c_str());
                    }
                    
                    ApplyMatrixNoAlpha(rgbaBuffer, numPixels, invMtx44);
                }
            }
        
        } // Op::process
        
        
        
    }  // Anon namespace
    
    
    
    
    
    void CreateScaleOp(OpRcPtrVec * opVec,
                       const float * scale4,
                       TransformDirection direction)
    {
        float offset4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        CreateScaleOffsetOp(opVec, scale4, offset4, direction);
    }
    
    void CreateMatrixOp(OpRcPtrVec * opVec,
                        const float * m44,
                        TransformDirection direction)
    {
        float offset4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        CreateMatrixOffsetOp(opVec, m44, offset4, direction);
    }
    
    void CreateOffsetOp(OpRcPtrVec * opVec,
                        const float * offset4,
                        TransformDirection direction)
    {
        float scale4[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        CreateScaleOffsetOp(opVec, scale4, offset4, direction);
    }
    
    void CreateScaleOffsetOp(OpRcPtrVec * opVec,
                             const float * scale4, const float * offset4,
                             TransformDirection direction)
    {
        float m44[16];
        memset(m44, 0, 16*sizeof(float));
        
        m44[0] = scale4[0];
        m44[5] = scale4[1];
        m44[10] = scale4[2];
        m44[15] = scale4[3];
        
        CreateMatrixOffsetOp(opVec,
                             m44, offset4,
                             direction);
    }
    
    void CreateSaturationOp(OpRcPtrVec * opVec,
                            float sat,
                            const float * lumaCoef3,
                            TransformDirection direction)
    {
        float m44[16];
        
        m44[0] = (1 - sat) * lumaCoef3[0] + sat;
        m44[1] = (1 - sat) * lumaCoef3[1];
        m44[2] = (1 - sat) * lumaCoef3[2];
        m44[3] = 0.0f;
        
        m44[4] = (1 - sat) * lumaCoef3[0];
        m44[5] = (1 - sat) * lumaCoef3[1] + sat;
        m44[6] = (1 - sat) * lumaCoef3[2];
        m44[7] = 0.0f;
        
        m44[8] = (1 - sat) * lumaCoef3[0];
        m44[9] = (1 - sat) * lumaCoef3[1];
        m44[10] = (1 - sat) * lumaCoef3[2] + sat;
        m44[11] = 0.0f;
        
        m44[12] = 0.0f;
        m44[13] = 0.0f;
        m44[14] = 0.0f;
        m44[15] = 1.0f;
        
        CreateMatrixOp(opVec, m44, direction);
    }
    
    void CreateMatrixOffsetOp(OpRcPtrVec * opVec,
                              const float * m44, const float * offset4,
                              TransformDirection direction)
    {
        bool mtxIsIdentity = IsM44Identity(m44);
        bool offsetIsIdentity = IsVecEqualToZero(offset4, 4);
        if(mtxIsIdentity && offsetIsIdentity) return;
        
        opVec->push_back( MatrixOffsetOpRcPtr(new MatrixOffsetOp(m44,
            offset4, direction)) );
    }
}
OCS_NAMESPACE_EXIT
