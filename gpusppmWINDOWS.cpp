// gpusppm, a GPU implementation of Stochastic Progressive Photon Mapping by T. Hachisuka
// gpusppm.exe is a Windows executable file.
// 10/30/2009: initial release.
// 07/01/2010: examples of full spectrum sampling and glossy reflections are added.

#include <stdlib.h>
#include <time.h>

#ifdef __APPLE__ // thanks for (http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16239)
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#  include <GL/glext.h>
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>



// the entry point API function
#if defined(_WIN32)
#  define glGetProcAddress(name) wglGetProcAddress(name)
#else
#  if defined(__APPLE__)
#    define glGetProcAddress(name) NSGLGetProcAddress(name)
#  else
#    if defined(__sgi) || defined(__sun)
#      define glGetProcAddress(name) dlGetProcAddress(name)
#    else // (thanks for http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16217)
#      define glGetProcAddress(name) (*glXGetProcAddressARB)((GLubyte*) name)
#    endif
#  endif
#endif



unsigned int FrameCount = 0;

GLuint PSDraw, PSHash, PSRayTrace, PSProgressiveUpdate, PSRadianceEstimate, PVSScatter, PVSCorrection;

float HashScale;
float BBMin[3], BBMax[3], BBSize[3];
float TotalPhotonNum = 0.0f;
float BufInfo[4];

GLuint QueryPositionTexture;
GLuint QueryNormalTexture;
GLuint QueryEmissionPhotonCountTexture;
GLuint QueryFluxRadiusTexture;
GLuint QueryReflectanceTexture;

GLuint PhotonIndexTexture;
GLuint PhotonFluxTexture;
GLuint PhotonPositionTexture;
GLuint PhotonDirectionTexture;
GLuint PhotonHashTexture;
GLuint PhotonCorrectionTexture;
GLuint RandomTexture;

GLuint EyeRayTraceSurface;
GLuint PhotonRayTraceSurface;

GLuint PhotonIndexSurface;
GLuint QueryPointSurface;
GLuint PhotonHashSurface;
GLuint PhotonHashDepthBuffer;
GLuint PhotonCorrectionSurface;

GLuint FragmentsVBO;



// max bounce
#define MAX_PATH_LENGTH 4


clock_t nextUpdate = clock_t(0);
clock_t startedTime = clock_t(0);

const int BufferSize = 512;
const int PhotonBufferSize = BufferSize;
const int HashNum = BufferSize * BufferSize;

float TempData[BufferSize * BufferSize * 4];
float VBOData[BufferSize * BufferSize * 2];
float InitialRadius = 0.0f;


// GL extention entry points
// glActiveTexture and possibly other functions could be predefined in some version of gl.h.
// if you get an compile error, just comment out corresponding function definitions. 
// (thanks for http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16217)
#ifndef __APPLE__ // thanks for (http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16239)
	PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
	PFNGLUNIFORM1IPROC glUniform1i = NULL;
	PFNGLUNIFORM4FVPROC glUniform4fv = NULL;
	PFNGLUNIFORM3FVPROC glUniform3fv = NULL;
	PFNGLUNIFORM1FPROC glUniform1f = NULL;
	PFNGLUSEPROGRAMPROC glUseProgram = NULL;
	PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = NULL;
	PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT = NULL;
	PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = NULL;
	PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT = NULL;
	PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
	PFNGLCREATESHADERPROC glCreateShader = NULL;
	PFNGLSHADERSOURCEPROC glShaderSource = NULL;
	PFNGLCOMPILESHADERPROC glCompileShader = NULL;
	PFNGLATTACHSHADERPROC glAttachShader = NULL;
	PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
	PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
	PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
	PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
	PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
	PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
	PFNGLDRAWBUFFERSPROC glDrawBuffers = NULL;
	PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = NULL;
	PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = NULL;
	PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = NULL;
	PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = NULL;
	PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
	PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
	PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
	PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
	PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT = NULL;
#endif


// enable/disable profiling
// note : enabling profiling slows down the rendering
//#define PROFILE

