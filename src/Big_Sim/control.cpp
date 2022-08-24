#include <iomanip>
#include <gtk/gtk.h>
#include "control.h"
#include "fileIO/build_file.h"
#include "ttyManip/tty.h"
#include "array_util.h"
#include "gui.h" /* tenuous inclide at best :pogO: */

const std::string BIN_EXT = "bin";

// private utility function. TODO: move to a better place
std::string getFileBasename(std::string fullFilePath)
{
	size_t sep = fullFilePath.find_last_of("\\/");
	if (sep != std::string::npos)
	    fullFilePath = fullFilePath.substr(sep + 1, fullFilePath.size() - sep - 1);
	
	size_t dot = fullFilePath.find_last_of(".");
	if (dot != std::string::npos)
	{
	    std::string name = fullFilePath.substr(0, dot);
	}
	else
	{
	    std::string name = fullFilePath;
	}
	return (dot != std::string::npos) ? fullFilePath.substr(0, dot) : fullFilePath;
}

Control::Control(enum vis_mode sim_vis_mode)
{
	this->sim_vis_mode = sim_vis_mode;
}

Control::Control(parsed_build_file &p_file)
{
	if (!con_params_populated) populate_con_params(p_file);
	if (!act_params_populated) populate_act_params(p_file);
	if (!simState) simState = new CBMState(numMZones);
	if (!simCore) simCore = new CBMSimCore(simState, gpuIndex, gpuP2);
	if (!mfFreq)
	{
		mfFreq = new ECMFPopulation(num_mf, mfRandSeed, CSTonicMFFrac, CSPhasicMFFrac,
			  contextMFFrac, nucCollFrac, bgFreqMin, csbgFreqMin, contextFreqMin, 
			  tonicFreqMin, phasicFreqMin, bgFreqMax, csbgFreqMax, contextFreqMax, 
			  tonicFreqMax, phasicFreqMax, collaterals_off, fracImport, secondCS, fracOverlap);
	}
	if (!mfs)
	{
		mfs = new PoissonRegenCells(num_mf, mfRandSeed, threshDecayTau, msPerTimeStep,
			  	numMZones, num_nc);
	}
	if (!output_arrays_initialized) initializeOutputArrays();
	if (!spike_sums_initialized) initialize_spike_sums();
}

Control::Control(char ***argv, enum vis_mode sim_vis_mode)
{
	parse_experiment_args(argv, expt); 
	std::string in_sim_filename = "";
	get_in_sim_file(argv, in_sim_filename);
	std::fstream sim_file_buf(in_sim_filename.c_str(), std::ios::in | std::ios::binary);

	if (!con_params_populated) read_con_params(sim_file_buf);
	if (!act_params_populated) read_act_params(sim_file_buf);
	if (!simState) simState = new CBMState(numMZones, sim_file_buf);
	if (!simCore) simCore = new CBMSimCore(simState, gpuIndex, gpuP2);
	if (!mfFreq)
	{
		mfFreq = new ECMFPopulation(num_mf, mfRandSeed,
			  CSTonicMFFrac, CSPhasicMFFrac, contextMFFrac, nucCollFrac,
			  bgFreqMin, csbgFreqMin, contextFreqMin, tonicFreqMin, phasicFreqMin, bgFreqMax,
			  csbgFreqMax, contextFreqMax, tonicFreqMax, phasicFreqMax, collaterals_off,
			  fracImport, secondCS, fracOverlap);
	}
	if (!mfs)
	{
		mfs = new PoissonRegenCells(num_mf, mfRandSeed,
				threshDecayTau, msPerTimeStep, numMZones, num_nc);
	}
	if (!output_arrays_initialized) initializeOutputArrays();
	if (!spike_sums_initialized) initialize_spike_sums();
	sim_file_buf.close();

	this->sim_vis_mode = sim_vis_mode;
}

