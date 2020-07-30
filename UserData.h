#pragma once
#include "esUtil.h"
#include "SceneContext.h"
typedef struct
{
	// Handle to a program object
	GLuint programObject;
	SceneContext *sceneContext;

	/*FbxScene *scene;

	GLuint vboIds[2];

	GLint mvpLoc;

	GLfloat angle;
	ESMatrix mvpMatrix;*/
} UserData;