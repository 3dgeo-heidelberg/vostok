#include "SimpleTextPointCloud.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include "../myUtils.h"

SimpleTextPointCloud::SimpleTextPointCloud(std::string filePath, std::string pattern) :
		AbstractPointCloud() {

	mLineNumber = 0;

	mFilePath = filePath;
	mPattern = pattern;

	mFieldNames = splitString(pattern, ' ');

	mMin.resize(mFieldNames.size());
	mMax.resize(mFieldNames.size());

	//############# BEGIN Try to open source file ############
	mSourceFile = new std::ifstream(mFilePath.c_str());

	if (!mSourceFile->good()) {
		mSourceFileGood = false;
		return;
	} else {
		mSourceFileGood = true;
	}
	//############# END Try to open source file ############

	std::string metaFilePath = mFilePath + ".vostokmeta";

	//########## BEGIN Try to read metafile. If it doesn't exist, create one ##########
	std::ifstream metaFileReader(metaFilePath.c_str());

	if (metaFileReader.good()) {
		metaFileReader >> mNumPoints;
		metaFileReader >> mMin[0];
		metaFileReader >> mMin[1];
		metaFileReader >> mMin[2];

		metaFileReader >> mMax[0];
		metaFileReader >> mMax[1];
		metaFileReader >> mMax[2];

		metaFileReader.close();
	} else {

		std::cout << "No metafile for " << filePath << " found. Creating it..." << std::flush;
		updateMetaFile();
		std::cout << "finished." << std::endl << std::flush;
	}
	//########## END Try to read metafile. If it doesn't exist, create one ##########
}

SimpleTextPointCloud::~SimpleTextPointCloud() {
}

void SimpleTextPointCloud::updateMetaFile() {

	// TODO 3: Create function that returns metadata file path
	std::string metaFilePath = mFilePath + ".vostokmeta";

	//######### BEGIN Reset metadata members ##########
	mNumPoints = 0;
	std::vector<double> min(mFieldNames.size(), std::numeric_limits<double>::max());
	std::vector<double> max(mFieldNames.size(), std::numeric_limits<double>::min());
	//######### END Reset metadata members ##########

	std::ifstream sf(mFilePath.c_str());

	while (!sf.eof()) {
		std::string line;

		// NOTE: Replaced getline with safeGetline from "myUtils.h" as an attempt to solve the file reading problem
		safeGetline(sf, line);

		//getline(sf, line);

		std::vector<std::string> split = splitString(line, ' ');

		if (split.size() >= mFieldNames.size()) {
			// Read point and update min/max:
			for (unsigned int ii = 0; ii < mFieldNames.size(); ++ii) {

				std::string::size_type sz;     // alias of size_t

				double tmp = std::stod(split[ii], &sz);

				if (tmp < min[ii]) {
					min[ii] = tmp;
				}

				if (tmp > max[ii]) {
					max[ii] = tmp;
				}
			}

			++mNumPoints;
		} else {
			//std::cout << "Invalid line" << std::endl;
		}
	}

	sf.close();

	mMin = min;
	mMax = max;

	// Write metadata file:
	std::ofstream metaFileWriter(metaFilePath.c_str());
	metaFileWriter << mNumPoints << " " << min[0] << " " << min[1] << " " << min[2] << " " << max[0] << " " << max[1] << " " << max[2];
	metaFileWriter.close();
}

std::vector<double> SimpleTextPointCloud::getNextPoint() {

	std::vector<double> result(mFieldNames.size(), 0);

	if (mSourceFile->eof()) {
		std::cout << "ERROR reading point: end of file reached";
		return result;
	}

	std::string line;

	// NOTE: Replaced getline with safeGetline from "myUtils.h" as an attempt to solve the file reading problem
	safeGetline(*mSourceFile, line);
	
	//getline(*mSourceFile, line);

	++mLineNumber;

	std::vector<std::string> split = splitString(line, ' ');

	if (split.size() >= mFieldNames.size()) {

		// Read point and update min/max:
		for (unsigned int ii = 0; ii < mFieldNames.size(); ++ii) {

			std::string::size_type sz;     // alias of size_t

			double tmp = std::stod(split[ii], &sz);

			result[ii] = tmp;
		}

	} else {
		std::cout << "ERROR: Invalid line " << mLineNumber << std::endl;
		std::cout << line << std::endl;
		std::cout << split.size() << " " << mFieldNames.size() << std::endl;
	}

	++mCursorPos;

	return result;
}

unsigned int SimpleTextPointCloud::getNumPoints() {
	return mNumPoints;
}

void SimpleTextPointCloud::resetCursor() {

	mLineNumber = 0;
	mCursorPos = 0;
	mSourceFile->seekg(0, mSourceFile->beg);
}


bool SimpleTextPointCloud::endOfCloud() {
	return mCursorPos >= mNumPoints;
}

bool SimpleTextPointCloud::sourceFileGood() {
	return mSourceFileGood;
}
