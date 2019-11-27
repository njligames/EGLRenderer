//
// Copyright 2011 Tero Saarni
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <EGL/egl.h> // requires ndk r5 or newer
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>

#include <strings.h>
#include <android/log.h>
#include <string>
#include <vector>

#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#include "Renderer.h"
#include "glm/glm.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//#include "WorldDebugDrawer.h"

#define LOG_TAG "EglSample"

extern FILE *android_fopen(const char *fname, const char *mode);

static void glErrorCheck()
{
    do                                                                           \
    {                                                                          \
      for (int error = glGetError(); error; error = glGetError())              \
        {

          switch (error)                                                       \
            {                                                                  \
            case GL_NO_ERROR:                                                  \
              LOG_INFO(                            \
                             "GL_NO_ERROR - No error has been recorded. The "  \
                             "value of this symbolic constant is guaranteed "  \
                             "to be 0.");                                      \
              break;                                                           \
            case GL_INVALID_ENUM:                                              \
              LOG_ERROR(                              \
                           "GL_INVALID_ENUM - An unacceptable value is "       \
                           "specified for an enumerated argument. The "        \
                           "offending command is ignored and has no other "    \
                           "side effect than to set the error flag.");         \
              break;                                                           \
            case GL_INVALID_VALUE:                                             \
              LOG_ERROR(                              \
                           "GL_INVALID_VALUE - A numeric argument is out of "  \
                           "range. The offending command is ignored and has "  \
                           "no other side effect than to set the error "       \
                           "flag.");                                           \
              break;                                                           \
            case GL_INVALID_OPERATION:                                         \
              LOG_ERROR(                              \
                           "GL_INVALID_OPERATION - The specified operation "   \
                           "is not allowed in the current state. The "         \
                           "offending command is ignored and has no other "    \
                           "side effect than to set the error flag.");         \
              break;                                                           \
            case GL_INVALID_FRAMEBUFFER_OPERATION:                             \
              LOG_ERROR(                              \
                           "GL_INVALID_FRAMEBUFFER_OPERATION - The command "   \
                           "is trying to render to or read from the "          \
                           "framebuffer while the currently bound "            \
                           "framebuffer is not framebuffer complete (i.e. "    \
                           "the return value from glCheckFramebufferStatus "   \
                           "is not GL_FRAMEBUFFER_COMPLETE). The offending "   \
                           "command is ignored and has no other side effect "  \
                           "than to set the error flag.");                     \
              break;                                                           \
            case GL_OUT_OF_MEMORY:                                             \
              LOG_ERROR(                              \
                           "GL_OUT_OF_MEMORY - There is not enough memory "    \
                           "left to execute the command. The state of the GL " \
                           "is undefined, except for the state of the error "  \
                           "flags, after this error is recorded.");            \
              break;                                                           \
            default:                                                           \
              LOG_ERROR( "Unknown (%x)", error);      \
            }                                                                  \
        }                                                                      \
    }                                                                          \
  while (0);
}

const std::string fragmentSource = R"(

uniform sampler2D videoFrame;

varying highp vec2 textureCoordinate;
varying lowp vec4 frag_Color;

void main(void) {
    gl_FragColor = texture2D(videoFrame, textureCoordinate) * frag_Color;
}

)";

const std::string vertexSource = R"(

attribute vec4 a_Position;
attribute vec4 a_Color;
attribute vec4 a_Texture;

varying vec2 textureCoordinate;
varying lowp vec4 frag_Color;

void main(void) {
    frag_Color = a_Color;
    gl_Position = a_Position;
    textureCoordinate = a_Texture.xy;
}


)";