Control::~Control()
{
	// delete all dynamic objects
	if (simState) delete simState;
	if (simCore)  delete simCore;
	if (mfFreq)   delete mfFreq;
	if (mfs)      delete mfs;

	// deallocate output arrays
	if (output_arrays_initialized) deleteOutputArrays();
	if (spike_sums_initialized) delete_spike_sums();
}

void Control::build_sim(parsed_build_file &p_file)
{
	// not sure if we want to save mfFreq and mfs in the simulation file
	if (!(con_params_populated && act_params_populated && simState))
	{
		populate_con_params(p_file);
		populate_act_params(p_file);
		simState = new CBMState(numMZones);
	}
}

void Control::init_sim_state(std::string stateFile)
{
	if (!con_params_populated)
	{
		fprintf(stderr, "[ERROR]: Trying to initialize state without first connectivity params.\n");
		fprintf(stderr, "[ERROR]: (Hint: Load a connectivity parameter file first then load the state.\n");
		return;
	}
	if (!act_params_populated)
	{
		fprintf(stderr, "[ERROR]: Trying to initialize state without first initializing activity params.\n");
		fprintf(stderr, "[ERROR]: (Hint: Load an activity parameter file first then load the state.\n");
		return;
	}
	if (!simState) simState = new CBMState(numMZones, stateFile);
	else fprintf(stderr, "[ERROR]: State already initialized.\n");
}

void Control::init_experiment(std::string in_expt_filename)
{
	std::cout << "[INFO]: Loading Experiment file..." << std::endl;
	parse_experiment_file(in_expt_filename, expt);
	std::cout << "[INFO]: Finished loading Experiment file." << std::endl;
}

void Control::init_sim(std::string in_sim_filename)
{
	std::fstream sim_file_buf(in_sim_filename.c_str(), std::ios::in | std::ios::binary);
	if (!con_params_populated) read_con_params(sim_file_buf);
	if (!act_params_populated) read_act_params(sim_file_buf);
	if (!simState) simState = new CBMState(numMZones, sim_file_buf);
	if (!simCore) simCore = new CBMSimCore(simState, gpuIndex, gpuP2);
	if (!mfFreq)
	{
		mfFreq = new ECMFPopulation(num_mf, mfRandSeed,
			  CSTonicMFFrac, CSPhasicMFFrac, contextMFFrac, nucCollFrac,
			  bgFreqMin, csbgFreqMin, contextFreqMin, tonicFreqMin, phasicFreqMin, bgFreqMax,
			  csbgFreqMax, contextFreqMax, tonicFreqMax, phasicFreqMax, collaterals_off,
			  fracImport, secondCS, fracOverlap);
	}
	if (!mfs)
	{
		mfs = new PoissonRegenCells(num_mf, mfRandSeed,
				threshDecayTau, msPerTimeStep, numMZones, num_nc);
	}
	if (!output_arrays_initialized) initializeOutputArrays();
	if (!spike_sums_initialized) initialize_spike_sums();
	sim_file_buf.close();
}

void Control::save_sim_state_to_file(std::string outStateFile)
{
	if (!(con_params_populated && act_params_populated && simState))
	{
		fprintf(stderr, "[ERROR]: Trying to write an uninitialized state to file.\n");
		fprintf(stderr, "[ERROR]: (Hint: Try loading activity parameter file and initializing the state first.)\n");
		return;
	}
	std::fstream outStateFileBuffer(outStateFile.c_str(), std::ios::out | std::ios::binary);
	if (simCore)
	{
		// if we have simCore, save from simcore else save from state.
		// this covers both edge case scenarios of if the user wants to save to
		// file an initialized bunny or if the user is using the gui and forgot 
		// to save to file after initializing state, but wants to do so after 
		// initializing simcore.
		simCore->writeState(outStateFileBuffer);
	}
	else simState->writeState(outStateFileBuffer); 
	outStateFileBuffer.close();
}

