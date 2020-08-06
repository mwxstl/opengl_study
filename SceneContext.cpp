#include "GameContext.h"
#include "SceneContext.h"
#include "SceneCache.h"
#include "ShaderProgram.h"
#include "targa.h"
#include "GetPosition.h"
FbxManager *mManager;
FbxScene *mScene;
FbxImporter *mImporter;
SceneContext::SceneContext(const char* pFileName)
: mFileName(pFileName), mSceneStatus(UNLOADED),
mManager(NULL), mScene(NULL), mImporter(NULL), mCurrentAnimLayer(NULL),
mPoseIndex(-1), mPause(false)
{
	if (mFileName == NULL)
	{
		cout << "error: fileName is NULL" << endl;
		exit(1);
	}

	//initialize cache start and stop time
	mCacheStart = FBXSDK_TIME_INFINITE;
	mCacheStop = FBXSDK_TIME_MINUS_INFINITE;

	// create the fbx sdk manager which is the object allocator
	// for almost all the classes in the sdk and create the scene.
	mManager = FbxManager::Create();
	if (!mManager)
	{
		cout << "error: unable to create fbx manager!" << endl;
		exit(1);
	}

	FbxIOSettings *ios = FbxIOSettings::Create(mManager, IOSROOT);
	mManager->SetIOSettings(ios);

	mScene = FbxScene::Create(mManager, "My Scene");
	if (!mScene)
	{
		cout << "error: unable to create fbx scene!" << endl;
		exit(1);
	}

	mImporter = FbxImporter::Create(mManager, "");
	if (!mImporter->Initialize(mFileName, -1, mManager->GetIOSettings()))
	{
		cout << "error: unable to initialize importer!" << endl;
		exit(1);
	}
	else
	{
		mSceneStatus = MUST_BE_LOADED;
	}

}
SceneContext::~SceneContext()
{
	FbxArrayDelete(mAnimStackNameArray);
	
}

bool SceneContext::loadFile(GameContext *gameContext)
{
	if (mSceneStatus == MUST_BE_LOADED)
	{
		if (!mImporter->Import(mScene))
		{
			mSceneStatus = UNLOADED;
			cout << "error: unable to import scene!" << endl;
			return false;
		}

		// check the scene integrity!
		FbxStatus status;
		FbxArray<FbxString *> details;
		FbxSceneCheckUtility sceneCheck(FbxCast<FbxScene>(mScene), &status, &details);
		bool notify = (!sceneCheck.Validate(FbxSceneCheckUtility::eCkeckData) && details.GetCount() > 0) || (mImporter->GetStatus().GetCode() != FbxStatus::eSuccess);
		if (notify)
		{
			cout << endl;
			cout << "*************************************************" << endl;
			if (details.GetCount())
			{
				cout << "scene integrity verification failed with the following errors:\n";

				for (int i = 0; i < details.GetCount(); i++)
				{
					cout << details[i]->Buffer() << endl;
				}
				FbxArrayDelete<FbxString *>(details);
			}

			if (mImporter->GetStatus().GetCode() != FbxStatus::eSuccess)
			{
				cout << endl;
				cout << "warning:" << endl;
				cout << "	the importer was able to read the file but with errors." << endl;
				cout << "	loaded scene may be incomplete." << endl << endl;
				cout << "	last error message: '" << mImporter->GetStatus().GetErrorString() << "'" << endl;
			}

			cout << "*************************************************" << endl;
			cout << endl;
		}
		// set the scene status flag to refresh
		// the scene in the first timer callback.
		mSceneStatus = MUST_BE_REFRESHED;

		// convert axis system.
		FbxAxisSystem sceneAxisSystem = mScene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem ourAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
		if (sceneAxisSystem != ourAxisSystem)
		{
			ourAxisSystem.ConvertScene(mScene);
		}

		//convert unit system.
		FbxSystemUnit sceneSystemUnit = mScene->GetGlobalSettings().GetSystemUnit();
		if (sceneSystemUnit != FbxSystemUnit::cm)
		{
			const FbxSystemUnit::ConversionOptions conversionOptions = {
				false,
				true,
				true,
				true,
				true,
				true
			};
			FbxSystemUnit::cm.ConvertScene(mScene, conversionOptions);
		}

		// get the list of all the animation stack
		mScene->FillAnimStackNameArray(mAnimStackNameArray);

		for (int i = 0; i < mAnimStackNameArray.GetCount(); i++)
		{
			cout << i << " " << (FbxString *)(mAnimStackNameArray.GetAt(i))->Buffer() << endl;
		}
		
		// todo get the list of all the cameras in the scene.
		
		//convert mesh, nurbs and patch into triangle mesh
		FbxGeometryConverter lGeomConverter(mManager);
		try {
			lGeomConverter.Triangulate(mScene, /*replace*/true);
		}
		catch (std::runtime_error) {
			cout << "Scene integrity verification failed!" << endl;
			return false;
		}
		loadCacheRecursive(mScene, mCurrentAnimLayer, gameContext);

		mFrameTime.SetTime(0, 0, 0, 1, 0, mScene->GetGlobalSettings().GetTimeMode());
		return true;
	}
	else
	{
		return false;
	}
}