// texture output for debugging
//#define DEBUG_OUTPUT
//#define DEBUG_OUTPUT_TEX 1


#ifdef PROFILE
	// profiling functions
	clock_t measurementStart = clock_t(0);
	clock_t profiles[7];
	void startMeasurement()
	{
		measurementStart = clock();
	}
	void endMeasurement(const int i)
	{
		profiles[i] += (clock() - measurementStart);
	}
	void forceSync(const int id)
	{
		// readback the target texture to force GPU-CPU sync
		glActiveTexture(GL_TEXTURE0 + id);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, TempData);
	}
#endif



namespace XORShift
{
	// XOR shift PRNG
	unsigned int x = 123456789;
	unsigned int y = 362436069;
	unsigned int z = 521288629;
	unsigned int w = 88675123; 

	inline float frand()
	{ 
		unsigned int t;
	 
		t = x ^ (x << 11);
		x = y; y = z; z = w;
		return (w = (w ^ (w >> 19)) ^ (t ^ (t >> 8))) * (1.0f / 4294967295.0f); 
	}
}



void InitializePPMData()
{
	// emission & local photon count
	glActiveTexture(GL_TEXTURE5);
	for (int j = 0; j < BufferSize; j++)
	{
		for (int i = 0; i < BufferSize; i++)
		{
			TempData[(i + j * BufferSize) * 4 + 0] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 1] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 2] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 3] = 0.0;
		}
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BufferSize, BufferSize, GL_RGBA, GL_FLOAT, TempData);

	// accumulated (unnormalized) flux & radius
	glActiveTexture(GL_TEXTURE6);
	for (int j = 0; j < BufferSize; j++)
	{
		for (int i = 0; i < BufferSize; i++)
		{
			TempData[(i + j * BufferSize) * 4 + 0] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 1] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 2] = 0.0;
			TempData[(i + j * BufferSize) * 4 + 3] = InitialRadius;
		}
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BufferSize, BufferSize, GL_RGBA, GL_FLOAT, TempData);

	// generate random numbers
	for (int j = 0; j < BufferSize; j++)
	{
		for (int i = 0; i < BufferSize; i++)
		{
			TempData[(i + j * BufferSize) * 4 + 0] = XORShift::frand() * 4194304.0;
			TempData[(i + j * BufferSize) * 4 + 1] = XORShift::frand() * 4194304.0;
			TempData[(i + j * BufferSize) * 4 + 2] = XORShift::frand() * 4194304.0;
			TempData[(i + j * BufferSize) * 4 + 3] = XORShift::frand() * 4194304.0;
		}
	}
	glActiveTexture(GL_TEXTURE4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BufferSize, BufferSize, GL_RGBA, GL_FLOAT, TempData);

	TotalPhotonNum = 0.0;
}


float Wavelength;
float CurrentSampledTime;

int primes[61]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,
83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,
191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283};
inline int rev(const int i,const int p) {if (i==0) return i; else return p-i;}
double hal(const int b, int j) { // Halton sequence with reverse permutation
const int p = primes[b]; double h = 0.0, f = 1.0 / (double)p, fct = f;
while (j > 0) {h += rev(j % p, p) * fct; j /= p; fct *= f;} return h;}

