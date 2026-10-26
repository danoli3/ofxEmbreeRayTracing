#pragma once

#include "ofMain.h"
#include "ofxEmbreeRayTracing.h"
#include "ofxUnitTests.h"

// Headless tests: rays against shapes with known answers, then a small render.
class ofApp : public ofxUnitTestsApp {
	void run() override;
	void testSphere();
	void testMeshAndTransform();
	void testOcclusionAndRange();
	void testStripsAndFans();
	void testBatchAndRemove();
	void testPick();
	void testRenderer();
};
