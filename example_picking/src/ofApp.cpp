#include "ofApp.h"

void ofApp::setup() {
	ofSetFrameRate(60);
	ofEnableDepthTest();
	ofSetSphereResolution(48);

	auto add = [&](ofMesh mesh, glm::vec3 position, glm::vec3 rotation, ofColor color) {
		Shape s;
		s.mesh = std::move(mesh);
		s.node.setPosition(position);
		s.node.setOrientation(rotation);
		s.color = color;
		// the same transform for drawing and for the Embree scene
		s.id = scene.addMesh(s.mesh, s.node.getGlobalTransformMatrix());
		shapes.push_back(std::move(s));
	};
	add(ofMesh::plane(1200, 1200, 2, 2), { 0, -100, 0 }, { -90, 0, 0 }, ofColor(90, 95, 110));
	add(ofBoxPrimitive(140, 140, 140).getMesh(), { -320, -30, 0 }, { 0, 30, 0 }, ofColor(230, 90, 70));
	add(ofSpherePrimitive(80, 48).getMesh(), { -100, -20, 120 }, { 0, 0, 0 }, ofColor(90, 170, 230));
	add(ofIcoSpherePrimitive(85, 2).getMesh(), { 120, -15, -60 }, { 20, 40, 0 }, ofColor(240, 200, 80));
	add(ofCylinderPrimitive(60, 180, 48, 1).getMesh(), { 330, -10, 80 }, { 0, 0, 20 }, ofColor(120, 210, 140));
	add(ofConePrimitive(80, 170, 48, 1).getMesh(), { 40, -15, 300 }, { 180, 0, 0 }, ofColor(200, 120, 230));
	scene.commit();

	light.setPosition(400, 600, 500);
	cam.setDistance(900);
	cam.setPosition(0, 350, 900);
	cam.lookAt({ 0, 0, 0 });
}

void ofApp::update() {
	uint64_t start = ofGetElapsedTimeMicros();
	hit = scene.pick(cam, { ofGetMouseX(), ofGetMouseY() });
	pickMicros = float(ofGetElapsedTimeMicros() - start);
}

void ofApp::draw() {
	ofBackgroundGradient(ofColor(40, 44, 52), ofColor(14, 15, 18));
	cam.begin();
	ofEnableLighting();
	light.enable();
	for (auto & s : shapes) {
		ofSetColor(hit && hit.geometry == s.id ? ofColor(255, 230, 60) : s.color);
		s.node.transformGL();
		s.mesh.draw();
		s.node.restoreTransformGL();
	}
	ofDisableLighting();
	if (hit) {
		ofSetColor(255);
		ofDrawSphere(hit.position, 6);
		ofSetColor(255, 60, 60);
		ofSetLineWidth(3);
		ofDrawLine(hit.position, hit.position + hit.normal * 60.f);
	}
	cam.end();

	ofDisableDepthTest();
	std::string info = "Embree picking: " + ofToString(scene.getNumTriangles()) + " triangles, pick in " + ofToString(pickMicros, 1) + " us\n";
	if (hit) {
		info += "hit geometry " + ofToString(hit.geometry) + ", triangle " + ofToString(hit.primitive)
			+ "\ndistance " + ofToString(hit.distance, 1) + "  position " + ofToString(hit.position, 1)
			+ "\nnormal " + ofToString(hit.normal, 2);
	} else {
		info += "nothing under the mouse";
	}
	info += "\n\nspace: cast 1,000,000 random rays (batch)" + (batchResult.empty() ? std::string() : "\n" + batchResult);
	ofDrawBitmapStringHighlight(info, 20, 30);
	ofEnableDepthTest();
}

void ofApp::keyPressed(int key) {
	if (key == ' ') {
		// rays from random points above the scene in random downward directions
		std::vector<ofxEmbree::Ray> rays(1000000);
		for (auto & r : rays) {
			r.origin = { ofRandom(-600, 600), 400, ofRandom(-600, 600) };
			r.direction = glm::normalize(glm::vec3(ofRandomf() * 0.5f, -1.f, ofRandomf() * 0.5f));
		}
		std::vector<ofxEmbree::Hit> hits;
		uint64_t start = ofGetElapsedTimeMicros();
		scene.intersect(rays, hits);
		double seconds = (ofGetElapsedTimeMicros() - start) / 1e6;
		size_t count = std::count_if(hits.begin(), hits.end(), [](const ofxEmbree::Hit & h) { return h.hit; });
		batchResult = ofToString(rays.size()) + " rays in " + ofToString(seconds * 1000, 1) + " ms ("
			+ ofToString(rays.size() / seconds / 1e6, 1) + " million rays/s), " + ofToString(count) + " hits";
	}
}