// Uniform index.
enum {
    UNIFORM_VIDEOFRAME,
//    UNIFORM_INPUTCOLOR,
//    UNIFORM_THRESHOLD,
            NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

enum {
    ATTRIB_VERTEX,
    ATTRIB_COLOR,
    ATTRIB_TEXTUREPOSITON,
    NUM_ATTRIBUTES
};

//static bool compileShader(GLuint &shader, GLenum type, const std::string &source)
//{
//    GLint status;
//    const GLchar *_source = (GLchar*)source.c_str();
//
//    if (!_source)
//    {
//        LOG_ERROR("Failed to load vertex shader");
//        return false;
//    }
//
//    shader = glCreateShader(type);
//    glShaderSource(shader, 1, &_source, NULL);
//    glCompileShader(shader);
//
//    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
//    if (status == GL_FALSE)
//    {
//        glDeleteShader(shader);
//
//        GLint logLength;
//        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
//        if (logLength > 0)
//        {
//            GLchar *log = (GLchar *)malloc(logLength);
//            glGetShaderInfoLog(shader, logLength, &logLength, log);
//            LOG_ERROR("Shader compile log:\n%s", log);
//            free(log);
//        }
//
//        return false;
//    }
//
//    return true;
//}
//
//static bool linkProgram(GLuint programPointer)
//{
//    GLint status(GL_FALSE);
//
//    glLinkProgram(programPointer);
//
//    glGetProgramiv(programPointer, GL_LINK_STATUS, &status);
//    if (status == GL_FALSE)
//    {
//        GLint logLength;
//        glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
//        if (logLength > 0)
//        {
//            GLchar *log = (GLchar *)malloc(logLength);
//            glGetProgramInfoLog(programPointer, logLength, &logLength, log);
//            LOG_ERROR("Program link log:\n%s", log);
//            free(log);
//        }
//
//        return false;
//    }
//
//    return true;
//}
//
//static bool validateProgram(GLuint programPointer)
//{
//    GLint status(GL_FALSE);
//
//    glValidateProgram(programPointer);
//
//    glGetProgramiv(programPointer, GL_VALIDATE_STATUS, &status);
//    if (status == GL_FALSE)
//    {
//        GLint logLength;
//        glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
//        if (logLength > 0)
//        {
//            GLchar *log = (GLchar *)malloc(logLength);
//            glGetProgramInfoLog(programPointer, logLength, &logLength, log);
//            LOG_ERROR("Program validate log:\n%s", log);
//            free(log);
//        }
//        return false;
//    }
//
//    return true;
//}
//
//static bool loadVertexShader(const std::string &vertShaderSource, const std::string &fragShaderSource, GLuint &programPointer)
//{
//    GLuint vertexShader(0), fragShader(0);
//
//    programPointer = glCreateProgram();
//
//    if(!compileShader(vertexShader, GL_VERTEX_SHADER, vertShaderSource))
//    {
//        return false;
//    }
//
//    if(!compileShader(fragShader, GL_FRAGMENT_SHADER, fragShaderSource))
//    {
//        return false;
//    }
//
//
//    // Attach vertex shader to program.
//    glAttachShader(programPointer, vertexShader);
//
//    // Attach fragment shader to program.
//    glAttachShader(programPointer, fragShader);
//
//    // Bind attribute locations.
//    // This needs to be done prior to linking.
//    glBindAttribLocation(programPointer, ATTRIB_VERTEX, "a_Position");
//    glErrorCheck();
//    glBindAttribLocation(programPointer, ATTRIB_COLOR, "a_Color");
//    glErrorCheck();
//    glBindAttribLocation(programPointer, ATTRIB_TEXTUREPOSITON, "a_Texture");
//    glErrorCheck();
////    glBindAttribLocation(programPointer, ATTRIB_TEXTUREPOSITON, "inputTextureCoordinate");
//
//    // Link program.
//    if(!linkProgram(programPointer))
//    {
//        LOG_ERROR("Failed to link program: %d", programPointer);
//
//        if (vertexShader) {
//            glDeleteShader(vertexShader);
//            vertexShader = 0;
//        }
//        if (fragShader) {
//            glDeleteShader(fragShader);
//            fragShader = 0;
//        }
//        if (programPointer) {
//            glDeleteProgram(programPointer);
//            programPointer = 0;
//        }
//
//        return false;
//    }
//
//    if(!validateProgram(programPointer))
//    {
//        return false;
//    }
//
//    // Get uniform locations.
//    GLint videoFrame = glGetUniformLocation(programPointer, "videoFrame");
//    uniforms[UNIFORM_VIDEOFRAME] = videoFrame;
////    uniforms[UNIFORM_INPUTCOLOR] = glGetUniformLocation(programPointer, "inputColor");
////    uniforms[UNIFORM_THRESHOLD] = glGetUniformLocation(programPointer, "threshold");
//
//    // Release vertex and fragment shaders.
//    if (vertexShader)
//    {
//        glDeleteShader(vertexShader);
//    }
//    if (fragShader)
//    {
//        glDeleteShader(fragShader);
//    }
//
//    return true;
//}
















// - (BOOL)createFramebuffers
static bool createFramebuffers(GLuint &framebuffer, GLuint &textureColorbuffer, GLuint width, GLuint height)
{
    bool ret = true;
    // framebuffer configuration
    // ----------------------
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // create a color attachment texture
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // This is necessary for non-power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
//    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
//    unsigned int rbo;
//    glGenRenderbuffers(1, &rbo);
//    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height); // use a single renderbuffer object for both a depth AND stencil buffer.
//    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
        ret = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return ret;
}

// Replace the implementation of this method to do your own custom drawing.
static const GLfloat squareVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        1.0f,  1.0f,
};

