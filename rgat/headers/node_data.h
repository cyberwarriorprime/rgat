/*
Copyright 2016-2017 Nia Catlin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*
Class describing each node
*/
#pragma once
#include "stdafx.h"
#include "graph_display_data.h"
#include "traceStructs.h"
#include "traceConstants.h"
#include "mathStructs.h"

#include <rapidjson\document.h>
#include <rapidjson\filewritestream.h>
#include <rapidjson\writer.h>

class node_data
{
public:
	node_data() {};
	~node_data() {};

	bool serialise(rapidjson::Writer<rapidjson::FileWriteStream>& writer, PROTOGRAPH_CASTPTR graphPtr);
	//takes a file with a pointer next to a node entry, loads it into the node
	int deserialise(const rapidjson::Value& nodeData, map <MEM_ADDRESS, INSLIST> *disassembly);
	void setLabelFromNearestSymbol(TRACERECORDPTR traceRecPtr);

	NODEINDEX index = 0;
	
	int conditional = 0;
	INS_DATA* ins = NULL;
	bool external = false;
	bool unreliableCount = false; //external executions not directly tracked - estimated using heatmap solver
	long globalModID;

	BLOCK_IDENTIFIER blockID;
	
	//an index used to lookup the caller/arguments of each instance of this being called
	vector<unsigned long> callRecordsIndexs;
	unsigned long long currentCallIndex = 1; //need to review how this works and if it achieves anything

	//number of external functions called
	unsigned int childexterns = 0;
	MEM_ADDRESS address = 0; //this is only used in externs. bit big?
	NODEINDEX parentIdx = 0;

	unsigned long long executionCount = 0;
	unsigned long long chain_remaining_in = 0;
	unsigned long long chain_remaining_out = 0;

	DWORD heat_run_marker;

	set<NODEINDEX> incomingNeighbours;
	set<NODEINDEX> outgoingNeighbours;

	QString label; //todo serialise
	bool placeholder = false;
};

