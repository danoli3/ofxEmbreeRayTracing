#include "ofApp.h"

namespace {
// a quad (two triangles) a, b, c, d counter-clockwise
ofMesh quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
	ofMesh m;
	m.addVertices({ a, b, c, d });
	m.addIndices({ 0, 1, 2, 0, 2, 3 });
	return m;
}
// an axis-aligned box from size, turned around y, standing on the floor at (x, z)
glm::mat4 standing(float x, float z, float height, float degrees) {
	return glm::translate(glm::mat4(1.f), { x, height * 0.5f, z }) * glm::rotate(glm::mat4(1.f), glm::radians(degrees), { 0, 1, 0 });
}
}

void ofApp::setup() {
	ofSetFrameRate(60);
	const float S = 550.f;
	const ofFloatColor white(0.73f, 0.73f, 0.73f), red(0.65f, 0.05f, 0.05f), green(0.12f, 0.45f, 0.15f);
	auto wall = [&](ofMesh mesh, ofFloatColor color) { renderer.setMaterial(scene.addMesh(mesh), { color }); };
	wall(quad({ 0, 0, 0 }, { 0, 0, S }, { S, 0, S }, { S, 0, 0 }), white);   // floor
	wall(quad({ 0, S, 0 }, { S, S, 0 }, { S, S, S }, { 0, S, S }), white);   // ceiling
	wall(quad({ 0, 0, 0 }, { S, 0, 0 }, { S, S, 0 }, { 0, S, 0 }), white);   // back
	wall(quad({ 0, 0, 0 }, { 0, S, 0 }, { 0, S, S }, { 0, 0, S }), red);     // left
	wall(quad({ S, 0, 0 }, { S, 0, S }, { S, S, S }, { S, S, 0 }), green);   // right

	// the light: a glowing panel just under the ceiling, and the point light under it
	ofxEmbree::Material panel { ofFloatColor(1.f), 0.f, ofFloatColor(1.f, 0.95f, 0.85f) };
	renderer.setMaterial(scene.addMesh(quad({ 210, S - 1, 210 }, { 340, S - 1, 210 }, { 340, S - 1, 340 }, { 210, S - 1, 340 })), panel);
	renderer.settings.pointLight = true;
	renderer.settings.lightPosition = { S * 0.5f, S - 20.f, S * 0.5f };
	renderer.settings.lightColor = ofFloatColor(0.85f, 0.8f, 0.72f);
	renderer.settings.ambientColor = ofFloatColor(0.32f, 0.32f, 0.34f);
	renderer.settings.ambientOcclusionDistance = 160.f;
	renderer.settings.background = ofFloatColor(0.f);

	renderer.setMaterial(scene.addMesh(ofMesh::box(165, 330, 165, 1, 1, 1), standing(370, 190, 330, 18)), { white });
	renderer.setMaterial(scene.addMesh(ofMesh::box(165, 165, 165, 1, 1, 1), standing(190, 370, 165, -18)), { white });
	renderer.setMaterial(scene.addSphere({ 420, 80, 440 }, 80), { ofFloatColor(0.95f), 0.85f });
	scene.commit();

	cam.setNearClip(1);
	cam.setFarClip(5000);
	cam.setFov(38);
	cam.setTarget({ S * 0.5f, S * 0.5f, S * 0.5f });
	cam.setPosition(S * 0.5f, S * 0.5f, S * 0.5f + 1400.f);
	cam.lookAt({ S * 0.5f, S * 0.5f, S * 0.5f });
	ofLogNotice("cornellbox") << scene.getNumTriangles() << " triangles, " << scene.getNumGeometries() << " geometries";
}

void ofApp::update() {
	glm::mat4 now = cam.getGlobalTransformMatrix();
	if (now != lastCamera) {
		renderer.reset();
		lastCamera = now;
	}
	renderer.render(scene, cam, int(ofGetWidth() * scale), int(ofGetHeight() * scale));
	texture.loadData(renderer.getPixels());
}

void ofApp::draw() {
	ofBackground(0);
	ofSetColor(255);
	texture.draw(0, 0, ofGetWidth(), ofGetHeight());
	std::string info = ofToString(renderer.getNumPasses()) + " passes at " + ofToString(texture.getWidth(), 0) + "x" + ofToString(texture.getHeight(), 0)
		+ ", " + ofToString(renderer.getLastPassMillis(), 1) + " ms/pass, " + ofToString(renderer.getLastPassRaysPerSecond() / 1e6, 1) + " million rays/s\n"
		+ "drag: orbit (restarts the image)\n"
		+ "1 shadows " + std::string(renderer.settings.shadows ? "on" : "off")
		+ "  2 ambient occlusion " + std::string(renderer.settings.ambientOcclusionSamples > 0 ? "on" : "off")
		+ "  3 reflections " + std::string(renderer.settings.maxBounces > 0 ? "on" : "off")
		+ "\nf: full resolution " + std::string(scale == 1.f ? "on" : "off") + "  s: save cornellbox.png";
	ofDrawBitmapStringHighlight(info, 20, 30);
}

void ofApp::keyPressed(int key) {
	if (key == '1') renderer.settings.shadows = !renderer.settings.shadows;
	if (key == '2') renderer.settings.ambientOcclusionSamples = renderer.settings.ambientOcclusionSamples > 0 ? 0 : 2;
	if (key == '3') renderer.settings.maxBounces = renderer.settings.maxBounces > 0 ? 0 : 3;
	if (key == 'f') scale = scale == 1.f ? 0.5f : 1.f;
	if (key == 's') {
		ofPixels png = renderer.getPixels();
		ofSaveImage(png, "cornellbox.png");
	}
	if (key == '1' || key == '2' || key == '3' || key == 'f') renderer.reset();
}
