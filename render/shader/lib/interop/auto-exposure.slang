module auto_exposure;

// Exposure parameters for auto-exposure calculation
public struct ExposureParam
{
	public float min_log_luminance;  // Lower bound of the smallest bin
	public float max_log_luminance;  // Upper bound of the largest bin
	public float delta_time;         // Delta-time of the current frame
	public float exposure_mult;      // Multiplier for the exposure value
	public float adaptation_rate;    // Rate at which the exposure adapts to changes in scene brightness
	public uint2 image_size;         // Size of the input image for which the histogram was computed
};