void display(void)
{
	#ifdef PROFILE
		clock_t currentStartTime = clock();
	#endif



	if ((FrameCount % MAX_PATH_LENGTH) == 0) 
	{
		Wavelength = hal(0, FrameCount / 4) * (0.780f - 0.380f) + 0.380f;// * (0.780f - 0.380f) + 0.380f;
		CurrentSampledTime = hal(1, FrameCount / 4);
	}

	if (FrameCount == 0) 
	{
		// intial radius estimate
		// eye ray tracing
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, EyeRayTraceSurface);

		glUseProgram(PSRayTrace);

		glUniform1f(glGetUniformLocation(PSRayTrace, "Time"), CurrentSampledTime);
		glUniform4fv(glGetUniformLocation(PSRayTrace, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PSRayTrace, "RandomTexture"), 4);
		glUniform1i(glGetUniformLocation(PSRayTrace, "QueryEmissionPhotonCountTexture"), 5);
		glUniform1i(glGetUniformLocation(PSRayTrace, "UseEyeRays"), 1);
		glUniform1i(glGetUniformLocation(PSRayTrace, "PathLength"), 0);
		glUniform1f(glGetUniformLocation(PSRayTrace, "MaxPathLength"), MAX_PATH_LENGTH);
		glUniform1f(glGetUniformLocation(PSRayTrace, "Wavelength"), Wavelength);

		glRecti(1, 1, -1, -1);

		// readback the result
		glActiveTexture(GL_TEXTURE3);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, TempData);

		// estimate the AABB of the visible part
		BBMin[0] = 1e+20;
		BBMin[1] = 1e+20;
		BBMin[2] = 1e+20;

		BBMax[0] = -1e+20;
		BBMax[1] = -1e+20;
		BBMax[2] = -1e+20;

		for (int j = 0; j < BufferSize; j++)
		{
			for (int i = 0; i < BufferSize; i++)
			{
				float x = TempData[(i + j * BufferSize) * 4 + 0];
				float y = TempData[(i + j * BufferSize) * 4 + 1];
				float z = TempData[(i + j * BufferSize) * 4 + 2];

				if (x < BBMin[0]) BBMin[0] = x;
				if (y < BBMin[1]) BBMin[1] = y;
				if (z < BBMin[2]) BBMin[2] = z;

				if (x > BBMax[0]) BBMax[0] = x;
				if (y > BBMax[1]) BBMax[1] = y;
				if (z > BBMax[2]) BBMax[2] = z;
			}
		}
		
		// compute the AABB size
		for (int i = 0; i < 3; i++)
		{
			BBSize[i] = BBMax[i] - BBMin[i];
		}

		// initial radius estimation
		InitialRadius = ((BBSize[0] + BBSize[1] + BBSize[2]) / 3.0f) / (float)(BufferSize) * 3.0f;

		// adjust the bounding box
		for (int i = 0; i < 3; i++)
		{
			BBMin[i] -= InitialRadius;
			BBMax[i] += InitialRadius;
		}

		// hashed grid resolution
		HashScale = 1.0f / (InitialRadius * 1.5f);

		// initialize PPM statistics
		InitializePPMData();
	}


	#ifdef PROFILE
		startMeasurement();
	#endif
		// photon tracing
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonRayTraceSurface);

		glUseProgram(PSRayTrace);

		glUniform1f(glGetUniformLocation(PSRayTrace, "Time"), CurrentSampledTime);
		glUniform4fv(glGetUniformLocation(PSRayTrace, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PSRayTrace, "RandomTexture"), 4);
		glUniform1i(glGetUniformLocation(PSRayTrace, "QueryEmissionPhotonCountTexture"), 5);
		glUniform1i(glGetUniformLocation(PSRayTrace, "UseEyeRays"), 0);
		glUniform1i(glGetUniformLocation(PSRayTrace, "PathLength"), FrameCount % MAX_PATH_LENGTH);
		glUniform1f(glGetUniformLocation(PSRayTrace, "MaxPathLength"), MAX_PATH_LENGTH);
		glUniform1f(glGetUniformLocation(PSRayTrace, "Wavelength"), Wavelength);

		glRecti(1, 1, -1, -1);
	#ifdef PROFILE
		forceSync(10);
		endMeasurement(0);
	#endif


	#ifdef PROFILE
		startMeasurement();
	#endif
		// eye ray tracing
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, EyeRayTraceSurface);

		glUseProgram(PSRayTrace);

		glUniform1f(glGetUniformLocation(PSRayTrace, "Time"), CurrentSampledTime);
		glUniform4fv(glGetUniformLocation(PSRayTrace, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PSRayTrace, "RandomTexture"), 4);
		glUniform1i(glGetUniformLocation(PSRayTrace, "QueryEmissionPhotonCountTexture"), 5);
		glUniform1i(glGetUniformLocation(PSRayTrace, "UseEyeRays"), 1);
		glUniform1i(glGetUniformLocation(PSRayTrace, "PathLength"), MAX_PATH_LENGTH);
		glUniform1f(glGetUniformLocation(PSRayTrace, "MaxPathLength"), MAX_PATH_LENGTH);
		glUniform1f(glGetUniformLocation(PSRayTrace, "Wavelength"), Wavelength);

		glRecti(1, 1, -1, -1);
	#ifdef PROFILE
		forceSync(3);
		endMeasurement(1);
	#endif


	#ifdef PROFILE
		startMeasurement();
	#endif
		// compute the hash values
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonIndexSurface);

		glUseProgram(PSHash);

		glUniform4fv(glGetUniformLocation(PSHash, "BufInfo"), 1, BufInfo);
		glUniform1f(glGetUniformLocation(PSHash, "HashNum"), HashNum);
		glUniform1f(glGetUniformLocation(PSHash, "HashScale"), HashScale);
		glUniform3fv(glGetUniformLocation(PSHash, "BBoxMin"), 1, BBMin);
		glUniform1i(glGetUniformLocation(PSHash, "PhotonIndexTexture"), 8);
		glUniform1i(glGetUniformLocation(PSHash, "PhotonPositionTexture"), 10);

		glRecti(1, 1, -1, -1);
	#ifdef PROFILE
		forceSync(8);
		endMeasurement(2);
	#endif


	#ifdef PROFILE
		startMeasurement();
	#endif
		// scatter photons based on their hash values
		glEnable(GL_DEPTH_TEST);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonHashSurface);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(PVSScatter);
		glUniform4fv(glGetUniformLocation(PVSScatter, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PVSScatter, "PhotonIndexTexture"), 8);
		glUniform1f(glGetUniformLocation(PVSScatter, "PhotonBufferSize"), float(PhotonBufferSize));

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, FragmentsVBO); 
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glDrawArrays(GL_POINTS, 0, BufferSize * BufferSize);
		glDisableClientState(GL_VERTEX_ARRAY);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		glDisable(GL_DEPTH_TEST); 
	#ifdef PROFILE
		forceSync(0);
		endMeasurement(3);
	#endif


	#ifdef PROFILE
		startMeasurement();
	#endif
		// count the number of overlapped photons in the hashed grid
		glEnable(GL_BLEND);

		glBlendFunc(GL_ONE, GL_ONE); 

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonCorrectionSurface);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(PVSCorrection);

		glUniform4fv(glGetUniformLocation(PVSCorrection, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PVSCorrection, "PhotonIndexTexture"), 8);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, FragmentsVBO); 
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glDrawArrays(GL_POINTS, 0, BufferSize * BufferSize);
		glDisableClientState(GL_VERTEX_ARRAY);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		glDisable(GL_BLEND);
	#ifdef PROFILE
		forceSync(1);
		endMeasurement(4);
	#endif


	#ifdef PROFILE
		startMeasurement();
	#endif
		// radiance estimation
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, QueryPointSurface);
		glUseProgram(PSProgressiveUpdate);

		glUniform4fv(glGetUniformLocation(PSProgressiveUpdate, "BufInfo"), 1, BufInfo);
		glUniform1f(glGetUniformLocation(PSProgressiveUpdate, "HashNum"), HashNum);
		glUniform1f(glGetUniformLocation(PSProgressiveUpdate, "HashScale"), HashScale);
		glUniform3fv(glGetUniformLocation(PSProgressiveUpdate, "BBoxMin"), 1, BBMin);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "QueryPositionTexture"), 3);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "QueryEmissionPhotonCountTexture"), 5);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "QueryFluxRadiusTexture"), 6);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "QueryReflectanceTexture"), 7);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "QueryDirectionTexture"), 2);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "HashedPhotonTexture"), 0);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "PhotonCorrectionTexture"), 1);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "PhotonFluxTexture"), 9);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "PhotonPositionTexture"), 10);
		glUniform1i(glGetUniformLocation(PSProgressiveUpdate, "PhotonDirectionTexture"), 11);
		glUniform1f(glGetUniformLocation(PSProgressiveUpdate, "Wavelength"), Wavelength);

		glRecti(1, 1, -1, -1);
	#ifdef PROFILE
		forceSync(6);
		endMeasurement(5);
	#endif


	TotalPhotonNum += BufferSize * BufferSize;

	#ifndef DEBUG_OUTPUT
		// rendering
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glUseProgram(PSRadianceEstimate);
		glClear(GL_COLOR_BUFFER_BIT);

		glUniform4fv(glGetUniformLocation(PSRadianceEstimate, "BufInfo"), 1, BufInfo);
		glUniform1f(glGetUniformLocation(PSRadianceEstimate, "TotalPhotonNum"), TotalPhotonNum);
		glUniform1i(glGetUniformLocation(PSRadianceEstimate, "QueryEmissionPhotonCountTexture"), 5);
		glUniform1i(glGetUniformLocation(PSRadianceEstimate, "QueryFluxRadiusTexture"), 6);

		glRecti(1, 1, -1, -1);

	#else
		// debug output
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(PSDraw);

		glUniform4fv(glGetUniformLocation(PSDraw, "BufInfo"), 1, BufInfo);
		glUniform1i(glGetUniformLocation(PSDraw, "input_tex"), DEBUG_OUTPUT_TEX);

		glRecti(1, 1, -1, -1);
	#endif

	// update
	glutSwapBuffers();

	FrameCount++;

	clock_t overtime = clock() - nextUpdate;

	#ifdef PROFILE
		profiles[6] += clock() - currentStartTime;
	#endif

	if (overtime > 0)
	{
		// output some info
		std::stringstream s;
		s << ((clock() - startedTime) / CLOCKS_PER_SEC) << "sec " << (TotalPhotonNum / 1024.0 / 1024.0 / float((clock() - startedTime) / CLOCKS_PER_SEC)) << "MPhotons/sec " << (TotalPhotonNum / 1024.0 / 1024.0) << "MPhotons" << std::endl;
		glutSetWindowTitle(s.str().c_str());

		// profiling
		#ifdef PROFILE
			std::cerr << std::endl;
			std::cerr << "photon tracing     :" << profiles[0] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "eye ray tracing    :" << profiles[1] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "hash precompute    :" << profiles[2] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "hash scatter       :" << profiles[3] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "hash correction    :" << profiles[4] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "progressive update :" << profiles[5] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << "total              :" << profiles[6] / float(FrameCount) / float(CLOCKS_PER_SEC) * 1000.0 << "msec" << std::endl;
			std::cerr << std::endl;
		#endif

		nextUpdate = clock() + CLOCKS_PER_SEC;
	}
}