static const GLfloat textureVertices[] = {
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f,  1.0f,
        0.0f,  0.0f,
};
static void renderFrameBuffer(GLuint programPointer, GLuint texture)
{
//    glUseProgram(programPointer);
//
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, texture);
//
//    // Update uniform values
//    glUniform1i(uniforms[UNIFORM_VIDEOFRAME], 0);
////    glUniform4f(uniforms[UNIFORM_INPUTCOLOR], thresholdColor[0], thresholdColor[1], thresholdColor[2], 1.0f);
////    glUniform1f(uniforms[UNIFORM_THRESHOLD], thresholdSensitivity);
//
//    // Update attribute values.
//    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
//    glEnableVertexAttribArray(ATTRIB_VERTEX);
//    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0, textureVertices);
//    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
//
//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


// - (void)destroyFramebuffer;
//static void destroyFramebuffer()
//{
//	if (viewFramebuffer)
//	{
//		glDeleteFramebuffers(1, &viewFramebuffer);
//		viewFramebuffer = 0;
//	}
//
//	if (viewRenderbuffer)
//	{
//		glDeleteRenderbuffers(1, &viewRenderbuffer);
//		viewRenderbuffer = 0;
//	}
//}


static GLfloat _vertices[] = {  1.0, -1.0, 0.0,    1.0, 1.0, 1.0, 1.0,    1.0, 1.0, //right-top
                                1.0,  1.0, 0.0,    1.0, 1.0, 1.0, 1.0,    1.0, 0.0, //right-bottom
                               -1.0,  1.0, 0.0,    1.0, 1.0, 1.0, 1.0,    0.0, 0.0, //left-bottom
                               -1.0, -1.0, 0.0,    1.0, 1.0, 1.0, 1.0,    0.0, 1.0, //left-top
};

//static GLfloat _vertices[] = {  0.5, -0.5, 0.0,    1.0, 1.0, 1.0, 1.0,    1.0, 1.0, //right-top
//                                0.5,  0.5, 0.0,    1.0, 1.0, 1.0, 1.0,    1.0, 0.0, //right-bottom
//                                -0.5,  0.5, 0.0,    1.0, 1.0, 1.0, 1.0,    0.0, 0.0, //left-bottom
//                                -0.5, -0.5, 0.0,    1.0, 1.0, 1.0, 1.0,    0.0, 1.0, //left-top
//};

static GLubyte _indices[] = {0, 1, 2,
                             2, 3, 0};