void SceneContext::loadCacheRecursive(FbxScene* pScene, FbxAnimLayer *pAnimLayer, GameContext *gameContext)
{
	loadTestLight(gameContext);
	
	//load the textures into gpu, only for file texture now
	const int count = pScene->GetTextureCount();
	for (int i = 0; i < count; i++)
	{
		FbxTexture *texture = pScene->GetTexture(i);
		FbxFileTexture *fileTexture = FbxCast<FbxFileTexture>(texture);
		if (fileTexture && !fileTexture->GetUserDataPtr())
		{
			const FbxString fileName = fileTexture->GetFileName();

			if (fileName.Right(3).Upper() != "TGA")
			{
				cout << "only tga texture are supported now: " << fileName.Buffer() << endl;
				continue;
			}

			GLuint textureObject = 0;
			bool status = loadTextureFromFile(fileName, textureObject);

			const FbxString absFbxFileName = FbxPathUtils::Resolve(absFbxFileName);
			const FbxString absFolderName = FbxPathUtils::GetFolderName(absFbxFileName);
			if (!status)
			{
				// load texture from relative file name (relative to fbx file)
				const FbxString resolvedFileName = FbxPathUtils::Bind(absFolderName, fileTexture->GetRelativeFileName());
				status = loadTextureFromFile(resolvedFileName, textureObject);
			}

			if (!status)
			{
				// load texture from file name only (relative to fbx file)
				const FbxString textureFileName = FbxPathUtils::GetFileName(fileName);
				const FbxString resolvedFileName = FbxPathUtils::Bind(absFolderName, textureFileName);
				status = loadTextureFromFile(resolvedFileName, textureObject);
			}

			if (!status)
			{
				cout << "failed to load texture file: " << fileName.Buffer() << endl;
				continue;
			}

			if (status)
			{
				GLuint *textureName = new GLuint(textureObject);
				fileTexture->SetUserDataPtr(textureName);
			}
		}

	}
	loadCacheRecursive(pScene->GetRootNode(), pAnimLayer);
}



void SceneContext::loadCacheRecursive(FbxNode* pNode, FbxAnimLayer *pAnimLayer)
{
	//bake material and hook as user data
	const int materialCount = pNode->GetMaterialCount();
	for (int i = 0; i < materialCount; i++)
	{
		FbxSurfaceMaterial *material = pNode->GetMaterial(i);
		if (material && !material->GetUserDataPtr())
		{
			FbxAutoPtr<MaterialCache> materialCache(new MaterialCache);
			if (materialCache->initialize(material))
			{
				material->SetUserDataPtr(materialCache.Release());
			}
		}
	}

	//bake mesh as vbo into gpu
	FbxNodeAttribute* nodeAttribute = pNode->GetNodeAttribute();
	if (nodeAttribute)
	{
		if (nodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh *lMesh = pNode->GetMesh();
			if (lMesh && !lMesh->GetUserDataPtr())
			{
				FbxAutoPtr<VBOMesh> lMeshCache(new VBOMesh);
				if (lMeshCache->initialize(lMesh))
				{
					lMesh->SetUserDataPtr(lMeshCache.Release());
				}
			}
		}
		else if (nodeAttribute->GetAttributeType() == FbxNodeAttribute::eLight)
		{
			FbxLight *light = pNode->GetLight();
			if (light && !light->GetUserDataPtr())
			{
				FbxAutoPtr<LightCache> lightCache(new LightCache);
				if (lightCache->initialize(light, pAnimLayer))
				{
					light->SetUserDataPtr(lightCache.Release());
				}
			}
		}
	}

	const int childCount = pNode->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		loadCacheRecursive(pNode->GetChild(i), pAnimLayer);
	}
}

