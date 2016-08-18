/*
 * Query.hpp
 *
 *  Created on: Jun 21, 2016
 *      Author: Burak Mandira
 */

#ifndef QUERY_HPP_
#define QUERY_HPP_

#include "libraries.hpp"

class Query
{
	public:
		Query( const string& query_image_path, const Ptr<FeatureDetector> detectorPointer = new SIFT(), const Ptr<DescriptorExtractor> extractorPointer = new SIFT(), const unsigned int longestSide = 400);
		~Query();
		bool processIndices( string path_to_date, vector<vector<string> >& similarImages, const int K, const int percentage);

	private:
		bool loadIndexSettings( const string& path_to_index_settings);
		bool loadMatBinary( const string& path_to_index_settings, Mat& loaded_mat);
		bool loadPrevIndexMatConfs( const string& path_to_index_settings, int& saved_rows, int& saved_cols, int& saved_type);
		bool loadRangesAndPaths( const string& path_to_index_settings);
		bool checkFileStatus( const std::ifstream& in, const string& fileName) const;
		bool processQueryImage( vector<vector<string> >& similarImages, const int& K, const int& percentage);

		void displayReadImage( const Mat& queryImage, const string& windowsName) const;
		bool detectAndExtractFeatures( Mat& query_gray, Mat& query_descriptors);
		void searchImages( Mat& query_descriptors, Mat& indices, Mat& distances);
		void setFrequencies( vector<vector<int> >& proximity_frequency, const Mat& indices, const Mat& distances);
		void displayMatchedImage( vector<vector<int> >& proximity_frequency, const string& origImgPath);
		void updateSimilarImages( vector<vector<string> >& similarImages, const vector<vector<int> >& proximity_frequency, const int& K, const int& percentage, const int& number_of_indices);
		void updateStats( const vector<vector<int> >& proximity_frequency, const string& origImgPath);
		void displayPrecision() const;
		Mat resizeImage( Mat& src);

		int maxLength, truePositives, falsePositives, origImgNameStartPos, queryImgNameStartPos, cantDetecteds;
		string query_image_path, currentDate;
		Ptr<DescriptorExtractor> descriptor_extractor;
		Ptr<FeatureDetector> feature_detector;
		vector<string> paths;
		vector<int> ranges;
		Index fln_index_saved;
		IndexParams savedIndexParams;
		Mat indexMat;
};

#endif /* QUERY_HPP_ */
