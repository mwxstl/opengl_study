#include "GameContext.h"
#include "ShaderProgram.h"
#include "SceneContext.h"
#include "Transform.h"

GLuint loadShader(GLenum type, const char *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);

	if (shader == 0)
	{
		return 0;
	}

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint infoLen = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			char *infoLog = (char *)malloc(sizeof(char) * infoLen);

			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);

			free(infoLog);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;

}

GLuint loadProgram(const char *vertShaderSrc, const char *fragShaderSrc)
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	// Load the vertex/fragment shaders
	vertexShader = loadShader(GL_VERTEX_SHADER, vertShaderSrc);

	if (vertexShader == 0)
	{
		return 0;
	}

	fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragShaderSrc);

	if (fragmentShader == 0)
	{
		glDeleteShader(vertexShader);
		return 0;
	}

	// Create the program object
	programObject = glCreateProgram();

	if (programObject == 0)
	{
		return 0;
	}

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		GLint infoLen = 0;

		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			char *infoLog = (char *)malloc(sizeof(char) * infoLen);

			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);

			free(infoLog);
		}

		glDeleteProgram(programObject);
		return 0;
	}

	// Free up no longer needed shader resources
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return programObject;
}
GameContext::GameContext(GLint pWidth, GLint pHeight)
	:mWidth(pWidth), mHeight(pHeight),
	mShaderProgram(NULL), mSceneContext(NULL),
	eglNativeDisplay(NULL), eglNativeWindow(NULL),
	eglDisplay(NULL), eglContext(NULL), eglSurface(NULL),
	drawFunc(NULL), updateFunc(NULL), shutdownFunc(NULL), keyFunc(NULL),
	mMouseX(0), mMouseY(0), mCameraStatus(CAMERA_NOTHING)
{
	modelMatrix.SetIdentity();
	viewMatrix.SetIdentity();
	proMatrix.SetIdentity();

	eyePos.Set(0, 0, 600.0);
	lookAt.Set(0, 0, 0);
	upDir.Set(0, 1, 0);
	
	setViewMatrix();

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esPerspective(&projection, 45.0f, mWidth / mHeight, 0.1f, 3000.0f);
	
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			proMatrix.Set(i, j, projection.m[i][j]);
		}
	}
}
GameContext::~GameContext()
{
	delete mShaderProgram;
	delete mSceneContext;
}

void GameContext::setViewMatrix()
{
	/*while (yaw >= 2.0 * 3.1415926)
	{
		yaw -= 2.0f * 3.1415926;
	}
	while (yaw <= -2.0 * 3.1415926)
	{
		yaw += 2.0f * 3.1415926;
	}
	while (pitch >= 0.5 * 3.1415926)
	{
		pitch = 89.5 / 180.0 * 3.1415926;
	}
	while (pitch <= -0.5 * 3.1415926)
	{
		pitch = -89.5 / 180.0 * 3.1415926;
	}
	float eyePosX = cos(pitch) * cos(yaw);
	float eyePosY = sin(pitch);
	float eyePosZ = cos(pitch) * sin(yaw);
	FbxVector4 eyePos(eyeLength * eyePosX, eyeLength * eyePosY, eyeLength * eyePosZ);
	FbxVector4 lookAt(0, 0, 0);
	FbxVector4 upDir(eyePosX, 1, eyePosZ);
	upDir.Normalize();*/
	
	viewMatrix.SetLookAtRH(eyePos, lookAt, upDir);
	
	FbxVector4 eyeDir = lookAt - eyePos;
	float eyeLength = eyeDir[0] * eyeDir[0] + eyeDir[1] * eyeDir[1] + eyeDir[2] * eyeDir[2];
	eyeLength = sqrt(eyeLength);
	
	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esPerspective(&projection, 45.0f, mWidth / mHeight, 0.1f, eyeLength * 2.0);

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			proMatrix.Set(i, j, projection.m[i][j]);
		}
	}
}

bool GameContext::loadShaderProgram()
{
	mShaderProgram = new ShaderProgram();
	char vShaderStr[] =
		"#version 300 es							\n"
		"uniform mat4 proMatrix;					\n"
		"uniform mat4 modelMatrix;					\n"
		"uniform mat4 viewMatrix;					\n"
		"uniform vec4 a_color;						\n"
		"layout(location = 0) in vec4 v_position;	\n"
		"out vec4 v_color;							\n"
		"void main()								\n"
		"{											\n"
		"   gl_Position = proMatrix * viewMatrix * modelMatrix * v_position;	\n"
		"	v_color = a_color;						\n"
		"}											\n";

	char fShaderStr[] =
		"#version 300 es                            \n"
		"precision mediump float;                   \n"
		"in vec4 v_color;							\n"
		"out vec4 fragColor;                        \n"
		"void main()                                \n"
		"{                                          \n"
		"   fragColor = v_color;					\n"
		"}											\n";
	GLuint programObject;
	// Create the program object
	programObject = loadProgram(vShaderStr, fShaderStr);
	if (programObject == 0)
	{
		return false;
	}
	mShaderProgram->programObject = programObject;


	glEnable(GL_DEPTH_TEST);
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	return true;
}

bool GameContext::loadScene(FbxString pFileName)
{
	try
	{
		mSceneContext = new SceneContext(pFileName);

	}
	catch (std::bad_alloc)
	{
		cout << "unable to load scene!\n";
		return false;
	}
	return mSceneContext->loadFile();
}
