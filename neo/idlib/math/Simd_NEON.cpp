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

#include "sys/platform.h"
#include "idlib/math/Vector.h"
#include "idlib/math/Matrix.h"
#include "idlib/math/Plane.h"
#include "idlib/geometry/DrawVert.h"
#include "idlib/geometry/JointTransform.h"
#include "renderer/Model.h"
#include "idlib/math/Simd_NEON.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)

#include <arm_neon.h>
#include <cassert>

#define DRAWVERT_SIZE            60
#define DRAWVERT_XYZ_OFFSET      (0*4)

/* Portable float32x4_t constructor (aggregate init not valid in C++) */
static inline __attribute__((always_inline)) float32x4_t make_f32x4(float a, float b, float c, float d) {
    float t[4] = {a, b, c, d};
    return vld1q_f32(t);
}
static inline __attribute__((always_inline)) float32x2_t make_f32x2(float a, float b) {
    float t[2] = {a, b};
    return vld1_f32(t);
}

/* 2-step Newton-Raphson rsqrt (~24-bit accuracy) */
static inline __attribute__((always_inline)) float32x4_t neon_rsqrt(float32x4_t v) {
    float32x4_t e = vrsqrteq_f32(v);
    e = vmulq_f32(vrsqrtsq_f32(vmulq_f32(v,e),e),e);
    e = vmulq_f32(vrsqrtsq_f32(vmulq_f32(v,e),e),e);
    return e;
}
static inline __attribute__((always_inline)) float32x2_t neon_rsqrt2(float32x2_t v) {
    float32x2_t e = vrsqrte_f32(v);
    e = vmul_f32(vrsqrts_f32(vmul_f32(v,e),e),e);
    e = vmul_f32(vrsqrts_f32(vmul_f32(v,e),e),e);
    return e;
}
/* 2-step Newton-Raphson rcp */
static inline __attribute__((always_inline)) float32x4_t neon_rcp(float32x4_t v) {
    float32x4_t e = vrecpeq_f32(v);
    e = vmulq_f32(vrecpsq_f32(v,e),e);
    e = vmulq_f32(vrecpsq_f32(v,e),e);
    return e;
}

static inline __attribute__((always_inline)) float32x4_t neon_zero_w(float32x4_t v) {
    return vsetq_lane_f32(0.0f, v, 3);
}

static inline __attribute__((always_inline)) uint8x8_t neon_cmp_bytes(uint32x4_t low, uint32x4_t high) {
    low = vshrq_n_u32(low, 31);
    high = vshrq_n_u32(high, 31);
    return vmovn_u16(vcombine_u16(vmovn_u32(low), vmovn_u32(high)));
}

static inline __attribute__((always_inline)) void neon_transpose4(float32x4_t r0, float32x4_t r1,
                                                                  float32x4_t r2, float32x4_t r3,
                                                                  float32x4_t &c0, float32x4_t &c1,
                                                                  float32x4_t &c2, float32x4_t &c3) {
    const float32x4x2_t t01 = vtrnq_f32(r0, r1);
    const float32x4x2_t t23 = vtrnq_f32(r2, r3);
    c0 = vcombine_f32(vget_low_f32(t01.val[0]), vget_low_f32(t23.val[0]));
    c1 = vcombine_f32(vget_low_f32(t01.val[1]), vget_low_f32(t23.val[1]));
    c2 = vcombine_f32(vget_high_f32(t01.val[0]), vget_high_f32(t23.val[0]));
    c3 = vcombine_f32(vget_high_f32(t01.val[1]), vget_high_f32(t23.val[1]));
}

const char* idSIMD_NEON::GetName(void) const { return "NEON"; }

