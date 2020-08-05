#pragma once

#include "preh.h"

FbxAMatrix getGlobalPosition(FbxNode *pNode,
	const FbxTime & pTime,
	FbxPose *pPose = NULL,
	FbxAMatrix *pParentGlobalPosition = NULL);

FbxAMatrix getPoseMatrix(FbxPose *pPose, int pNodeIndex);

FbxAMatrix getGeometry(FbxNode *pNode);