void Control::save_sim_to_file(std::string outSimFile)
{
	if (!(con_params_populated && act_params_populated && simState))
	{
		fprintf(stderr, "[ERROR]: Trying to write an uninitialized simulation to file.\n");
		fprintf(stderr, "[ERROR]: (Hint: Try loading a build file first.)\n");
		return;
	}
	std::fstream outSimFileBuffer(outSimFile.c_str(), std::ios::out | std::ios::binary);
	write_con_params(outSimFileBuffer);
	write_act_params(outSimFileBuffer);
	if (simCore) simCore->writeState(outSimFileBuffer);
	else simState->writeState(outSimFileBuffer); 
	outSimFileBuffer.close();
}

void Control::initialize_spike_sums()
{
	spike_sums[MF].num_cells  = num_mf;
	spike_sums[GR].num_cells  = num_gr;
	spike_sums[GO].num_cells  = num_go;
	spike_sums[BC].num_cells  = num_bc;
	spike_sums[SC].num_cells  = num_sc;
	spike_sums[PC].num_cells  = num_pc;
	spike_sums[IO].num_cells  = num_io;
	spike_sums[DCN].num_cells = num_nc;

	FOREACH(spike_sums, ssp)
	{
		ssp->non_cs_spike_sum = 0;
		ssp->cs_spike_sum     = 0;
		ssp->non_cs_spike_counter = new ct_uint32_t[ssp->num_cells];
		ssp->cs_spike_counter = new ct_uint32_t[ssp->num_cells];
		memset((void *)ssp->non_cs_spike_counter, 0, ssp->num_cells * sizeof(ct_uint32_t));
		memset((void *)ssp->cs_spike_counter, 0, ssp->num_cells * sizeof(ct_uint32_t));
	}
	spike_sums_initialized = true;
}

void Control::initializeOutputArrays()
{
	// DEBUG: looking at a sample of the granules of size 4096
	sampleGRRaster = allocate2DArray<ct_uint8_t>(4096, rasterColumnSize);
	memset((void *)sampleGRRaster[0], 0, 4096 * rasterColumnSize * sizeof(ct_uint8_t));

	//allMFRaster = allocate2DArray<ct_uint8_t>(num_mf, rasterColumnSize);
	//memset((void *)allMFRaster[0], 0, num_mf * rasterColumnSize * sizeof(ct_uint8_t));

	allGORaster = allocate2DArray<ct_uint8_t>(num_go, rasterColumnSize);
	memset((void *)allGORaster[0], 0, num_go * rasterColumnSize * sizeof(ct_uint8_t));

	allPCRaster = allocate2DArray<ct_uint8_t>(num_pc, rasterColumnSize);
	memset((void *)allPCRaster[0], 0, num_pc * rasterColumnSize * sizeof(ct_uint8_t));
	
	allNCRaster = allocate2DArray<ct_uint8_t>(num_nc, rasterColumnSize);
	memset((void *)allNCRaster[0], 0, num_nc * rasterColumnSize * sizeof(ct_uint8_t));

	//allSCRaster = allocate2DArray<ct_uint8_t>(num_sc, rasterColumnSize);
	//memset((void *)allSCRaster[0], 0, num_sc * rasterColumnSize * sizeof(ct_uint8_t));

	//allBCRaster = allocate2DArray<ct_uint8_t>(num_bc, rasterColumnSize);
	//memset((void *)allBCRaster[0], 0, num_bc * rasterColumnSize * sizeof(ct_uint8_t));

	allIORaster = allocate2DArray<ct_uint8_t>(num_io, rasterColumnSize);
	memset((void *)allIORaster[0], 0, num_io * rasterColumnSize * sizeof(ct_uint8_t));

	sample_pfpc_syn_weights = new float[4096];
	memset((void *)sample_pfpc_syn_weights, 0, 4096);

	output_arrays_initialized = true;
}