bool SceneContext::loadTextureFromFile(const FbxString& pFilePath, unsigned& pTextureObject)
{
	if (pFilePath.Right(3).Upper() == "TGA")
	{
		tga_image tgaImage;

		if (tga_read(&tgaImage, pFilePath.Buffer()) == TGA_NOERR)
		{
			if (tga_is_right_to_left(&tgaImage))
			{
				tga_flip_horiz(&tgaImage);
			}

			if (tga_is_top_to_bottom(&tgaImage))
			{
				tga_flip_vert(&tgaImage);
			}

			tga_convert_depth(&tgaImage, 24);

			glGenTextures(1, &pTextureObject);
			glBindTexture(GL_TEXTURE_2D, pTextureObject);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// todo ? gltexEnvi

			glTexImage2D(GL_TEXTURE_2D, 0, 3, tgaImage.width, tgaImage.height, 0, GL_RGB,
				GL_UNSIGNED_BYTE, tgaImage.image_data);
			glBindTexture(GL_TEXTURE_2D, 0);

			tga_free_buffers(&tgaImage);
			return true;
		}
	}
	return false;
}
// compute the transform matrix that the cluster will transform the vertex.
void computeClusterDeformation(
	FbxAMatrix & pGlobalPosition,
	FbxMesh *pMesh,
	FbxCluster *pCluster,
	FbxAMatrix & pVertexTransformMatrix,
	FbxTime pTime,
	FbxPose *pPose
)
{
	FbxCluster::ELinkMode clusterMode = pCluster->GetLinkMode();

	FbxAMatrix referenceGlobalInitPos, referenceGlobalCurrentPos;
	FbxAMatrix associateGlobalInitPos, associateGlobalCurrentPos;
	FbxAMatrix clusterGlobalInitPos, clusterGlobalCurrentPos;

	FbxAMatrix referenceGeometry;
	FbxAMatrix associateGeometry;
	FbxAMatrix clusterGeometry;

	FbxAMatrix clusterRelativeInitPos;
	FbxAMatrix clusterRelativeCurrentPositionInverse;

	if (clusterMode == FbxCluster::eAdditive && pCluster->GetAssociateModel())
	{
		pCluster->GetTransformAssociateModelMatrix(associateGlobalInitPos);
		// geometric transform of the model
		associateGeometry = getGeometry(pCluster->GetAssociateModel());
		associateGlobalInitPos *= associateGeometry;
		associateGlobalCurrentPos = getGlobalPosition(pCluster->GetAssociateModel(), pTime, pPose);

		pCluster->GetTransformMatrix(referenceGlobalInitPos);
		//multiply referenceGlobalInitPosition by Geometric Transformation
		referenceGeometry = getGeometry(pMesh->GetNode());
		referenceGlobalInitPos *= referenceGeometry;
		referenceGlobalCurrentPos = pGlobalPosition;

		// get the link initial global position and the link current global position.
		pCluster->GetTransformLinkMatrix(clusterGlobalInitPos);
		// multiply clusterGlobalInitPosition by geometric transformation.
		clusterGeometry = getGeometry(pCluster->GetLink());
		clusterGlobalInitPos *= clusterGeometry;
		clusterGlobalCurrentPos = getGlobalPosition(pCluster->GetLink(), pTime, pPose);

		// compute the shift of the link relative to the reference.
		// modelM-1 * AssoM * AssoGX-1 * LinkGX * linkM-1 * ModelM
		pVertexTransformMatrix = referenceGlobalInitPos.Inverse() * associateGlobalInitPos
			* associateGlobalCurrentPos.Inverse() * clusterGlobalCurrentPos
			* clusterGlobalInitPos.Inverse() * referenceGlobalInitPos;
	}
	else
	{
		pCluster->GetTransformMatrix(referenceGlobalInitPos);
		referenceGlobalCurrentPos = pGlobalPosition;
		// multiply referenceGlobalInitPosition by Geometric Transformation
		referenceGeometry = getGeometry(pMesh->GetNode());
		referenceGlobalInitPos *= referenceGeometry;

		// get the link initial global position and the link current global position.
		pCluster->GetTransformLinkMatrix(clusterGlobalInitPos);
		clusterGlobalCurrentPos = getGlobalPosition(pCluster->GetLink(), pTime, pPose);

		// compute the initial position of the link relative to the reference.
		clusterRelativeInitPos = clusterGlobalInitPos.Inverse() * referenceGlobalInitPos;

		// compute the current position of the link relative to the reference.
		clusterRelativeCurrentPositionInverse = referenceGlobalCurrentPos.Inverse() * clusterGlobalCurrentPos;

		// compute the shift of the link relative to the reference.
		pVertexTransformMatrix = clusterRelativeCurrentPositionInverse * clusterRelativeInitPos;
	}
}

