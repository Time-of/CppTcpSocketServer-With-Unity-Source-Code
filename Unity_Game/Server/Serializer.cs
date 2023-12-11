using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;


/// <summary>
/// <author>이성수(Time-of)</author>
/// 직렬화/역직렬화 기능 모음 클래스입니다.
/// </summary>
public static class Serializer
{

	#region 문자열 직렬화
	public static byte[] SerializeEurKrString(string str)
	{
		return GetEucKrEncoding().GetBytes(str);
	}


	public static string DeserializeEucKrString(byte[] data, int recvLength)
	{
		return GetEucKrEncoding().GetString(data, 0, recvLength);
	}


	public static byte[] SerializeASCIIString(string str)
	{
		return Encoding.ASCII.GetBytes(str);
	}


	public static string DeserializeASCIIString(byte[] data, int recvLength)
	{
		return Encoding.ASCII.GetString(data, 0, recvLength);
	}


	private static Encoding GetEucKrEncoding()
	{
		const int eucKrCodepage = 51949;
		return System.Text.Encoding.GetEncoding(eucKrCodepage);
	}
	#endregion


	#region 구조체 직렬화
	/// <summary>
	/// 구조체를 바이트로 변경.
	/// 주의! C++ 서버에 완전히 동일한 형태의 구조체가 있어야 사용 가능.
	/// </summary>
	/// <param name="obj">직렬화할 구조체</param>
	/// <returns>직렬화 결과 byte 배열</returns>
	public static unsafe byte[] StructureToByte(object obj)
	{
		int dataSize = Marshal.SizeOf(obj);

		IntPtr buffer = Marshal.AllocHGlobal(dataSize);
		Marshal.StructureToPtr(obj, buffer, false);
		byte[] data = new byte[dataSize];
		Marshal.Copy(buffer, data, 0, dataSize);
		Marshal.FreeHGlobal(buffer);

		return data;
	}


	public static unsafe object ByteToStructure(byte[] data, Type type)
	{
		if (data == null)
		{
			Debug.LogWarning("Serializer - ByteToStructure: data가 유효하지 않음!!!");
			return null;
		}

		// 언매니지드 영역에 malloc
		IntPtr buffer = Marshal.AllocHGlobal(data.Length);
		Marshal.Copy(data, 0, buffer, data.Length);
		object obj = Marshal.PtrToStructure(buffer, type);
		Marshal.FreeHGlobal(buffer);

		return (Marshal.SizeOf(obj) == data.Length) ? obj : null;
	}
	#endregion
}
