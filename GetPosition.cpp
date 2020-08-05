#include "GetPosition.h"

FbxAMatrix getGlobalPosition(FbxNode* pNode, const FbxTime& pTime, FbxPose* pPose, FbxAMatrix* pParentGlobalPosition)
{
	FbxAMatrix globalPosition;
	bool positionFound = false;

	if (pPose)
	{
		int nodeIndex = pPose->Find(pNode);

		if (nodeIndex > -1)
		{
			// the bind pose is always a global matrix.
			// if we have a rest pose, we need to check if it is
			// stored in global or local space.
			if (pPose->IsBindPose() || !pPose->IsLocalMatrix(nodeIndex))
			{
				globalPosition = getPoseMatrix(pPose, nodeIndex);
			}
			else
			{
				FbxAMatrix parentGlobalPosition;

				if (parentGlobalPosition)
				{
					parentGlobalPosition = *pParentGlobalPosition;
				}
				else
				{
					if (pNode->GetParent())
					{
						parentGlobalPosition = getGlobalPosition(pNode->GetParent(), pTime, pPose);
					}
				}

				FbxAMatrix localPosition = getPoseMatrix(pPose, nodeIndex);
				globalPosition = parentGlobalPosition * localPosition;
			}
			positionFound = true;
		}
	}

	if (!positionFound)
	{
		globalPosition = pNode->EvaluateGlobalTransform(pTime);
	}

	return globalPosition;
}

FbxAMatrix getPoseMatrix(FbxPose* pPose, int pNodeIndex)
{
	FbxAMatrix poseMatrix;
	FbxMatrix matrix = pPose->GetMatrix(pNodeIndex);

	memcpy((double *)poseMatrix, (double *)matrix, sizeof(matrix.mData));

	return poseMatrix;
}

FbxAMatrix getGeometry(FbxNode* pNode)
{
	const FbxVector4 t = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 r = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 s = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(t, r, s);
}