// scale all the elements of a matrix.
void matrixScale(FbxAMatrix & pMatrix, double pValue)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}

// add a value to all the elements in the diagonal of the matrix.
void matrixAddToDiagonal(FbxAMatrix & pMatrix, double pValue)
{
	for (int i = 0; i < 4; i++)
	{
		pMatrix[i][i] += pValue;
	}
}

// sum two matrices element by element.
void matrixAdd(FbxAMatrix & pDstMatrix, FbxAMatrix & pSrcMatrix)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}

// deform the vertex array in classic linear way.
void computeLinearDeformation(
	FbxAMatrix & pGlobalPosition,
	FbxMesh *pMesh,
	FbxTime &pTime,
	FbxVector4 *pVertexArray,
	FbxPose *pPose)
{
	// all the links must have the same link mode.
	FbxCluster::ELinkMode clusterMode = ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))
		->GetCluster(0)->GetLinkMode();

	int vertexCount = pMesh->GetControlPointsCount();
	FbxAMatrix* clusterDeformation = new FbxAMatrix[vertexCount];
	memset(clusterDeformation, 0, vertexCount * sizeof(FbxAMatrix));

	double *clusterWeight = new double[vertexCount];
	memset(clusterWeight, 0, vertexCount * sizeof(double));

	if (clusterMode == FbxCluster::eAdditive)
	{
		for (int i = 0; i < vertexCount; ++i)
		{
			clusterDeformation[i].SetIdentity();
		}
	}

	// for all skins and all clusters, accumulate their deformation and weight
	// on each vertices and store them in clusterDeformation and clusterWeight.
	int skinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int skinIndex = 0; skinIndex < skinCount; ++skinIndex)
	{
		FbxSkin *skinDeformer = (FbxSkin *)pMesh->GetDeformer(skinIndex, FbxDeformer::eSkin);

		int clusterCount = skinDeformer->GetClusterCount();
		for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
		{
			FbxCluster *cluster = skinDeformer->GetCluster(clusterIndex);
			if (!cluster->GetLink())
			{
				continue;
			}

			FbxAMatrix vertexTransformMatrix;
			computeClusterDeformation(pGlobalPosition, pMesh, cluster, vertexTransformMatrix, pTime, pPose);

			int vertexIndexCount = cluster->GetControlPointIndicesCount();
			for (int k = 0; k < vertexIndexCount; ++k)
			{
				int index = cluster->GetControlPointIndices()[k];

				// sometimes, the mesh can have less points than at the time of the skinning
				// because a smooth operator was active when skinning but has been deactivated
				// during export.

				if (index >= vertexCount)
				{
					continue;
				}

				double weight = cluster->GetControlPointWeights()[k];

				if (weight == 0.0)
				{
					continue;
				}

				// compute the influence of the link on the vertex.
				FbxAMatrix influence = vertexTransformMatrix;
				matrixScale(influence, weight);

				if (clusterMode == FbxCluster::eAdditive)
				{
					// multiply with the product of the deformations on the 
					matrixAddToDiagonal(influence, 1.0 - weight);
					clusterDeformation[index] = influence * clusterDeformation[index];

					// set the link to 1.0 just to know this vertex is influenced by a link.
					clusterWeight[index] = 1.0;
				}
				else // eNormalize || eTotalOne
				{
					// add to the sum of the deformations on the vertex.
					matrixAdd(clusterDeformation[index], influence);

					// add to the sum of weights to either normalize or complete the vertex.
					clusterWeight[index] += weight;
				}
			} // for each vertex
		} // clusterCount
	}

	// actually deform each vertices here by information stored in
	// clusterDeformation and clusterWeight
	for (int i = 0; i < vertexCount; i++)
	{
		FbxVector4 srcVertex = pVertexArray[i];
		FbxVector4 & dstVertex = pVertexArray[i];
		double weight = clusterWeight[i];

		// deform the vertex if there was at least a link with an influence
		// on the vertex
		if (weight != 0.0)
		{
			dstVertex = clusterDeformation[i].MultT(srcVertex);
			if (clusterMode == FbxCluster::eNormalize)
			{
				// in the normalized link mode, a vertex is always totally
				// influenced by the links.
				dstVertex /= weight;
			}
			else if (clusterMode == FbxCluster::eTotalOne)
			{
				srcVertex *= (1.0 - weight);
				dstVertex += srcVertex;
			}
		}
	}

	delete[] clusterDeformation;
	delete[] clusterWeight;
}

