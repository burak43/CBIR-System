/*
 * Query.cpp
 *
 *  Created on: Jun 21, 2016
 *      Author: Burak Mandira
 */

#include "Query.hpp"

Query::Query( const string& query_image_path, const Ptr<FeatureDetector> detectorPointer, const Ptr<DescriptorExtractor> extractorPointer, const unsigned int longestSide)
{
	this->query_image_path = query_image_path;
	currentDate = "";
	maxLength = longestSide;
	truePositives = falsePositives = cantDetecteds =  origImgNameStartPos = 0;

	if( query_image_path[query_image_path.length()-1] == '/')				// if "/" is placed at the end of query path
		queryImgNameStartPos = query_image_path.find_last_of( '/', query_image_path.find_last_of( '/')-1) + 1;
	else																	// if "/" is not placed at the end of query path
		queryImgNameStartPos = query_image_path.find_last_of( '/') + 1;

	feature_detector = detectorPointer;
	descriptor_extractor = extractorPointer;
}

Query::~Query() {}

// public abstract trigger method
bool Query::processIndices( string path_to_date, vector<vector<string> >& similarImages, const int K, const int percentage)		// path_to_date = "./Indices/ddmm1yy"
{
	currentDate = path_to_date.substr( path_to_date.find( '/', 2) + 1);
	currentDate = currentDate.substr( 0, 4).append( currentDate.substr( 5));
	// process every index parts
	for( directory_iterator it( path_to_date); it != directory_iterator(); ++it)
	{
		// 1. load index settings
		if( !loadIndexSettings( it->path().string()))
		{
			cerr << "Index settings couldn't be loaded from " << it->path() << endl;
			return false;
		}

		// 2. process query image and find the K-most similar images within the current index(it)
		if( !processQueryImage( similarImages, K, percentage))
			return false;

		// 3. release memory for the next index part
		indexMat.release();
		ranges.clear();
		paths.clear();
	}
	return true;
}

bool Query::loadIndexSettings( const string& path_to_index_settings)		// path_to_index_settings = "./Indices/ddmm1yy/part.X"
{
	cout << "Loading settings (part " << path_to_index_settings.substr( path_to_index_settings.rfind( '.') + 1) << ")..." << endl;

	// load previously created index matrix
	if( !loadMatBinary( path_to_index_settings, indexMat))
	{
		cerr << "Error while loading saved indexMat." << endl;
		return false;
	}

	string date = path_to_index_settings;
	date = date.substr( date.find( '/', 2), 8);			// date = "/ddmm1yy"
	string partNo = path_to_index_settings.substr( path_to_index_settings.rfind( '.')+1);

	// load previously saved flann::Index
	string fileName = date;
	fileName.append( "-flann_index");
	fileName.append( partNo);
	fileName.append( ".bin");
	string indexPath = path_to_index_settings;
	indexPath.append( fileName);								// indexPath = "./Indices/ddmm1yy/part.X/ddmm1yy-flann_indexX.bin"
	if( !exists( indexPath))
	{
		cerr << "No such file exist: \"" << indexPath << "\"" << endl;
		return false;
	}
//	savedIndexParams = *new SavedIndexParams( indexPath);
//	fln_index_saved = *new Index( indexMat, savedIndexParams);
	fln_index_saved.load( indexMat, indexPath);

	// load ranges & paths
	return loadRangesAndPaths( path_to_index_settings);
}

// load previous index mat
bool Query::loadMatBinary( const string& path_to_index_settings, Mat& loaded_mat)
{
	string date = path_to_index_settings;
	date = date.substr( date.find( '/', 2), 8);			// date = "/ddmm1yy"
	date.append( "-index_mat");
	string partNo = path_to_index_settings.substr( path_to_index_settings.rfind( '.')+1);
	date.append( partNo);
	date.append( ".bin");

	string indexConfPath = path_to_index_settings;
	indexConfPath.append( date);							// indexConfPath = "./Indices/ddmm1yy/part.X/ddmm1yy-index_matX.bin"

	std::ifstream ifs( indexConfPath.c_str(), std::ios::binary);
	if( !checkFileStatus( ifs, indexConfPath))
		return false;

	int rows, cols, type;
	ifs.read( (char*)(&rows), sizeof( int));
	assert( rows != 0);

	ifs.read( (char*)(&cols), sizeof( int));
	ifs.read( (char*)(&type), sizeof( int));

	loaded_mat.release();
	loaded_mat.create( rows, cols, type);
	ifs.read( (char*)(loaded_mat.data), loaded_mat.elemSize() * loaded_mat.total());

	return true;
}