void VPCALL idSIMD_NEON::Add(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vaddq_f32(vld1q_f32(s+i),  vc));
        vst1q_f32(d+i+4,vaddq_f32(vld1q_f32(s+i+4),vc));
    }
    for(;i<n;++i) d[i]=s[i]+c;
}
void VPCALL idSIMD_NEON::Add(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vaddq_f32(vld1q_f32(s0+i),  vld1q_f32(s1+i)));
        vst1q_f32(d+i+4,vaddq_f32(vld1q_f32(s0+i+4),vld1q_f32(s1+i+4)));
    }
    for(;i<n;++i) d[i]=s0[i]+s1[i];
}
void VPCALL idSIMD_NEON::Sub(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vsubq_f32(vc,vld1q_f32(s+i)));
        vst1q_f32(d+i+4,vsubq_f32(vc,vld1q_f32(s+i+4)));
    }
    for(;i<n;++i) d[i]=c-s[i];
}
void VPCALL idSIMD_NEON::Sub(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vsubq_f32(vld1q_f32(s0+i),  vld1q_f32(s1+i)));
        vst1q_f32(d+i+4,vsubq_f32(vld1q_f32(s0+i+4),vld1q_f32(s1+i+4)));
    }
    for(;i<n;++i) d[i]=s0[i]-s1[i];
}
void VPCALL idSIMD_NEON::Mul(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmulq_f32(vld1q_f32(s+i),  vc));
        vst1q_f32(d+i+4,vmulq_f32(vld1q_f32(s+i+4),vc));
    }
    for(;i<n;++i) d[i]=s[i]*c;
}
void VPCALL idSIMD_NEON::Mul(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmulq_f32(vld1q_f32(s0+i),  vld1q_f32(s1+i)));
        vst1q_f32(d+i+4,vmulq_f32(vld1q_f32(s0+i+4),vld1q_f32(s1+i+4)));
    }
    for(;i<n;++i) d[i]=s0[i]*s1[i];
}
void VPCALL idSIMD_NEON::Div(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmulq_f32(vc,neon_rcp(vld1q_f32(s+i))));
        vst1q_f32(d+i+4,vmulq_f32(vc,neon_rcp(vld1q_f32(s+i+4))));
    }
    for(;i<n;++i) d[i]=c/s[i];
}
void VPCALL idSIMD_NEON::Div(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmulq_f32(vld1q_f32(s0+i),  neon_rcp(vld1q_f32(s1+i))));
        vst1q_f32(d+i+4,vmulq_f32(vld1q_f32(s0+i+4),neon_rcp(vld1q_f32(s1+i+4))));
    }
    for(;i<n;++i) d[i]=s0[i]/s1[i];
}
void VPCALL idSIMD_NEON::MulAdd(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmlaq_f32(vld1q_f32(d+i),  vld1q_f32(s+i),  vc));
        vst1q_f32(d+i+4,vmlaq_f32(vld1q_f32(d+i+4),vld1q_f32(s+i+4),vc));
    }
    for(;i<n;++i) d[i]+=s[i]*c;
}
void VPCALL idSIMD_NEON::MulAdd(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmlaq_f32(vld1q_f32(d+i),  vld1q_f32(s0+i),  vld1q_f32(s1+i)));
        vst1q_f32(d+i+4,vmlaq_f32(vld1q_f32(d+i+4),vld1q_f32(s0+i+4),vld1q_f32(s1+i+4)));
    }
    for(;i<n;++i) d[i]+=s0[i]*s1[i];
}
void VPCALL idSIMD_NEON::MulSub(float*d,const float c,const float*s,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmlsq_f32(vld1q_f32(d+i),  vld1q_f32(s+i),  vc));
        vst1q_f32(d+i+4,vmlsq_f32(vld1q_f32(d+i+4),vld1q_f32(s+i+4),vc));
    }
    for(;i<n;++i) d[i]-=s[i]*c;
}
void VPCALL idSIMD_NEON::MulSub(float*d,const float*s0,const float*s1,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmlsq_f32(vld1q_f32(d+i),  vld1q_f32(s0+i),  vld1q_f32(s1+i)));
        vst1q_f32(d+i+4,vmlsq_f32(vld1q_f32(d+i+4),vld1q_f32(s0+i+4),vld1q_f32(s1+i+4)));
    }
    for(;i<n;++i) d[i]-=s0[i]*s1[i];
}

void VPCALL idSIMD_NEON::CmpGE(byte*dst,const float*src0,const float constant,const int count){
    const float32x4_t c=vdupq_n_f32(constant); int i=0;
    for(;i<=count-8;i+=8){
        const uint8x8_t result=neon_cmp_bytes(vcgeq_f32(vld1q_f32(src0+i),c),vcgeq_f32(vld1q_f32(src0+i+4),c));
        vst1_u8(dst+i,result);
    }
    for(;i<count;++i) dst[i]=src0[i]>=constant;
}

void VPCALL idSIMD_NEON::CmpLT(byte*dst,const byte bitNum,const float*src0,const float constant,const int count){
    const float32x4_t c=vdupq_n_f32(constant); const int8x8_t shift=vdup_n_s8(bitNum); int i=0;
    for(;i<=count-8;i+=8){
        uint8x8_t result=neon_cmp_bytes(vcltq_f32(vld1q_f32(src0+i),c),vcltq_f32(vld1q_f32(src0+i+4),c));
        result=vshl_u8(result,shift);
        vst1_u8(dst+i,vorr_u8(vld1_u8(dst+i),result));
    }
    for(;i<count;++i) dst[i]|=(src0[i]<constant)<<bitNum;
}

