#include "SceneContext.h"
#include <vector>
#include "SceneCache.h"
SceneContext::SceneContext(const char* pFileName, int pWindowWidth, int pWindowHeight, GLint pMvpLoc)
	:mFileName(pFileName), mSceneStatus(UNLOADED),
mManager(NULL), mScene(NULL), mImporter(NULL),
mWindowWidth(pWindowWidth), mWindowHeight(pWindowHeight),
mMvpLoc(pMvpLoc), mAngle(0), vboIds{0, 0}
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

	if (!mImporter->Import(mScene))
	{
		cout << "error: unable to import scene!" << endl;
		exit(1);
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
		exit(1);
	}

	ESMatrix perspective;
	ESMatrix modelview;
	float    aspect;

	// Compute the window aspect ratio
	aspect = (GLfloat)mWindowWidth / (GLfloat)mWindowHeight;

	// Generate a perspective matrix with a 60 degree FOV
	esMatrixLoadIdentity(&perspective);
	esPerspective(&perspective, 60.0f, aspect, 1.0f, 20.0f);

	// Generate a model view matrix to rotate/translate the cube
	esMatrixLoadIdentity(&modelview);

	// Translate away from the viewer
	esTranslate(&modelview, 0.0, 0.0, -10.0);

	// scale
	esScale(&modelview, 0.01, 0.01, 0.01);


	// Rotate the cube
	//esRotate(&modelview, userData->angle, 1.0, 1.0, 1.0);


	// Compute the final MVP by multiplying the
	// modevleiw and perspective matrices together
	esMatrixMultiply(&mVpMatrix, &modelview, &perspective);
}
SceneContext::~SceneContext()
{
	
}
struct Vertex
{
	GLfloat x, y, z;
};
vector<Vertex> vertices;
vector<int> indices;
int verticesCount = 0;
int indicesCount = 0;
GLfloat *triangleVertices;
GLuint *triangleIndices;
void readMesh(FbxMesh *mesh)
{
	int count = mesh->GetControlPointsCount();
	FbxVector4 *points = mesh->GetControlPoints();
	for (int i = 0; i < count; i++)
	{
		Vertex v;
		v.x = points[i].mData[0];
		v.y = points[i].mData[1];
		v.z = points[i].mData[2];
		vertices.push_back(v);
	}

	int pvcount = mesh->GetPolygonVertexCount();
	int pcount = mesh->GetPolygonCount();
	for (int i = 0; i < pcount; i++)
	{
		int size = mesh->GetPolygonSize(i);
		for (int j = 0; j < size; j++)
		{
			indices.push_back(indicesCount + mesh->GetPolygonVertex(i, j));
		}
	}
	verticesCount += count;
	indicesCount += count;
}
void parseNode(FbxNode* node)
{
	int attrCount = node->GetNodeAttributeCount();
	for (int i = 0; i < attrCount; i++)
	{
		FbxNodeAttribute *attr = node->GetNodeAttributeByIndex(i);
		if (attr->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh *mesh = (FbxMesh *)attr;
			readMesh(mesh);
		}
	}

	int count = node->GetChildCount();
	for (int i = 0; i < count; i++)
	{
		parseNode(node->GetChild(i));
	}
}
void SceneContext::loadCacheRecursive(FbxScene* pScene)
{
	cout << "asfdasdf" << endl;
	loadCacheRecursive(pScene->GetRootNode());
}

void SceneContext::loadCacheRecursive(FbxNode* pNode)
{
	cout << "step 0" << endl;
	//todo
	//bake material and hook as user data

	//bake mesh as vbo into gpu
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
	if (lNodeAttribute)
	{
		cout << "step 1" << endl;
		if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			cout << "step 2" << endl;
			FbxMesh *lMesh = pNode->GetMesh();
			if (lMesh && !lMesh->GetUserDataPtr())
			{
				cout << "step 3" << endl;
				FbxAutoPtr<VBOMesh> lMeshCache(new VBOMesh);
				if (lMeshCache->initialize(lMesh))
				{
					cout << "step 4" << endl;
					lMesh->SetUserDataPtr(lMeshCache.Release());
				} else
				{
					cout << "get bad";
				}
			}
		}
	}

	int lChildCount = pNode->GetChildCount();
	for (int i = 0; i < lChildCount; i++)
	{
		loadCacheRecursive(pNode->GetChild(i));
	}
}


bool SceneContext::loadFile()
{
	bool lResult = false;
	cout << "loadfile" << endl;
	mSceneStatus = MUST_BE_REFRESHED;

	loadCacheRecursive(mScene);
	/*FbxNode *rootNode = mScene->GetRootNode();
	int childCount = rootNode->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		FbxNode *node = rootNode->GetChild(i);
		parseNode(node);
	}

	triangleVertices = new GLfloat[vertices.size() * 3];
	for (int i = 0; i < vertices.size(); i++)
	{
		triangleVertices[3 * i] = vertices[i].x;
		triangleVertices[3 * i + 1] = vertices[i].y;
		triangleVertices[3 * i + 2] = vertices[i].z;
	}

	triangleIndices = new GLuint[indices.size()];
	for (int i = 0; i < indices.size(); i++)
	{
		triangleIndices[i] = indices[i];
	}*/
	return true;
}

void SceneContext::displayGrid()
{
	
}
GLuint vboIds[] = { 0, 0 };

void drawMesh(FbxNode *pNode, ESContext *esContext)
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
		lMeshCache->draw(esContext);
	} else
	{
	}
}
void drawNode(FbxNode *pNode, ESContext *esContext)
{
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();

	if (lNodeAttribute)
	{
		switch (lNodeAttribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			drawMesh(pNode, esContext);
			break;
		default:
			break;
		}
	}
}

void drawNodeRecursive(FbxNode *pNode, ESContext *esContext)
{
	drawNode(pNode, esContext);
	const int lChildCount = pNode->GetChildCount();
	for (int i = 0; i < lChildCount; i++)
	{
		drawNodeRecursive(pNode->GetChild(i), esContext);
	}
}


bool SceneContext::onDisplay(ESContext *esContext)
{
	/*GLfloat vs[] = {
		0.2f, 0.3f, 0.1f,
		0.1f, 0.7f, 0.2f,
		0.5f, 0.6f, 0.3f,
		0.9f, 0.1f, 0.5f,
	};*/
	//GLfloat *vs = triangleVertices;
	///*GLuint is[] = {
	//	0, 1, 2,
	//	1, 2, 3
	//};*/
	//GLuint *is = triangleIndices;
	//if (vboIds[0] == 0 && vboIds[1] == 0)
	//{
	//	glGenBuffers(2, vboIds);

	//	glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
	//	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * vertices.size(), vs, GL_STATIC_DRAW);

	//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[1]);
	//	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), is, GL_STATIC_DRAW);

	//}

	//glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[1]);

	//glUniformMatrix4fv(mMvpLoc, 1, GL_FALSE, (GLfloat *)&mVpMatrix.m[0][0]);

	////glDrawArrays(GL_TRIANGLES, 0, vertices.size() * 3);
	//glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	FbxNode *lRootNode = mScene->GetRootNode();
	drawNodeRecursive(lRootNode, esContext);
	return true;
}

