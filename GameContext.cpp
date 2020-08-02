#include "GameContext.h"
#include "ShaderProgram.h"
#include "SceneContext.h"
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
GameContext::GameContext()
	:mWidth(0), mHeight(0),
mShaderProgram(NULL), mSceneContext(NULL),
eglNativeDisplay(NULL), eglNativeWindow(NULL),
eglDisplay(NULL), eglContext(NULL), eglSurface(NULL),
drawFunc(NULL), updateFunc(NULL), shutdownFunc(NULL), keyFunc(NULL)
{
	modelMatrix.SetIdentity();
	viewMatrix.SetIdentity();
	proMatrix.SetIdentity();
}
GameContext::~GameContext()
{
	delete mShaderProgram;
	delete mSceneContext;
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
	
	// Store the program object
	//userData->programObject = programObject;
	
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
