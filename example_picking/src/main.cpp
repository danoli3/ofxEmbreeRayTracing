#include "ofApp.h"
#include "ofMain.h"

int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 800);
	ofCreateWindow(settings);
	ofRunApp(new ofApp());
}
