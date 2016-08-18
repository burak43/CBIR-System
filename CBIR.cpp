/*
 * CBIR.cpp
 *
 *  Created on: Jun 14, 2016
 *      Author: Burak Mandira
 *  	   Use: ./CBIRSystem "path to original directory" "path to query directory"
 */

#include <ctime>
#include "Indexing.hpp"
#include "Query.hpp"

//	(+)	size daki boyut ve yontem
//	(+)	ayni descriptor kullan
//	(+)	1. SIFT 2.SURF (ayri ayri dosyala, precision hesapla)
//	(+) 2 dosyalari birlestir
//	(+) include var
//	(+) index ile read query ayir (methodlara ayir)
//	boost::logging bak, incele, arastir

int checkArguments( int argc, char**& argv)
{
	string opt = "";
	if( argv[1] != NULL)
		opt.append( argv[1]);
	if( argc == 1 || (opt != "-i" && opt != "-q"))	// invalid params or no params
	{
		cerr << endl << "Correct Use: ./CBIRSystem [OPTIONS] [INFO]" << endl;
		cerr << "------------------------------------------------------" << endl;
		cerr << "OPTIONS" << endl << "\t-i    Index DB images (*)" << endl << "\t-q    Query an image  (**)" << endl;
		cerr << "INFO" << endl;
		cerr << "\t(*)    \"path to DB images' directory\"" << endl;
		cerr << "\t(**)   \"path to query image\" \"# of days for span\" \"similarity percentage(%)\" \"max # of results per index\"" << endl << endl;
		return 0;
	}
	else
	{
		if( opt == "-i")
		{
			if( argv[2] == NULL)
			{
				cerr << endl << "Missing argument. Correct use: ./CBIRSystem -i \"path to DB images' directory\"" << endl << endl;
				return 0;
			}
			else return 1;
		}
		else	// argv[1] == "-q"
		{
			if( argv[2] == NULL || argv[3] == NULL || argv[4] == NULL || argv[5] == NULL )
			{
				cerr << endl << "Missing argument(s). Correct use: ./CBIRSystem -q \"path to query image\" \"# of days for span\" \"similarity percentage(%)\" \"max # of results per index\"" << endl << endl;
				return 0;
			}
			else if( atoi( argv[3]) <= 0 || atoi( argv[4]) < 0 || atoi( argv[4]) > 100 || atoi( argv[5]) <= 0)
			{
				cerr << endl << "Invalid argument(s). Correct use: ./CBIRSystem -q \"path to query image\" \"# of days for span\" \"similarity percentage(%)\" \"max # of results per index\"" << endl << endl;
				return 0;
			}
			else return 2;
		}
	}
}

int main(int argc, char *argv[])
{
	// check arguments
	int status = checkArguments( argc, argv);
	switch( status)
	{
		case 0:
			return -1;
		case 1:
		{
			// Start DB indexing
			string db_dir( argv[2]);
			Indexing imageIndex( db_dir, new SURF( 500, 4, 2, false), new SURF( 500, 4, 2, false));
			if( !imageIndex.createIndex())
				return -1;
			cout << "Indices are created and stored successfully!" << endl;
			break;
		}
		case 2:
		{
			// Start image query
			string img_path( argv[2]);
			const int dayRange = atoi( argv[3]);
			const int percentage = atoi( argv[4]);
			const int K = atoi( argv[5]);

			// get current time as seconds since 1 January 1970
			time_t now = time( NULL);
			tm *ltm = localtime( &now);
			Query queryRequest( img_path, new SURF( 500, 4, 2, false), new SURF( 500, 4, 2, false));

			vector<vector<string> > similarImages;		// [ # of similar images x 2] vector( [][0]: date, [][1]: image path)
			for( int i = 0; i < dayRange; ++i)
			{
				char date[11];
				strftime( date, 11, "%d%m1%y", ltm);
				string path_to_date = "./Indices/";
				path_to_date.append( date);

				cout << "Processing " << date << endl;
				// Query image request among indices set
				if( !queryRequest.processIndices( path_to_date, similarImages, K, percentage))
					return -1;

				ltm->tm_mday--;
				mktime( ltm);
				cout << path_to_date << " has been processed." << endl << endl;
			}

			cout << endl << "Most similar images are as follows:" << endl;
			for( size_t i = 0; i < similarImages.size(); ++i)
			{
				cout << similarImages[i][0] << ": " << similarImages[i][1] << " " << similarImages[i][2] <<endl;
				Mat mat = imread( similarImages[i][1]);
				assert( !mat.empty());
				resize( mat, mat, Size( 400, 200), INTER_CUBIC);
				imshow( "match", mat);
				waitKey(0);
			}
			break;
		}
	} // end of switch

	cout << "Au revoir!" << endl;
	return 0;
}