static void setupVertexBuffer(GLuint &vao, GLuint &vertexBuffer, GLuint &indexBuffer)
{
    glGenVertexArraysOES(1, &vao);
    glErrorCheck();
    glBindVertexArrayOES(vao);
    glErrorCheck();

    glGenBuffers(GLsizei(1), &vertexBuffer);
    glErrorCheck();
    glBindBuffer(GLenum(GL_ARRAY_BUFFER), vertexBuffer);
    glErrorCheck();

    size_t v_count = 4;//_vertices.size() / 7;
    size_t v_size = sizeof(GLfloat)* 9;
    const void *vertices = (const void*)_vertices;
    glBufferData(GLenum(GL_ARRAY_BUFFER), v_count * v_size, vertices, GLenum(GL_STATIC_DRAW));
    glErrorCheck();

    glGenBuffers(GLsizei(1), &indexBuffer);
    glErrorCheck();
    glBindBuffer(GLenum(GL_ELEMENT_ARRAY_BUFFER), indexBuffer);
    glErrorCheck();
    size_t i_count = 6;//_indices.size();
    size_t i_size = sizeof(GLubyte);
    const void *indices = (const void*)_indices;
    glBufferData(GLenum(GL_ELEMENT_ARRAY_BUFFER), i_count * i_size, indices, GLenum(GL_STATIC_DRAW));
    glErrorCheck();

    GLsizei s1 = GLsizei(sizeof(GLfloat) * 9);
    int p1 = 0;
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glErrorCheck();
    glVertexAttribPointer(
            ATTRIB_VERTEX,
            3,
            GLenum(GL_FLOAT),
            GLboolean(GL_FALSE),
            s1,
            (const GLvoid *)p1);
    glErrorCheck();

    GLsizei s2 = GLsizei(sizeof(GLfloat) * 9);
    int p2 = (3 * sizeof(GLfloat));
    glEnableVertexAttribArray(ATTRIB_COLOR);
    glErrorCheck();
    glVertexAttribPointer(
            ATTRIB_COLOR,
            4,
            GLenum(GL_FLOAT),
            GLboolean(GL_FALSE),
            s2,
            (const GLvoid *)p2);
    glErrorCheck();


    GLsizei s3 = GLsizei(sizeof(GLfloat) * 9);
    int p3 = ((3 + 4) * sizeof(GLfloat));
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);
    glErrorCheck();
    glVertexAttribPointer(
            ATTRIB_TEXTUREPOSITON,
            2,
            GLenum(GL_FLOAT),
            GLboolean(GL_FALSE),
            s3,
            (const GLvoid *)p3);
    glErrorCheck();

    glBindVertexArrayOES(0);
    glBindBuffer(GLenum(GL_ARRAY_BUFFER), 0);
    glBindBuffer(GLenum(GL_ELEMENT_ARRAY_BUFFER), 0);



}


Renderer::Renderer()
        : _msg(MSG_NONE), _display(0), _surface(0), _context(0), _angle(0)
{
    LOG_INFO("Renderer instance created");
    pthread_mutex_init(&_mutex, 0);
    return;
}

Renderer::~Renderer()
{
    LOG_INFO("Renderer instance destroyed");
    pthread_mutex_destroy(&_mutex);
    return;
}

void Renderer::start()
{
    LOG_INFO("Creating renderer thread");
    pthread_create(&_threadId, 0, threadStartCallback, this);
    return;
}

void Renderer::stop()
{
    LOG_INFO("Stopping renderer thread");

    // send message to render thread to stop rendering
    pthread_mutex_lock(&_mutex);
    _msg = MSG_RENDER_LOOP_EXIT;
    pthread_mutex_unlock(&_mutex);

    pthread_join(_threadId, 0);
    LOG_INFO("Renderer thread stopped");

    return;
}

void Renderer::setWindow(ANativeWindow *window)
{
    // notify render thread that window has changed
    pthread_mutex_lock(&_mutex);
    _msg = MSG_WINDOW_SET;
    _window = window;
    pthread_mutex_unlock(&_mutex);

    return;
}



void Renderer::renderLoop()
{
    bool renderingEnabled = true;

    LOG_INFO("renderLoop()");

    while (renderingEnabled) {

        pthread_mutex_lock(&_mutex);

        // process incoming messages
        switch (_msg) {

            case MSG_WINDOW_SET:
                initialize();

                break;

            case MSG_RENDER_LOOP_EXIT:
                renderingEnabled = false;
                destroy();
                break;

            default:
                break;
        }
        _msg = MSG_NONE;

        if (_display) {

            drawFrame();

            if (!eglSwapBuffers(_display, _surface)) {
                LOG_ERROR("eglSwapBuffers() returned error %d", eglGetError());
            }
        }

        pthread_mutex_unlock(&_mutex);
    }

    LOG_INFO("Render loop exits");

    return;
}

