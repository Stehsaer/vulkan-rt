implementing model;

namespace model
{
	public enum Filter : uint32_t
	{
		Nearest = 0,
		Linear = 1,
	};

	public enum Wrap : uint32_t
	{
		Repeat = 0,
		MirroredRepeat = 1,
		ClampToEdge = 2,
	};

	// Sample mode for a texture
	public struct SampleMode
	{
		public Filter min_filter;
		public Filter mag_filter;
		public Filter mipmap_filter;

		public Wrap wrap_u;
		public Wrap wrap_v;

		public float max_mipmap_level;
	};

	// Material parameters for a material
	public struct MaterialParam
	{
		public float4 base_color_factor;
		public float3 emissive_factor;
		public float alpha_cutoff;
		public float metallic_factor;
		public float roughness_factor;
		public float2 normal_scale;
	};

	// Texture index containing indices into the texture array for each texture type
	public struct TextureIndex
	{
		public uint32_t albedo, emissive, orm, normal;
	};

	// Texture sample mode contains corresponding sample mode for each texture in `TextureIndex`
	public struct TextureSampleMode
	{
		public SampleMode albedo;
	};

	// Texture set for a materials, containing samplers for each texture
	public struct TextureSet
	{
		public Sampler2D albedo, emissive, orm, normal;
	};

	// Material info for a material, containing texture index and material parameters
	public struct MaterialInfo
	{
		public TextureIndex texture_index;     // Texture index for the material
		public TextureSampleMode sample_mode;  // Texture sample modes for the textures
		public MaterialParam param;            // Parameters for the material
	};

	// Material list, containing entire material system
	public struct MaterialList
	{
		public StructuredBuffer<MaterialInfo> info;
		public Sampler2D textures[];

		// Get material info for a primitive
		// - `primitive_attr`: Primitive attribute of the primitive
		[[ForceInline]]
		public func get_material(material_index: uint32_t)->MaterialInfo
		{
			let material_index =
				select(material_index == PrimitiveAttribute.DEFAULT_MATERIAL, 0, material_index + 1);

			return info[material_index];
		}

		// Get texture set for a material
		// - `index`: Texture index of the material
		[[ForceInline]]
		public func get_texture_set(index: TextureIndex)->TextureSet
		{
			TextureSet set;
			set.albedo = textures[index.albedo];
			set.emissive = textures[index.emissive];
			set.orm = textures[index.orm];
			set.normal = textures[index.normal];
			return set;
		}
	};
}