void computeDualQuaternionDeformation(
	FbxAMatrix & pGlobalPosition,
	FbxMesh *pMesh,
	FbxTime &pTime,
	FbxVector4 *pVertexArray,
	FbxPose *pPose)
{
	// all the links must have the smae link mode.
	FbxCluster::ELinkMode clusterMode = ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))
		->GetCluster(0)->GetLinkMode();

	int vertexCount = pMesh->GetControlPointsCount();
	int skinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);

	FbxDualQuaternion* dqClusterDeformation = new FbxDualQuaternion[vertexCount];
	memset(dqClusterDeformation, 0, vertexCount * sizeof(FbxDualQuaternion));

	double *clusterWeight = new double[vertexCount];
	memset(clusterWeight, 0, vertexCount * sizeof(double));

	// for all skins and all cluster, accumulate their deformation and weight
	// on each vertices and store them in clusterDeformation and clusterWeight.
	for (int skinIndex = 0; skinIndex < skinCount; ++skinIndex)
	{
		FbxSkin *skinDeformer = (FbxSkin *)pMesh->GetDeformer(skinIndex, FbxDeformer::eSkin);
		int clusterCount = skinDeformer->GetClusterCount();
		for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
		{
			FbxCluster *cluster = skinDeformer->GetCluster(clusterIndex);
			if (!cluster->GetLink())
			{
				continue;
			}

			FbxAMatrix vertexTransformMatrix;
			computeClusterDeformation(pGlobalPosition, pMesh, cluster, vertexTransformMatrix, pTime, pPose);

			FbxQuaternion Q = vertexTransformMatrix.GetQ();
			FbxVector4 T = vertexTransformMatrix.GetT();
			FbxDualQuaternion dualQuaternion(Q, T);

			int vertexIndexCount = cluster->GetControlPointIndicesCount();
			for (int k = 0; k < vertexIndexCount; ++k)
			{
				int index = cluster->GetControlPointIndices()[k];

				// sometimes, the mesh can have less points than at the time of the
				// skinning, because a smooth operator was active when skinning but
				// has been deactivated during export.
				if (index >= vertexCount)
				{
					continue;
				}

				double weight = cluster->GetControlPointWeights()[k];

				if (weight == 0.0)
				{
					continue;
				}

				// compute the influence of the link on the vertex.
				FbxDualQuaternion influence = dualQuaternion * weight;
				if( clusterMode == FbxCluster::eAdditive)
				{
					// simply influenced by the dual quaternion.
					dqClusterDeformation[index] = influence;

					// set the link to 1.0 just to know this vertex is influenced
					// by a link.
					clusterWeight[index] = 1.0;
				}
				else
				{
					if (clusterIndex == 0)
					{
						dqClusterDeformation[index] = influence;
					}
					else
					{
						// add to the sum of the deformations on the vertex.
						// make sure the deformation is accumulated in the same rotation direction.
						// use dot product to judge the sign.
						double sign = dqClusterDeformation[index].GetFirstQuaternion()
							.DotProduct(dualQuaternion.GetFirstQuaternion());
						if (sign >= 0.0)
						{
							dqClusterDeformation[index] += influence;
						}
						else
						{
							dqClusterDeformation[index] -= influence;
						}
					}
					// add to the sum of weights to either normalize or complete the vertex.
					clusterWeight[index] += weight;
				}
			}// for each vertex
		}// clusterCount
	}

	// actually deform each vertices here by information stored
	// in clusterDeformation and clusterWeight
	for (int i = 0; i < vertexCount; ++i)
	{
		FbxVector4 srcVertex = pVertexArray[i];
		FbxVector4 & dstVertex = pVertexArray[i];
		double weightSum = clusterWeight[i];

		// deform the vertex if there was at least a link with a influence on the vertex.
		if (weightSum != 0.0)
		{
			dqClusterDeformation[i].Normalize();
			dstVertex = dqClusterDeformation[i].Deform(dstVertex);

			if (clusterMode == FbxCluster::eNormalize)
			{
				// in the normalized link mode, a vertex is always totally influenced by the links.
				dstVertex /= weightSum;
			}
			else if (clusterMode == FbxCluster::eTotalOne)
			{
				// in the total 1 link mode, a vertex can be partially influenced by the links.
				srcVertex *= (1.0 - weightSum);
				dstVertex += srcVertex;
			}
		}
	}

	delete[] dqClusterDeformation;
	delete[] clusterWeight;
}

