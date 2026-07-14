module shadow;

/*===== COMMON CONSTANT =====*/

public static const let VARIANCE_STORAGE_RATIO = 4.0;

/*===== UTILITY =====*/

public func pow128(x: float)->float
{
	let x2 = x * x;
	let x4 = x2 * x2;
	let x8 = x4 * x4;
	let x16 = x8 * x8;
	let x32 = x16 * x16;
	let x64 = x32 * x32;
	return x64 * x64;
}


