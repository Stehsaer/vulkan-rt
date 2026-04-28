#pragma once

#include <glm/glm.hpp>
#include <optional>

namespace scene::camera
{
	///
	/// @brief Concept that requires a type to have camera view interface
	///
	template <typename T>
	concept ViewType = requires(const T& view) {
		{ view.matrix() } -> std::convertible_to<glm::dmat4>;
		{ view.view_position() } -> std::convertible_to<glm::dvec3>;
	};

	///
	/// @brief Common view interface
	/// @note The actual implementations should not inherit from this interface, breaking the trivial
	/// copyability requirement. Use @p from to create an interface wrapper for the actual view
	/// implementations instead.
	///
	struct ViewInterface
	{
		virtual ~ViewInterface() = default;

		///
		/// @brief Get the view matrix of the camera
		///
		/// @return View matrix of the camera
		///
		[[nodiscard]]
		virtual glm::dmat4 matrix() const noexcept = 0;

		///
		/// @brief Get the view position of the camera, which is the position of the camera in world space.
		///
		/// @return View position of the camera in world space
		///
		[[nodiscard]]
		virtual glm::dvec3 view_position() const noexcept = 0;

		template <ViewType T>
		struct Impl;

		///
		/// @brief Create a `CameraViewInterface` wrapper for a given camera view that satisfies the
		/// `CameraViewType`
		///
		/// @tparam T Type of the camera view that satisfies the `CameraViewType` concept
		/// @param view Camera view
		/// @return `CameraViewInterface` wrapper for the given camera view
		///
		template <ViewType T>
		static Impl<T> from(const T& view) noexcept;
	};

	template <ViewType T>
	struct ViewInterface::Impl : public ViewInterface
	{
		T view;

		explicit Impl(T view) :
			view(std::move(view))
		{}

		[[nodiscard]]
		glm::dmat4 matrix() const noexcept override
		{
			return view.matrix();
		}

		[[nodiscard]]
		glm::dvec3 view_position() const noexcept override
		{
			return view.view_position();
		}

		Impl(const Impl&) = default;
		Impl(Impl&&) = default;
		Impl& operator=(const Impl&) = default;
		Impl& operator=(Impl&&) = default;
	};

	template <ViewType T>
	ViewInterface::Impl<T> ViewInterface::from(const T& view) noexcept
	{
		return Impl<T>(view);
	}

	///
	/// @brief Camera view defined by a center position, distance, pitch and yaw angles.
	/// @details
	/// - The camera is looking at the center position from a certain distance, and can be rotated around the
	/// center position by changing the pitch and yaw angles.
	/// - The up direction is fixed to be the positive Y axis
	///
	struct CenterView
	{
		glm::dvec3 center_position;  // Center position the camera is looking at
		double distance;             // Distance from the center position

		double pitch_degrees;  // Pitch angles, positive means looking from above
		double yaw_degrees;    // Yaw angles, positive means looking to the right

		///
		/// @brief Calculate view matrix
		///
		/// @return Calculated camera view matrix
		///
		[[nodiscard]]
		glm::dmat4 matrix() const noexcept;

		///
		/// @brief Get the view position of the camera, a.k.a the position of the camera in world space
		///
		/// @return View position of the camera in world space
		///
		[[nodiscard]]
		glm::dvec3 view_position() const noexcept;

		///
		/// @brief Generate a new `CenterView` by applying mouse rotation input to the current view
		/// @note The pitch angle is clamped inside `(-89.9 deg, 89.9 deg)` to prevent gimbal lock,
		/// while the yaw angle is wrapped around `0 deg` to `360 deg`
		///
		/// @param delta_uv Mouse movement delta in UV coordinates, where U points to the right and V points
		/// downwards
		/// @param rotation_rate Rotation rate in degree per unit UV coordinate
		/// @return New `CenterView` with applied mouse rotation input
		///
		[[nodiscard]]
		CenterView mouse_rotate(
			glm::dvec2 delta_uv,
			glm::dvec2 rotation_rate = glm::dvec2(180.0, 90.0)
		) const noexcept;

		///
		/// @brief Generate a new `CenterView` by applying mouse scroll input to the current view
		/// @details
		/// The distance is multiplied by `pow(2, -delta_scroll * scroll_rate)` to achieve a smooth zooming
		/// effect.
		///
		/// @note
		/// The distance is clamped inside `[1e-16, 1e16]` to prevent numerical issues and overflow. This
		/// should cover basically all practical usages.
		///
		/// @param delta_scroll Mouse scroll delta
		/// @param scroll_rate Scroll rate
		/// @return New `CenterView` with applied mouse scroll input
		///
		[[nodiscard]]
		CenterView mouse_scroll(double delta_scroll, double scroll_rate = 0.2) const noexcept;

		///
		/// @brief Generate a new `CenterView` by applying mouse pan input to the current view
		///
		/// @param delta_uv Mouse movement delta in UV coordinates, where U points to the right and V points
		/// downwards
		/// @param aspect_ratio Aspect ratio of the viewport, which is `width / height`
		/// @param pan_rate Pan rate
		/// @return New `CenterView` with applied mouse pan input
		///
		[[nodiscard]]
		CenterView mouse_pan(glm::dvec2 delta_uv, double aspect_ratio, double pan_rate = 1.0) const noexcept;

		///
		/// @brief Mix two `CenterView`s by interpolating their parameters with a given factor `a`
		///
		/// @param x First `CenterView` to mix
		/// @param y Second `CenterView` to mix
		/// @param a Interpolation factor, preferably in `[0, 1]`
		/// @return Interpolated `CenterView`
		///
		[[nodiscard]]
		static CenterView mix(const CenterView& x, const CenterView& y, double a) noexcept;
	};

	///
	/// @brief Lookat camera view defined by a camera position, a look position and an up direction.
	///
	///
	struct LookatView
	{
		glm::dvec3 position;       // Camera position in world space
		glm::dvec3 look_position;  // Point in world space the camera is looking at
		glm::dvec3 up_direction;   // Up direction for the camera

		///
		/// @brief Calculate view matrix
		///
		/// @return Calculated camera view matrix
		///
		[[nodiscard]]
		glm::dmat4 matrix() const noexcept;

		///
		/// @brief Camera position in world space, which is the same as the view position for lookat camera
		///
		/// @return View position of the camera in world space
		///
		[[nodiscard]]
		glm::dvec3 view_position() const noexcept;
	};

	///
	/// @brief Simple perspective projection defined by a field of view, near plane and optional far plane.
	/// @note Combined with view components such as `CenterView` and `LookatView`
	///
	struct PerspectiveProjection
	{
		double fov_degrees;         // Field of view in degrees
		double near;                // Near clipping plane distance
		std::optional<double> far;  // Far clipping plane distance if set, infinite far if empty

		///
		/// @brief Calculate projection matrix for the given aspect ratio
		///
		/// @param aspect_ratio Aspect ratio of the viewport, which is `width / height`
		/// @return Calculated camera projection matrix
		///
		[[nodiscard]]
		glm::dmat4 matrix(double aspect_ratio) const noexcept;
	};

	///
	/// @brief Get the reverse-Z projection matrix if `reverse` is true, otherwise a unit matrix
	///
	/// @param reverse True to get the reverse-Z projection matrix, false to get a unit matrix
	/// @return Reverse-Z projection matrix if `reverse` is true, otherwise a unit matrix
	///
	[[nodiscard]]
	glm::dmat4 reverse_z(bool reverse = true) noexcept;
}