/* 16-element aligned helpers */
void VPCALL idSIMD_NEON::Zero16(float*d,const int n){
    float32x4_t z=vdupq_n_f32(0.f); int i=0;
    for(;i<=n-4;i+=4) vst1q_f32(d+i,z);
    for(;i<n;++i) d[i]=0.f;
}
void VPCALL idSIMD_NEON::Negate16(float*d,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vnegq_f32(vld1q_f32(d+i)));vst1q_f32(d+i+4,vnegq_f32(vld1q_f32(d+i+4)));}
    for(;i<n;++i) d[i]=-d[i];
}
void VPCALL idSIMD_NEON::Copy16(float*d,const float*s,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vld1q_f32(s+i));vst1q_f32(d+i+4,vld1q_f32(s+i+4));}
    for(;i<n;++i) d[i]=s[i];
}
void VPCALL idSIMD_NEON::Add16(float*d,const float*s1,const float*s2,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vaddq_f32(vld1q_f32(s1+i),vld1q_f32(s2+i)));vst1q_f32(d+i+4,vaddq_f32(vld1q_f32(s1+i+4),vld1q_f32(s2+i+4)));}
    for(;i<n;++i) d[i]=s1[i]+s2[i];
}
void VPCALL idSIMD_NEON::Sub16(float*d,const float*s1,const float*s2,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vsubq_f32(vld1q_f32(s1+i),vld1q_f32(s2+i)));vst1q_f32(d+i+4,vsubq_f32(vld1q_f32(s1+i+4),vld1q_f32(s2+i+4)));}
    for(;i<n;++i) d[i]=s1[i]-s2[i];
}
void VPCALL idSIMD_NEON::Mul16(float*d,const float*s1,const float c,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vmulq_f32(vld1q_f32(s1+i),vc));vst1q_f32(d+i+4,vmulq_f32(vld1q_f32(s1+i+4),vc));}
    for(;i<n;++i) d[i]=s1[i]*c;
}
void VPCALL idSIMD_NEON::AddAssign16(float*d,const float*s,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vaddq_f32(vld1q_f32(d+i),vld1q_f32(s+i)));vst1q_f32(d+i+4,vaddq_f32(vld1q_f32(d+i+4),vld1q_f32(s+i+4)));}
    for(;i<n;++i) d[i]+=s[i];
}
void VPCALL idSIMD_NEON::SubAssign16(float*d,const float*s,const int n){
    int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vsubq_f32(vld1q_f32(d+i),vld1q_f32(s+i)));vst1q_f32(d+i+4,vsubq_f32(vld1q_f32(d+i+4),vld1q_f32(s+i+4)));}
    for(;i<n;++i) d[i]-=s[i];
}
void VPCALL idSIMD_NEON::MulAssign16(float*d,const float c,const int n){
    float32x4_t vc=vdupq_n_f32(c); int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vmulq_f32(vld1q_f32(d+i),vc));vst1q_f32(d+i+4,vmulq_f32(vld1q_f32(d+i+4),vc));}
    for(;i<n;++i) d[i]*=c;
}

/* Clamp */
void VPCALL idSIMD_NEON::Clamp(float*d,const float*s,const float mn,const float mx,const int n){
    float32x4_t vmin=vdupq_n_f32(mn),vmax=vdupq_n_f32(mx); int i=0;
    for(;i<=n-8;i+=8){
        vst1q_f32(d+i,  vmaxq_f32(vmin,vminq_f32(vmax,vld1q_f32(s+i))));
        vst1q_f32(d+i+4,vmaxq_f32(vmin,vminq_f32(vmax,vld1q_f32(s+i+4))));
    }
    for(;i<n;++i){float v=s[i];d[i]=v<mn?mn:(v>mx?mx:v);}
}
void VPCALL idSIMD_NEON::ClampMin(float*d,const float*s,const float mn,const int n){
    float32x4_t vmin=vdupq_n_f32(mn); int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vmaxq_f32(vmin,vld1q_f32(s+i)));vst1q_f32(d+i+4,vmaxq_f32(vmin,vld1q_f32(s+i+4)));}
    for(;i<n;++i) d[i]=s[i]<mn?mn:s[i];
}
void VPCALL idSIMD_NEON::ClampMax(float*d,const float*s,const float mx,const int n){
    float32x4_t vmax=vdupq_n_f32(mx); int i=0;
    for(;i<=n-8;i+=8){vst1q_f32(d+i,vminq_f32(vmax,vld1q_f32(s+i)));vst1q_f32(d+i+4,vminq_f32(vmax,vld1q_f32(s+i+4)));}
    for(;i<n;++i) d[i]=s[i]>mx?mx:s[i];
}

