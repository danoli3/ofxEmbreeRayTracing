#include "ofxEmbreeRenderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <thread>

namespace ofxEmbree {

namespace {

constexpr float EPSILON = 1e-3f;

glm::vec3 rgb(const ofFloatColor & c) { return { c.r, c.g, c.b }; }

// cosine-weighted direction around n
glm::vec3 cosineSample(const glm::vec3 & n, float u1, float u2) {
	float r = std::sqrt(u1), phi = 6.2831853f * u2;
	glm::vec3 t = std::abs(n.x) > 0.5f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);
	glm::vec3 b1 = glm::normalize(glm::cross(n, t)), b2 = glm::cross(n, b1);
	return glm::normalize(b1 * (r * std::cos(phi)) + b2 * (r * std::sin(phi)) + n * std::sqrt(std::max(0.f, 1.f - u1)));
}

float toSRGB(float c) {
	c = std::clamp(c, 0.f, 1.f);
	return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
}

} // namespace

void Renderer::setMaterial(unsigned geometry, const Material & material) {
	materials[geometry] = material;
	reset();
}

const Material & Renderer::getMaterial(unsigned geometry) const {
	auto it = materials.find(geometry);
	return it == materials.end() ? defaultMaterial : it->second;
}

void Renderer::reset() {
	passes = 0;
	std::fill(accumulation.begin(), accumulation.end(), glm::vec3(0.f));
}

void Renderer::render(const Scene & scene, const ofCamera & camera, int width, int height) {
	if (width <= 0 || height <= 0) return;
	if (int(image.getWidth()) != width || int(image.getHeight()) != height) {
		image.allocate(width, height, OF_PIXELS_RGB);
		accumulation.assign(size_t(width) * height, glm::vec3(0.f));
		passes = 0;
	}
	scene.commit();
	auto start = std::chrono::steady_clock::now();

	const ofRectangle viewport(0, 0, width, height);
	const glm::mat4 inverse = glm::inverse(camera.getModelViewProjectionMatrix(viewport));
	const bool flipped = camera.isVFlipped();
	const glm::vec3 toLight = -glm::normalize(settings.lightDirection);
	const int pass = passes;
	std::atomic<int> nextRow { 0 };
	std::atomic<long long> rays { 0 };

	auto worker = [&](unsigned threadIndex) {
		std::mt19937 rng(1234567u + 7919u * pass + 104729u * threadIndex);
		std::uniform_real_distribution<float> uniform(0.f, 1.f);
		long long count = 0;

		// returns the color along a ray, following mirror bounces
		auto trace = [&](Ray ray) {
			glm::vec3 color(0.f), weight(1.f);
			for (int bounce = 0; bounce <= settings.maxBounces; bounce++) {
				Hit hit = scene.intersect(ray);
				count++;
				if (!hit) {
					color += weight * rgb(settings.background);
					break;
				}
				const Material & m = getMaterial(hit.geometry);
				glm::vec3 dir = glm::normalize(ray.direction);
				glm::vec3 n = glm::dot(hit.normal, dir) > 0.f ? -hit.normal : hit.normal;
				glm::vec3 p = hit.position + n * EPSILON;

				glm::vec3 lightDir = toLight;
				float lightDistance = std::numeric_limits<float>::infinity();
				if (settings.pointLight) {
					glm::vec3 d = settings.lightPosition - p;
					lightDistance = glm::length(d);
					lightDir = lightDistance > 0.f ? d / lightDistance : n;
				}
				float direct = std::max(0.f, glm::dot(n, lightDir));
				if (direct > 0.f && settings.shadows) {
					Ray shadow;
					shadow.origin = p;
					shadow.direction = lightDir;
					shadow.far = lightDistance;
					count++;
					if (scene.occluded(shadow)) direct = 0.f;
				}
				float ambient = 1.f;
				if (settings.ambientOcclusionSamples > 0) {
					int open = 0;
					for (int s = 0; s < settings.ambientOcclusionSamples; s++) {
						Ray ao;
						ao.origin = p;
						ao.direction = cosineSample(n, uniform(rng), uniform(rng));
						ao.far = settings.ambientOcclusionDistance;
						count++;
						if (!scene.occluded(ao)) open++;
					}
					ambient = float(open) / settings.ambientOcclusionSamples;
				}
				glm::vec3 surface = rgb(m.color) * (rgb(settings.lightColor) * direct + rgb(settings.ambientColor) * ambient);
				color += weight * ((1.f - m.reflection) * surface + rgb(m.emission));
				if (m.reflection <= 0.f) break;
				weight *= m.reflection * rgb(m.color);
				ray = Ray();
				ray.origin = p;
				ray.direction = glm::reflect(dir, n);
			}
			return color;
		};

		for (int y = nextRow++; y < height; y = nextRow++) {
			for (int x = 0; x < width; x++) {
				// jittered pixel position -> camera ray (anti-aliasing over passes)
				float sx = x + (pass == 0 ? 0.5f : uniform(rng));
				float sy = y + (pass == 0 ? 0.5f : uniform(rng));
				float ndcX = 2.f * sx / width - 1.f;
				float ndcY = 1.f - 2.f * sy / height;
				if (flipped) ndcY = -ndcY;
				glm::vec4 a = inverse * glm::vec4(ndcX, ndcY, -1.f, 1.f);
				glm::vec4 b = inverse * glm::vec4(ndcX, ndcY, 1.f, 1.f);
				Ray ray;
				ray.origin = glm::vec3(a) / a.w;
				ray.direction = glm::normalize(glm::vec3(b) / b.w - ray.origin);
				accumulation[size_t(y) * width + x] += trace(ray);
			}
		}
		rays += count;
	};

	unsigned threads = settings.threads > 0 ? settings.threads : std::max(1u, std::thread::hardware_concurrency());
	std::vector<std::thread> pool;
	for (unsigned t = 0; t < threads; t++) pool.emplace_back(worker, t);
	for (auto & thread : pool) thread.join();
	passes++;

	float * out = image.getData();
	for (size_t i = 0; i < accumulation.size(); i++) {
		glm::vec3 c = accumulation[i] / float(passes);
		out[3 * i + 0] = toSRGB(c.r);
		out[3 * i + 1] = toSRGB(c.g);
		out[3 * i + 2] = toSRGB(c.b);
	}

	double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	lastPassMillis = float(seconds * 1000.0);
	lastPassRaysPerSecond = seconds > 0 ? rays / seconds : 0.0;
}

} // namespace ofxEmbree
