#include "ofApp.h"

using namespace ofxEmbree;

namespace {
bool near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) < eps; }
bool near(const glm::vec3 & a, const glm::vec3 & b, float eps = 1e-3f) { return glm::length(a - b) < eps; }
}

void ofApp::run() {
	testSphere();
	testMeshAndTransform();
	testOcclusionAndRange();
	testStripsAndFans();
	testBatchAndRemove();
	testPick();
	testRenderer();
}

void ofApp::testSphere() {
	Scene scene;
	ofxTest(Scene::getDevice() != nullptr, "Embree device");
	unsigned id = scene.addSphere({ 0, 0, 0 }, 1.f);
	ofxTest(id != RTC_INVALID_GEOMETRY_ID, "addSphere");
	Hit hit = scene.intersect({ 0, 0, 5 }, { 0, 0, -1 });
	ofxTest(bool(hit), "ray hits the sphere");
	ofxTest(near(hit.distance, 4.f), "distance 4, got " + ofToString(hit.distance));
	ofxTest(near(hit.position, { 0, 0, 1 }), "position (0, 0, 1)");
	ofxTest(near(hit.normal, { 0, 0, 1 }), "normal (0, 0, 1)");
	ofxTestEq(hit.geometry, id, "geometry id");
	ofxTest(!scene.intersect({ 0, 3, 5 }, { 0, 0, -1 }), "ray beside the sphere misses");
	ofxTest(!scene.intersect({ 0, 0, 5 }, { 0, 0, 1 }), "ray pointing away misses");
}

void ofApp::testMeshAndTransform() {
	Scene scene;
	ofMesh box = ofMesh::box(2, 2, 2, 1, 1, 1); // -1..1
	unsigned id = scene.addMesh(box, glm::translate(glm::mat4(1.f), glm::vec3(10, 0, 0)));
	ofxTest(id != RTC_INVALID_GEOMETRY_ID, "addMesh");
	ofxTestEq(scene.getNumTriangles(), size_t(12), "box: 12 triangles");
	Hit hit = scene.intersect({ 0, 0, 0 }, { 1, 0, 0 });
	ofxTest(bool(hit), "ray hits the transformed box");
	ofxTest(near(hit.distance, 9.f), "distance 9 (box moved to x = 10), got " + ofToString(hit.distance));
	ofxTest(near(std::abs(hit.normal.x), 1.f), "normal along x");
	ofxTest(!scene.intersect({ 0, 0, 0 }, { -1, 0, 0 }), "nothing at -x");
	auto bounds = scene.getBounds();
	ofxTest(near(bounds.first, { 9, -1, -1 }) && near(bounds.second, { 11, 1, 1 }), "bounds (9,-1,-1) .. (11,1,1)");
}

void ofApp::testOcclusionAndRange() {
	Scene scene;
	scene.addSphere({ 0, 0, 0 }, 1.f);
	Ray ray;
	ray.origin = { 0, 0, 5 };
	ray.direction = { 0, 0, -1 };
	ofxTest(scene.occluded(ray), "occluded");
	ray.far = 3.f;
	ofxTest(!scene.occluded(ray), "not occluded when far ends before the sphere");
	ofxTest(!scene.intersect(ray), "intersect respects far");
	ray.far = std::numeric_limits<float>::infinity();
	ray.near = 4.5f;
	Hit back = scene.intersect(ray);
	ofxTest(bool(back) && near(back.distance, 6.f), "near skips the front: back face at 6, got " + ofToString(back.distance));
}

void ofApp::testStripsAndFans() {
	ofMesh strip;
	strip.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	strip.addVertices({ { 0, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 1, 1, 0 } }); // a unit quad
	ofMesh fan;
	fan.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
	fan.addVertices({ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } });
	for (auto * mesh : { &strip, &fan }) {
		Scene scene;
		scene.addMesh(*mesh);
		std::string name = mesh == &strip ? "strip" : "fan";
		ofxTestEq(scene.getNumTriangles(), size_t(2), name + ": 2 triangles");
		ofxTest(bool(scene.intersect({ 0.25f, 0.75f, 1 }, { 0, 0, -1 })) && bool(scene.intersect({ 0.75f, 0.25f, 1 }, { 0, 0, -1 })),
			name + ": both halves of the quad hit");
		ofxTest(!scene.intersect({ 1.5f, 0.5f, 1 }, { 0, 0, -1 }), name + ": outside misses");
	}
}