bool Renderer::initialize()
{
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };
    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;
    EGLSurface surface;
    EGLContext context;
    GLfloat ratio;

    LOG_INFO("Initializing context");

    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOG_ERROR("eglGetDisplay() returned error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(display, 0, 0)) {
        LOG_ERROR("eglInitialize() returned error %d", eglGetError());
        return false;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOG_ERROR("eglGetConfigAttrib() returned error %d", eglGetError());
        destroy();
        return false;
    }

    ANativeWindow_setBuffersGeometry(_window, 0, 0, format);

    if (!(surface = eglCreateWindowSurface(display, config, _window, 0))) {
        LOG_ERROR("eglCreateWindowSurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    EGLint ctxattr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 2,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
    };

    if (!(context = eglCreateContext(display, config, 0, ctxattr))) {
        LOG_ERROR("eglCreateContext() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &mWidth) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &mHeight)) {
        LOG_ERROR("eglQuerySurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    _display = display;
    _surface = surface;
    _context = context;

    if(Shader::load(vertexSource, fragmentSource, mProgram))
    {
        GLint videoFrame = glGetUniformLocation(mProgram, "videoFrame");
        uniforms[UNIFORM_VIDEOFRAME] = videoFrame;

        setupVertexBuffer(mVao, mVertexBuffer, mIndexBuffer);

        char const *filename = "img0.bmp";
        int x;
        int y;
        int channels_in_file;
        int desired_channels=4;


        FILE *f = android_fopen(filename, "r");
        void *buffer = (void*)stbi_load_from_file(f, &x, &y, &channels_in_file, desired_channels);

        GLsizei bufferHeight = y;
        GLsizei bufferWidth = x;
        size_t currSize = bufferHeight * bufferWidth * channels_in_file;
        unsigned char *outBuff = (unsigned char*)malloc(currSize);
//        memset(outBuff, 0, currSize);
        memcpy(outBuff, buffer, currSize);

        // Create a new texture from the camera frame data, display that using the shaders
        glGenTextures(1, &mVideoFrameTexture);
        glErrorCheck();
        glBindTexture(GL_TEXTURE_2D, mVideoFrameTexture);
        glErrorCheck();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // This is necessary for non-power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


        // Using BGRA extension to pull in video frame data directly
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)bufferWidth, (GLsizei)bufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, outBuff);
        glErrorCheck();

        free(outBuff);

        return true;
    }




    return false;
}

void Renderer::destroy() {
    LOG_INFO("Destroying context");

    eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(_display, _context);
    eglDestroySurface(_display, _surface);
    eglTerminate(_display);

    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;

    return;
}

void Renderer::drawFrame()
{
    glViewport(0, 0, mWidth, mHeight);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram);
    glErrorCheck();

    glActiveTexture(GL_TEXTURE0);
    glErrorCheck();
    glBindTexture(GL_TEXTURE_2D, mVideoFrameTexture);
    glErrorCheck();

    // Update uniform values
    glUniform1i(uniforms[UNIFORM_VIDEOFRAME], 0);
    glErrorCheck();

    glBindVertexArrayOES(mVao);
    glErrorCheck();
    glDrawElements(GLenum(GL_TRIANGLES), GLsizei(6), GLenum(GL_UNSIGNED_BYTE), 0);
    glErrorCheck();
    glBindVertexArrayOES(0);

//    glMatrixMode(GL_MODELVIEW);
//    glLoadIdentity();
//    glTranslatef(0, 0, -3.0f);
//    glRotatef(_angle, 0, 1, 0);
//    glRotatef(_angle*0.25f, 1, 0, 0);
//
//    glEnableClientState(GL_VERTEX_ARRAY);
//    glEnableClientState(GL_COLOR_ARRAY);
//
//    glFrontFace(GL_CW);
//    glVertexPointer(3, GL_FIXED, 0, vertices);
//    glColorPointer(4, GL_FIXED, 0, colors);
//    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, indices);
//
//    _angle += 1.2f;
}