void Control::runExperiment(experiment &experiment)
{
	float medTrials;
	std::time_t curr_time = std::time(nullptr);
	std::tm *local_time = std::localtime(&curr_time);
	clock_t timer;
	
	int rasterCounter = 0;
	int goSpkCounter[num_go] = {0};

	for (int trial = 0; trial < experiment.num_trials; trial++)
	{
		std::string trialName = experiment.trials[trial].TrialName;

		int useCS     = experiment.trials[trial].CSuse;
		int onsetCS   = experiment.trials[trial].CSonset;
		int offsetCS  = experiment.trials[trial].CSoffset;
		int percentCS = experiment.trials[trial].CSpercent;
		int useUS     = experiment.trials[trial].USuse;
		int onsetUS   = experiment.trials[trial].USonset;
		
		timer = clock();
		int PSTHCounter = 0;
		float gGRGO_sum = 0;
		float gMFGO_sum = 0;
		memset(goSpkCounter, 0, num_go * sizeof(int));

		for (int ts = 0; ts < trialTime; ts++)
		{
			if (useUS && ts == onsetUS) /* deliver the US */
			{
				simCore->updateErrDrive(0, 0.0);
			}
			if (ts < onsetCS || ts >= offsetCS)
			{
				mfAP = mfs->calcPoissActivity(mfFreq->getMFBG(),
					  simCore->getMZoneList());
			}
			if (ts >= onsetCS && ts < offsetCS)
			{
				if (useCS)
				{
					mfAP = mfs->calcPoissActivity(mfFreq->getMFInCSTonicA(),
						  simCore->getMZoneList());
				}
				else
				{
					mfAP = mfs->calcPoissActivity(mfFreq->getMFBG(),
						  simCore->getMZoneList());
				}
			}
			
			bool *isTrueMF = mfs->calcTrueMFs(mfFreq->getMFBG());
			simCore->updateTrueMFs(isTrueMF);
			simCore->updateMFInput(mfAP);
			simCore->calcActivity(mfgoW, gogrW, grgoW, gogoW, spillFrac); 

			if (ts >= onsetCS && ts < offsetCS)
			{
				mfgoG  = simCore->getInputNet()->exportgSum_MFGO();
				grgoG  = simCore->getInputNet()->exportgSum_GRGO();
				goSpks = simCore->getInputNet()->exportAPGO();
			
				for (int i = 0; i < num_go; i++)
				{
						goSpkCounter[i] += goSpks[i];
						gGRGO_sum       += grgoG[i];
						gMFGO_sum       += mfgoG[i];
				}
			}
			
			/* upon offset of CS, report what we got*/
			if (ts == offsetCS)
			{
				countGOSpikes(goSpkCounter, medTrials);
				std::cout << "mean gGRGO   = " << gGRGO_sum / (num_go * (offsetCS - onsetCS)) << std::endl;
				std::cout << "mean gMFGO   = " << gMFGO_sum / (num_go * (offsetCS - onsetCS)) << std::endl;
				std::cout << "GR:MF ratio  = " << gGRGO_sum / gMFGO_sum << std::endl;
			}

			if (sim_vis_mode == GUI)
			{
				if (gtk_events_pending()) gtk_main_iteration();
			}
		}
		
		timer = clock() - timer;
		std::cout << "[INFO]: " << trialName << " took " << (float)timer / CLOCKS_PER_SEC << "s."
				  << std::endl;
		
		if (sim_vis_mode == GUI)
		{
			if (sim_is_paused)
			{
				std::cout << "[INFO]: Simulation is paused at end of trial " << trial << "." << std::endl;
				while(true)
				{
					// Weird edge case not taken into account: if there are events pending after user hits continue...
					if (gtk_events_pending() || sim_is_paused) gtk_main_iteration();
					else
					{
						std::cout << "[INFO]: Continuing..." << std::endl;
						break;
					}
				}
			}
		}
	}
}

void gen_gr_sample(int gr_indices[], int sample_size, int data_size)
{
	CRandomSFMT0 randGen(0); // replace seed later
	bool chosen[data_size] = {false}; 
	int counter = 0;
	while (counter < sample_size)
	{
		int index = randGen.IRandom(0, data_size - 1);
		if (!chosen[index])
		{
			gr_indices[counter] = index;
			chosen[index] = true;
			counter++;
		} 
	}
}