void ofApp::testBatchAndRemove() {
	Scene scene;
	unsigned plane = scene.addMesh(ofMesh::plane(100, 100, 2, 2)); // z = 0, facing +z
	unsigned sphere = scene.addSphere({ 0, 0, 2 }, 1.f);
	std::vector<Ray> rays;
	for (int y = -20; y <= 20; y++) {
		for (int x = -20; x <= 20; x++) {
			Ray r;
			r.origin = { x * 0.25f, y * 0.25f, 10 };
			r.direction = { 0, 0, -1 };
			rays.push_back(r);
		}
	}
	std::vector<Hit> hits;
	uint64_t t0 = ofGetElapsedTimeMicros();
	scene.intersect(rays, hits);
	uint64_t t1 = ofGetElapsedTimeMicros();
	bool same = hits.size() == rays.size();
	int onSphere = 0;
	for (size_t i = 0; same && i < rays.size(); i++) {
		Hit one = scene.intersect(rays[i]);
		same = one.hit == hits[i].hit && one.geometry == hits[i].geometry && near(one.distance, hits[i].distance);
		if (hits[i].geometry == sphere) onSphere++;
	}
	ofxTest(same, "batch of " + ofToString(rays.size()) + " rays matches single queries (" + ofToString(t1 - t0) + " us)");
	ofxTest(onSphere > 0 && onSphere < int(rays.size()), "batch: some rays hit the sphere, the rest the plane");
	scene.remove(sphere);
	Hit hit = scene.intersect({ 0, 0, 10 }, { 0, 0, -1 });
	ofxTest(bool(hit) && hit.geometry == plane && near(hit.distance, 10.f), "after remove(sphere) the ray reaches the plane");
	scene.clear();
	ofxTest(!scene.intersect({ 0, 0, 10 }, { 0, 0, -1 }) && scene.getNumGeometries() == 0, "clear()");
}

void ofApp::testPick() {
	Scene scene;
	unsigned target = scene.addSphere({ 0, 0, 0 }, 50.f);
	ofCamera camera;
	camera.setPosition(0, 0, 500);
	camera.lookAt({ 0, 0, 0 });
	ofRectangle viewport(0, 0, 640, 480);
	Hit center = scene.pick(camera, viewport.getCenter(), viewport);
	ofxTest(bool(center) && center.geometry == target, "pick: the centre pixel hits the sphere");
	ofxTest(near(center.position, { 0, 0, 50 }, 0.5f), "pick: the front of the sphere, got " + ofToString(center.position));
	ofxTest(!scene.pick(camera, { 10, 10 }, viewport), "pick: a corner pixel misses");
	glm::vec3 screen = camera.worldToScreen({ 30, 20, 0 }, viewport);
	Ray ray = Scene::rayFromScreen(camera, { screen.x, screen.y }, viewport);
	glm::vec3 toPoint = glm::normalize(glm::vec3(30, 20, 0) - ray.origin);
	ofxTest(glm::dot(toPoint, ray.direction) > 0.9999f, "rayFromScreen goes through worldToScreen's point");
}

void ofApp::testRenderer() {
	Scene scene;
	unsigned floor = scene.addMesh(ofMesh::plane(400, 400, 2, 2), glm::rotate(glm::mat4(1.f), -glm::half_pi<float>(), glm::vec3(1, 0, 0)));
	unsigned ball = scene.addSphere({ 0, 50, 0 }, 50.f);
	Renderer renderer;
	renderer.settings.lightDirection = { 1, -1, 0 }; // 45 degrees: the shadow falls towards +x
	renderer.setMaterial(floor, { ofFloatColor(0.8f), 0.f });
	renderer.setMaterial(ball, { ofFloatColor(0.9f, 0.2f, 0.2f), 0.f });
	ofCamera camera;
	camera.setPosition(0, 300, 300);
	camera.lookAt({ 0, 0, 0 });
	for (int i = 0; i < 4; i++) renderer.render(scene, camera, 160, 120);
	const ofFloatPixels & pixels = renderer.getPixels();
	ofxTestEq(renderer.getNumPasses(), 4, "4 passes");
	ofxTestEq(int(pixels.getWidth()), 160, "image width");
	ofRectangle viewport(0, 0, 160, 120);
	auto at = [&](glm::vec3 world) {
		glm::vec3 s = camera.worldToScreen(world, viewport);
		return pixels.getColor(ofClamp(s.x, 0, 159), ofClamp(s.y, 0, 119));
	};
	// (70, 0, 0) is in the ball's shadow and visible from the camera; (-150, 0, 0) is lit
	ofFloatColor ballColor = at({ 0, 50, 50 }), shadow = at({ 70, 0, 0 }), lit = at({ -150, 0, 0 });
	ofxTest(ballColor.r > ballColor.g + 0.2f, "the ball is red: " + ofToString(ballColor));
	ofxTest(lit.getBrightness() > shadow.getBrightness() + 0.1f,
		"the ball's shadow is darker than the lit floor (" + ofToString(shadow.getBrightness(), 2) + " < " + ofToString(lit.getBrightness(), 2) + ")");
	ofxTest(renderer.getLastPassRaysPerSecond() > 0, ofToString(renderer.getLastPassRaysPerSecond() / 1e6, 2) + " million rays/s");
	ofPixels png = pixels; // 8 bit
	ofSaveImage(png, "render.png");
}
