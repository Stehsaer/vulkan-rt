module compute;

namespace compute
{
	// Provides name aliases for shader variables
	// (Because Microsoft's namings are insane)
	public struct ShaderVar
	{
		// 3D coordinate of current thread in the current group
		public uint3 local_thread_coord : SV_GroupThreadID;

		// Flattened index of current thread in current group
		public uint local_thread_index : SV_GroupIndex;

		// 3D coordinate of current thread in the whole dispatch
		public uint3 global_thread_coord : SV_DispatchThreadID;

		// 3D coordinate of current group in the whole dispatch
		public uint3 global_group_coord : SV_GroupID;
	};
}


