#include "ofxEmbreeScene.h"

#include "ofAppRunner.h"
#include "ofGraphics.h"
#include "ofLog.h"

#include <algorithm>
#include <thread>

namespace ofxEmbree {

namespace {

void onError(void *, RTCError code, const char * message) {
	ofLogError("ofxEmbree") << "Embree error " << int(code) << ": " << (message ? message : "");
}

// One device for every scene (Embree builds its thread pool per device).
std::shared_ptr<RTCDeviceTy> sharedDevice() {
	static std::weak_ptr<RTCDeviceTy> weak;
	auto device = weak.lock();
	if (!device) {
		RTCDevice d = rtcNewDevice(nullptr);
		if (!d) {
			ofLogError("ofxEmbree") << "couldn't create the Embree device: " << int(rtcGetDeviceError(nullptr));
			return nullptr;
		}
		rtcSetDeviceErrorFunction(d, onError, nullptr);
		device = std::shared_ptr<RTCDeviceTy>(d, [](RTCDevice d) { rtcReleaseDevice(d); });
		weak = device;
	}
	return device;
}

// mesh -> triangle list indices (strips and fans unrolled)
std::vector<unsigned> triangleIndices(const ofMesh & mesh) {
	std::vector<unsigned> indices;
	if (mesh.hasIndices()) {
		indices.assign(mesh.getIndices().begin(), mesh.getIndices().end());
	} else {
		indices.resize(mesh.getNumVertices());
		for (unsigned i = 0; i < indices.size(); i++) indices[i] = i;
	}
	std::vector<unsigned> triangles;
	switch (mesh.getMode()) {
	case OF_PRIMITIVE_TRIANGLES:
		triangles.assign(indices.begin(), indices.begin() + indices.size() / 3 * 3);
		break;
	case OF_PRIMITIVE_TRIANGLE_STRIP:
		for (size_t i = 2; i < indices.size(); i++) {
			if (i % 2 == 0) triangles.insert(triangles.end(), { indices[i - 2], indices[i - 1], indices[i] });
			else triangles.insert(triangles.end(), { indices[i - 1], indices[i - 2], indices[i] });
		}
		break;
	case OF_PRIMITIVE_TRIANGLE_FAN:
		for (size_t i = 2; i < indices.size(); i++) {
			triangles.insert(triangles.end(), { indices[0], indices[i - 1], indices[i] });
		}
		break;
	default:
		ofLogWarning("ofxEmbree") << "addMesh: only triangles, triangle strips and fans can be ray traced";
		break;
	}
	return triangles;
}

RTCRayHit toRTC(const Ray & ray) {
	RTCRayHit r {};
	r.ray.org_x = ray.origin.x;
	r.ray.org_y = ray.origin.y;
	r.ray.org_z = ray.origin.z;
	r.ray.dir_x = ray.direction.x;
	r.ray.dir_y = ray.direction.y;
	r.ray.dir_z = ray.direction.z;
	r.ray.tnear = ray.near;
	r.ray.tfar = ray.far;
	r.ray.mask = 0xFFFFFFFF;
	r.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	r.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
	return r;
}

} // namespace

RTCDevice Scene::getDevice() {
	auto device = sharedDevice();
	return device ? device.get() : nullptr;
}

Scene::Scene()
	: device(sharedDevice()) {
	if (device) {
		scene = rtcNewScene(device.get());
		rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_HIGH);
	}
}

Scene::~Scene() {
	if (scene) rtcReleaseScene(scene);
}

unsigned Scene::attach(RTCGeometry geometry) {
	rtcCommitGeometry(geometry);
	unsigned id = rtcAttachGeometry(scene, geometry);
	rtcReleaseGeometry(geometry);
	numGeometries++;
	dirty = true;
	return id;
}

unsigned Scene::addMesh(const ofMesh & mesh, const glm::mat4 & transform) {
	if (!scene) return RTC_INVALID_GEOMETRY_ID;
	std::vector<unsigned> triangles = triangleIndices(mesh);
	if (triangles.empty() || mesh.getNumVertices() == 0) {
		ofLogWarning("ofxEmbree") << "addMesh: the mesh has no triangles";
		return RTC_INVALID_GEOMETRY_ID;
	}
	const auto & vertices = mesh.getVertices();
	for (unsigned index : triangles) {
		if (index >= vertices.size()) {
			ofLogError("ofxEmbree") << "addMesh: index " << index << " is out of range (" << vertices.size() << " vertices)";
			return RTC_INVALID_GEOMETRY_ID;
		}
	}

	RTCGeometry geometry = rtcNewGeometry(device.get(), RTC_GEOMETRY_TYPE_TRIANGLE);
	auto * v = static_cast<float *>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0,
		RTC_FORMAT_FLOAT3, 3 * sizeof(float), vertices.size()));
	for (size_t i = 0; i < vertices.size(); i++) {
		glm::vec3 p = transform * glm::vec4(vertices[i], 1.f);
		v[3 * i + 0] = p.x;
		v[3 * i + 1] = p.y;
		v[3 * i + 2] = p.z;
	}
	auto * idx = static_cast<unsigned *>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0,
		RTC_FORMAT_UINT3, 3 * sizeof(unsigned), triangles.size() / 3));
	std::copy(triangles.begin(), triangles.end(), idx);

	unsigned id = attach(geometry);
	numTriangles += triangles.size() / 3;
	trianglesPerGeometry[id] = triangles.size() / 3;
	return id;
}