void Control::runTrials(int simNum, float GOGR, float GRGO, float MFGO, struct gui *gui)
{
	int preTrialNumber   = homeoTuningTrials + granuleActDetectTrials;
	int numTotalTrials   = preTrialNumber + numTrainingTrials;  

	float medTrials;
	std::time_t curr_time = std::time(nullptr);
	std::tm *local_time = std::localtime(&curr_time);
	clock_t timer;
	int rasterCounter = 0;
	int goSpkCounter[num_go] = {0};

	gen_gr_sample(gr_indices, 4096, num_gr);

	GOGR = 0.017;
	GRGO = 0.0007 * 0.9;
	MFGO = 0.00350 * 0.9;

	FILE *fp = NULL;
	if (sim_vis_mode == TUI)
	{
		init_tty(&fp);
	}

	in_run = true;
	trial = 0;
	while (trial < numTotalTrials && in_run)
	{
		timer = clock();
		
		// re-initialize spike counter vector
		std::fill(goSpkCounter, goSpkCounter + num_go, 0);

		int PSTHCounter = 0;
		float gGRGO_sum = 0;
		float gMFGO_sum = 0;
		float gGOGR_sum = 0;
		float gMFGR_sum = 0;

		trial <= homeoTuningTrials ?
			std::cout << "Pre-tuning trial number: " << trial + 1 << std::endl :
			std::cout << "Post-tuning trial number: " << trial + 1 << std::endl;
		
		// Homeostatic plasticity trials
		if (trial >= homeoTuningTrials)
		{
			for (int tts = 0; tts < trialTime; tts++)
			{
				if (tts == csStart + csLength)
				{
					// Deliver US 
					simCore->updateErrDrive(0, 0.3);
				}
				if (tts < csStart || tts >= csStart + csLength)
				{
					// Background MF activity in the Pre and Post CS period
					mfAP = mfs->calcPoissActivity(mfFreq->getMFBG(),
							simCore->getMZoneList());
				}
				else if (tts >= csStart && tts < csStart + csPhasicSize) 
				{
					// Phasic MF activity during the CS for a duration set in control.h 
					mfAP = mfs->calcPoissActivity(mfFreq->getMFFreqInCSPhasic(),
							simCore->getMZoneList());
				}
				else
				{
					// Tonic MF activity during the CS period
					// this never gets reached...
					mfAP = mfs->calcPoissActivity(mfFreq->getMFInCSTonicA(),
							simCore->getMZoneList());
				}

				bool *isTrueMF = mfs->calcTrueMFs(mfFreq->getMFBG());
				simCore->updateTrueMFs(isTrueMF); /* two unnecessary fnctn calls: isTrueMF doesn't change its value! */
				simCore->updateMFInput(mfAP);
				simCore->calcActivity(MFGO, GOGR, GRGO, gogoW, spillFrac);
				
				if (gui != NULL) update_spike_sums(tts);

				if (tts >= csStart && tts < csStart + csLength)
				{
					// TODO: refactor so that we do not export vars, instead we calc sum inside inputNet
					// e.g. simCore->updategGRGOSum(gGRGOSum); <- call by reference
					// even better: simCore->updateGSum<Granule, Golgi>(gGRGOSum); <- granule and golgi are their own cell objs
					mfgoG  = simCore->getInputNet()->exportgSum_MFGO();
					grgoG  = simCore->getInputNet()->exportgSum_GRGO();
					//mfgrG  = simCore->getInputNet()->exportGESumGR(); // excite
					//gogrG  = simCore->getInputNet()->exportGISumGR(); // inhibit
					goSpks = simCore->getInputNet()->exportAPGO();
				
					for (int i = 0; i < num_go; i++)
					{
							goSpkCounter[i] += goSpks[i];
							gGRGO_sum       += grgoG[i];
							gMFGO_sum       += mfgoG[i];
					}
					//for (int i = 0; i < 10000; i++)
					//{
					//		gGOGR_sum       += gogrG[i];
					//		gMFGR_sum       += mfgrG[i];
					//}
				}
				//if (tts == csStart + csLength)
				//{
				//	countGOSpikes(goSpkCounter, medTrials);
				//	std::cout << "mean gGRGO   = " << gGRGO_sum / (num_go * csLength) << std::endl;
				//	std::cout << "mean gMFGO   = " << gMFGO_sum / (num_go * csLength) << std::endl;
				//	//std::cout << "mean gGOGR   = " << gGOGR_sum / (10000 * csLength) << std::endl;
				//	//std::cout << "mean gMFGR   = " << gMFGR_sum / (10000 * csLength) << std::endl;
				//	std::cout << "GR:MF ratio  = " << gGRGO_sum / gMFGO_sum << std::endl;
				//}

				//if (trial >= preTrialNumber && tts >= csStart-msPreCS &&
				//		tts < csStart + csLength + msPostCS)
				//{
				//	fillOutputArrays(mfAP, simCore, PSTHCounter, rasterCounter);

				//	PSTHCounter++;
				//	rasterCounter++;
				//}
				if (gui != NULL)
				{
					if (gtk_events_pending()) gtk_main_iteration(); // place this here?
				}
				else if (sim_vis_mode == TUI) process_input(&fp, tts, trial + 1); /* process user input from kb */
			}
		}
		timer = clock() - timer;
		std::cout << "Trial time seconds: " << (float)timer / CLOCKS_PER_SEC << std::endl;
		
		// check the event queue after every iteration
		if (gui != NULL)
		{
			// for now, compute the mean and median firing rates for all cells regardless
			calculate_firing_rates();
			gdk_threads_add_idle((GSourceFunc)update_fr_labels, gui);
			if (sim_is_paused)
			{
				std::cout << "[INFO]: Simulation is paused at end of trial " << trial+1 << "." << std::endl;
				while(in_run)
				{
					// Weird edge case not taken into account: if there are events pending after user hits continue...
					if (gtk_events_pending() || sim_is_paused) gtk_main_iteration();
					else
					{
						std::cout << "[INFO]: Continuing..." << std::endl;
						break;
					}
				}
			}
			reset_spike_sums();
		}
		trial++;
	}
	if (sim_vis_mode == TUI) reset_tty(&fp); /* reset the tty for later use */
	//saveOutputArraysToFile(0, 0, local_time, 0);
	in_run = false;
}


