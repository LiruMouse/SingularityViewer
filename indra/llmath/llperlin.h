/** 
 * @file llperlin.h
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_PERLIN_H
#define LL_PERLIN_H

#include "stdtypes.h"
#include "llmath.h"
#include "v2math.h"
#include "v3math.h"

// namespace wrapper
class LLPerlinNoise
{
public:
	static F32 noise(const F32& x, U32 wrap_at = 256)
	{
		U8 b[1][2];
		F32 r[1][2], s[1], u, v;

		fast_setup(&x, b, r, s, wrap_at);

		u = grad(p[b[VX][0]], r[VX][0]);
		v = grad(p[b[VX][1]], r[VX][1]);

		return lerp(u, v, s[VX]);
	}
	static F32 noise(const LLVector2& vec, U32 wrap_at = 256)
	{
		U8 b[2][2];
		F32 r[2][2], s[2], u, v, A, B;

		fast_setup(vec.mV, b, r, s, wrap_at);

		u = grad(p[p[b[VX][0]] + b[VY][0]], r[VX][0], r[VY][0]);
		v = grad(p[p[b[VX][1]] + b[VY][0]], r[VX][1], r[VY][0]);
		A = lerp(u, v, s[VX]);

		u = grad(p[p[b[VX][0]] + b[VY][1]], r[VX][0], r[VY][1]);
		v = grad(p[p[b[VX][1]] + b[VY][1]], r[VX][1], r[VY][1]);
		B = lerp(u, v, s[VX]);

		return lerp(A, B, s[VY]);
	}
	static F32 noise(const LLVector3& vec, U32 wrap_at = 256)
	{
		U8 b[3][2];
		F32 r[3][2], s[3], u, v, A, B, C, D;

		fast_setup(vec.mV, b, r, s, wrap_at);

		u = grad(p[p[p[b[VX][0]] + b[VY][0]] + b[VZ][0]], r[VX][0], r[VY][0], r[VZ][0]);
		v = grad(p[p[p[b[VX][1]] + b[VY][0]] + b[VZ][0]], r[VX][1], r[VY][0], r[VZ][0]);
		A = lerp(u, v, s[VX]);

		u = grad(p[p[p[b[VX][0]] + b[VY][1]] + b[VZ][0]], r[VX][0], r[VY][1], r[VZ][0]);
		v = grad(p[p[p[b[VX][1]] + b[VY][1]] + b[VZ][0]], r[VX][1], r[VY][1], r[VZ][0]);
		B = lerp(u, v, s[VX]);

		C = lerp(A, B, s[VY]);

		u = grad(p[p[p[b[VX][0]] + b[VY][0]] + b[VZ][1]], r[VX][0], r[VY][0], r[VZ][1]);
		v = grad(p[p[p[b[VX][1]] + b[VY][0]] + b[VZ][1]], r[VX][1], r[VY][0], r[VZ][1]);
		A = lerp(u, v, s[VX]);

		u = grad(p[p[p[b[VX][0]] + b[VY][1]] + b[VZ][1]], r[VX][0], r[VY][1], r[VZ][1]);
		v = grad(p[p[p[b[VX][1]] + b[VY][1]] + b[VZ][1]], r[VX][1], r[VY][1], r[VZ][1]);
		B = lerp(u, v, s[VX]);

		D = lerp(A, B, s[VY]);

		return lerp(C, D, s[VZ]) * 0.77f;
	}
	template <typename T>
	static F32 turbulence(const T& vec, F32 freq, U32 wrap_at = 256)
	{
		F32 t;
		for (t = 0.f; freq >= 1.f; freq *= 0.5f)
		{
			t += noise(vec * freq, wrap_at) / freq;
			//		t += fabs(noise(vec * freq)) / freq;					// Like snow - bubbly at low frequencies
			//		t += sqrt(fabs(noise(vec * freq))) / freq;				// Better at low freq
			//		t += (noise(vec * freq)*noise(vec * freq)) / freq;
		}
		return t;
	}
	template <typename T>
	static F32 clouds(const T& vec, F32 freq, U32 wrap_at = 256)
	{
		F32 t;
		for (t = 0.f; freq >= 1.f; freq *= 0.5f)
		{
			//		t += noise(vec * freq)/freq;
			//		t += fabs(noise(vec * freq)) / freq;					// Like snow - bubbly at low frequencies
			//		t += sqrt(fabs(noise(vec * freq))) / freq;				// Better at low freq
			t += (noise(vec * freq, wrap_at)*noise(vec * freq, wrap_at)) / freq;
		}
		return t;
	}
private:
	static F32 s_curve(const F32 t)
	{
		return t * t * t * (t * (6.f * t - 15.f) + 10.f);		//5th degree
		//return t * t * (3.f - 2.f * t);						//3rd degree
	}

	static F32 grad(U32 hash, F32 x)
	{
		return x * (2.f*F32(hash) / 255.f - 1.f);
	}

	static F32 grad(U32 hash, F32 x, F32 y)
	{
		//Rotated slightly off the axes. Reduces directional artifacts.
		//Scaled to match the old perlin method's output range
		static const F32 l[2] = { .466666667f, .933333332f };
		static const LLVector2 vecs[] = {
			LLVector2(l[0], l[1]), LLVector2(l[0], -l[1]), LLVector2(-l[0], l[1]), LLVector2(-l[0], -l[1]),
			LLVector2(l[1], l[0]), LLVector2(l[1], -l[0]), LLVector2(-l[1], l[0]), LLVector2(-l[1], -l[0]) };

		return vecs[hash % LL_ARRAY_SIZE(vecs)] * LLVector2(x, y);
	}

	static F32 grad(U32 hash, F32 x, F32 y, F32 z)
	{
		static const LLVector3 vecs[] = {
			OO_SQRT3*LLVector3(1, 1, 0), OO_SQRT3*LLVector3(-1, 1, 0), OO_SQRT3*LLVector3(1, -1, 0), OO_SQRT3*LLVector3(-1, -1, 0),
			OO_SQRT3*LLVector3(1, 0, 1), OO_SQRT3*LLVector3(-1, 0, 1), OO_SQRT3*LLVector3(1, 0, -1), OO_SQRT3*LLVector3(-1, 0, -1),
			OO_SQRT3*LLVector3(0, 1, 1), OO_SQRT3*LLVector3(0, -1, 1), OO_SQRT3*LLVector3(0, 1, -1), OO_SQRT3*LLVector3(0, -1, -1) };

		return vecs[hash % LL_ARRAY_SIZE(vecs)] * LLVector3(x, y, z);
	}

	template <int N>
	static void fast_setup(const F32* vec, U8 (&b)[N][2], F32 (&r)[N][2], F32 (&s)[N], const U32& wrap_at)
	{
		const U32 limit = llclamp(wrap_at, U32(1), U32(256));
		for (U32 i = 0; i < N; ++i)
		{
			const S32 t_S32 = llfloor(vec[i]);
			b[i][0] = (t_S32) % limit;
			b[i][1] = (t_S32 + 1) % limit;
			r[i][0] = vec[i] - F32(t_S32);
			r[i][1] = r[i][0] - 1.f;
			s[i] = s_curve(r[i][0]);
		}
	}

	static const U32 sPremutationCount = 512;
	static const U8 p[sPremutationCount];
};

#endif // LL_PERLIN_
