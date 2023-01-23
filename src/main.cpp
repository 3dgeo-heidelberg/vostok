// TODO 4: Re-calculate SOLPOS results if a point is too far away from the last point for which they were computed

#include <cstdlib>
#include <ctime>
// NOTE: 'iomanip' is required for 'std::setprecision()':
#include <iomanip>
#include <iostream>
#include <sstream>
#include <omp.h>

#if __cplusplus < 201703L // If the version of C++ is less than 17
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include "pointCloud/SimpleTextPointCloud.h"
#include "IrradianceCalc.h"
#include "ShadowCalc.h"
#include "ProjectConfig.h"

int main(int argc, char* argv[]) {

	if (argc < 2) {
		std::cout << "ERROR: Please specify a .sol project file." << std::endl;
		return -1;
	}

	//######### BEGIN Load project settings #########
	std::string projectFilePath;

	projectFilePath = argv[1];

	ProjectConfig cfg;
	bool success = cfg.loadFromFile(projectFilePath);

	if (!success) {
		std::cout << "ERROR: Failed to load project settings file." << std::endl;
		return -1;
	}

	std::cout << cfg.getSettingsString() << std::endl << std::endl;

	//######### END Load project settings #########

	//###################### BEGIN Load query and shadow point clouds #######################
	//SimpleTextPointCloud queryPoints = SimpleTextPointCloud(cfg.mQueryPointsFilePath, "x y z nx ny nz");
	SimpleTextPointCloud queryPoints = SimpleTextPointCloud(cfg.mQueryPointsFilePath, cfg.mQueryPointsInputFormat);

	if (!queryPoints.sourceFileGood()) {
		std::cout << "ERROR: Failed to open query point cloud" << std::endl;
		return -1;
	}

	//SimpleTextPointCloud shadowPoints = SimpleTextPointCloud(cfg.mShadowPointsFilePath, "x y z nx ny nz");
	SimpleTextPointCloud shadowPoints = SimpleTextPointCloud(cfg.mShadowPointsFilePath, cfg.mShadowPointsInputFormat);

	if (!shadowPoints.sourceFileGood()) {
		std::cout << "ERROR: Failed to open shadow point cloud" << std::endl;
		return -1;
	}
	//###################### END Load query and shadow point clouds #########################

	//##################### BEGIN Initial configuration of solpos input struct (time will be changed later) ##################
	posdata solpos;

	S_init(&solpos);

	// Enable "month/day" date input mode (instead of "n-th day of year"):
	//solposData.function &= ~S_DOY;

	solpos.latitude = cfg.m_lat;
	solpos.longitude = cfg.m_lon;
	solpos.timezone = cfg.m_timezone;

	//##################### END Initial configuration of solpos input struct (time will be changed later) ##################

	//#################################### BEGIN Compute irradiation ####################################

	std::cout << std::endl;

	//########### BEGIN Load all query points into cache for parallelization #############
	std::vector<std::vector<double> > queryPointsCache;

	queryPoints.resetCursor();

	while (!queryPoints.endOfCloud()) {
		queryPointsCache.push_back(queryPoints.getNextPoint());
	}
	//########### END Load all query points into cache for parallelization #############

	ShadowCalc shadowCalc(shadowPoints, cfg.mVoxelSize);

	//Create subdirectory for shadow point clouds
	if (cfg.m_computeShadows > 1) {
		fs::create_directory("shadow_clouds");
		std::cout << "Single shadow point clouds are written to sub-directory: shadow_cloud" << std::endl;
	}

	std::cout << "Computing irradiation for each query point..." << std::endl << std::flush;

	// Prepare vector for irradiation for all points over entire measurement time:
	std::vector<double> totalIrradiation;

	clock_t begin = clock();

	//###### BEGIN DayStep loop #######

	for (int day = cfg.m_dayStart; day <= cfg.m_dayEnd; day += cfg.m_dayStep) {

		//######### BEGIN Get sunset & sunrise time for the current day using SOLPOS ##########
		solpos.year = cfg.m_year;
		solpos.daynum = day;
		solpos.hour = 12;
		solpos.minute = 0;
		solpos.second = 0;

		// Compute sunset and sunrise minutes for the current day & print error message if something is wrong:
		S_decode(S_solpos(&solpos), &solpos);

		int sunriseMinute = solpos.sretr;
		int sunsetMinute = solpos.ssetr;

		std::cout << std::endl << "Day " << std::setw(3) << day;

		std::cout << std::setfill('0');
		std::cout << "   Sunrise: " << std::setw(2) << (sunriseMinute / 60) << ":" << std::setw(2) << (sunriseMinute - (sunriseMinute / 60) * 60);
		std::cout << "   Sunset:  " << std::setw(2) << (sunsetMinute / 60) << ":" << std::setw(2) << (sunsetMinute - (sunsetMinute / 60) * 60);

		std::cout << std::setfill(' ');

		//######### END Get sunset & sunrise time for the current day using SOLPOS ##########

		//########### BEGIN Minutes step loop #########
		for (int currentMinute = sunriseMinute; currentMinute < sunsetMinute; currentMinute += cfg.m_minuteStep) {

			//########## BEGIN Get sun position for current time using SOLPOS ###########
			solpos.hour = currentMinute / 60;
			solpos.minute = currentMinute - solpos.hour * 60;
			solpos.second = 0;

			// Compute sun position and other values for current time:
			S_decode(S_solpos(&solpos), &solpos);
			//########## END Get sun position for current time using SOLPOS ###########

			//######## BEGIN Compute irradiation for the current minute step for each query point #########

			std::ostringstream shadowOutfilePath;
			std::ofstream shadowOutfile;
			if (cfg.m_computeShadows > 1) {
				// Construct shadow output file name:
				shadowOutfilePath << std::setfill('0');
				shadowOutfilePath << ".\\shadow_clouds\\" << solpos.year << "_" << std::setw(3) << solpos.daynum << "_" << std::setw(2) << solpos.hour << "-" << std::setw(2) << solpos.minute << "_shadow.txt";

				shadowOutfile.open(shadowOutfilePath.str().c_str());
				shadowOutfile << std::setprecision(3);
			}
			IrradianceCalc irrCalc(solpos);

			// Prepare vector for irradiation for all points over current minute step:
			std::vector<double> minuteStepIrradiance;

			// TODO: Simplify this by unifying OpenMP and Non-OpenMP versions. 
			// The only real difference could be the OpenMP pragma after all.
			// sbecht 2018-11-01
			
			if (cfg.m_openMp == 1 && cfg.m_computeShadows < 2) {

				//##################### BEGIN OpenMP-parallelized version ######################
				minuteStepIrradiance.resize(queryPointsCache.size(), 0);

				#pragma omp parallel for
				for (int ii = 0; ii < queryPointsCache.size(); ii++) {
					std::vector<double>& p = queryPointsCache[ii];

					bool illuminated = true;
					double elevation_deg = solpos.elevref;

					//std::cout << elevation_deg << " " << solpos.hour << std::endl;

					//check solar angle first
					if (elevation_deg >= cfg.m_minSunAngle)
					{

						if (cfg.m_computeShadows > 0) {
							illuminated = shadowCalc.computeShadow(solpos, p);
						}
					}
					else { 
						illuminated = false;
						//std::cout << "Sun too low: " << solpos.elevref << " < " << cfg.m_minSunAngle << std::endl;
					}


					// Compute irradiance for current time step (energy for moment with no time unit):
					double irr = irrCalc.getIrradiance(p, illuminated);

					minuteStepIrradiance[ii] = irr;
				}
			}
			//##################### END OpenMP-parallelized version ######################
			else {
				//##################### BEGIN Single-threaded version ######################

				queryPoints.resetCursor();

				while (!queryPoints.endOfCloud()) {

					std::vector<double> p = queryPoints.getNextPoint();


					bool illuminated = true;
					double elevation_deg = solpos.elevref;
					if (elevation_deg >= cfg.m_minSunAngle)
					{

						if (cfg.m_computeShadows > 0) {
							illuminated = shadowCalc.computeShadow(solpos, p);
						}
					}
					else {
						illuminated = false;
					}

					if (cfg.m_computeShadows > 1) {

						shadowOutfile << std::fixed << p[0] << " " << std::fixed << p[1] << " " << std::fixed << p[2] << " " << illuminated << std::endl;
					}

					// Compute irradiance for current time step (energy for moment with no time unit):
					double irr = irrCalc.getIrradiance(p, illuminated);

					minuteStepIrradiance.push_back(irr);

				}
				//##################### END Single-threaded version ######################
			}

			if (cfg.m_computeShadows > 1) {
				// Close shadow output file:
				shadowOutfile.close();
			}
			//########## END Compute irradiation for the current minute step for each query point ##########

			//######### BEGIN Get number of sunny minutes in current time step in order to compute irradiation ###########
			int sunStart = currentMinute;

			// If current minute step begins before sunrise, move begin to sunrise:
			if (sunStart < solpos.sretr) {
				sunStart = solpos.sretr;
			}

			int sunEnd = currentMinute + cfg.m_minuteStep;

			// If current minute step ends after sunset, move end to sunset:
			if (sunEnd > solpos.ssetr) {
				sunEnd = solpos.ssetr;
			}

			float sunnyHoursInCurrentMinuteStep = (float) (sunEnd - sunStart) / 60;

			if (sunnyHoursInCurrentMinuteStep < 0) {
				std::cout << "WARNING: Negative # of sunny hours!" << std::endl;

				sunnyHoursInCurrentMinuteStep = 0;
			}
			//######### END Get number of sunny minutes in current time step in order to compute irradiation ###########

			// Resize total irradiation vector if necessary:
			if (totalIrradiation.size() != minuteStepIrradiance.size()) {
				totalIrradiation.resize(minuteStepIrradiance.size(), 0);
			}

			//########### BEGIN Sum up minute step irradiation values ############
			for (unsigned int ii = 0; ii < minuteStepIrradiance.size(); ++ii) {

				// ATTENTION:
				// We multiply the irradiation value with cfg_dayStep to take into account the time until the next dayStep.
				totalIrradiation[ii] += minuteStepIrradiance[ii] * sunnyHoursInCurrentMinuteStep * cfg.m_dayStep;
			}
			//########### END Sum up minute step irradiation values ############
		}

		//########### END minuteStep loop #########
	}

	//###### END DayStep loop #######

	clock_t end = clock();
	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

	std::cout << std::endl << std::endl;
	std::cout << "Finished in " << elapsed_secs << " seconds." << std::endl << std::endl;

//#################################### END Compute irradiation ####################################

//##################################### BEGIN Write output file ##################################
	std::ofstream outfile;
	outfile.open(cfg.mOutputFilePath.c_str());

	outfile << std::setprecision(3);

	queryPoints.resetCursor();

	int index = 0;
	while (!queryPoints.endOfCloud()) {

		std::vector<double> p = queryPoints.getNextPoint();

		for (int ii = 0; ii < p.size(); ii++) {
			outfile << std::fixed << p[ii] << " ";
		}
	
		// Replaced by loop above:
		//outfile << std::fixed << p[0] << " " << std::fixed << p[1] << " " << std::fixed << p[2] << " " << std::fixed << p[3] << " " << std::fixed << p[4] << " " << std::fixed << p[5] << " ";
		
		outfile << std::fixed << totalIrradiation[index] << std::endl;
		
		++index;
	}

	outfile.close();
//##################################### END Write output file ##################################
}
