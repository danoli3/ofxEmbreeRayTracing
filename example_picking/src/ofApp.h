#pragma once

#include "ofMain.h"
#include "ofxEmbreeRayTracing.h"

// Mouse picking with Embree: move the mouse over the shapes.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	struct Shape {
		ofMesh mesh;
		ofNode node;
		ofColor color;
		unsigned id = 0;
	};
	std::vector<Shape> shapes;
	ofxEmbree::Scene scene;
	ofEasyCam cam;
	ofLight light;

	ofxEmbree::Hit hit;
	float pickMicros = 0.f;
	std::string batchResult;
};
