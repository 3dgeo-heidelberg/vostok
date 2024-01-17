# VOSTOK - The Voxel Octree Solar Toolkit
VOSTOK is a command-line tool to compute a detailed model of incoming solar radiation distribution on a patch of land, including structures like buildings and vegetation, represented by a 3D point cloud data set. "Vostok" is also the Russian word for "east" - the direction in which the sun rises.

## General

The program is written in C++ and makes use of the "SOLPOS.H" library to compute the angular position of the sun in the sky for a given location on Earth and a given moment in time. SOLPOS.H was created by the U.S. Department of Energy National Renewable Energy Laboratory ( http://rredc.nrel.gov/solar/codesandalgorithms/solpos/ ) and released under the public domain.

VOSTOK can use any 3D point cloud (e.g. LIDAR, SfM) as XYZ-locations (with surface normal vector information) for which the incoming sunlight shall be computed (*the computation points*). VOSTOK works by transforming a second input point cloud (*the shadow points*) into a voxel volume with configurable resolution (voxel size). The computation and shadow points can be the same point cloud. The occupied voxels determine the shadow casting over time for each computation point. The voxels are represented as a sparse octree, and incoming sunlight and shadowing effects are simulated by raycasting on the voxel octree geometry. Transmission of voxels is currently not considered. Raycasting is parallelized with OpenMP to make full use of multi-core processors.

The current version of VOSTOK assumes clear sky conditions in the calculation of solar irradiance, cloud coverage is not considered. To account for overcast conditions, a correction of the modelled absolute values computed by VOSTOK needs to be done by using measurements at meteorological stations to derive the ratio between overcast and clear sky values at the specific geographic location (e.g. [Suri & Hofierka 2004](https://doi.org/10.1111/j.1467-9671.2004.00174.x)). This must be done individually for each study area.
	
The Linke atmospheric turbidity coefficient, which models the atmospheric absorption and scattering of solar radiation under clear sky, is currently hardcoded and [set to a fixed value of 3](https://github.com/GIScience/vostok/blob/803dcb7200942cc69d755a43d360673eb13290c5/src/IrradianceCalc.cpp#L37). This value is near the annual average for rural-city areas in Europe, i.e. mild climate in the Northern hemisphere (cf. https://grass.osgeo.org/grass77/manuals/r.sun.html). The [factor must be adapted in the source code](https://github.com/GIScience/vostok/blob/803dcb7200942cc69d755a43d360673eb13290c5/src/IrradianceCalc.cpp#L37) for other study areas, see reference literature.

## Using VOSTOK
### Precompiled 
Pre-compiled binaries (64 bit) for Windows can be downloaded [here](https://heibox.uni-heidelberg.de/f/f195fdc4629a41d29403/).<br>
For faster simulations compiling VOSTOK on your specific system is recommended.

### Building VOSTOK
In order to build VOSTOK, you need to have the GNU C++ compiler (g++), CMake (cmake) and any build system supported by CMake (e.g. GNU make, see [all supported generators](https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html)) installed on your computer. Optionally, VOSTOK may take advantage of OpenMP installed in your system.

To build VOSTOK, create a build directory (CMake prefers out-of-source builds to not clutter up your sources with build artifacts), navigate to it and invoke cmake:

```bash
git clone https://github.com/GIScience/vostok
mkdir vostok_build && cd vostok_build
cmake ../vostok
cmake --build .
```

The resulting executable `vostok` resides in your build directory. See [all supported CMake generators](https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html), if you want CMake to generate project files for your favourite IDE (e.g. `cmake -G"Visual Studio 12 2013 Win64" ../vostok`).

## Running VOSTOK

To run VOSTOK, you need to provide a .sol file containing information about the input files, the location of the scene, the time period for which the solar potential shall be calculated, etc.

An `example.sol` file:

    example.xyz
    x y z nx ny nz
    example.xyz
    x y z nx ny nz
    1.0
    48.0
    16.0
    1
    2016
    1
    31
    1
    60
    1
    example_solPot_January2016.xyz
    1
    30.0

The meaning of each line entry is as follows:

    line 1	File with points used for shadowing
    line 2	Line format of shadow point cloud input file
    line 3	Point cloud for solar pot. calculation
    line 4	Line format of query point cloud input file
    line 5	Voxel size for shadow voxels [m]
    line 6	Latitude of scene in decimal degrees
    line 7	Longitude of scene in decimal degrees
    line 8	Time zone (according to SOLPOS timezone: https://www.nrel.gov/grid/solar-resource/solpos.html)
    line 9	Year of calculation
    line 10	First day of calculation
    line 11	Last day of calculation
    line 12	Day step [days]
    line 13	Minutes step [min]
    line 14	Enable shadowing (0=ignore shadows, 1=compute shadows, 2=output shadow clouds)
    line 15	Output file name
    line 16	Enable multi threading (0=single-threading, 1=enable multi-threading)
    line 17	Minimum threshold for Solar elevation angle (refracted), degrees from horizon (optional)

The first input file (line 1) contains the points which are used for shadowing the scene.

The second file (line 3) corresponds to the points for which the solar potential is calculated. The file must contain xyz coordinates and normals nxnynz, each in subsequent columns separated by blanks.

Please not the following restrictions regarding the shadow and query point cloud input file formats:

1. Although you can specify the line formats of the input files, the indices of the individual values for point position and normal vector are still *fixed* in the current version of VOSTOK. This means that *the first three values (position 1 to 3) will always be interpreted as POINT POSITION and the following three values (position 4 to 6) will always be interpreted as the NORMAL VECTOR*.

2. The query point cloud *MUST* have a normal vector specified for each point. For the shadow point cloud, normals are not required.

`example.xyz` file with four points:

    78.750000 344.250000 40.615002 0.114376 0.220988 0.968547
    79.250000 344.250000 40.542000 0.063874 0.249720 0.966209
    79.750000 344.250000 40.520000 -0.196111 -0.040646 0.979739
    80.250000 344.250000 40.956001 -0.187723 -0.375608 0.907568

The threshold on Solar elevation angle as degrees from horizon (line 17) is an additional parameter (SOLPOS variable [elevref](https://www.nrel.gov/grid/solar-resource/solpos.html)), which can be useful if the extent of the shadow point cloud is limited closely to the extent of the computation points. In this case, the sun illuminates from the side (i.e. from a low angle) at the edge of the shadow data and can cause illumination in areas which in reality would be shadowed by their surroundings. The effect occurs for forest plot data, but may be applicable elsewhere, too. If not specified, the parameter value is set below the horizon and thereby disregarded (default value: -99.9).

If the .sol file is adjusted and the input files are provided, run VOSTOK via

    vostok.exe example.sol

The tool will first generate a .vostokmeta file and then run the solar potential calculation. On your screen, somethin like the following messages should appear:

    ------------------------------------------------------
    Shadow points file path:      example.xyz
    Shadow points input format:   x y z nx ny nz
    Query points file path:       example.xyz
    Query points input format:    x y z nx ny nz
    Output file(s) path:          example_solPot_January2016.xyz
    Use Multithreading:           1

    Lat:                          48 degrees
    Lon:                          16 degrees
    Time Zone:                    1

    Year:                         2016
    Day Start:                    1
    Day End:                      31
    Days step:                    1
    Minutes step:                 60

    Shadow mode:                  1
    Shadow voxel size:            1 m
    Min. solar elevation angle:   30.0 degrees
    ------------------------------------------------------


    No metafile for example.xyz found. Creating it...finished.

    Point cloud size:            270.5 x 170 x 68.084
    Required octree volume size: 512 x 512 x 512
    Required octree depth:       9

    Building octree... finished.

    Computing irradiation for each query point...

    Day   1   Sunrise: 07:51   Sunset:  16:06
    Day   2   Sunrise: 07:51   Sunset:  16:07
    Day   3   Sunrise: 07:51   Sunset:  16:08
    Day   4   Sunrise: 07:51   Sunset:  16:09
    Day   5   Sunrise: 07:51   Sunset:  16:10
    Day   6   Sunrise: 07:51   Sunset:  16:11
    Day   7   Sunrise: 07:51   Sunset:  16:13
    Day   8   Sunrise: 07:50   Sunset:  16:14
    Day   9   Sunrise: 07:50   Sunset:  16:15
    Day  10   Sunrise: 07:49   Sunset:  16:16
    Day  11   Sunrise: 07:49   Sunset:  16:17
    Day  12   Sunrise: 07:48   Sunset:  16:19
    Day  13   Sunrise: 07:48   Sunset:  16:20
    Day  14   Sunrise: 07:47   Sunset:  16:21
    Day  15   Sunrise: 07:47   Sunset:  16:23
    Day  16   Sunrise: 07:46   Sunset:  16:24
    Day  17   Sunrise: 07:45   Sunset:  16:26
    Day  18   Sunrise: 07:44   Sunset:  16:27
    Day  19   Sunrise: 07:44   Sunset:  16:28
    Day  20   Sunrise: 07:43   Sunset:  16:30
    Day  21   Sunrise: 07:42   Sunset:  16:31
    Day  22   Sunrise: 07:41   Sunset:  16:33
    Day  23   Sunrise: 07:40   Sunset:  16:34
    Day  24   Sunrise: 07:39   Sunset:  16:36
    Day  25   Sunrise: 07:38   Sunset:  16:38
    Day  26   Sunrise: 07:37   Sunset:  16:39
    Day  27   Sunrise: 07:36   Sunset:  16:41
    Day  28   Sunrise: 07:34   Sunset:  16:42
    Day  29   Sunrise: 07:33   Sunset:  16:44
    Day  30   Sunrise: 07:32   Sunset:  16:45
    Day  31   Sunrise: 07:31   Sunset:  16:47

The resulting file will contain the initial xyz coordinates, the nxnynz normals, and a new column with the calculated solar potential in Watt hours per square meter and day, summed up for the respective point.

## Authorship

3DGeo Research Group  
Institute of Geography  
Heidelberg University

[http://www.uni-heidelberg.de/3dgeo](http://www.uni-heidelberg.de/3dgeo)

Source code repository: [https://github.com/3dgeo-heidelberg/vostok](https://github.com/3dgeo-heidelberg/vostok)

### How to cite

Bechtold, S. & Höfle, B. (2020): VOSTOK - The Voxel Octree Solar Toolkit. heiDATA, V1. DOI: [10.11588/data/QNA02B](https://doi.org/10.11588/data/QNA02B).

If you use VOSTOK in your work, please cite:

```
@data{data/QNA02B,
author = {Bechtold, Sebastian and H\"ofle, Bernhard},
publisher = {heiDATA},
title = "{VOSTOK - The Voxel Octree Solar Toolkit}",
year = {2020},
version = {V1},
doi = {10.11588/data/QNA02B},
url = {https://doi.org/10.11588/data/QNA02B}
}
```

We are happy if you are using VOSTOK in your work - [let us know](https://www.uni-heidelberg.de/3dgeo)!

## Videos on 3D Solar Potential Analysis using VOSTOK
* [Simulation of sunshine in forests using dense 3D point clouds](https://youtu.be/l-_EoMN5If8)
* [3D Solar Potential computed in 3D Point Cloud with VOSTOK](https://youtu.be/4vqcFjbzJ8Q)
* [3D solar potential assessment for renewable energy supply (with subtitles)](https://youtu.be/VAiFMgNcvxM)
* [3D solar potential assessment for renewable energy supply](https://youtu.be/TaCUaG5XEas)

## Relevant Publications
Foshag, K., Aeschbach, N., Höfle, B., Winkler, R., Siegmund, A. & Aeschbach, W. (2020): Viability of public spaces in cities under increasing heat: A transdisciplinary approach. Sustainable Cities and Society. Vol. 59, pp. 102215. DOI: [10.1016/10.1016/j.scs.2020.102215](https://doi.org/10.1016/j.scs.2020.102215).

Lin, T.-P., Lin, F.-Y., Wu, P.-R., Hämmerle, M., Höfle, B., Bechtold, S., Hwang, R.-L. & Chen, Y.-C. (2017): Multiscale Analysis and Reduction Measures of Urban Carbon Dioxide Budget Based on Building Energy Consumption. Energy and Buildings. Vol. 153, pp. 356-367. DOI: [10.1016/j.enbuild.2017.07.084](https://doi.org/10.1016/j.enbuild.2017.07.084).

Gündra, H., Barron, C., Henrichs, T., Jäger, S., Höfle, B., Marx, S., Peters, R., Reimer, A. & Zipf, A. (2015): [Standortkataster für Lärmschutzanlagen mit Ertragsprognose für potenzielle Photovoltaik-Anwendungen](https://www.geog.uni-heidelberg.de/md/chemgeo/geog/3dgeo/guendra_et_al_2015_bast_v252.pdf). Berichte der Bundesanstalt für Straßenwesen (BASt), Heft V 252, pp. 1-48. ISBN: 978-3-95606-150-9.

Regvat, R., Hämmerle, M., Marx, S., Koenig, K. & Höfle, B. (2014): [3D-punktbasierte Solarpotenzialanalyse für Gebäudefassaden mit freien Geodaten](https://www.geog.uni-heidelberg.de/md/chemgeo/geog/3dgeo/regvat_et_al_2014_agit.pdf). In: Strobl, J., Blaschke, T., Griesebner, G. & Zagel, B. Angewandte Geoinformatik 2014, pp. 196-204. Wichmann.

Höfle, B. (2012): [Nachhaltige Stromerzeugung - Geoinformationen optimieren Solaranlagen](https://www.geog.uni-heidelberg.de/md/chemgeo/geog/3dgeo/hoefle_2012_ruperto_carola.pdf). In: Ruperto Carola Forschungsmagazin. Vol. 1/2012, pp. 44-46.

Jochem, A., Höfle, B. & Rutzinger, M. (2011): Extraction of Vertical Walls from Mobile Laser Scanning Data for Solar Potential Assessment. Remote Sensing. Vol. 3 (4), pp. 650-667. DOI: [10.3390/rs3030650](https://doi.org/10.3390/rs3030650).

Jochem, A., Höfle, B., Rutzinger, M. & Pfeifer, N. (2009): Automatic roof plane detection and analysis in airborne LIDAR point clouds for solar potential assessment. Sensors. Vol. 9 (7), pp. 5241-5262. DOI: [10.3390/s90705241](https://doi.org/10.3390/s90705241).

Jochem, A., Höfle, B., Hollaus, M. & Rutzinger, M. (2009): [Object detection in airborne LIDAR data for improved solar radiation modeling in urban areas](https://www.geog.uni-heidelberg.de/md/chemgeo/geog/3dgeo/jochem_et_al_iaprs_paris_2009.pdf). In: International Archives of Photogrammetry, Remote Sensing and Spatial Information Sciences. Vol. XXXVIII(Part 3/W8), pp. 1-6.

## License
See [LICENSE.md](LICENSE.md).