// error checking for GLSL (from http://www.mathematik.uni-dortmund.de/~goeddeke/gpgpu/tutorial.html)
void printInfoLogs(GLuint obj, GLuint shader)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength > 1)
	{
		infoLog = (char *)malloc(infologLength);
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::cerr << infoLog << std::endl;
		free(infoLog);
	}
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength > 1)
	{
		infoLog = (char *)malloc(infologLength);
		glGetShaderInfoLog(shader, infologLength, &charsWritten, infoLog);
		std::cerr << infoLog << std::endl;
		free(infoLog);
	}
}



GLuint CreateFullShader(const char* vertex_shader_path, const char* fragment_shader_path)
{
	// create a fragment shader and a vertex shader
	GLuint p = glCreateProgram();

	{
		std::ifstream ifs(vertex_shader_path, std::ios::binary);
		ifs.seekg(0, std::ios::end);
		std::ifstream::pos_type filesize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		std::vector<char> bytes(filesize);
		ifs.read(&bytes[0], filesize);
		GLint size = filesize;
		const char* c = &bytes[0];

		GLuint s = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(s,1, &c, &size);
		glCompileShader(s);
		glAttachShader(p,s);
		printInfoLogs(p, s);
	}
	{
		std::ifstream ifs(fragment_shader_path, std::ios::binary);
		ifs.seekg(0, std::ios::end);
		std::ifstream::pos_type filesize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		std::vector<char> bytes(filesize);
		ifs.read(&bytes[0], filesize);
		GLint size = filesize;
		const char* c = &bytes[0];

		GLuint s = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(s,1, &c, &size);
		glCompileShader(s);
		glAttachShader(p,s);
		printInfoLogs(p, s);
	}

	glLinkProgram(p);

	return p;
}