void VPCALL idSIMD_NEON::MixedSoundToSamples(short*samples,const float*mb,const int n){
    int i=0;
    for(;i<=n-8;i+=8){
        vst1q_s16(samples+i,
            vcombine_s16(vqmovn_s32(vcvtq_s32_f32(vld1q_f32(mb+i))),
                         vqmovn_s32(vcvtq_s32_f32(vld1q_f32(mb+i+4)))));
    }
    for(;i<n;++i){float v=mb[i];samples[i]=(v<=-32768.f)?-32768:(v>=32767.f)?32767:(short)v;}
}

void VPCALL idSIMD_NEON::UpSamplePCMTo44kHz(float*dest,const short*src,const int ns,const int kHz,const int ch){
    const int ratio=(kHz==11025)?4:(kHz==22050)?2:1;
    if(ch==1&&ratio==1){
        int i=0;
        for(;i<=ns-8;i+=8){
            int16x8_t s=vld1q_s16(src+i);
            vst1q_f32(dest+i,  vcvtq_f32_s32(vmovl_s16(vget_low_s16(s))));
            vst1q_f32(dest+i+4,vcvtq_f32_s32(vmovl_s16(vget_high_s16(s))));
        }
        for(;i<ns;++i) dest[i]=(float)src[i];
        return;
    }
    if(ch==1){
        for(int i=0;i<ns;++i){float v=(float)src[i];for(int r=0;r<ratio;++r)dest[i*ratio+r]=v;}
        return;
    }
    for(int i=0;i<ns/2;++i){
        float l=(float)src[i*2],r=(float)src[i*2+1];
        for(int k=0;k<ratio;++k){dest[(i*ratio+k)*2+0]=l;dest[(i*ratio+k)*2+1]=r;}
    }
}

void VPCALL idSIMD_NEON::MixSoundTwoSpeakerMono(float*mb,const float*s,const int n,
                                                  const float lV[2],const float cV[2]){
    assert(n==MIXBUFFER_SAMPLES);
    float iL=(cV[0]-lV[0])/n, iR=(cV[1]-lV[1])/n;
    float sL=lV[0]+iL, sR=lV[1]+iR;
    float iL2=iL*2.f, iR2=iR*2.f;
    int i=0;
    for(;i<=n-2;i+=2){
        float32x4_t sc=make_f32x4(sL,sR,sL+iL,sR+iR);
        float32x4_t sv=make_f32x4(s[i],s[i],s[i+1],s[i+1]);
        vst1q_f32(mb+i*2, vmlaq_f32(vld1q_f32(mb+i*2),sv,sc));
        sL+=iL2; sR+=iR2;
    }
    for(;i<n;++i){mb[i*2+0]+=s[i]*sL;mb[i*2+1]+=s[i]*sR;sL+=iL;sR+=iR;}
}

void VPCALL idSIMD_NEON::MixSoundTwoSpeakerStereo(float*mb,const float*s,const int n,
                                                    const float lV[2],const float cV[2]){
    assert(n==MIXBUFFER_SAMPLES);
    float iL=(cV[0]-lV[0])/n, iR=(cV[1]-lV[1])/n;
    float sL=lV[0]+iL, sR=lV[1]+iR;
    float iL2=iL*2.f, iR2=iR*2.f;
    int i=0;
    for(;i<=n-2;i+=2){
        float32x4_t sc=make_f32x4(sL,sR,sL+iL,sR+iR);
        vst1q_f32(mb+i*2, vmlaq_f32(vld1q_f32(mb+i*2),vld1q_f32(s+i*2),sc));
        sL+=iL2; sR+=iR2;
    }
}

