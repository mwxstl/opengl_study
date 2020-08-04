#include "GameContext.h"
#include "SceneContext.h"
#include "SceneCache.h"
#include "ShaderProgram.h"
#include "targa.h"
FbxManager *mManager;
FbxScene *mScene;
FbxImporter *mImporter;
SceneContext::SceneContext(const char* pFileName)
: mFileName(pFileName), mSceneStatus(UNLOADED),
mManager(NULL), mScene(NULL), mImporter(NULL), mCurrentAnimLayer(NULL)
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
	
}

bool SceneContext::loadFile()
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

		// todo get the list of all the animation stack

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
		loadCacheRecursive(mScene, mCurrentAnimLayer);
		return true;
	}
	else
	{
		return false;
	}
}


void SceneContext::loadCacheRecursive(FbxScene* pScene, FbxAnimLayer *pAnimLayer)
{
	loadTestLight();
	
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
				cout << "onlu tga texture are supported now: " << fileName.Buffer() << endl;
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
void drawMesh(FbxNode *pNode, GameContext *gameContext)
{
	FbxMesh *lMesh = pNode->GetMesh();
	const int lVertexCount = lMesh->GetControlPointsCount();

	if (lVertexCount == 0)
	{
		return;
	}

	const VBOMesh *lMeshCache = static_cast<VBOMesh *>(lMesh->GetUserDataPtr());

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
				GLint loc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "a_color");

				if (materialCache)
				{
					materialCache->setCurrentMaterial(loc);
				}
				else
				{
					materialCache->setDefaultMaterial(loc);
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
}
void drawNode(FbxNode *pNode, GameContext *gameContext)
{
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();

	if (lNodeAttribute)
	{
		switch (lNodeAttribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			drawMesh(pNode, gameContext);
			break;
		default:
			break;
		}
	}
}
void drawNodeRecursive(FbxNode *pNode, GameContext *gameContext)
{
	drawNode(pNode, gameContext);
	const int lChildCount = pNode->GetChildCount();
	for (int i = 0; i < lChildCount; i++)
	{
		drawNodeRecursive(pNode->GetChild(i), gameContext);
	}
}

bool SceneContext::onDisplay(GameContext* gameContext)
{
	FbxNode *rootNode = mScene->GetRootNode();
	displayGrid();
	displayTestLight();
	drawNodeRecursive(rootNode, gameContext);
	return true;
}

void SceneContext::loadTestLight()
{
	GLfloat vertices[] = {
		-1, 19, 1, 1,
		1, 19, 1, 1,
		1, 21, 1, 1,
		-1, 21, 1, 1,
		-1, 19, -1, 1,
		1, 19, -1, 1,
		1, 21, -1, 1,
		-1, 21, -1, 1
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

void SceneContext::displayGrid()
{
	
}

GLfloat *getMatrix(FbxMatrix mat)
{
	GLfloat *ret = new GLfloat[16];
	double *tmp = (double *)mat;
	for (int i = 0; i < 16; i++)
	{
		ret[i] = tmp[i];
	}
	return ret;
}

void SceneContext::displayTestLight(GameContext *gameContext)
{
	glBindBuffer(GL_ARRAY_BUFFER, testLightVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testLightIndiceVBO);

	GLfloat* model = getMatrix(globalTransform);
	glUniformMatrix4fv((gameContext->mShaderProgram)->modelLoc, 1, GL_FALSE, model);

	GLfloat *view = getMatrix(gameContext->viewMatrix);
	glUniformMatrix4fv((gameContext->mShaderProgram)->viewLoc, 1, GL_FALSE, view);

	GLfloat *projection = getMatrix(gameContext->proMatrix);
	glUniformMatrix4fv((gameContext->mShaderProgram)->proLoc, 1, GL_FALSE, projection);

	delete[] model;
	delete[] view;
	delete[] projection;
	GLsizei offset = mSubMeshes[materialIndex]->IndexOffset * sizeof(GLuint);
	const GLsizei elementCount = mSubMeshes[materialIndex]->TriangleCount * 3;
	glDrawElements(GL_TRIANGLES, elementCount, GL_UNSIGNED_INT, reinterpret_cast<const GLvoid *>(offset));
}

