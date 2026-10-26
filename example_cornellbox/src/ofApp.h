#pragma once

#include "ofMain.h"
#include "ofxEmbreeRayTracing.h"

// A Cornell box ray traced on the CPU with ofxEmbree::Renderer. Drag to orbit:
// the image restarts, then refines every frame.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	ofxEmbree::Scene scene;
	ofxEmbree::Renderer renderer;
	ofEasyCam cam;
	glm::mat4 lastCamera;
	ofTexture texture;
	float scale = 0.5f; // render at half the window size
};