// read index ranges & image paths
bool Query::loadRangesAndPaths( const string& path_to_index_settings)
{
	cout << "Reading index ranges & image paths..." << endl;

	string date = path_to_index_settings;
	date = date.substr( date.find( '/', 2), 8);			// date = "/ddmm1yy"
	date.append( "-ranges&paths");
	string partNo = path_to_index_settings.substr( path_to_index_settings.rfind( '.')+1);
	date.append( partNo);
	date.append( ".txt");

	string rpPath = path_to_index_settings, path;
	rpPath.append( date);									// rpPath = "./Indices/ddmm1yy/part.X/ddmm1yy-ranges&pathsX.txt"
	int range;

	std::ifstream in;
	in.open( rpPath.c_str());
	if( !checkFileStatus( in, rpPath))
		return false;
	while( !in.eof())
	{
		in >> range;
		in.get();				// get space between range and path
		getline( in, path);
		if( !in.eof())
		{
			ranges.push_back( range);
			paths.push_back( path);
		}
	}
	in.close();
	origImgNameStartPos = paths[0].find_last_of( '/') + 1;	// assuming all indexed images(within the same index set) share the same path except their names

	return true;
}

bool Query::checkFileStatus( const std::ifstream& in, const string& fileName) const
{
	if( !in.good())
	{
		cerr << "No such file exist: \"" << fileName << "\"" << endl;
		return false;
	}
	return true;
}

// read query image and find the most K similar images
bool Query::processQueryImage( vector<vector<string> >& similarImages, const int& K, const int& percentage)
{
	Mat query = imread( query_image_path);
	if( query.empty())
	{
		cerr << "Query image couldn't be read." << endl;
		return false;
	}
	// resize the image in proportion so that its longest side will "maxLength"
	query = resizeImage( query);

	// display current query image
//	string windowsName = "Query Image: ";
//	windowsName.append( query_image_path);
//	displayReadImage( query, windowsName);

	Mat query_gray;
	cvtColor( query, query_gray, CV_RGB2GRAY);

	// extract features and if succesfull, search for similar image among the indices
	Mat query_descriptors;
	if( detectAndExtractFeatures( query_gray, query_descriptors))
	{
		cout << "Features are extracted from the query image." << endl;
		vector<vector<int> > proximity_frequency( paths.size(), vector<int>(2, 0));

		Mat indices, distances;
		searchImages( query_descriptors, indices, distances);
		setFrequencies( proximity_frequency, indices, distances);
//		displayMatchedImage( proximity_frequency, query_image_path);
		updateSimilarImages( similarImages, proximity_frequency, K, percentage, indices.rows);
//		updateStats( proximity_frequency, query_image_path);
		return true;
	}
	else
	{
		cerr << "KeyPoints couldn't be detected for the query image \"" << query_image_path << "\"." << endl;
		return false;
	}

//	destroyWindow( windowsName);
//	displayPrecision();
//	return true;
}

void Query::displayReadImage( const Mat& queryImage, const string& windowsName) const
{
	imshow( windowsName, queryImage);
	moveWindow( windowsName, 200, 150);

	cout << "Press a key to continue..." << endl;
	waitKey(0);
}

bool Query::detectAndExtractFeatures( Mat& query_gray, Mat& query_descriptors)
{
	vector<KeyPoint> query_kp;
	feature_detector->detect( query_gray, query_kp);
	// check whether KeyPoints are detected
	if( !query_kp.size())
	{
		++cantDetecteds;
		return false;
	}
	descriptor_extractor->compute( query_gray, query_kp, query_descriptors);
 	return true;
}

void Query::searchImages( Mat& query_descriptors, Mat& indices, Mat& distances)
{
	const int KNN = 7;
	fln_index_saved.knnSearch( query_descriptors, indices, distances, KNN, SearchParams(512));
}

int FirstColumnDescCmp( const vector<int>& lhs, const vector<int>& rhs)  { return lhs[0] > rhs[0]; }

