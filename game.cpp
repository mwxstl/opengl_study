// The MIT License (MIT)
//
// Copyright (c) 2013 Dan Ginsburg, Budirijanto Purnomo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
// Book:      OpenGL(R) ES 3.0 Programming Guide, 2nd Edition
// Authors:   Dan Ginsburg, Budirijanto Purnomo, Dave Shreiner, Aaftab Munshi
// ISBN-10:   0-321-93388-5
// ISBN-13:   978-0-321-93388-1
// Publisher: Addison-Wesley Professional
// URLs:      http://www.opengles-book.com
//            http://my.safaribooksonline.com/book/animation-and-3d/9780133440133
//
// Example_6_6.c
//
//    This example demonstrates drawing a primitive with
//    a separate VBO per attribute
//
#include "esUtil.h"
#include <fbxsdk.h>
#include <vector>
#include "SceneContext.h"
#include "UserData.h"
const int DEFAULT_WINDOW_WIDTH = 960;
const int DEFAULT_WINDOW_HEIGHT = 720;

const char * GAME_NAME = "MyGame";



bool loadProgram(ESContext* esContext)
{
	UserData *userData = (UserData *)esContext->userData;
	char vShaderStr[] =
		"#version 300 es                          \n"
		"uniform mat4 u_mvpMatrix;				  \n"
		"uniform mat4 model;					  \n"
		"uniform mat4 view;						  \n"
		"uniform mat4 projection;				  \n"
		"uniform vec4 a_color;					  \n"
		"layout(location = 0) in vec4 vPosition;  \n"
		"out vec4 v_color;						  \n"
		"void main()                              \n"
		"{                                        \n"
		"   gl_Position = u_mvpMatrix * vPosition; \n"
		"	v_color = a_color;				  \n"
		"}                                        \n";

	char fShaderStr[] =
		"#version 300 es                              \n"
		"precision mediump float;                     \n"
		"in vec4 v_color;"
		"out vec4 fragColor;                          \n"
		"void main()                                  \n"
		"{                                            \n"
		"   fragColor = v_color;  \n"
		"}                    ";

	GLuint programObject;

	// Create the program object
	programObject = esLoadProgram(vShaderStr, fShaderStr);

	if (programObject == 0)
	{
		return false;
	}

	// Store the program object
	userData->programObject = programObject;
	
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	return true;
}


bool loadScene(ESContext* esContext)
{
	UserData *userData = (UserData *)esContext->userData;
	//FbxString filePath("F:\\resources\\farm-life\\Map_7.fbx");
	//FbxString filePath("F:\\resources\\farm-life\\Animals\\Animals\\Animated\\MediumPigAnimations.fbx");
	//FbxString filePath("D:\\resource\\farm-life\\Animals\\Animals\\Animated\\MediumPigAnimations.fbx");
	FbxString filePath("D:\\resource\\farm-life\\Buildings\\Stable.fbx");
	//FbxString filePath("D:\\resource\\farm-life\\Map_7.fbx");
	try
	{
		userData->sceneContext = new SceneContext(filePath, esContext->width, esContext->height, glGetUniformLocation(userData->programObject, "u_mvpMatrix"));
		
	}
	catch (std::bad_alloc)
	{
		cout << "unable to load scene!\n";
		return false;
	}
	
	SceneContext *sceneContext = userData->sceneContext;

	return sceneContext->loadFile();
	
}
int Init(ESContext *esContext)
{
	if (!loadProgram(esContext) || !loadScene(esContext))
	{
		return FALSE;
	}
	return TRUE;
}

void Update(ESContext *esContext, float deltaTime)
{
	UserData *userData = (UserData *) esContext->userData;
	
}

void Draw(ESContext *esContext)
{
	UserData *userData = (UserData *)esContext->userData;

	glViewport(0, 0, esContext->width, esContext->height);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(userData->programObject);
	userData->sceneContext->onDisplay(esContext);

}


void Shutdown(ESContext *esContext)
{
	UserData *userData = (UserData *)esContext->userData;

	glDeleteProgram(userData->programObject);
}

int esMain(ESContext *esContext)
{
	esContext->userData = malloc(sizeof(UserData));

	esCreateWindow(esContext, GAME_NAME, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, ES_WINDOW_RGB);

	if (!Init(esContext))
	{
		return GL_FALSE;
	}

	esRegisterShutdownFunc(esContext, Shutdown);
	esRegisterDrawFunc(esContext, Draw);
	//esRegisterUpdateFunc(esContext, Update);
	return GL_TRUE;
}
