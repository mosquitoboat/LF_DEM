/**
 \class Simulation
 \brief Class launching the simulation by setting up the System class and performing predefined shear evolution
 \author Ryohei Seto
 \author Romain Mari
 */
//
//  Simulation.h
//  LF_DEM
//
//  Created by Ryohei Seto and Romain Mari on 11/15/12.
//  Copyright (c) 2012 Ryohei Seto and Romain Mari. All rights reserved.
//
#ifndef __LF_DEM__Simulation__
#define __LF_DEM__Simulation__
#include <iostream>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <ctime>
#include <map>
#include <algorithm>
#include "System.h"
#include "ParameterSet.h"

inline void removeBlank(string &str)
{
	str.erase(std::remove_if(str.begin(), str.end(), (int(*)(int))isspace), str.end());
}

inline bool getSuffix(const string &str, string &value, string &suffix){
	std::size_t suffix_pos = str.find_first_of("abcdefghijklmnopqrstuvwxyz");
	value = str.substr(0, suffix_pos);
	suffix = str.substr(suffix_pos, str.length());
	if (suffix.empty()) {
		return false;
	}
	return true;
}

inline void errorNoSuffix(string quantity){
	cerr << "Error : no unit scale (suffix) provided for " << quantity << endl; exit(1);
}

	
class Simulation{
private:
	System sys;
	ParameterSet p;
	std::map <string, string> suffixes;   // pairs: (force_type, suffix)
	std::map <string, double> values;   // pairs: (force_type, values_in_suffix_units)
	std::map <string, double> dimensionless_numbers; // pairs: (force_type, rate/force_value)
	
	double volume_or_area_fraction;
	string filename_import_positions;
	string filename_parameters;
	string filename_sequence;
	ostringstream string_control_parameters;
	string import_line[2];
	string control_var;
	bool user_sequence;
	double shear_rate_expectation;
	double time_interval_output_data;
	double time_interval_output_config;
	
	/*
	 * Resultant data
	 */
	double viscosity;
	double normalstress_diff_1;
	double normalstress_diff_2;
	double particle_pressure;
	double viscosity_hydro; // Only lubrication...
	double normalstress_diff_1_hydro;
	double normalstress_diff_2_hydro;
	double viscosity_cont_XF;
	double normalstress_diff_1_cont_XF;
	double normalstress_diff_2_cont_XF;
	double particle_pressure_cont;
	double viscosity_friction; // Fc_tan contribution.
	double normalstress_diff_1_friction;
	double normalstress_diff_2_friction;
	double viscosity_cont_GU;
	double normalstress_diff_1_cont_GU;
	double normalstress_diff_2_cont_GU;
	double viscosity_repulsive_XF;
	double normalstress_diff_1_repulsive_XF;
	double normalstress_diff_2_repulsive_XF;
	double particle_pressure_repulsive;
	double viscosity_repulsive_GU;
	double normalstress_diff_1_repulsive_GU;
	double normalstress_diff_2_repulsive_GU;
	double viscosity_brownian;
	double normalstress_diff_1_brownian;
	double normalstress_diff_2_brownian;
	double initial_lees_edwards_disp;
	string unit_scales;
	int time_strain_0;
	int time_strain_1;
	int time_strain_end;
	int timestep_1;
	int timestep_end;
	/*
	 * For output data.
	 */
	ofstream fout_rheo;
	ofstream fout_particle;
	ofstream fout_interaction;
	ofstream fout_st;
	ofstream fout_time;
	ofstream fout_input;
	/*
	 * For inputs
	 */
	void setDefaultParameters();
	void readParameterFile();
	void openOutputFiles(bool);
	void prepareSimulationName(bool);
	void echoInputFiles(string in_args, vector<string> &input_files);
	void autoSetParameters(const string &keyword, const string &value);
	void importInitialPositionFile();
	void contactForceParameter(string filename);
	void contactForceParameterBrownian(string filename);
	void importPreSimulationData(string filename);
	void importConfigurationBinary();
	//	void setUnitScalesBrownian(double dimensionlessnumber);
	//	void setUnitScalesNonBrownianRate(double dimensionlessnumber);
	void setUnitScalesNonBrownianStress(double dimensionlessnumber);
	void determineDimensionlessNumbers(double dimensionlessnumber, string rate_unit);
	void setLowPeclet();
	void convertForceValues();
	void setUnitScale();
	/*
	 * For outputs
	 */
	void evaluateData();
	void outputDataHeader(ofstream &fout);
	void outputRheologyData();
	void outputStressTensorData();
	void outputConfigurationData();
	void outputFinalConfiguration();
	void outputConfigurationBinary();
	void outputConfigurationBinary(string);
	vec3d shiftUpCoordinate(double x, double y, double z);
	void setupSimulationSteadyShear(string in_args,
									vector<string> &input_files,
									bool binary_conf,
									double dimensionlessnumber,
									string input_scale,
									string control_variable);
//	void exportParameterSet();
	void outputComputationTime();
	
public:
	/* For DEMsystem*/
	Simulation();
	~Simulation();
	void simulationSteadyShear(string in_args, vector<string> &input_files, bool binary_conf,
							   double dimensionless_number, string input_scale, string control_variable);
	void simulationUserDefinedSequence(string seq_type, string in_args, vector<string> &input_files, bool binary_conf, string control_variable);
};
#endif /* defined(__LF_DEM__Simulation__) */