void computeSkinDeformation(FbxAMatrix & pGlobalPosition,
	FbxMesh *pMesh,
	FbxTime &pTime,
	FbxVector4 *pVertexArray,
	FbxPose *pPose)
{
	FbxSkin *skinDeformer = (FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin);
	FbxSkin::EType skinningType = skinDeformer->GetSkinningType();

	if (skinningType == FbxSkin::eLinear || skinningType == FbxSkin::eRigid)
	{
		computeLinearDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
	}
	else if (skinningType == FbxSkin::eDualQuaternion)
	{
		computeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
	}
	else if (skinningType == FbxSkin::eBlend)
	{
		int vertexCount = pMesh->GetControlPointsCount();

		FbxVector4 *vertexArrayLinear = new FbxVector4[vertexCount];
		memcpy(vertexArrayLinear, pMesh->GetControlPoints(), vertexCount * sizeof(FbxVector4));

		FbxVector4* vertexArrayDQ = new FbxVector4[vertexCount];
		memcpy(vertexArrayDQ, pMesh->GetControlPoints(), vertexCount * sizeof(FbxVector4));

		computeLinearDeformation(pGlobalPosition, pMesh, pTime, vertexArrayLinear, pPose);
		computeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, vertexArrayLinear, pPose);

		int blendWeightsCount = skinDeformer->GetControlPointIndicesCount();
		for (int bwIndex = 0; bwIndex < blendWeightsCount; ++bwIndex)
		{
			double blendWeight = skinDeformer->GetControlPointBlendWeights()[bwIndex];
			pVertexArray[bwIndex] = vertexArrayDQ[bwIndex] * blendWeight + vertexArrayLinear[bwIndex] * (1 - blendWeight);
		}
	}
}


