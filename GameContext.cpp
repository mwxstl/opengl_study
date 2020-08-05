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
			cout << infoLog << endl;

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
	mShaderProgram(NULL), mLightShaderProgram(NULL), mSceneContext(NULL),
	eglNativeDisplay(NULL), eglNativeWindow(NULL),
	eglDisplay(NULL), eglContext(NULL), eglSurface(NULL),
	drawFunc(NULL), updateFunc(NULL), shutdownFunc(NULL), keyFunc(NULL),
	mMouseX(0), mMouseY(0), mCameraStatus(CAMERA_NOTHING),
	yaw(PI * 3.0 / 2.0), pitch(0.0)
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
	if (mShaderProgram != NULL)
	{
		GLfloat eyePosition[] = { eyePos[0], eyePos[1], eyePos[2] };
		glUniform3fv(glGetUniformLocation(mShaderProgram->programObject, "view_position"), 1, eyePosition);
	}
	
	FbxVector4 eyeDir = lookAt - eyePos;
	float eyeLength = eyeDir[0] * eyeDir[0] + eyeDir[1] * eyeDir[1] + eyeDir[2] * eyeDir[2];
	eyeLength = sqrt(eyeLength);
	
	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esPerspective(&projection, 60.0f, mWidth / mHeight, 0.1f, eyeLength > 100.0 ? eyeLength * 2.0 : 200.0);

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
	mLightShaderProgram = new ShaderProgram();
	char vShaderStr[] =
		"#version 300 es														\n"
		"uniform mat4 proMatrix;												\n"
		"uniform mat4 modelMatrix;												\n"
		"uniform mat4 viewMatrix;												\n"
		//"uniform vec4 a_color;													\n"
		"uniform vec4 light_position;											\n"
		"uniform vec4 view_position;											\n"
		"layout(location = 0) in vec4 v_position;								\n"
		"layout(location = 1) in vec3 v_normal;									\n"
		"layout(location = 2) in vec2 v_text_cord;								\n"
		//"out vec4 v_color;														\n"
		"out vec2 text_cord;													\n"
		"out vec3 normal;														\n"
		"out vec4 FragPos;														\n"
		"out vec4 lightPos;														\n"
		"out vec4 viewPos;														\n"
		"void main()															\n"
		"{																		\n"
		"   gl_Position = proMatrix * viewMatrix * modelMatrix * v_position;	\n"
		//"	v_color = a_color;													\n"
		"	text_cord = v_text_cord;											\n"
		//"	l_color = light_color;												\n"
		"	normal = mat3(transpose(inverse(modelMatrix))) * v_normal;			\n"
		"	FragPos = vec4(modelMatrix * v_position);							\n"
		"	lightPos = vec4(modelMatrix * light_position);						\n"
		"	viewPos = vec4(modelMatrix * view_position);						\n"
		"}																		\n";

	char fShaderStr[] =
		"#version 300 es														\n"
		"precision mediump float;												\n"
		"struct Material {														\n"
		"	vec4 emissive;														\n"
		"	vec4 ambient;														\n"
		"	vec4 diffuse;														\n"
		"	vec4 specular;														\n"
		"	float shininess;													\n"
		"};																		\n"
		"uniform Material material;												\n"
		//"in vec4 v_color;														\n"
		"in vec2 text_cord;														\n"
		"in vec3 normal;														\n"
		"in vec4 FragPos;														\n"
		"in vec4 lightPos;														\n"
		"in vec4 viewPos;														\n"	
		"out vec4 fragColor;													\n"
		"uniform sampler2D our_texture;											\n"
		"uniform vec4 light_color;												\n"
		
		
		"void main()															\n"
		"{																		\n"
		"	vec4 emissive = light_color * material.emissive;					\n"
		//"   fragColor = texture(ourTexture, text_cord) * v_color;				\n"
		//"	float ambientStrength = 0.6;										\n"
		//"	vec4 ambient = ambientStrength * light_color;						\n"
		"	vec4 ambient = light_color * material.ambient;							\n"

		//"	float diffuseStrength = 0.5;										\n"
		"	vec3 norm = normalize(normal);										\n"
		"	vec3 lightDir = normalize(vec3(lightPos) - vec3(FragPos));	\n"
		"	float diff = max(dot(norm, lightDir), 0.0);							\n"
		//"	vec4 diffuse = diffuseStrength * diff * light_color;				\n"
		"	vec4 diffuse = light_color * (diff * material.diffuse);"
	
		//"	float specularStrength = 0.5;										\n"
		"	vec3 viewDir = normalize(vec3(viewPos) - vec3(FragPos));			\n"
		"	vec3 reflectDir = reflect(-lightDir, norm);							\n"
		"	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);			\n"
		"	if (dot(norm, lightDir) < 0.0) {									\n"
		"		spec = 0.0;														\n"
		"	}																	\n"
		//"	vec4 specular = specularStrength * spec * material.specular;				\n"
		"	vec4 specular = light_color * (spec * material.specular);				\n"		

		"   fragColor = 0.2 * emissive + 0.2 * ambient + 0.5 * diffuse + specular;							\n"

		"}																		\n";

	char fLightShaderStr[] =
		"#version 300 es														\n"
		"precision mediump float;												\n"
		"in vec4 v_color;														\n"
		"in vec2 text_cord;														\n"
		"out vec4 fragColor;													\n"
		"uniform sampler2D our_texture;											\n"
		"void main()															\n"
		"{																		\n"
		//"   fragColor = texture(ourTexture, text_cord) * v_color;				\n"
		"   fragColor = vec4(1.0, 0.0, 0.0, 1.0);				\n"
		"}																		\n";
	GLuint programObject, lightProgramObject;
	// Create the program object
	programObject = loadProgram(vShaderStr, fShaderStr);
	lightProgramObject = loadProgram(vShaderStr, fLightShaderStr);
	if (programObject == 0)
	{
		return false;
	}
	mShaderProgram->programObject = programObject;
	mLightShaderProgram->programObject = lightProgramObject;


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

	if (mSceneContext->getSceneStatus() == SceneContext::MUST_BE_LOADED)
	{
		return mSceneContext->loadFile(this);
	}
	return false;
}