GLuint CreateFragmentShader(const char* shader_path)
{
	// create a fragment shader (the vertex shader is using the fixed-function pipeline)
	GLuint p = glCreateProgram();

	{
		std::ifstream ifs(shader_path, std::ios::binary);
		ifs.seekg(0, std::ios::end);
		std::ifstream::pos_type filesize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);
		std::vector<char> bytes(filesize);
		ifs.read(&bytes[0], filesize);
		GLint size = filesize;
		const char* c = &bytes[0];

		GLuint s = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(s,1, &c, &size);
		glCompileShader(s);
		glAttachShader(p,s);
		printInfoLogs(p, s);
	}

	glLinkProgram(p);

	return p;
}



void mouse(int button, int state, int x, int y)
{
	switch (button) 
	{
		case GLUT_RIGHT_BUTTON:
		{
			if (state == GLUT_DOWN) 
			{
				// initialize rendering
				// reload the shader
				glDeleteProgram(PSRayTrace);
				PSRayTrace = CreateFragmentShader("raytrace.fs");

				std::cerr << x << " " << y << std::endl;

				// initialize misc data
				FrameCount = 0;
				startedTime = clock();

				// profiler
				#ifdef PROFILE
					for (int i = 0; i < 7; i++)
					{
						profiles[i] = clock_t(0);
					}
				#endif
			}

			break;
		}
		default:
		{
			break;
		}
	}
}