void* Renderer::threadStartCallback(void *myself)
{
    Renderer *renderer = (Renderer*)myself;

    renderer->renderLoop();
    pthread_exit(0);

    return 0;
}



bool Shader::compileShader(GLuint &shader, GLenum type, const std::string &source)
{
    GLint status;
    const GLchar *_source = (GLchar*)source.c_str();

    if (!_source)
    {
        LOG_ERROR("Failed to load vertex shader");
        return false;
    }

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &_source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        glDeleteShader(shader);

        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            GLchar *log = (GLchar *)malloc(logLength);
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            LOG_ERROR("Shader compile log:\n%s", log);
            free(log);
        }

        return false;
    }

    return true;
}
bool Shader::linkProgram(GLuint programPointer)
{
    GLint status(GL_FALSE);

    glLinkProgram(programPointer);

    glGetProgramiv(programPointer, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint logLength;
        glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            GLchar *log = (GLchar *)malloc(logLength);
            glGetProgramInfoLog(programPointer, logLength, &logLength, log);
            LOG_ERROR("Program link log:\n%s", log);
            free(log);
        }

        return false;
    }

    return true;
}

bool Shader::validateProgram(GLuint programPointer)
{
    GLint status(GL_FALSE);

    glValidateProgram(programPointer);

    glGetProgramiv(programPointer, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint logLength;
        glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0)
        {
            GLchar *log = (GLchar *)malloc(logLength);
            glGetProgramInfoLog(programPointer, logLength, &logLength, log);
            LOG_ERROR("Program validate log:\n%s", log);
            free(log);
        }
        return false;
    }

    return true;
}

//bool Shader::load(const std::string &vertShaderSource, const std::string &fragShaderSource, GLuint &programPointer);
bool Shader::load(const std::string &vertShaderSource, const std::string &fragShaderSource, GLuint &programPointer)
{
    GLuint vertexShader(0), fragShader(0);

    programPointer = glCreateProgram();

    if(!compileShader(vertexShader, GL_VERTEX_SHADER, vertShaderSource))
    {
        return false;
    }

    if(!compileShader(fragShader, GL_FRAGMENT_SHADER, fragShaderSource))
    {
        return false;
    }


    // Attach vertex shader to program.
    glAttachShader(programPointer, vertexShader);

    // Attach fragment shader to program.
    glAttachShader(programPointer, fragShader);

    // Bind attribute locations.
    // This needs to be done prior to linking.
    glBindAttribLocation(programPointer, ATTRIB_VERTEX, "a_Position");
    glErrorCheck();
    glBindAttribLocation(programPointer, ATTRIB_COLOR, "a_Color");
    glErrorCheck();
    glBindAttribLocation(programPointer, ATTRIB_TEXTUREPOSITON, "a_Texture");
    glErrorCheck();
//    glBindAttribLocation(programPointer, ATTRIB_TEXTUREPOSITON, "inputTextureCoordinate");

    // Link program.
    if(!linkProgram(programPointer))
    {
        LOG_ERROR("Failed to link program: %d", programPointer);

        if (vertexShader) {
            glDeleteShader(vertexShader);
            vertexShader = 0;
        }
        if (fragShader) {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (programPointer) {
            glDeleteProgram(programPointer);
            programPointer = 0;
        }

        return false;
    }

    if(!validateProgram(programPointer))
    {
        return false;
    }

    // Get uniform locations.
//    GLint videoFrame = glGetUniformLocation(programPointer, "videoFrame");
//    uniforms[UNIFORM_VIDEOFRAME] = videoFrame;
//    uniforms[UNIFORM_INPUTCOLOR] = glGetUniformLocation(programPointer, "inputColor");
//    uniforms[UNIFORM_THRESHOLD] = glGetUniformLocation(programPointer, "threshold");

    // Release vertex and fragment shaders.
    if (vertexShader)
    {
        glDeleteShader(vertexShader);
    }
    if (fragShader)
    {
        glDeleteShader(fragShader);
    }

    return true;
}