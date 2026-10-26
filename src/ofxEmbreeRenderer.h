#pragma once

// ofxEmbree::Renderer: a progressive CPU ray tracer for an ofxEmbree::Scene.
// Directional light with shadows, ambient occlusion, reflections, emission,
// anti-aliasing; every pass refines the image, on all cores.

#include "ofxEmbreeScene.h"

#include "ofColor.h"
#include "ofPixels.h"

#include <map>

namespace ofxEmbree {

struct Material {
	ofFloatColor color { 0.8f, 0.8f, 0.8f };
	/// 0: matte, 1: mirror.
	float reflection = 0.f;
	/// Light the surface gives off, added to the result.
	ofFloatColor emission { 0.f, 0.f, 0.f };
};

class Renderer {
public:
	struct Settings {
		/// Direction the light travels (from the light into the scene).
		glm::vec3 lightDirection { -0.4f, -1.f, -0.6f };
		/// Or a point light (e.g. inside a room) at lightPosition, no falloff.
		bool pointLight = false;
		glm::vec3 lightPosition { 0.f };
		ofFloatColor lightColor { 1.f, 1.f, 1.f };
		ofFloatColor ambientColor { 0.35f, 0.38f, 0.45f };
		ofFloatColor background { 0.06f, 0.07f, 0.09f };
		bool shadows = true;
		/// Ambient occlusion rays per pixel per pass (0: off), and how far they look.
		int ambientOcclusionSamples = 2;
		float ambientOcclusionDistance = 100.f;
		/// Mirror bounces.
		int maxBounces = 3;
		/// 0: all cores.
		int threads = 0;
	};
	Settings settings;

	/// Material for a geometry id from Scene::addMesh() / addSphere().
	void setMaterial(unsigned geometry, const Material & material);
	const Material & getMaterial(unsigned geometry) const;
	/// Material for geometries without one.
	Material defaultMaterial;

	/// Renders one pass from the camera at width x height and adds it to the
	/// image. Call every frame; reset() when the camera or scene changes.
	void render(const Scene & scene, const ofCamera & camera, int width, int height);
	/// Starts the image again (also done when the size changes).
	void reset();

	/// The image so far (sRGB, averaged over getNumPasses() passes).
	const ofFloatPixels & getPixels() const { return image; }
	int getNumPasses() const { return passes; }
	/// Milliseconds the last pass took, and its rays per second.
	float getLastPassMillis() const { return lastPassMillis; }
	double getLastPassRaysPerSecond() const { return lastPassRaysPerSecond; }

private:
	std::map<unsigned, Material> materials;
	std::vector<glm::vec3> accumulation;
	ofFloatPixels image;
	int passes = 0;
	float lastPassMillis = 0.f;
	double lastPassRaysPerSecond = 0.0;
};

} // namespace ofxEmbree