void idle(void)
{
	glutPostRedisplay();
}



GLuint CreateTexture(const int TextureIndex, GLenum format) 
{
	// create a screen-sized 32bits floating point texture
	GLuint Texture;

	glActiveTexture(GL_TEXTURE0 + TextureIndex);
	glGenTextures(1, &Texture);
	glBindTexture(GL_TEXTURE_2D, Texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, format, BufferSize, BufferSize, 0, GL_LUMINANCE, GL_FLOAT, 0);

	return Texture;
}



int main(int argc, char *argv[])
{
	// GL things
	glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - BufferSize) / 2, (glutGet(GLUT_SCREEN_HEIGHT) - BufferSize) / 2);
	glutInitWindowSize(BufferSize, BufferSize);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GL_DOUBLE | GLUT_DEPTH);
	glutCreateWindow(argv[0]);
	glutDisplayFunc(display);
	glutMouseFunc(mouse);
	glutIdleFunc(idle);


	BufInfo[0] = float(BufferSize);
	BufInfo[1] = float(BufferSize);
	BufInfo[2] = 1.0f / float(BufferSize);
	BufInfo[3] = 1.0f / float(BufferSize);


	// profiler
	#ifdef PROFILE
		for (int i = 0; i < 7; i++)
		{
			profiles[i] = clock_t(0);
		}
	#endif

