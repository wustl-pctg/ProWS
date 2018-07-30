//-------------------------------------------------------------
//      ____                        _      _
//     / ___|____ _   _ ____   ____| |__  | |
//    | |   / ___| | | |  _  \/ ___|  _  \| |
//    | |___| |  | |_| | | | | |___| | | ||_|
//     \____|_|  \_____|_| |_|\____|_| |_|(_) Media benchmarks
//                  
//	  2006, Intel Corporation, licensed under Apache 2.0 
//
//  file : TrackingModelCilk.h
//  author : Scott Ettinger - scott.m.ettinger@intel.com
//  description : Observation model for kinematic tree body 
//				  tracking threaded with OpenMP.
//				  
//  modified : 
//--------------------------------------------------------------

#ifndef TRACKINGMODELCILK_H
#define TRACKINGMODELCILK_H

#include "TrackingModel.h"
#include <iostream>
#include <iomanip>
#include <sstream>

class TrackingModelCilk : public TrackingModel {
	//Generate an edge map from the original camera image - threaded
	void CreateEdgeMap(const FlexImage8u &src, FlexImage8u &dst);

public:
	//load and process images - overloaded for future threading
	bool GetObservationCilk(float timeval,
                            std::vector<BinaryImage> *iter_mFGMaps,
                            std::vector<FlexImage8u> *iter_mEdgeMaps);

	//give the model object the processed images
	void SetObservation(std::vector<BinaryImage> *iter_mFGMaps,
                            std::vector<FlexImage8u> *iter_mEdgeMaps) {
            mFGMaps = std::move(*iter_mFGMaps);
            mEdgeMaps = std::move(*iter_mEdgeMaps);
        }
};

#endif

