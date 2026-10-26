#pragma once

// ofxEmbree::Scene: meshes and spheres in an Embree scene, with fast ray queries
// (nearest hit, occlusion, mouse picking) on Intel Embree 4.

#include "ofCamera.h"
#include "ofMesh.h"
#include "ofRectangle.h"

#include <embree4/rtcore.h>

#include <limits>
#include <map>
#include <memory>
#include <vector>

namespace ofxEmbree {

/// A ray: origin, direction (doesn't need to be normalized; distances are then
/// in units of its length) and the range [near, far] to search.
struct Ray {
	glm::vec3 origin { 0.f };
	glm::vec3 direction { 0.f, 0.f, -1.f };
	float near = 0.f;
	float far = std::numeric_limits<float>::infinity();
};

/// The nearest intersection along a ray.
struct Hit {
	bool hit = false;
	/// Along the ray, in units of the direction's length.
	float distance = 0.f;
	glm::vec3 position { 0.f };
	/// Geometric normal, normalized (not flipped towards the ray).
	glm::vec3 normal { 0.f };
	/// Barycentric coordinates for triangles, (u, v) for spheres.
	glm::vec2 uv { 0.f };
	/// The id addMesh() / addSphere() returned, and the triangle index.
	unsigned geometry = RTC_INVALID_GEOMETRY_ID;
	unsigned primitive = RTC_INVALID_GEOMETRY_ID;

	explicit operator bool() const { return hit; }
};

class Scene {
public:
	Scene();
	~Scene();
	Scene(const Scene &) = delete;
	Scene & operator=(const Scene &) = delete;

	/// Adds a mesh's triangles (OF_PRIMITIVE_TRIANGLES, _TRIANGLE_STRIP or
	/// _TRIANGLE_FAN, indexed or not), transformed. Returns its geometry id.
	unsigned addMesh(const ofMesh & mesh, const glm::mat4 & transform = glm::mat4(1.f));
	/// Adds an analytic sphere. Returns its geometry id.
	unsigned addSphere(const glm::vec3 & center, float radius);
	/// Removes one geometry (its id isn't reused).
	void remove(unsigned geometry);
	/// Removes everything.
	void clear();

	/// Builds the acceleration structure. Queries do this when the scene has
	/// changed, so calling it is optional (e.g. to do it outside a timed loop).
	void commit() const;

	/// Nearest hit along the ray.
	Hit intersect(const Ray & ray) const;
	Hit intersect(const glm::vec3 & origin, const glm::vec3 & direction) const;
	/// Is anything between near and far (shadow rays)? Faster than intersect().
	bool occluded(const Ray & ray) const;
	/// Many rays, on all cores. hits gets one result per ray.
	void intersect(const std::vector<Ray> & rays, std::vector<Hit> & hits) const;

	/// The ray through a screen position (pixels) for a camera.
	static Ray rayFromScreen(const ofCamera & camera, const glm::vec2 & screen, const ofRectangle & viewport);
	/// What's under a screen position, e.g. the mouse.
	Hit pick(const ofCamera & camera, const glm::vec2 & screen) const;
	Hit pick(const ofCamera & camera, const glm::vec2 & screen, const ofRectangle & viewport) const;

	/// Bounds of everything in the scene (min, max).
	std::pair<glm::vec3, glm::vec3> getBounds() const;
	size_t getNumGeometries() const { return numGeometries; }
	size_t getNumTriangles() const { return numTriangles; }

	/// The Embree scene and device, for the rest of the Embree API.
	RTCScene getRTCScene() const { return scene; }
	static RTCDevice getDevice();

private:
	std::shared_ptr<RTCDeviceTy> device;
	RTCScene scene = nullptr;
	mutable bool dirty = true;
	size_t numGeometries = 0;
	size_t numTriangles = 0;
	std::map<unsigned, size_t> trianglesPerGeometry;

	unsigned attach(RTCGeometry geometry);
	Hit toHit(const RTCRayHit & rayhit) const;
};

} // namespace ofxEmbree