#ifndef __APPLE__ // thanks for (http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16239)
	// obtain entry points to gl extensions
	glActiveTexture = (PFNGLACTIVETEXTUREPROC)glGetProcAddress("glActiveTexture");
	glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glGetProcAddress("glGetUniformLocation");
	glUniform1i = (PFNGLUNIFORM1IPROC)glGetProcAddress("glUniform1i");
	glUniform4fv = (PFNGLUNIFORM4FVPROC)glGetProcAddress("glUniform4fv");
	glUniform3fv = (PFNGLUNIFORM3FVPROC)glGetProcAddress("glUniform3fv");
	glUniform1f = (PFNGLUNIFORM1FPROC)glGetProcAddress("glUniform1f");
	glUseProgram = (PFNGLUSEPROGRAMPROC)glGetProcAddress("glUseProgram");
	glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)glGetProcAddress("glBindFramebufferEXT");
	glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)glGetProcAddress("glFramebufferTexture2DEXT");
	glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)glGetProcAddress("glGenFramebuffersEXT");
	glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC)glGetProcAddress("glDeleteFramebuffersEXT");
	glDeleteProgram = (PFNGLDELETEPROGRAMPROC)glGetProcAddress("glDeleteProgram");
	glCreateShader = (PFNGLCREATESHADERPROC)glGetProcAddress("glCreateShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)glGetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)glGetProcAddress("glCompileShader");
	glAttachShader = (PFNGLATTACHSHADERPROC)glGetProcAddress("glAttachShader");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)glGetProcAddress("glLinkProgram");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)glGetProcAddress("glCreateProgram");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glGetProcAddress("glGetProgramInfoLog");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)glGetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glGetProcAddress("glGetShaderInfoLog");
	glDrawBuffers = (PFNGLDRAWBUFFERSPROC)glGetProcAddress("glDrawBuffers");
	glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)glGetProcAddress("glGenRenderbuffersEXT");
	glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)glGetProcAddress("glBindRenderbufferEXT");
	glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)glGetProcAddress("glRenderbufferStorageEXT");
	glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)glGetProcAddress("glFramebufferRenderbufferEXT");
	glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)glGetProcAddress("glGenBuffersARB");
	glBindBufferARB = (PFNGLBINDBUFFERARBPROC)glGetProcAddress("glBindBufferARB");
	glBufferDataARB = (PFNGLBUFFERDATAARBPROC)glGetProcAddress("glBufferDataARB");
	glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)glGetProcAddress("glDeleteBuffersARB");
	glDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC)glGetProcAddress("glDeleteRenderbuffersEXT");
