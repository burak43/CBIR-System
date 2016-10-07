/*
 * Indexing.cpp
 *
 *  Created on: Jun 20, 2016
 *      Author: Burak Mandira
 */

#include "Indexing.hpp"

// constructor
Indexing::Indexing( const string& directory, const Ptr<FeatureDetector> detectorPointer, const Ptr<DescriptorExtractor> extractorPointer, const unsigned int longestSide)
{
	db_directory = directory;
	if( db_directory[db_directory.length()-1] == '/')									// if "/" is placed at the end of the path
	{
		date_of_DB_images = db_directory.substr( 0, db_directory.length() - 1);			// remove "/" from the end
		date_of_DB_images = date_of_DB_images.substr( date_of_DB_images.find_last_of( '/')+1);
	}
	else
		date_of_DB_images = db_directory.substr( db_directory.find_last_of( '/')+1);

	maxLength = longestSide;
	feature_detector = detectorPointer;
	descriptor_extractor = extractorPointer;
}

// destructor
Indexing::~Indexing() {}

// process DB images and create flann::Index
bool Indexing::createIndex()
{
	int cnt = 0, partNo = 1, skipped = 0;
	Mat concatMat;
	cout << "Reading DB images..." << endl;
	for( directory_iterator it( db_directory); it != directory_iterator(); it++)
	{
		++cnt;
		string path = it->path().string();

		Mat img = imread( path);
		if( img.rows <= 50 && img.cols <= 50)
		{
			++skipped;
			continue;
		}
		// resize the image in proportion so that its longest side will be "maxLength"
		if( img.rows > img.cols)
		{
			if( img.rows > maxLength)
			{
				double factor = (double)img.rows / maxLength;
				resize( img, img, Size(), 1/factor, 1/factor, INTER_CUBIC);
			}
		}
		else
		{
			if( img.cols > maxLength)
			{
				double factor = (double)img.cols / maxLength;
				resize( img, img, Size(), 1/factor, 1/factor, INTER_CUBIC);
			}
		}
		Mat img_gray;
		cvtColor( img, img_gray, CV_RGB2GRAY);
		Mat descriptors;
		if( !detectAndExtractFeatures( img_gray, descriptors, path))
			continue;

		if( descriptors.rows <= 10)
		{
			++skipped;
			continue;
		}

		concatMat.push_back( descriptors);

		if( index_ranges.size() != 0)
			index_ranges.push_back( index_ranges[index_ranges.size()-1] + descriptors.rows);
		else
			index_ranges.push_back( descriptors.rows);
		db_images_paths.push_back( path);

		size_t sz= sizeof( concatMat) + concatMat.total()*sizeof( CV_32F);
		if( sz > 2000000000)		// produce a new part of index when reached 2 GB
		{
			Index fln_index;
			fln_index.build( concatMat, KMeansIndexParams());
			if( !saveIndexSettings( concatMat, fln_index, partNo))
			{
				cerr << "Error while saving flann::Index part." << partNo << " settings." << endl;
				return false;
			}
			concatMat.release();
			index_ranges.clear();
			db_images_paths.clear();
			cout << "Continuing to read DB images [" << cnt << " so far]..." << endl;
		}

	} // end of DB Images loop

	Index fln_index;
//	fln_index = *new Index( concatMat, *new KMeansIndexParams());
	fln_index.build( concatMat, KMeansIndexParams());
	cout << "Total images read: " << cnt << endl;
	cout << "Total images skipped: " << skipped << endl;

	if( !saveIndexSettings( concatMat, fln_index, partNo))
	{
		cerr << "Error while saving flann::Index settings." << endl;
		return false;
	}
	return true;
}

// detect key points & extract features
bool Indexing::detectAndExtractFeatures( Mat& img, Mat& descriptors, const string& DBImagePath)
{
	// detect key points
	vector<KeyPoint> key_points;
	feature_detector->detect( img, key_points);

	if( key_points.size() == 0)
	{
		cerr << "KeyPoints couldn't be detected for the DB image \"" << DBImagePath << "\"." << endl;
		return false;
	}

	// extract features
	descriptor_extractor->compute( img, key_points, descriptors);
	assert( descriptors.rows == key_points.size());
	return true;
}

// save settings to the disk
bool Indexing::saveIndexSettings( const Mat& concatMat, const Index& fln_index, int& partNo) const
{
	string dir_path = "./Indices/";
	if( !createDirectory( dir_path))
	{
		cerr << "Couldn't create \"" << dir_path << "\"." << endl;
		return false;
	}
	
	dir_path.append( date_of_DB_images);
	if( !createDirectory( dir_path))
	{
		cerr << "Couldn't create \"" << dir_path << "\"." << endl;
		return false;
	}

	char buffer[13];			// supports up to 5 digit partNo
	snprintf( buffer, 13, "/part.%d/", partNo);
	dir_path.append( buffer);						// dir_path = "./Indices/ddmm1yy/part.X/"
	if( !createDirectory( dir_path))
	{
		cerr << "Couldn't create \"" << dir_path << "\"." << endl;
		return false;
	}

	snprintf( buffer, 10, "%d.bin", partNo);
	cout << "Saving settings (part " << partNo << ")..." << endl;

	// save flann::Index to disk
	string index_path = dir_path;
	index_path.append( date_of_DB_images);
	index_path.append( "-flann_index");
	index_path.append( buffer);
	fln_index.save( index_path);

	// save concatMat
	string desc_path = dir_path;
	desc_path.append( date_of_DB_images);
	desc_path.append( "-index_mat");
	desc_path.append( buffer);

	if( !saveMatBinary( desc_path, concatMat))
	{
		cerr << "Error while saving indexMat." << endl;
		return false;
	}

	// save index ranges & paths of images
	string rp_path = dir_path;
	rp_path.append( date_of_DB_images);
	rp_path.append( "-ranges&paths");
	snprintf( buffer, 10, "%d.txt", partNo);
	rp_path.append( buffer);

	std::ofstream out;
	out.open( rp_path.c_str());
	for( size_t i = 0; i < db_images_paths.size(); ++i)
		out << index_ranges[i] << " " << db_images_paths[i] << endl;
	out.close();

	++partNo;
	return true;
}

bool Indexing::createDirectory( const string& path) const
{
	if( exists( path))
		return true;
	else
		return create_directory( path);
}

bool Indexing::saveMatBinary(const string& filename, const Mat& output) const
{
	std::ofstream ofs( filename.c_str(), std::ios::binary);
	if( !ofs.good())
		return false;
	assert( !output.empty());

	int type = output.type();
	ofs.write( (const char*)(&output.rows), sizeof( int));
	ofs.write( (const char*)(&output.cols), sizeof( int));
	ofs.write( (const char*)(&type), sizeof( int));
	ofs.write( (const char*)(output.data), output.elemSize() * output.total());

	return true;
}
