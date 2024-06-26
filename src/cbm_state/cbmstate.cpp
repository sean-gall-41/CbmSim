/*
 * cbmstate.cpp
 *
 *  Created on: Dec 5, 2012
 *      Author: consciousness
 */

#include "cbmstate.h"
#include "logger.h"

CBMState::CBMState() {}

CBMState::CBMState(unsigned int nZones) : numZones(nZones) {
  LOG_DEBUG("Generating cbm state...");
  CRandomSFMT randGen(time(0));

  int innetCRSeed = randGen.IRandom(0, INT_MAX);
  int *mzoneCRSeed = new int[nZones];
  int *mzoneARSeed = new int[nZones];

  innetConState = new InNetConnectivityState(innetCRSeed);
  innetActState = new InNetActivityState();

  mzoneConStates = new MZoneConnectivityState *[nZones];
  mzoneActStates = new MZoneActivityState *[nZones];
  for (int i = 0; i < nZones; i++) {
    mzoneCRSeed[i] = randGen.IRandom(0, INT_MAX);
    mzoneARSeed[i] = randGen.IRandom(0, INT_MAX);
    mzoneConStates[i] = new MZoneConnectivityState(mzoneCRSeed[i]);
    mzoneActStates[i] = new MZoneActivityState(mzoneARSeed[i]);
  }
  delete[] mzoneCRSeed;
  delete[] mzoneARSeed;
  LOG_DEBUG("Finished generating cbm state.");
}

CBMState::CBMState(unsigned int nZones, enum plasticity plast_type,
                   std::fstream &sim_file_buf)
    : numZones(nZones) {
  LOG_DEBUG("Initializing cbm state from file...");
  innetConState = new InNetConnectivityState(sim_file_buf);
  innetActState = new InNetActivityState(sim_file_buf);

  mzoneConStates = new MZoneConnectivityState *[nZones];
  mzoneActStates = new MZoneActivityState *[nZones];

  for (int i = 0; i < nZones; i++) {
    mzoneConStates[i] = new MZoneConnectivityState(sim_file_buf);
    mzoneActStates[i] = new MZoneActivityState(plast_type, sim_file_buf);
  }
  LOG_DEBUG("Finished initializing cbm state.");
}

// CBMState::CBMState(unsigned int nZones,
//	std::string inFile) : numZones(nZones)
//{
//	std::fstream inStateFileBuffer(inFile.c_str(), std::ios::in |
//std::ios::binary); 	innetConState  = new
//InNetConnectivityState(inStateFileBuffer); 	innetActState  = new
//InNetActivityState(inStateFileBuffer);
//
//	mzoneConStates = new MZoneConnectivityState*[nZones];
//	mzoneActStates = new MZoneActivityState*[nZones];
//
//	for (int i = 0; i < nZones; i++)
//	{
//		mzoneConStates[i] = new
//MZoneConnectivityState(inStateFileBuffer); 		mzoneActStates[i] = new
//MZoneActivityState(inStateFileBuffer);
//	}
//	inStateFileBuffer.close();
// }

CBMState::~CBMState() {
  delete innetConState;
  delete innetActState;
  for (int i = 0; i < numZones; i++) {
    delete mzoneConStates[i];
    delete mzoneActStates[i];
  }
  delete[] mzoneConStates;
  delete[] mzoneActStates;
}

void CBMState::readState(std::fstream &infile) {
  innetConState->readState(infile);
  innetActState->readState(infile);

  for (int i = 0; i < numZones; i++) {
    mzoneConStates[i]->readState(infile);
    mzoneActStates[i]->readState(infile);
  }
}

void CBMState::writeState(std::fstream &outfile) {
  innetConState->writeState(outfile);
  innetActState->writeState(outfile);
  for (int i = 0; i < numZones; i++) {
    mzoneConStates[i]->writeState(outfile);
    mzoneActStates[i]->writeState(outfile);
  }
}

uint32_t CBMState::getNumZones() { return numZones; }

InNetActivityState *CBMState::getInnetActStateInternal() {
  return innetActState;
}

MZoneActivityState *CBMState::getMZoneActStateInternal(unsigned int zoneN) {
  return mzoneActStates[zoneN];
}

InNetConnectivityState *CBMState::getInnetConStateInternal() {
  return innetConState;
}

MZoneConnectivityState *CBMState::getMZoneConStateInternal(unsigned int zoneN) {
  return mzoneConStates[zoneN];
}