void Control::saveOutputArraysToFile(int goRecipParam, int trial, std::tm *local_time, int simNum)
{
	//std::cout << "Filling MF files..." << std::endl;
	//std::string allMFRasterFileName = OUTPUT_DATA_PATH + "allMFRaster.bin";
	//write2DCharArray(allMFRasterFileName, allMFRaster, num_mf, rasterColumnSize);

	std::cout << "Filling GO files..." << std::endl;
	std::string allGORasterFileName = OUTPUT_DATA_PATH + "allGORaster.bin";
	write2DCharArray(allGORasterFileName, allGORaster, num_go, rasterColumnSize);

	//std::cout << "Filling GR files..." << std::endl;
	//std::string sampleGRRasterFileName = OUTPUT_DATA_PATH + "sampleGRRaster.bin";
	//write2DCharArray(sampleGRRasterFileName, sampleGRRaster, 4096, rasterColumnSize);

	std::cout << "Filling PC files..." << std::endl;
	std::string allPCRasterFileName = OUTPUT_DATA_PATH + "allPCRaster.bin";
	write2DCharArray(allPCRasterFileName, allPCRaster, num_pc, rasterColumnSize);

	std::cout << "Filling NC files..." << std::endl;
	std::string allNCRasterFileName = OUTPUT_DATA_PATH + "allNCRaster.bin";
	write2DCharArray(allNCRasterFileName, allNCRaster, num_nc, rasterColumnSize);

	//std::cout << "Filling SC files..." << std::endl;
	//std::string allSCRasterFileName = OUTPUT_DATA_PATH + "allSCRaster.bin";
	//write2DCharArray(allSCRasterFileName, allSCRaster, num_sc, rasterColumnSize);

	//std::cout << "Filling BC files..." << std::endl;
	//std::string allBCRasterFileName = OUTPUT_DATA_PATH + "allBCRaster.bin";
	//write2DCharArray(allBCRasterFileName, allBCRaster, num_bc, rasterColumnSize);

	std::cout << "Filling IO files..." << std::endl;
	std::string allIORasterFileName = OUTPUT_DATA_PATH + "allIORaster.bin";
	write2DCharArray(allIORasterFileName, allIORaster, num_io, rasterColumnSize);
}