void drawMesh(FbxNode *pNode, GameContext *gameContext, FbxTime &pTime, FbxAnimLayer *pAnimLayer,
	FbxAMatrix & pGlobalPosition, FbxPose *pPose)
{
	FbxMesh *lMesh = pNode->GetMesh();
	const int lVertexCount = lMesh->GetControlPointsCount();

	if (lVertexCount == 0)
	{
		return;
	}

	const VBOMesh *lMeshCache = static_cast<VBOMesh *>(lMesh->GetUserDataPtr());
	
	// if it has some defomer conection, update the vertices position

	const bool hasVertexCache = false;
	const bool hasShape = false;
	const bool hasSkin = lMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	const bool hasDeformation = hasVertexCache || hasShape || hasSkin;
	FbxVector4 *vertexArray = NULL;
	if (!lMeshCache || hasDeformation)
	{
		vertexArray = new FbxVector4[lVertexCount];
		memcpy(vertexArray, lMesh->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
	}

	if (hasDeformation)
	{
		// active vertex cache deformer will overwrite any other deformer
		if (hasVertexCache)
		{
			// todo readVertexCacheData
		}
		else
		{
			if (hasShape)
			{
				// todo deform the vertex array with the shapes.
			}

			// we need to get the number of clusters
			const int skinCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
			int clusterCount = 0;
			for (int skinIndex = 0; skinIndex < skinCount; ++skinIndex)
			{
				clusterCount += ((FbxSkin *)(lMesh->GetDeformer(skinIndex, FbxDeformer::eSkin)))->GetClusterCount();
			}
			if (clusterCount)
			{
				// deform the vertex array with the skin deformer
				computeSkinDeformation(pGlobalPosition, lMesh, pTime, vertexArray, pPose);
			}
		}

		if (lMeshCache)
		{
			lMeshCache->updateVertexPosition(lMesh, vertexArray);
		}
	}
	
	if (lMeshCache)
	{
		// begin draw
		lMeshCache->beginDraw();
		const int subMeshCount = lMeshCache->getSubMeshCount();
		for (int i = 0; i < subMeshCount; i++)
		{
			const FbxSurfaceMaterial *material = pNode->GetMaterial(i);
			if (material)
			{
				const MaterialCache *materialCache = static_cast<const MaterialCache *>(material->GetUserDataPtr());

				if (materialCache)
				{
					materialCache->setCurrentMaterial(gameContext);
				}
				else
				{
					materialCache->setDefaultMaterial(gameContext);
				}
			}
			
			// draw
			lMeshCache->draw(gameContext, pNode->EvaluateGlobalTransform(), i);
		}
		//end draw
		lMeshCache->endDraw();
	}
	else
	{
		
	}

	delete[] vertexArray;
}
void drawNode(FbxNode *pNode, 
	GameContext *gameContext,
	FbxTime & pTime,
	FbxAnimLayer *pAnimLayer,
	FbxAMatrix & pParentGlobalPosition,
	FbxAMatrix & pGlobalPosition,
	FbxPose *pPose)
{
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();

	if (lNodeAttribute)
	{
		switch (lNodeAttribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			drawMesh(pNode, gameContext, pTime, pAnimLayer, pGlobalPosition, pPose);
			break;
		default:
			break;
		}
	}
}

void drawNodeRecursive(FbxNode *pNode, GameContext *gameContext, FbxTime & pTime,
	FbxAnimLayer *pAnimLayer, FbxAMatrix &pParentGlobalPosition, FbxPose *pPose)
{
	FbxAMatrix globalPosition = getGlobalPosition(pNode, pTime, pPose, &pParentGlobalPosition);
	if (pNode->GetNodeAttribute())
	{
		// geometry offset.
		// it is not inherited by the children.
		FbxAMatrix geometryOffset = getGeometry(pNode);
		FbxAMatrix globalOffPosition = globalPosition * geometryOffset;
		drawNode(pNode, gameContext, pTime, pAnimLayer, pParentGlobalPosition, globalPosition, pPose);

	}
	const int lChildCount = pNode->GetChildCount();
	for (int i = 0; i < lChildCount; i++)
	{
		drawNodeRecursive(pNode->GetChild(i), gameContext, pTime, pAnimLayer, globalPosition, pPose);
	}
}

bool SceneContext::onDisplay(GameContext* gameContext)
{
	mCurrentTime += mFrameTime;
	FbxNode *rootNode = mScene->GetRootNode();

	glViewport(0, 0, gameContext->mWidth, gameContext->mHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
	glUseProgram(gameContext->mLightShaderProgram->programObject);
	gameContext->mLightShaderProgram->modelLoc = glGetUniformLocation(gameContext->mLightShaderProgram->programObject, "modelMatrix");
	gameContext->mLightShaderProgram->viewLoc = glGetUniformLocation(gameContext->mLightShaderProgram->programObject, "viewMatrix");
	gameContext->mLightShaderProgram->proLoc = glGetUniformLocation(gameContext->mLightShaderProgram->programObject, "proMatrix");
	
	displayTestLight(gameContext);
	
	glUseProgram(gameContext->mShaderProgram->programObject);

	gameContext->mShaderProgram->modelLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "modelMatrix");
	gameContext->mShaderProgram->viewLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "viewMatrix");
	gameContext->mShaderProgram->proLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "proMatrix");

	GLfloat eyePosition[] = {gameContext->eyePos[0], gameContext->eyePos[1], gameContext->eyePos[2]};
	glUniform3fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "view_position"), 1, eyePosition);

	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "light_color"), 1, gameContext->lightColor);
	
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "light_position"), 1, gameContext->lightPosition);


	FbxPose *pose = NULL;
	if (mPoseIndex != -1)
	{
		pose = mScene->GetPose(mPoseIndex);
	}

	// if one node is selected, draw it and its children.
	FbxAMatrix dummyGlobalPosition;

	// todo mSelectedNode
	if (false)
	{
		// set the lighting before other things.
		
	}
	else // otherwise, draw the whole scene.
	{
		drawNodeRecursive(rootNode, gameContext, mCurrentTime, mCurrentAnimLayer, dummyGlobalPosition, pose);
		displayGrid(gameContext, dummyGlobalPosition);
	}
	
	return true;
}

