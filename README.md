Use: ./CBIRSystem [OPTIONS] [INFO]
------------------------------------------------------
OPTIONS

	-i    Index DB images (*)
	-q    Query an image  (**)
INFO

	(*)    "path to DB images' directory"
	(**)   "path to query image" "# of days for span" "minimum similarity (%)" "max # of results per index"

Example:

For indexing:	./CBIRSystem -i ./2806116/

For querying:	./CBIRSystem -q ./holiday/query/123602.jpg 3 20 5
(Search for the given image within 3 days including today with a minimum 20 percentage similarity and get maximum 5 images from each index)


IMPORTANT NOTE: DB images that will be indexed must be in a directory named with this format: "ddmm1yy"
