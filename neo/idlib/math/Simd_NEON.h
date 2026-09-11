/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#ifndef __MATH_SIMD_NEON_H__
#define __MATH_SIMD_NEON_H__

#include "idlib/math/Simd_Generic.h"

/*
===============================================================================

	NEON implementation of idSIMDProcessor

===============================================================================
*/

class idSIMD_NEON : public idSIMD_Generic
{
public:
    virtual const char* VPCALL GetName(void) const;

    virtual void VPCALL Add(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL Add(float*dst,const float*s0,const float*s1,const int n);
    virtual void VPCALL Sub(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL Sub(float*dst,const float*s0,const float*s1,const int n);
    virtual void VPCALL Mul(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL Mul(float*dst,const float*s0,const float*s1,const int n);
    virtual void VPCALL Div(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL Div(float*dst,const float*s0,const float*s1,const int n);
    virtual void VPCALL MulAdd(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL MulAdd(float*dst,const float*s0,const float*s1,const int n);
    virtual void VPCALL MulSub(float*dst,const float c,const float*src,const int n);
    virtual void VPCALL MulSub(float*dst,const float*s0,const float*s1,const int n);

    virtual void VPCALL CmpGE(byte*dst,const float*src0,const float constant,const int count);
    virtual void VPCALL CmpLT(byte*dst,const byte bitNum,const float*src0,const float constant,const int count);

    virtual void VPCALL Zero16(float*dst,const int count);
    virtual void VPCALL Negate16(float*dst,const int count);
    virtual void VPCALL Copy16(float*dst,const float*src,const int count);
    virtual void VPCALL Add16(float*dst,const float*s1,const float*s2,const int count);
    virtual void VPCALL Sub16(float*dst,const float*s1,const float*s2,const int count);
    virtual void VPCALL Mul16(float*dst,const float*s1,const float c,const int count);
    virtual void VPCALL AddAssign16(float*dst,const float*src,const int count);
    virtual void VPCALL SubAssign16(float*dst,const float*src,const int count);
    virtual void VPCALL MulAssign16(float*dst,const float c,const int count);

    virtual void VPCALL Clamp(float*dst,const float*src,const float mn,const float mx,const int count);
    virtual void VPCALL ClampMin(float*dst,const float*src,const float mn,const int count);
    virtual void VPCALL ClampMax(float*dst,const float*src,const float mx,const int count);

    virtual void VPCALL Dot(float*dst,const idPlane&constant,const idDrawVert*src,const int count);
    virtual void VPCALL Dot(float*dst,const idVec3&constant,const idPlane*src,const int count);

    virtual void VPCALL MinMax(idVec3&mn,idVec3&mx,const idDrawVert*src,const int count);
    virtual void VPCALL MinMax(idVec3&mn,idVec3&mx,const idDrawVert*src,const int*indexes,const int count);
    virtual void VPCALL MinMax(idVec3&mn,idVec3&mx,const idDrawVert*src,const short*indexes,const int count);

    virtual void VPCALL ConvertJointQuatsToJointMats(idJointMat*jm,const idJointQuat*jq,const int n);
    virtual void VPCALL TransformJoints(idJointMat*jm,const int*parents,const int f,const int l);
    virtual void VPCALL UntransformJoints(idJointMat*jm,const int*parents,const int f,const int l);
    virtual void VPCALL TransformVerts(idDrawVert*verts,const int nv,const idJointMat*joints,
                                        const idVec4*weights,const int*index,const int numWeights);
    virtual void VPCALL DeriveUnsmoothedTangents(idDrawVert*verts,const dominantTri_s*dominantTris,const int numVerts);

    virtual int  VPCALL CreateShadowCache(idVec4*vc,int*vr,const idVec3&lo,const idDrawVert*v,const int n);
    virtual int  VPCALL CreateVertexProgramShadowCache(idVec4*vc,const idDrawVert*v,const int n);

    virtual void VPCALL MixedSoundToSamples(short*samples,const float*mixBuffer,const int numSamples);
    virtual void VPCALL UpSamplePCMTo44kHz(float*dest,const short*src,const int ns,const int kHz,const int ch);
    virtual void VPCALL MixSoundTwoSpeakerMono(float*mb,const float*s,const int n,const float lV[2],const float cV[2]);
    virtual void VPCALL MixSoundTwoSpeakerStereo(float*mb,const float*s,const int n,const float lV[2],const float cV[2]);
    virtual void VPCALL MixSoundSixSpeakerMono(float*mb,const float*s,const int n,const float lV[6],const float cV[6]);
    virtual void VPCALL MixSoundSixSpeakerStereo(float*mb,const float*s,const int n,const float lV[6],const float cV[6]);
};

#endif /* __MATH_SIMD_NEON_H__ */