void VPCALL idSIMD_NEON::MixSoundSixSpeakerMono(float*mb,const float*s,const int n,
                                                  const float lV[6],const float cV[6]){
    assert(n==MIXBUFFER_SAMPLES);
    float inc[6],sv[6];
    for(int c=0;c<6;++c){inc[c]=(cV[c]-lV[c])/n;sv[c]=lV[c]+inc[c];}
    for(int i=0;i<n;++i){
        float f=s[i]; float*p=mb+i*6;
        float32x4_t sv4=make_f32x4(sv[0],sv[1],sv[2],sv[3]);
        vst1q_f32(p,  vmlaq_f32(vld1q_f32(p),  vdupq_n_f32(f),sv4));
        float32x2_t sv2=make_f32x2(sv[4],sv[5]);
        vst1_f32(p+4, vmla_f32 (vld1_f32(p+4), vdup_n_f32(f), sv2));
        for(int c=0;c<6;++c) sv[c]+=inc[c];
    }
}

void VPCALL idSIMD_NEON::MixSoundSixSpeakerStereo(float*mb,const float*s,const int n,
                                                    const float lV[6],const float cV[6]){
    assert(n==MIXBUFFER_SAMPLES);
    float inc[6],sv[6];
    for(int c=0;c<6;++c){inc[c]=(cV[c]-lV[c])/n;sv[c]=lV[c]+inc[c];}
    for(int i=0;i<n;++i){
        float sl=s[i*2+0],sr=s[i*2+1]; float*p=mb+i*6;
        p[0]+=sl*sv[0];p[1]+=sr*sv[1];p[2]+=sl*sv[2];
        p[3]+=sl*sv[3];p[4]+=sl*sv[4];p[5]+=sr*sv[5];
        for(int c=0;c<6;++c) sv[c]+=inc[c];
    }
}

void VPCALL idSIMD_NEON::ConvertJointQuatsToJointMats(idJointMat*jm,
                                                        const idJointQuat*jq,
                                                        const int n){
    for(int i=0;i<n;++i){
        const float*q=jq[i].q.ToFloatPtr(); float*m=jm[i].ToFloatPtr();
        m[0*4+3]=q[4]; m[1*4+3]=q[5]; m[2*4+3]=q[6];
        const float x2=q[0]+q[0],y2=q[1]+q[1],z2=q[2]+q[2];
        const float xx=q[0]*x2,yy=q[1]*y2,zz=q[2]*z2;
        m[0*4+0]=1.f-yy-zz; m[1*4+1]=1.f-xx-zz; m[2*4+2]=1.f-xx-yy;
        const float yz=q[1]*z2,wx=q[3]*x2; m[2*4+1]=yz-wx; m[1*4+2]=yz+wx;
        const float xy=q[0]*y2,wz=q[3]*z2; m[1*4+0]=xy-wz; m[0*4+1]=xy+wz;
        const float xz=q[0]*z2,wy=q[3]*y2; m[0*4+2]=xz-wy; m[2*4+0]=xz+wy;
    }
}

void VPCALL idSIMD_NEON::TransformJoints(idJointMat*jm,const int*p,
                                           const int f,const int l){
    for(int i=f;i<=l;++i){assert(p[i]<i);jm[i]*=jm[p[i]];}
}
void VPCALL idSIMD_NEON::UntransformJoints(idJointMat*jm,const int*p,
                                             const int f,const int l){
    for(int i=l;i>=f;--i){assert(p[i]<i);jm[i]/=jm[p[i]];}
}

void VPCALL idSIMD_NEON::TransformVerts(idDrawVert*verts,const int nv,
                                          const idJointMat*joints,
                                          const idVec4*weights,const int*index,
                                          const int /*numWeights*/){
    const byte*jp=reinterpret_cast<const byte*>(joints); int j=0;
    for(int i=0;i<nv;++i){
        const float*wf=weights[j].ToFloatPtr();
        const idJointMat&M=*reinterpret_cast<const idJointMat*>(jp+index[j*2]);
        idVec3 r=M*idVec4(wf[0],wf[1],wf[2],wf[3]);
        while(index[j*2+1]==0){
            ++j;
            const float*wf2=weights[j].ToFloatPtr();
            const idJointMat&M2=*reinterpret_cast<const idJointMat*>(jp+index[j*2]);
            r+=M2*idVec4(wf2[0],wf2[1],wf2[2],wf2[3]);
        }
        ++j;
        verts[i].xyz=r;
    }
}

