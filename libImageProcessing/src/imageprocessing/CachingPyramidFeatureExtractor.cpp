/*
 * CachingPyramidFeatureExtractor.cpp
 *
 *  Created on: 22.03.2013
 *      Author: poschmann
 */

#include "imageprocessing/CachingPyramidFeatureExtractor.hpp"
#include "imageprocessing/VersionedImage.hpp"
#include "imageprocessing/Patch.hpp"
#include <stdexcept>

using cv::Point;
using std::make_shared;
using std::invalid_argument;

namespace imageprocessing {

CachingPyramidFeatureExtractor::CachingPyramidFeatureExtractor(shared_ptr<PyramidFeatureExtractor> extractor, Strategy strategy) :
		extractor(extractor), cache(), firstCacheIndex(0), strategy(strategy), version(-1) {}

CachingPyramidFeatureExtractor::~CachingPyramidFeatureExtractor() {}

void CachingPyramidFeatureExtractor::buildCache() {
	cache.clear();
	double minScaleFactor = getMinScaleFactor();
	double maxScaleFactor = getMaxScaleFactor();
	double incrementalScaleFactor = extractor->getIncrementalScaleFactor();
	double scaleFactor = 1;
	for (int i = 0; ; ++i, scaleFactor *= incrementalScaleFactor) {
		if (scaleFactor > maxScaleFactor)
			continue;
		if (scaleFactor < minScaleFactor)
			break;
		if (cache.empty())
			firstCacheIndex = i;
		cache.push_back(CacheLayer(i, scaleFactor));
	}
}

void CachingPyramidFeatureExtractor::update(const Mat& image) {
	extractor->update(image);
	buildCache();
	version = -1;
}

void CachingPyramidFeatureExtractor::update(shared_ptr<VersionedImage> image) {
	extractor->update(image);
	if (version != image->getVersion()) {
		buildCache();
		version = image->getVersion();
	}
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extract(int x, int y, int width, int height) const {
	int layerIndex = getLayerIndex(width, height);
	int index = layerIndex - firstCacheIndex;
	if (index < 0 || static_cast<unsigned int>(index) >= cache.size())
		return shared_ptr<Patch>();
	CacheLayer& layer = cache[index];
	switch (strategy) {
	case Strategy::SHARING:
		return extractSharing(layer, layer.getScaled(x), layer.getScaled(y));
	case Strategy::COPYING:
		return extractCopying(layer, layer.getScaled(x), layer.getScaled(y));
	case Strategy::INPUT_COPYING:
		return extractInputCopying(layer, layer.getScaled(x), layer.getScaled(y));
	case Strategy::OUTPUT_COPYING:
		return extractOutputCopying(layer, layer.getScaled(x), layer.getScaled(y));
	default: // should never be reached
		return extractor->extract(layerIndex, x, y);
	}
}

vector<shared_ptr<Patch>> CachingPyramidFeatureExtractor::extract(int stepX, int stepY, Rect roi,
		int firstLayer, int lastLayer, int stepLayer) const {
	if (stepX < 1)
		throw invalid_argument("CachingPyramidFeatureExtractor: stepX has to be greater than zero");
	if (stepY < 1)
		throw invalid_argument("CachingPyramidFeatureExtractor: stepY has to be greater than zero");
	if (stepLayer < 1)
		throw invalid_argument("CachingPyramidFeatureExtractor: stepLayer has to be greater than zero");
	Size imageSize = getImageSize();
	if (roi.x == 0 && roi.y == 0 && roi.width == 0 && roi.height == 0) {
		roi.width = imageSize.width;
		roi.height = imageSize.height;
	} else {
		roi.x = std::max(0, roi.x);
		roi.y = std::max(0, roi.y);
		roi.width = std::min(imageSize.width, roi.width + roi.x) - roi.x;
		roi.height = std::min(imageSize.height, roi.height + roi.y) - roi.y;
	}
	vector<shared_ptr<Patch>> patches;
	if (firstLayer < 0)
		firstLayer = cache.front().getIndex();
	if (lastLayer < 0)
		lastLayer = cache.back().getIndex();
	for (auto layer = cache.begin(); layer != cache.end(); layer += stepLayer) {
		if (layer->getIndex() < firstLayer)
			continue;
		if (layer->getIndex() > lastLayer)
			break;

		Rect centerRoi = getCenterRoi(
				Rect(layer->getScaled(roi.x), layer->getScaled(roi.y), layer->getScaled(roi.x + roi.width), layer->getScaled(roi.y + roi.height)));
		Point centerRoiBegin = centerRoi.tl();
		Point centerRoiEnd = centerRoi.br();
		Point center(centerRoiBegin.x, centerRoiBegin.y);
		while (center.y <= centerRoiEnd.y) {
			center.x = centerRoiBegin.x;
			while (center.x <= centerRoiEnd.x) {
				switch (strategy) {
				case Strategy::SHARING:
					patches.push_back(extractSharing(*layer, center.x, center.y));
					break;
				case Strategy::COPYING:
					patches.push_back(extractCopying(*layer, center.x, center.y));
					break;
				case Strategy::INPUT_COPYING:
					patches.push_back(extractInputCopying(*layer, center.x, center.y));
					break;
				case Strategy::OUTPUT_COPYING:
					patches.push_back(extractOutputCopying(*layer, center.x, center.y));
					break;
				}
				center.x += stepX;
			}
			center.y += stepY;
		}
	}
	return patches;
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extract(int layerIndex, int x, int y) const {
	int index = layerIndex - firstCacheIndex;
	if (index < 0 || static_cast<unsigned int>(index) >= cache.size())
		return shared_ptr<Patch>();
	CacheLayer& layer = cache[index];
	switch (strategy) {
	case Strategy::SHARING:
		return extractSharing(layer, x, y);
	case Strategy::COPYING:
		return extractCopying(layer, x, y);
	case Strategy::INPUT_COPYING:
		return extractInputCopying(layer, x, y);
	case Strategy::OUTPUT_COPYING:
		return extractOutputCopying(layer, x, y);
	default: // should never be reached
		return extractor->extract(layerIndex, x, y);
	}
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extractSharing(CacheLayer& layer, int x, int y) const {
	CacheKey key(x, y);
	unordered_map<CacheKey, shared_ptr<Patch>, CacheKey::hash>& layerCache = layer.getCache();
	auto iterator = layerCache.find(key);
	if (iterator == layerCache.end()) {
		shared_ptr<Patch> patch = extractor->extract(layer.getIndex(), x, y);
		layerCache.emplace(key, patch);
		return patch;
	}
	return iterator->second;
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extractCopying(CacheLayer& layer, int x, int y) const {
	CacheKey key(x, y);
	unordered_map<CacheKey, shared_ptr<Patch>, CacheKey::hash>& layerCache = layer.getCache();
	auto iterator = layerCache.find(key);
	if (iterator == layerCache.end()) {
		shared_ptr<Patch> patch = extractor->extract(layer.getIndex(), x, y);
		if (patch) // store a copy of the patch only if it exists
			layerCache.emplace(key, make_shared<Patch>(*patch));
		return patch;
	}
	return make_shared<Patch>(*(iterator->second));
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extractInputCopying(CacheLayer& layer, int x, int y) const {
	CacheKey key(x, y);
	unordered_map<CacheKey, shared_ptr<Patch>, CacheKey::hash>& layerCache = layer.getCache();
	auto iterator = layerCache.find(key);
	if (iterator == layerCache.end()) {
		shared_ptr<Patch> patch = extractor->extract(layer.getIndex(), x, y);
		if (patch) // store a copy of the patch only if it exists
			layerCache.emplace(key, make_shared<Patch>(*patch));
		return patch;
	}
	return iterator->second;
}

shared_ptr<Patch> CachingPyramidFeatureExtractor::extractOutputCopying(CacheLayer& layer, int x, int y) const {
	CacheKey key(x, y);
	unordered_map<CacheKey, shared_ptr<Patch>, CacheKey::hash>& layerCache = layer.getCache();
	auto iterator = layerCache.find(key);
	if (iterator == layerCache.end()) {
		shared_ptr<Patch> patch = extractor->extract(layer.getIndex(), x, y);
		layerCache.emplace(key, patch);
		return patch;
	}
	return make_shared<Patch>(*(iterator->second));
}

} /* namespace imageprocessing */
