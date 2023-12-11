using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;



namespace CVSP
{
	public static class RPCTarget
	{
		public const byte UNDEFINED = 0;
		public const byte AllClients = 1;
		public const byte AllClientsExceptMe = 2;
		public const byte Server = 3;
	}


	public class RpcInfo : ISerializable
	{
		public uint actorGuid = 0;
		public string functionName = "";
		
		public void Serialize(CVSPv2Serializer ser) => ser.Push(actorGuid).Push(functionName);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out actorGuid).Get(out functionName);
	}
}
