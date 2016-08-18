/*
 * Indexing.hpp
 *
 *  Created on: Jun 20, 2016
 *      Author: Burak Mandira
 */

#ifndef INDEXING_HPP_
#define INDEXING_HPP_

#include "libraries.hpp"

class Indexing
{
	public:
		Indexing( const string& directory,
				  const Ptr<FeatureDetector> detectorPointer = new SIFT(),
				  const Ptr<DescriptorExtractor> extractorPointer = new SIFT(),
				  const unsigned int longestSide = 400);
		~Indexing();
		bool createIndex();

	private:
		bool detectAndExtractFeatures( Mat& img, Mat& descriptors, const string& DBImagePath);
		bool saveIndexSettings( const Mat& concatMat, const Index& fln_index, int& partNo) const;
		bool createDirectory( const string& path) const;
		bool saveMatBinary( const string& filename, const Mat& output) const;

		string db_directory, date_of_DB_images;
		int maxLength;
		Ptr<FeatureDetector> feature_detector;
		Ptr<DescriptorExtractor> descriptor_extractor;
		vector<string> db_images_paths;
		vector<int> index_ranges;
};

#endif /* INDEXING_HPP_ */