#endif


	// create shaders
	PSDraw = CreateFragmentShader("draw.fs");
	PSHash = CreateFragmentShader("hash.fs");
	PSProgressiveUpdate = CreateFragmentShader("progressive.fs");
	PSRadianceEstimate = CreateFragmentShader("re.fs");
	PVSScatter = CreateFullShader("scatter.vs", "scatter.fs");
	PVSCorrection = CreateFullShader("correction.vs", "correction.fs");
	PSRayTrace = CreateFragmentShader("raytrace.fs");


	// create textures
	// thanks for (http://ompf.org/forum/viewtopic.php?f=10&t=1492#p16239)
	PhotonHashTexture = CreateTexture(0, GL_RGBA32F_ARB);
	PhotonCorrectionTexture = CreateTexture(1, GL_RGBA32F_ARB);

	QueryNormalTexture = CreateTexture(2, GL_RGBA32F_ARB);
	QueryPositionTexture = CreateTexture(3, GL_RGBA32F_ARB);
	RandomTexture = CreateTexture(4, GL_RGBA32F_ARB);
	QueryEmissionPhotonCountTexture = CreateTexture(5, GL_RGBA32F_ARB);
	QueryFluxRadiusTexture = CreateTexture(6, GL_RGBA32F_ARB);
	QueryReflectanceTexture = CreateTexture(7, GL_RGBA32F_ARB);

	PhotonIndexTexture = CreateTexture(8, GL_RGBA32F_ARB);
	PhotonFluxTexture = CreateTexture(9, GL_RGBA32F_ARB);
	PhotonPositionTexture = CreateTexture(10, GL_RGBA32F_ARB);
	PhotonDirectionTexture = CreateTexture(11, GL_RGBA32F_ARB);


	// create FBOs
	glGenFramebuffersEXT(1, &PhotonIndexSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonIndexSurface);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, PhotonIndexTexture, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glGenFramebuffersEXT(1, &PhotonHashSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonHashSurface);
	glGenRenderbuffersEXT(1, &PhotonHashDepthBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, PhotonHashDepthBuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, BufferSize, BufferSize);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, PhotonHashTexture, 0);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, PhotonHashDepthBuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glGenFramebuffersEXT(1, &PhotonCorrectionSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonCorrectionSurface);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, PhotonCorrectionTexture, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	GLenum EyeRayTraceBuffers[] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT};
	glGenFramebuffersEXT(1, &EyeRayTraceSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, EyeRayTraceSurface);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, QueryPositionTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, QueryReflectanceTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, QueryNormalTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_TEXTURE_2D, RandomTexture, 0);
	glDrawBuffers(4, EyeRayTraceBuffers);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	GLenum PhotonRayTraceBuffers[] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT};
	glGenFramebuffersEXT(1, &PhotonRayTraceSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, PhotonRayTraceSurface);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, PhotonPositionTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, PhotonFluxTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, PhotonDirectionTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_TEXTURE_2D, RandomTexture, 0);
	glDrawBuffers(4, PhotonRayTraceBuffers);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	GLenum QueryBuffers[] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
	glGenFramebuffersEXT(1, &QueryPointSurface);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, QueryPointSurface);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, QueryFluxRadiusTexture, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, QueryEmissionPhotonCountTexture, 0);
	glDrawBuffers(2, QueryBuffers);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);


	// create a VBO
	glGenBuffersARB(1, &FragmentsVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, FragmentsVBO);

	for (int j = 0; j < BufferSize; j++)
	{
		for (int i = 0; i < BufferSize; i++)
		{
			VBOData[(i + j * BufferSize) * 2 + 0] = 2.0f * ((i + 0.5f) / float(BufferSize)) - 1.0f;
			VBOData[(i + j * BufferSize) * 2 + 1] = 2.0f * ((j + 0.5f) / float(BufferSize)) - 1.0f;
		}
	}
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float) * 2 * BufferSize * BufferSize, (void*)VBOData, GL_STATIC_DRAW_ARB);


	// initialize misc data
	FrameCount = 0;
	glClearColor(0.0, 0.0, 0.0, 0.0);
	startedTime = clock();

	// enter the main loop
	glutMainLoop();


	// delete things
	glDeleteBuffersARB(1, &FragmentsVBO);

	glDeleteProgram(PSDraw);
	glDeleteProgram(PSHash);
	glDeleteProgram(PSProgressiveUpdate);
	glDeleteProgram(PSRadianceEstimate);
	glDeleteProgram(PVSScatter);
	glDeleteProgram(PVSCorrection);
	glDeleteProgram(PSRayTrace);

	glDeleteTextures(1, &QueryPositionTexture);
	glDeleteTextures(1, &QueryNormalTexture);
	glDeleteTextures(1, &QueryEmissionPhotonCountTexture);
	glDeleteTextures(1, &QueryFluxRadiusTexture);
	glDeleteTextures(1, &QueryReflectanceTexture);

	glDeleteTextures(1, &PhotonIndexTexture);
	glDeleteTextures(1, &PhotonFluxTexture);
	glDeleteTextures(1, &PhotonPositionTexture);
	glDeleteTextures(1, &PhotonDirectionTexture);
	glDeleteTextures(1, &RandomTexture);
	glDeleteTextures(1, &PhotonHashTexture);
	glDeleteTextures(1, &PhotonCorrectionTexture);

	glDeleteFramebuffersEXT(1, &PhotonHashSurface);
	glDeleteFramebuffersEXT(1, &PhotonCorrectionSurface);
	glDeleteFramebuffersEXT(1, &PhotonIndexSurface);
	glDeleteFramebuffersEXT(1, &QueryPointSurface);
	glDeleteFramebuffersEXT(1, &EyeRayTraceSurface);
	glDeleteFramebuffersEXT(1, &PhotonRayTraceSurface);
	glDeleteRenderbuffersEXT(1, &PhotonHashDepthBuffer);

	return 0;
}


