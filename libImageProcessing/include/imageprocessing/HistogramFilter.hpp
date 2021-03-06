/*
 * HistogramFilter.hpp
 *
 *  Created on: 06.06.2013
 *      Author: poschmann
 */

#ifndef HISTOGRAMFILTER_HPP_
#define HISTOGRAMFILTER_HPP_

#include "imageprocessing/ImageFilter.hpp"
#include <vector>

using std::vector;

namespace imageprocessing {

/**
 * Filter whose results are histograms. Does only provide functions for creating and normalizing histograms, can not be
 * instantiated directly.
 */
class HistogramFilter : public ImageFilter {
public:

	/**
	 * Normalization method.
	 */
	enum class Normalization { NONE, L2NORM, L2HYS, L1NORM, L1SQRT };

	/**
	 * Constructs a new histogram feature extractor.
	 *
	 * @param[in] normalization The normalization method of the histograms.
	 */
	explicit HistogramFilter(Normalization normalization);

	virtual ~HistogramFilter();

	virtual Mat applyTo(const Mat& image, Mat& filtered) const = 0;

protected:

	/**
	 * Creates histograms over a regular grid of cells using an image containing bin information per pixel.
	 *
	 * The input image must have a depth CV_8U containing bin information (bin indices and optionally weights). It must have
	 * one, two or four channels. In case of one channel, it just contains the bin index. The second channel adds a weight
	 * that will be divided by 255 to be between zero and one. The third channel is another bin index and the fourth channel
	 * is the corresponding weight. The output will be a row vector of type CV_32F containing the concatenated histograms of
	 * the cells in row-major order, the size (column count) will be cellRows * cellCols * bins.
	 *
	 * @param[in] image Image containing bin data per pixel.
	 * @param[out] histograms Row vector containing the histogram values of the cells in row-major order.
	 * @param[in] binCount Bin count of the histogram.
	 * @param[in] rowCount Row count of the cell grid.
	 * @param[in] columnCount Column count of the cell grid.
	 * @param[in] interpolate Flag that indicates whether each pixel should contribute to the four cells around it using bilinear interpolation.
	 */
	void createCellHistograms(const Mat& image, Mat& histograms, int binCount, int rowCount, int columnCount, bool interpolate) const;

	/**
	 * Normalizes the given histogram.
	 *
	 * @param[in,out] histogram The histogram that should be normalized.
	 */
	void normalize(Mat& histogram) const;

	static const float eps; ///< The small value being added to the norm to prevent division by zero.

	Normalization normalization; ///< The normalization method of the histograms.

private:

	/**
	 * Entry of the cache for the linear interpolation.
	 */
	struct CacheEntry {
		int index;    ///< First index (second one is index + 1).
		float weight; ///< Weight of the second index (weight of first index is 1 - weight).
	};

	/**
	 * Creates the cache contents if the size of the cache does not match the requested size.
	 *
	 * @param[in] cache The cache.
	 * @param[in] size The necessary size of the cache.
	 * @param[in] count The number of cells.
	 */
	void createCache(vector<CacheEntry>& cache, unsigned int size, int count) const;

	/**
	 * Normalizes the given histogram according to L2-norm.
	 *
	 * @param[in,out] histogram The histogram that should be normalized.
	 */
	void normalizeL2(Mat& histogram) const;

	/**
	 * Normalizes the given histogram according to L2-hys (L2-norm followed by clipping (limit maximum values to 0.2)
	 * and renormalizing).
	 *
	 * @param[in,out] histogram The histogram that should be normalized.
	 */
	void normalizeL2Hys(Mat& histogram) const;

	/**
	 * Normalizes the given histogram according to L1-norm.
	 *
	 * @param[in,out] histogram The histogram that should be normalized.
	 */
	void normalizeL1(Mat& histogram) const;

	/**
	 * Normalizes the given histogram according to L1-sqrt (L1-norm followed by computing the square root over all elements).
	 *
	 * @param[in,out] histogram The histogram that should be normalized.
	 */
	void normalizeL1Sqrt(Mat& histogram) const;

	mutable vector<CacheEntry> rowCache; ///< Cache for the linear interpolation of the row indices.
	mutable vector<CacheEntry> colCache; ///< Cache for the linear interpolation of the column indices.
};

} /* namespace imageprocessing */
#endif /* HISTOGRAMFILTER_HPP_ */