void SceneContext::loadTestLight(GameContext *gameContext)
{
	gameContext->lightPosition = new GLfloat[4]{ 200.0, 200.0, 200.0, 1.0 };
	gameContext->lightColor = new GLfloat[4]{ 1.0, 1.0, 1.0, 1.0 };
	gameContext->lightSize = 1.0;
	GLfloat lightSize = 1.0;
	GLfloat lightColor[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat vertices[] = {
		gameContext->lightPosition[0] - gameContext->lightSize, gameContext->lightPosition[1] - gameContext->lightSize, gameContext->lightPosition[2] + gameContext->lightSize, 1,
		gameContext->lightPosition[0] + gameContext->lightSize, gameContext->lightPosition[1] - gameContext->lightSize, gameContext->lightPosition[2] + gameContext->lightSize, 1,
		gameContext->lightPosition[0] + gameContext->lightSize, gameContext->lightPosition[1] + gameContext->lightSize, gameContext->lightPosition[2] + gameContext->lightSize, 1,
		gameContext->lightPosition[0] - gameContext->lightSize, gameContext->lightPosition[1] + gameContext->lightSize, gameContext->lightPosition[2] + gameContext->lightSize, 1,
		gameContext->lightPosition[0] - gameContext->lightSize, gameContext->lightPosition[1] - gameContext->lightSize, gameContext->lightPosition[2] - gameContext->lightSize, 1,
		gameContext->lightPosition[0] + gameContext->lightSize, gameContext->lightPosition[1] - gameContext->lightSize, gameContext->lightPosition[2] - gameContext->lightSize, 1,
		gameContext->lightPosition[0] + gameContext->lightSize, gameContext->lightPosition[1] + gameContext->lightSize, gameContext->lightPosition[2] - gameContext->lightSize, 1,
		gameContext->lightPosition[0] - gameContext->lightSize, gameContext->lightPosition[1] + gameContext->lightSize, gameContext->lightPosition[2] - gameContext->lightSize, 1
	};
	GLuint indices[] = {
		0, 1, 2, 0, 2, 3,
		0, 1, 5, 0, 5, 4,
		0, 4, 7, 0, 7, 3,
		1, 5, 6, 1, 6, 2,
		4, 5, 6, 4, 6, 7,
		3, 2, 6, 3, 6, 7
	};
	glGenBuffers(1, &testLightVBO);
	glBindBuffer(GL_ARRAY_BUFFER, testLightVBO);
	glBufferData(GL_ARRAY_BUFFER, 8 * 4 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &testLightIndiceVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testLightIndiceVBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * 6 * sizeof(GLuint), indices, GL_STATIC_DRAW);

	

}

void SceneContext::displayGrid(GameContext *gameContext, const FbxAMatrix & pTransform)
{
	
}

//GLfloat *getMatrix(FbxMatrix mat)
//{
//	GLfloat *ret = new GLfloat[16];
//	double *tmp = (double *)mat;
//	for (int i = 0; i < 16; i++)
//	{
//		ret[i] = tmp[i];
//	}
//	return ret;
//}
GLfloat *getMatrix(FbxMatrix mat);
void SceneContext::displayTestLight(GameContext *gameContext)
{
	glBindBuffer(GL_ARRAY_BUFFER, testLightVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testLightIndiceVBO);

	FbxMatrix globalTransform;
	globalTransform.SetIdentity();
	GLfloat* model = getMatrix(globalTransform);
	glUniformMatrix4fv((gameContext->mLightShaderProgram)->modelLoc, 1, GL_FALSE, model);

	GLfloat *view = getMatrix(gameContext->viewMatrix);
	glUniformMatrix4fv((gameContext->mLightShaderProgram)->viewLoc, 1, GL_FALSE, view);

	GLfloat *projection = getMatrix(gameContext->proMatrix);
	glUniformMatrix4fv((gameContext->mLightShaderProgram)->proLoc, 1, GL_FALSE, projection);

	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "light_color"), 1, gameContext->lightColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "light_position"), 1, gameContext->lightPosition);

	
	delete[] model;
	delete[] view;
	delete[] projection;
	
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	
}

