/*
 * GrayscaleFilter.hpp
 *
 *  Created on: 19.02.2013
 *      Author: poschmann
 */

#ifndef GRAYSCALEFILTER_HPP_
#define GRAYSCALEFILTER_HPP_

#include "imageprocessing/ImageFilter.hpp"

namespace imageprocessing {

/**
 * Image filter that converts images to grayscale. If the input image does have only one channel, then the data
 * will be copied to the output image without any conversion. Otherwise, the image data is assumed to be BGR.
 */
class GrayscaleFilter : public ImageFilter {
public:

	/**
	 * Constructs a new grayscale filter.
	 */
	GrayscaleFilter();

	~GrayscaleFilter();

	using ImageFilter::applyTo;

	Mat applyTo(const Mat& image, Mat& filtered) const;

	void applyInPlace(Mat& image) const;
};

} /* namespace imageprocessing */
#endif /* GRAYSCALEFILTER_HPP_ */
