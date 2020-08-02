#include "GameContext.h"
#include "SceneContext.h"
#include "SceneCache.h"
#include "ShaderProgram.h"
FbxManager *mManager;
FbxScene *mScene;
FbxImporter *mImporter;
SceneContext::SceneContext(const char* pFileName)
	:mFileName(pFileName), mSceneStatus(UNLOADED),
mManager(NULL), mScene(NULL), mImporter(NULL)
{
	if (mFileName == NULL)
	{
		cout << "error: fileName is NULL" << endl;
		exit(1);
	}

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

}
SceneContext::~SceneContext()
{
	
}

bool SceneContext::loadFile()
{
	if (mSceneStatus == UNLOADED)
	{
		if (!mImporter->Import(mScene))
		{
			cout << "error: unable to import scene!" << endl;
			return false;
		}

		// convert axis system.
		FbxAxisSystem sceneAxisSystem = mScene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem ourAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
		if (sceneAxisSystem != ourAxisSystem)
		{
			ourAxisSystem.ConvertScene(mScene);
		}

		//convert unit system.
		FbxSystemUnit sceneSystemUnit = mScene->GetGlobalSettings().GetSystemUnit();
		if (sceneSystemUnit != 1.0)
		{
			FbxSystemUnit::cm.ConvertScene(mScene);
		}

		//convert mesh, nurbs and patch into triangle mesh
		FbxGeometryConverter lGeomConverter(mManager);
		try {
			lGeomConverter.Triangulate(mScene, /*replace*/true);
		}
		catch (std::runtime_error) {
			cout << "Scene integrity verification failed!" << endl;
			return false;
		}
		loadCacheRecursive(mScene);
	}
	mSceneStatus = MUST_BE_REFRESHED;
	return true;
}
void SceneContext::loadCacheRecursive(FbxScene* pScene)
{
	//load the textures into gpu, only for file texture now
	const int count = pScene->GetTextureCount();
	for (int i = 0; i < count; i++)
	{
		FbxTexture *texture = pScene->GetTexture(i);
		FbxFileTexture *fileTexture = FbxCast<FbxFileTexture>(texture);
		if (fileTexture && !fileTexture->GetUserDataPtr())
		{
			//todo load
		}

	}
	loadCacheRecursive(pScene->GetRootNode());
}

void SceneContext::loadCacheRecursive(FbxNode* pNode)
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
	}

	const int childCount = pNode->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		loadCacheRecursive(pNode->GetChild(i));
	}
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
	drawNodeRecursive(rootNode, gameContext);
	return true;
}