void Control::update_spike_sums(int tts)
{
	cell_spikes[MF]  = mfAP;
	cell_spikes[GR]  = simCore->getInputNet()->exportAPGR();
	cell_spikes[GO]  = simCore->getInputNet()->exportAPGO();
	cell_spikes[BC]  = simCore->getMZoneList()[0]->exportAPBC();
	cell_spikes[SC]  = simCore->getInputNet()->exportAPSC();
	cell_spikes[PC]  = simCore->getMZoneList()[0]->exportAPPC();
	cell_spikes[IO]  = simCore->getMZoneList()[0]->exportAPIO();
	cell_spikes[DCN] = simCore->getMZoneList()[0]->exportAPNC();

	// update cs spikes
	if (tts >= csStart && tts < csStart + csLength)
	{
		for (int i = 0; i < NUM_CELL_TYPES; i++)
		{
			for (int j = 0; j < spike_sums[i].num_cells; j++)
			{
				spike_sums[i].cs_spike_sum += cell_spikes[i][j];
				spike_sums[i].cs_spike_counter[j] += cell_spikes[i][j];
			}
		}
	}
	// update non-cs spikes
	else if (tts < csStart)
	{
		for (int i = 0; i < NUM_CELL_TYPES; i++)
		{
			for (int j = 0; j < spike_sums[i].num_cells; j++)
			{
				spike_sums[i].non_cs_spike_sum += cell_spikes[i][j];
				spike_sums[i].non_cs_spike_counter[j] += cell_spikes[i][j];
			}
		}
	}
}

void Control::reset_spike_sums()
{
		for (int i = 0; i < NUM_CELL_TYPES; i++)
		{
			spike_sums[i].cs_spike_sum = 0;
			spike_sums[i].non_cs_spike_sum = 0;
			memset((void *)(spike_sums[i].non_cs_spike_counter), 0, spike_sums[i].num_cells * sizeof(ct_uint32_t));
			memset((void *)(spike_sums[i].cs_spike_counter), 0, spike_sums[i].num_cells * sizeof(ct_uint32_t));
		}
}


void Control::calculate_firing_rates()
{
	float non_cs_time = (csStart - 1) / 1000.0;
	// array name: firing_rates, spike_sums
	for (int i = 0; i < NUM_CELL_TYPES; i++)
	{
		// sort sums for medians 
		std::sort(spike_sums[i].non_cs_spike_counter,
			spike_sums[i].non_cs_spike_counter + spike_sums[i].num_cells);
		std::sort(spike_sums[i].cs_spike_counter,
			spike_sums[i].cs_spike_counter + spike_sums[i].num_cells);
		
		// calculate medians
		firing_rates[i].non_cs_median_fr =
			(spike_sums[i].non_cs_spike_counter[spike_sums[i].num_cells / 2 - 1]
		   + spike_sums[i].non_cs_spike_counter[spike_sums[i].num_cells / 2]) / (2.0 * non_cs_time);
		firing_rates[i].cs_median_fr     =
			(spike_sums[i].cs_spike_counter[spike_sums[i].num_cells / 2 - 1]
		   + spike_sums[i].cs_spike_counter[spike_sums[i].num_cells / 2]) / 4.0;
		
		// calculate means
		firing_rates[i].non_cs_mean_fr = spike_sums[i].non_cs_spike_sum / (non_cs_time * spike_sums[i].num_cells);
		firing_rates[i].cs_mean_fr     = spike_sums[i].cs_spike_sum / (2.0 * spike_sums[i].num_cells);
	}
}