void VPCALL idSIMD_NEON::DeriveUnsmoothedTangents(idDrawVert*verts,
                                                    const dominantTri_s*dominantTris,
                                                    const int numVerts){
    int i=0;
    for(;i<=numVerts-4;i+=4){
        const dominantTri_s &d0=dominantTris[i+0],&d1=dominantTris[i+1];
        const dominantTri_s &d2=dominantTris[i+2],&d3=dominantTris[i+3];
        const float*a0=verts[i+0].xyz.ToFloatPtr(),*a1=verts[i+1].xyz.ToFloatPtr();
        const float*a2=verts[i+2].xyz.ToFloatPtr(),*a3=verts[i+3].xyz.ToFloatPtr();
        const float*b0=verts[d0.v2].xyz.ToFloatPtr(),*b1=verts[d1.v2].xyz.ToFloatPtr();
        const float*b2=verts[d2.v2].xyz.ToFloatPtr(),*b3=verts[d3.v2].xyz.ToFloatPtr();
        const float*c0=verts[d0.v3].xyz.ToFloatPtr(),*c1=verts[d1.v3].xyz.ToFloatPtr();
        const float*c2=verts[d2.v3].xyz.ToFloatPtr(),*c3=verts[d3.v3].xyz.ToFloatPtr();

        float32x4_t ax,ay,az,unused,bx,by,bz,cx,cy,cz;
        neon_transpose4(vld1q_f32(a0),vld1q_f32(a1),vld1q_f32(a2),vld1q_f32(a3),ax,ay,az,unused);
        neon_transpose4(vld1q_f32(b0),vld1q_f32(b1),vld1q_f32(b2),vld1q_f32(b3),bx,by,bz,unused);
        neon_transpose4(vld1q_f32(c0),vld1q_f32(c1),vld1q_f32(c2),vld1q_f32(c3),cx,cy,cz,unused);

        const float32x4_t x0=vsubq_f32(bx,ax),y0=vsubq_f32(by,ay),z0=vsubq_f32(bz,az);
        const float32x4_t x1=vsubq_f32(cx,ax),y1=vsubq_f32(cy,ay),z1=vsubq_f32(cz,az);
        const float32x4_t sy0=make_f32x4(b0[4]-a0[4],b1[4]-a1[4],b2[4]-a2[4],b3[4]-a3[4]);
        const float32x4_t sy1=make_f32x4(c0[4]-a0[4],c1[4]-a1[4],c2[4]-a2[4],c3[4]-a3[4]);
        const float32x4_t ns=make_f32x4(d0.normalizationScale[2],d1.normalizationScale[2],d2.normalizationScale[2],d3.normalizationScale[2]);
        const float32x4_t ts=make_f32x4(d0.normalizationScale[0],d1.normalizationScale[0],d2.normalizationScale[0],d3.normalizationScale[0]);
        const float32x4_t bs=make_f32x4(d0.normalizationScale[1],d1.normalizationScale[1],d2.normalizationScale[1],d3.normalizationScale[1]);

        const float32x4_t nx=vmulq_f32(ns,vsubq_f32(vmulq_f32(y1,z0),vmulq_f32(z1,y0)));
        const float32x4_t ny=vmulq_f32(ns,vsubq_f32(vmulq_f32(z1,x0),vmulq_f32(x1,z0)));
        const float32x4_t nz=vmulq_f32(ns,vsubq_f32(vmulq_f32(x1,y0),vmulq_f32(y1,x0)));
        const float32x4_t tx=vmulq_f32(ts,vsubq_f32(vmulq_f32(x0,sy1),vmulq_f32(sy0,x1)));
        const float32x4_t ty=vmulq_f32(ts,vsubq_f32(vmulq_f32(y0,sy1),vmulq_f32(sy0,y1)));
        const float32x4_t tz=vmulq_f32(ts,vsubq_f32(vmulq_f32(z0,sy1),vmulq_f32(sy0,z1)));
        const float32x4_t ux=vmulq_f32(bs,vsubq_f32(vmulq_f32(nz,ty),vmulq_f32(ny,tz)));
        const float32x4_t uy=vmulq_f32(bs,vsubq_f32(vmulq_f32(nx,tz),vmulq_f32(nz,tx)));
        const float32x4_t uz=vmulq_f32(bs,vsubq_f32(vmulq_f32(ny,tx),vmulq_f32(nx,ty)));

        float32x4_t o0,o1,o2,o3;
        neon_transpose4(nx,ny,nz,tx,o0,o1,o2,o3);
        vst1q_f32(verts[i+0].normal.ToFloatPtr(),o0);
        vst1q_f32(verts[i+1].normal.ToFloatPtr(),o1);
        vst1q_f32(verts[i+2].normal.ToFloatPtr(),o2);
        vst1q_f32(verts[i+3].normal.ToFloatPtr(),o3);
        neon_transpose4(ty,tz,ux,uy,o0,o1,o2,o3);
        vst1q_f32(&verts[i+0].tangents[0][1],o0);
        vst1q_f32(&verts[i+1].tangents[0][1],o1);
        vst1q_f32(&verts[i+2].tangents[0][1],o2);
        vst1q_f32(&verts[i+3].tangents[0][1],o3);
        verts[i+0].tangents[1][2]=vgetq_lane_f32(uz,0);
        verts[i+1].tangents[1][2]=vgetq_lane_f32(uz,1);
        verts[i+2].tangents[1][2]=vgetq_lane_f32(uz,2);
        verts[i+3].tangents[1][2]=vgetq_lane_f32(uz,3);
    }
    for(;i<numVerts;++i){
        idDrawVert&a=verts[i];
        const idDrawVert&b=verts[dominantTris[i].v2],&c=verts[dominantTris[i].v3];
        const idVec3 e0=b.xyz-a.xyz,e1=c.xyz-a.xyz;
        const dominantTri_s&d=dominantTris[i];
        a.normal=d.normalizationScale[2]*idVec3(e1.y*e0.z-e1.z*e0.y,e1.z*e0.x-e1.x*e0.z,e1.x*e0.y-e1.y*e0.x);
        const float sy0=b.st.y-a.st.y,sy1=c.st.y-a.st.y;
        a.tangents[0]=d.normalizationScale[0]*idVec3(e0.x*sy1-sy0*e1.x,e0.y*sy1-sy0*e1.y,e0.z*sy1-sy0*e1.z);
        a.tangents[1]=d.normalizationScale[1]*idVec3(a.normal.z*a.tangents[0].y-a.normal.y*a.tangents[0].z,a.normal.x*a.tangents[0].z-a.normal.z*a.tangents[0].x,a.normal.y*a.tangents[0].x-a.normal.x*a.tangents[0].y);
    }
}