void Query::setFrequencies( vector<vector<int> >& proximity_frequency, const Mat& indices, const Mat& distances)
{
	// proximity_frequency = [# of imgs in the current part x 2] vector ( [][0]: frequency, [][1]: index )
	for( size_t i = 0; i < proximity_frequency.size(); ++i)
		proximity_frequency[i][1] = i;

//	cout << "indices.rows = " << indices.rows << endl;
//	cout << indices << endl;
//	cout << distances << endl;
//	cout << "distances.rows = " << distances.rows << endl;

	// set frequencies
	int img_cnt = 0;
	for( int i = 0; i < indices.rows; ++i)
		for( int j = 0; j < indices.cols; ++j)
			if( distances.at<float>(i,j) < 0.05)
			{
				while( indices.at<int>(i,j) >= ranges[img_cnt])
					++img_cnt;
				proximity_frequency[img_cnt][0]++;
				img_cnt = 0;
			}

	// sort proximity frequency by freq descending
	sort( proximity_frequency.begin(), proximity_frequency.end(), FirstColumnDescCmp);

//	for( size_t i = 0; i < proximity_frequency.size(); ++i)
//		cout << proximity_frequency[i][0] << " " << proximity_frequency[i][1] << endl;
}

void Query::displayMatchedImage( vector<vector<int> >& proximity_frequency, const string& queryImgPath)
{
	 Mat display_img;
	 for( int i = 0; proximity_frequency[i][0]; ++i)
//	 for( int i = 0; i < 3; ++i)
	 {
		 cout << i+1 << "th image: " << paths[proximity_frequency[i][1]] << " Freq: " << proximity_frequency[i][0] << endl;
		 if( paths[proximity_frequency[i][1]].compare( origImgNameStartPos, 4, queryImgPath, queryImgNameStartPos, 4) == 0)
		 {
			 cout << "Most frequent " << i+1 << "th image: " << paths[proximity_frequency[i][1]] << endl;
			 display_img = imread( paths[proximity_frequency[i][1]]);
			 display_img = resizeImage( display_img);
			 imshow( "Matched Image", display_img);
			 moveWindow( "Matched Image", 700, 150);

			 cout << "Press a key to continue..." << endl;
			 waitKey(0);
			 destroyWindow( "Matched Image");
			 return;
		 }
	 }

	 // original image is not matched
	 cerr << "No match found within the given parameters." << endl;
	 cout << "Press a key to continue..." << endl;
	 waitKey(0);
}

void Query::updateSimilarImages( vector<vector<string> >& similarImages, const vector<vector<int> >& proximity_frequency, const int& K, const int& percentage, const int& number_of_indices)
{
	cout << number_of_indices << endl;
	int minimum = min( K, (int)proximity_frequency.size());
	double threshold;
	if( percentage == 0)
		threshold = 0;
	else
		threshold = (double)percentage/100 * number_of_indices;

	for( int i = 0; i < minimum; ++i)
		if( proximity_frequency[i][0] > (int)(threshold))
		{
			vector<string> imageInfo;				// ( imageInfo[0]: date, [1]: image path, [2]: frequency)
			imageInfo.push_back( currentDate);
			imageInfo.push_back( paths[proximity_frequency[i][1]]);
			char buffer[5];
			snprintf( buffer, 5, "%d", proximity_frequency[i][0]);
			imageInfo.push_back( buffer);
			similarImages.push_back( imageInfo);
		}
}

void Query::updateStats( const vector<vector<int> >& proximity_frequency, const string& queryImgPath)
{
//	 for( int i = 0; proximity_frequency[i][0]; ++i)
	 for( int i = 0; i < 3; ++i)
	 {
		 if( paths[proximity_frequency[i][1]].compare( origImgNameStartPos, 4, queryImgPath, queryImgNameStartPos, 4) == 0)
		 {
			 ++truePositives;
			 return;
		 }
	 }
	 ++falsePositives;
}

// calculate precision [ presicion = # of true positives / (# of true positives + false positives) ]
void Query::displayPrecision() const
{
	cout << "# of true positives: " << truePositives << "\t  # of false positives: " << falsePositives << "\t  # of cantDetecteds: " << cantDetecteds << endl;
	cout << "Precision: " << (double)truePositives / (truePositives + falsePositives + cantDetecteds) << endl;
}

Mat Query::resizeImage( Mat& src)
{
	Mat dst;
	assert( !src.empty());
	if( src.rows > src.cols)
	{
		if( src.rows > maxLength)
		{
			double factor = (double)src.rows / maxLength;
			resize( src, dst, Size(), 1/factor, 1/factor, INTER_CUBIC);
		}
	}
	else
	{
		if( src.cols > maxLength)
		{
			double factor = (double)src.cols / maxLength;
			resize( src, dst, Size(), 1/factor, 1/factor, INTER_CUBIC);
		}
	}
	return dst;
}
