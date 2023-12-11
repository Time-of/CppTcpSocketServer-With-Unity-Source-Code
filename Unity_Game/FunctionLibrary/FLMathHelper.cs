using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;


/// <summary>
/// 수학 도우미 함수 라이브러리 클래스입니다...
/// </summary>
public static class FLMathHelper
{
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static float GetSignedVectorLengthSquared(this Vector3 vec)
	{
		return Mathf.Sign(vec.x) * vec.x * vec.x + Mathf.Sign(vec.y) * vec.y * vec.y + Mathf.Sign(vec.z) * vec.z * vec.z;
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool NearlyEqual(Vector3 a, Vector3 b, float tolerance)
	{
		return Vector3.Distance(a, b) < tolerance;
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Vector3 GetVelocity(Vector3 lastPos, Vector3 curPos, float deltaTime)
	{
		float timeInverse = 1.0f / deltaTime;
		return timeInverse * (curPos - lastPos);
	}


	public static float CalculateDirection(Vector3 velocity, Quaternion rotation, Vector3 forward)
	{
		if (velocity == Vector3.zero) { return 0.0f; }

		float side = Vector3.Dot(Vector3.up, Vector3.Cross(forward, velocity));
		return Quaternion.Angle(Quaternion.FromToRotation(Vector3.forward, velocity), rotation) * Mathf.Sign(side);
	}
}
