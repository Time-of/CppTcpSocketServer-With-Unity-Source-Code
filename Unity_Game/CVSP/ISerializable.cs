using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CVSP
{
	public interface ISerializable
	{
		public void Serialize(CVSPv2Serializer ser);
		public void Deserialize(CVSPv2Serializer ser);
	}
}