int VPCALL idSIMD_NEON::CreateVertexProgramShadowCache(idVec4*vc,
                                                         const idDrawVert*v,
                                                         const int n){
    const char*base=reinterpret_cast<const char*>(v);
    for(int i=0;i<n;++i){
        const float*xyz=reinterpret_cast<const float*>(
            base+i*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float x=xyz[0],y=xyz[1],z=xyz[2];
        vst1q_f32(vc[i*2+0].ToFloatPtr(), make_f32x4(x,y,z,1.f));
        vst1q_f32(vc[i*2+1].ToFloatPtr(), make_f32x4(x,y,z,0.f));
    }
    return n*2;
}

int VPCALL idSIMD_NEON::CreateShadowCache(idVec4*vc,int*vr,
                                            const idVec3&lo,
                                            const idDrawVert*v,const int n){
    const char*base=reinterpret_cast<const char*>(v);
    int out=0;
    for(int i=0;i<n;++i){
        if(vr[i]!=0) continue;   /* already remapped */
        const float*xyz=reinterpret_cast<const float*>(
            base+i*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        vst1q_f32(vc[out+0].ToFloatPtr(),
                  make_f32x4(xyz[0],       xyz[1],       xyz[2],       1.f));
        vst1q_f32(vc[out+1].ToFloatPtr(),
                  make_f32x4(xyz[0]-lo[0], xyz[1]-lo[1], xyz[2]-lo[2], 0.f));
        vr[i]=out; out+=2;
    }
    return out;
}

void VPCALL idSIMD_NEON::MinMax(idVec3&mn,idVec3&mx,
                                  const idDrawVert*src,
                                  const int*indexes,const int count){
    float32x4_t vmin=vdupq_n_f32( idMath::INFINITY);
    float32x4_t vmax=vdupq_n_f32(-idMath::INFINITY);
    const char*base=reinterpret_cast<const char*>(src);
    for(int i=0;i<count;++i){
        const float*xyz=reinterpret_cast<const float*>(
            base+indexes[i]*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        float32x4_t v=make_f32x4(xyz[0],xyz[1],xyz[2],0.f);
        vmin=vminq_f32(vmin,v); vmax=vmaxq_f32(vmax,v);
    }
    float a[4],b[4]; vst1q_f32(a,vmin); vst1q_f32(b,vmax);
    mn.Set(a[0],a[1],a[2]); mx.Set(b[0],b[1],b[2]);
}

void VPCALL idSIMD_NEON::MinMax(idVec3&mn,idVec3&mx,
                                  const idDrawVert*src,
                                  const short*indexes,const int count){
    float32x4_t vmin=vdupq_n_f32( idMath::INFINITY);
    float32x4_t vmax=vdupq_n_f32(-idMath::INFINITY);
    const char*base=reinterpret_cast<const char*>(src);
    for(int i=0;i<count;++i){
        const float*xyz=reinterpret_cast<const float*>(
            base+indexes[i]*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float32x4_t v=neon_zero_w(vld1q_f32(xyz));
        vmin=vminq_f32(vmin,v); vmax=vmaxq_f32(vmax,v);
    }
    float a[4],b[4]; vst1q_f32(a,vmin); vst1q_f32(b,vmax);
    mn.Set(a[0],a[1],a[2]); mx.Set(b[0],b[1],b[2]);
}

void VPCALL idSIMD_NEON::Dot(float*dst,const idPlane&constant,
                               const idDrawVert*src,const int count){
    const float*c=constant.ToFloatPtr();
    float32x4_t c0=vdupq_n_f32(c[0]),c1=vdupq_n_f32(c[1]),
                c2=vdupq_n_f32(c[2]),c3=vdupq_n_f32(c[3]);
    const char*base=reinterpret_cast<const char*>(src);
    int i=0;
    for(;i<=count-4;i+=4){
        const float*p0=reinterpret_cast<const float*>(base+(i+0)*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float*p1=reinterpret_cast<const float*>(base+(i+1)*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float*p2=reinterpret_cast<const float*>(base+(i+2)*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float*p3=reinterpret_cast<const float*>(base+(i+3)*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        const float32x4_t p0v=vld1q_f32(p0);
        const float32x4_t p1v=vld1q_f32(p1);
        const float32x4_t p2v=vld1q_f32(p2);
        const float32x4_t p3v=vld1q_f32(p3);
        const float32x4x2_t t01=vtrnq_f32(p0v,p1v), t23=vtrnq_f32(p2v,p3v);
        const float32x4_t x4=vcombine_f32(vget_low_f32(t01.val[0]),vget_low_f32(t23.val[0]));
        const float32x4_t y4=vcombine_f32(vget_low_f32(t01.val[1]),vget_low_f32(t23.val[1]));
        const float32x4_t z4=vcombine_f32(vget_high_f32(t01.val[0]),vget_high_f32(t23.val[0]));
        float32x4_t r=c3;
        r=vmlaq_f32(r,x4,c0); r=vmlaq_f32(r,y4,c1); r=vmlaq_f32(r,z4,c2);
        vst1q_f32(dst+i,r);
    }
    for(;i<count;++i){
        const float*xyz=reinterpret_cast<const float*>(
            base+i*DRAWVERT_SIZE+DRAWVERT_XYZ_OFFSET);
        dst[i]=c[0]*xyz[0]+c[1]*xyz[1]+c[2]*xyz[2]+c[3];
    }
}

void VPCALL idSIMD_NEON::Dot(float*dst,const idVec3&constant,
                               const idPlane*src,const int count){
    float32x4_t c0=vdupq_n_f32(constant[0]);
    float32x4_t c1=vdupq_n_f32(constant[1]);
    float32x4_t c2=vdupq_n_f32(constant[2]);
    int i=0;
    for(;i<=count-4;i+=4){
        float32x4_t n0=vld1q_f32(src[i+0].ToFloatPtr());
        float32x4_t n1=vld1q_f32(src[i+1].ToFloatPtr());
        float32x4_t n2=vld1q_f32(src[i+2].ToFloatPtr());
        float32x4_t n3=vld1q_f32(src[i+3].ToFloatPtr());
        /* Transpose 4x4 to get column vectors */
        float32x4x2_t t01=vtrnq_f32(n0,n1), t23=vtrnq_f32(n2,n3);
        float32x4_t col0=vcombine_f32(vget_low_f32(t01.val[0]),vget_low_f32(t23.val[0]));
        float32x4_t col1=vcombine_f32(vget_low_f32(t01.val[1]),vget_low_f32(t23.val[1]));
        float32x4_t col2=vcombine_f32(vget_high_f32(t01.val[0]),vget_high_f32(t23.val[0]));
        float32x4_t col3=vcombine_f32(vget_high_f32(t01.val[1]),vget_high_f32(t23.val[1]));
        float32x4_t r=col3;
        r=vmlaq_f32(r,col0,c0); r=vmlaq_f32(r,col1,c1); r=vmlaq_f32(r,col2,c2);
        vst1q_f32(dst+i,r);
    }
    for(;i<count;++i){
        const float*p=src[i].ToFloatPtr();
        dst[i]=constant[0]*p[0]+constant[1]*p[1]+constant[2]*p[2]+p[3];
    }
}

#endif /* __ARM_NEON */
