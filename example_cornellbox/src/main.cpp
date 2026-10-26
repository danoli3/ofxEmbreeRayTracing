#include "ofApp.h"
#include "ofMain.h"

int main() {
	ofGLWindowSettings settings;
	settings.setSize(1024, 1024);
	ofCreateWindow(settings);
	ofRunApp(new ofApp());
}