unsigned Scene::addSphere(const glm::vec3 & center, float radius) {
	if (!scene) return RTC_INVALID_GEOMETRY_ID;
	RTCGeometry geometry = rtcNewGeometry(device.get(), RTC_GEOMETRY_TYPE_SPHERE_POINT);
	auto * p = static_cast<float *>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0,
		RTC_FORMAT_FLOAT4, 4 * sizeof(float), 1));
	p[0] = center.x;
	p[1] = center.y;
	p[2] = center.z;
	p[3] = radius;
	return attach(geometry);
}

void Scene::remove(unsigned id) {
	if (!scene || !rtcGetGeometry(scene, id)) return;
	rtcDetachGeometry(scene, id);
	numGeometries--;
	auto it = trianglesPerGeometry.find(id);
	if (it != trianglesPerGeometry.end()) {
		numTriangles -= it->second;
		trianglesPerGeometry.erase(it);
	}
	dirty = true;
}

void Scene::clear() {
	if (!scene) return;
	rtcReleaseScene(scene);
	scene = rtcNewScene(device.get());
	rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_HIGH);
	numGeometries = 0;
	numTriangles = 0;
	trianglesPerGeometry.clear();
	dirty = true;
}

void Scene::commit() const {
	if (scene && dirty) {
		rtcCommitScene(scene);
		dirty = false;
	}
}

Hit Scene::toHit(const RTCRayHit & r) const {
	Hit hit;
	if (r.hit.geomID == RTC_INVALID_GEOMETRY_ID) return hit;
	hit.hit = true;
	hit.distance = r.ray.tfar;
	hit.position = glm::vec3(r.ray.org_x, r.ray.org_y, r.ray.org_z) + r.ray.tfar * glm::vec3(r.ray.dir_x, r.ray.dir_y, r.ray.dir_z);
	glm::vec3 n(r.hit.Ng_x, r.hit.Ng_y, r.hit.Ng_z);
	float length = glm::length(n);
	hit.normal = length > 0.f ? n / length : n;
	hit.uv = { r.hit.u, r.hit.v };
	hit.geometry = r.hit.geomID;
	hit.primitive = r.hit.primID;
	return hit;
}

Hit Scene::intersect(const Ray & ray) const {
	if (!scene) return {};
	commit();
	RTCRayHit r = toRTC(ray);
	rtcIntersect1(scene, &r);
	return toHit(r);
}

Hit Scene::intersect(const glm::vec3 & origin, const glm::vec3 & direction) const {
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	return intersect(ray);
}

bool Scene::occluded(const Ray & ray) const {
	if (!scene) return false;
	commit();
	RTCRay r = toRTC(ray).ray;
	rtcOccluded1(scene, &r);
	return r.tfar == -std::numeric_limits<float>::infinity();
}

void Scene::intersect(const std::vector<Ray> & rays, std::vector<Hit> & hits) const {
	hits.assign(rays.size(), Hit {});
	if (!scene || rays.empty()) return;
	commit();
	size_t threads = std::max(1u, std::thread::hardware_concurrency());
	threads = std::min(threads, (rays.size() + 1023) / 1024);
	auto work = [&](size_t begin, size_t end) {
		for (size_t i = begin; i < end; i++) {
			RTCRayHit r = toRTC(rays[i]);
			rtcIntersect1(scene, &r);
			hits[i] = toHit(r);
		}
	};
	if (threads <= 1) {
		work(0, rays.size());
		return;
	}
	std::vector<std::thread> pool;
	size_t chunk = (rays.size() + threads - 1) / threads;
	for (size_t t = 0; t < threads; t++) {
		size_t begin = t * chunk, end = std::min(rays.size(), begin + chunk);
		if (begin < end) pool.emplace_back(work, begin, end);
	}
	for (auto & thread : pool) thread.join();
}

Ray Scene::rayFromScreen(const ofCamera & camera, const glm::vec2 & screen, const ofRectangle & viewport) {
	// screen -> normalized device coordinates, through the inverse view projection
	glm::mat4 inverse = glm::inverse(camera.getModelViewProjectionMatrix(viewport));
	float x = 2.f * (screen.x - viewport.x) / viewport.width - 1.f;
	float y = 1.f - 2.f * (screen.y - viewport.y) / viewport.height;
	if (camera.isVFlipped()) y = -y;
	glm::vec4 nearPoint = inverse * glm::vec4(x, y, -1.f, 1.f);
	glm::vec4 farPoint = inverse * glm::vec4(x, y, 1.f, 1.f);
	glm::vec3 a = glm::vec3(nearPoint) / nearPoint.w;
	glm::vec3 b = glm::vec3(farPoint) / farPoint.w;
	Ray ray;
	ray.origin = a;
	ray.direction = glm::normalize(b - a);
	ray.far = glm::length(b - a);
	return ray;
}

Hit Scene::pick(const ofCamera & camera, const glm::vec2 & screen, const ofRectangle & viewport) const {
	return intersect(rayFromScreen(camera, screen, viewport));
}

Hit Scene::pick(const ofCamera & camera, const glm::vec2 & screen) const {
	return pick(camera, screen, ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));
}

std::pair<glm::vec3, glm::vec3> Scene::getBounds() const {
	if (!scene || numGeometries == 0) return { glm::vec3(0.f), glm::vec3(0.f) };
	commit();
	RTCBounds b;
	rtcGetSceneBounds(scene, &b);
	return { { b.lower_x, b.lower_y, b.lower_z }, { b.upper_x, b.upper_y, b.upper_z } };
}

} // namespace ofxEmbree