void Control::countGOSpikes(int *goSpkCounter, float &medTrials)
{
	std::sort(goSpkCounter, goSpkCounter + 4096);
	
	float m = (goSpkCounter[2047] + goSpkCounter[2048]) / 2.0;
	float goSpkSum = 0;

	for (int i = 0; i < num_go; i++) goSpkSum += goSpkCounter[i];
	
	std::cout << "Mean GO Rate: " << goSpkSum / ((float)num_go * 2.0) << std::endl;

	medTrials += m / 2.0;
	std::cout << "Median GO Rate: " << m / 2.0 << std::endl;
}

void Control::fillOutputArrays(const ct_uint8_t *mfAP, CBMSimCore *simCore, int PSTHCounter, int rasterCounter)
{
	const ct_uint8_t* goSpks = simCore->getInputNet()->exportAPGO();
	const ct_uint8_t* grSpks = simCore->getInputNet()->exportAPGR();
	const ct_uint8_t* pcSpks = simCore->getMZoneList()[0]->exportAPPC();
	const ct_uint8_t* ncSpks = simCore->getMZoneList()[0]->exportAPNC();
	//const ct_uint8_t* scSpks = simCore->getInputNet()->exportAPSC();
	//const ct_uint8_t* bcSpks = simCore->getMZoneList()[0]->exportAPBC();
	const ct_uint8_t* ioSpks = simCore->getMZoneList()[0]->exportAPIO();

	//for (int i = 0; i < num_mf; i++)
	//{
	//	allMFRaster[i][rasterCounter] = mfAP[i];
	//}

	for (int i = 0; i < num_go; i++)
	{
		allGORaster[i][rasterCounter] = goSpks[i];
	}

	// 4096 sample size
	for (int i = 0; i < 4096; i++)
	{
		sampleGRRaster[i][rasterCounter] = grSpks[gr_indices[i]];
	}

	for (int i = 0; i < num_pc; i++)
	{
		allPCRaster[i][rasterCounter] = pcSpks[i];
	}

	for (int i = 0; i < num_nc; i++)
	{
		allNCRaster[i][rasterCounter] = ncSpks[i];
	}

	//for (int i = 0; i < num_sc; i++)
	//{
	//	allSCRaster[i][rasterCounter] = scSpks[i];
	//}

	//for (int i = 0; i < num_bc; i++)
	//{
	//	allBCRaster[i][rasterCounter] = bcSpks[i];
	//}

	for (int i = 0; i < num_io; i++)
	{
		allIORaster[i][rasterCounter] = ioSpks[i];
	}
}

// TODO: 1) find better place to put this 2) generalize
void Control::write2DCharArray(std::string outFileName, ct_uint8_t **inArr,
	unsigned int numRow, unsigned int numCol)
{
	std::fstream outStream(outFileName.c_str(), std::ios::out | std::ios::binary);

	if (!outStream.is_open())
	{
		std::cerr << "couldn't open '" << outFileName << "' for writing." << std::endl;
		exit(-1);
	}
	rawBytesRW((char *)inArr[0], numRow * numCol * sizeof(ct_uint8_t), false, outStream);
	outStream.close();
}

void Control::delete_spike_sums()
{
	FOREACH(spike_sums, ssp)
	{
		delete[] ssp->non_cs_spike_counter;
		delete[] ssp->cs_spike_counter;
	}

}

void Control::deleteOutputArrays()
{
	//delete2DArray<ct_uint8_t>(allMFRaster);
	delete2DArray<ct_uint8_t>(allGORaster);
	delete2DArray<ct_uint8_t>(sampleGRRaster);
	delete2DArray<ct_uint8_t>(allPCRaster);
	delete2DArray<ct_uint8_t>(allNCRaster);
	//delete2DArray<ct_uint8_t>(allSCRaster);
	//delete2DArray<ct_uint8_t>(allBCRaster);
	delete2DArray<ct_uint8_t>(allIORaster);
	delete[] sample_pfpc_syn_weights;
}

