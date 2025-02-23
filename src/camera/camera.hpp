#ifndef __fps_camera_
#define __fps_camera_

#include "pch.hpp"

namespace mr {
  struct ShaderCameraData {
    mr::Matr4f vp;
    mr::Vec4f campos;
    float fov;
    float gamma;
    float speed;
    float sens;
  };

  class FPSCamera {
  public:
    /**
 * @brief Default constructor for FPSCamera.
 *
 * Constructs an FPSCamera instance with preset default parameters suitable for a first-person shooter camera.
 * The defaults include a field of view of 90 degrees, gamma correction of 2.2, a movement speed of 0.01, and a sensitivity of 1.
 */
FPSCamera() = default;
    /**
 * @brief Move constructs an FPSCamera instance.
 *
 * Transfers ownership of the internal camera state from another FPSCamera,
 * leaving the moved-from instance in a valid but unspecified state.
 * This constructor is defaulted.
 */
FPSCamera(FPSCamera &&) = default;
    /**
 * @brief Move-assigns another FPSCamera, transferring its state.
 *
 * Moves the internal state from the source FPSCamera into this instance, leaving
 * the source in a valid, unspecified state.
 *
 * @return A reference to this FPSCamera instance.
 */
FPSCamera & operator=(FPSCamera &&) = default;

    /**
     * @brief Rotates the camera.
     *
     * Adjusts the FPS camera's orientation by applying yaw, pitch, and roll rotations based on the provided delta,
     * which is scaled by the current sensitivity. The x and y components of delta modify the yaw and pitch, respectively,
     * while the z component refines the roll after compensating for the camera’s alignment with the world's y-axis.
     *
     * @param delta Rotation increments for yaw, pitch, and roll.
     * @return Reference to the updated FPSCamera instance.
     */
    FPSCamera & turn(mr::Vec3f delta) noexcept {
      delta *= _sensetivity;
      _cam += mr::Yaw(mr::Radiansf(delta.x()));
      _cam += mr::Pitch(mr::Radiansf(delta.y()));
      _cam += mr::Roll(mr::Radiansf(std::acos(_cam.right() & mr::axis::y)) - mr::pi / 2 + mr::Radiansf(delta.z()));
      return *this;
    }

    /**
     * @brief Moves the camera's position.
     *
     * Scales the provided delta vector by the camera's movement speed and adds it to the current position.
     * This operation enables smooth translational movement for the FPS camera and supports method chaining.
     *
     * @param delta The 3D vector specifying the desired movement direction and magnitude.
     * @return FPSCamera& A reference to the updated camera instance.
     */
    FPSCamera & move(mr::Vec3f delta) noexcept {
      _cam += delta * _speed;
      return *this;
    }

    /**
     * @brief Computes and returns the camera's view-projection matrix.
     *
     * Combines the camera's perspective and frustum matrices to produce a matrix that
     * transforms coordinates from world space to clip space.
     *
     * @return mr::Matr4f The resulting view-projection matrix.
     */
    mr::Matr4f viewproj() const noexcept {
      return _cam.perspective() * _cam.frustum();
    }

    /**
 * @brief Retrieves a mutable reference to the internal camera object.
 *
 * Provides direct access to the underlying camera used for view and projection computations.
 * Modifications to the returned camera will directly affect the FPS camera's behavior.
 *
 * @return mr::Camera<float>& A reference to the internal camera.
 */
    mr::Camera<float> & cam() noexcept { return _cam; }
    /**
 * @brief Retrieves the camera's field of view.
 *
 * Returns the current field of view (FOV) angle, expressed in degrees,
 * used for the camera's perspective.
 *
 * @return The camera's field of view.
 */
constexpr mr::Degreesf fov() const noexcept { return _fov; }
    /**
 * @brief Retrieves the camera's gamma correction value.
 *
 * This function returns the current gamma value used by the camera system,
 * which affects brightness and contrast in rendering.
 *
 * @return The gamma correction value.
 */
constexpr float gamma() const noexcept { return _gamma; }
    /**
 * @brief Gets the current movement speed of the FPS camera.
 *
 * Returns the movement speed value that controls how fast the camera moves within the scene.
 *
 * @return float The current movement speed.
 */
constexpr float speed() const noexcept { return _speed; }
    /**
 * @brief Retrieves the camera's sensitivity factor.
 *
 * Returns the sensitivity value that scales input for camera control adjustments.
 *
 * @return The current sensitivity.
 */
constexpr float sensetivity() const noexcept { return _sensetivity; }

    /**
 * @brief Sets the camera's field of view.
 *
 * Updates the camera's field of view angle using the provided value in degrees.
 *
 * @param fov New field of view angle.
 */
    constexpr void fov(mr::Degreesf fov) noexcept { _fov = fov; }
    /**
 * @brief Sets the gamma correction value for the camera.
 *
 * Updates the camera's gamma setting, which is used to adjust color correction.
 *
 * @param gamma The new gamma correction value.
 */
constexpr void gamma(float gamma) noexcept { _gamma = gamma; }
    /**
 * @brief Sets the camera's movement speed.
 *
 * Updates the internal movement speed value used for controlling the camera's motion.
 *
 * @param speed The new movement speed.
 */
constexpr void speed(float speed) noexcept { _speed = speed; }
    /**
 * @brief Updates the camera's sensitivity factor.
 *
 * Sets the internal sensitivity, which scales the turn input deltas for camera orientation adjustments.
 *
 * @param sens The new sensitivity value.
 */
constexpr void sensetivity(float sens) noexcept { _sensetivity = sens; }

  private:
    mr::Camera<float> _cam;
    mr::Degreesf _fov = 90_deg;
    float _gamma = 2.2;
    float _speed = .01;
    float _sensetivity = 1;
  };
}

#endif // __fps_camera_
