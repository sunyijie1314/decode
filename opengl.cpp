#include <GLES2/gl2.h>
// #include <EGL/egl.h>
// #include <EGL/eglext.h>
#include <emscripten.h>
// #include <emscripten/bind.h>
// #include <emscripten/val.h>
#include <emscripten/html5.h>
 
// #include  <X11/Xlib.h>
// #include  <X11/Xatom.h>
// #include  <X11/Xutil.h>
 
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <assert.h>
 
#define SR_OK 0;
#define SR_FAIL 1;
 
#define Y 0
#define U 1
#define V 2
 
/// esCreateWindow flag - RGB color buffer
#define ES_WINDOW_RGB           0
/// esCreateWindow flag - ALPHA color buffer
#define ES_WINDOW_ALPHA         1 
/// esCreateWindow flag - depth buffer
#define ES_WINDOW_DEPTH         2 
/// esCreateWindow flag - stencil buffer
#define ES_WINDOW_STENCIL       4
/// esCreateWindow flat - multi-sample buffer
#define ES_WINDOW_MULTISAMPLE   8
 
GLuint flags=ES_WINDOW_RGB;
GLuint g_ShaderProgram;
GLuint g_Texture2D[3]; 
GLuint g_vertexPosBuffer;
GLuint g_texturePosBuffer;
 
/*顶点着色器代码*/
static const char g_pGLVS[] =              ///<普通纹理渲染顶点着色器
{
    "precision mediump float;"
    "attribute vec4 position; "
    "attribute vec4 texCoord; "
    "varying vec4 pp; "
 
    "void main() "
    "{ "
    "    gl_Position = position; "
    "    pp = texCoord; "
    "} "
};
 
/*像素着色器代码*/
const char* g_pGLFS =              ///<YV12片段着色器
{
    "precision mediump float;"
    "varying vec4 pp; "
    "uniform sampler2D Ytexture; "
    "uniform sampler2D Utexture; "
    "uniform sampler2D Vtexture; "
    "void main() "
    "{ "
    "    float r,g,b,y,u,v; "
 
    "    y=texture2D(Ytexture, pp.st).r; "
    "    u=texture2D(Utexture, pp.st).r; "
    "    v=texture2D(Vtexture, pp.st).r; "
 
    "    y=1.1643*(y-0.0625); "
    "    u=u-0.5; "
    "    v=v-0.5; "
 
    "    r=y+1.5958*v; "
    "    g=y-0.39173*u-0.81290*v; "
    "    b=y+2.017*u; "
    "    gl_FragColor=vec4(r,g,b,1.0); "
    "} "
};
 
const GLfloat g_Vertices[] = {
    -1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f,  1.0f,
	 1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f, -1.0f,
};
 
const GLfloat g_TexCoord[] = {
     0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 0.0f,
	1.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
};
 
#ifdef __cplusplus
extern "C" {
#endif
EMSCRIPTEN_KEEPALIVE
void initBuffers()
{
    GLuint vertexPosBuffer;
    glGenBuffers(1, &vertexPosBuffer);//创建缓冲区对象
    glBindBuffer(GL_ARRAY_BUFFER, vertexPosBuffer);//将该缓冲区对象绑定到管线上
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_Vertices), g_Vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, NULL);
    g_vertexPosBuffer=vertexPosBuffer;
 
    GLuint texturePosBuffer;
    glGenBuffers(1, &texturePosBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, texturePosBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_TexCoord), g_TexCoord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, NULL);
    g_texturePosBuffer=texturePosBuffer;
 
}
EMSCRIPTEN_KEEPALIVE
void initTexture()
{
    glUseProgram(g_ShaderProgram);
    GLuint          nTexture2D[3];        ///<< YUV三层纹理数组
    glGenTextures(3, nTexture2D);
    for(int i = 0; i < 3; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, nTexture2D[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, NULL);
    }
    
    GLuint          nTextureUniform[3];
    nTextureUniform[Y] = glGetUniformLocation(g_ShaderProgram, "Ytexture");
    nTextureUniform[U] = glGetUniformLocation(g_ShaderProgram, "Utexture");
    nTextureUniform[V] = glGetUniformLocation(g_ShaderProgram, "Vtexture");
    glUniform1i(nTextureUniform[Y], 0);
    glUniform1i(nTextureUniform[U], 1);
    glUniform1i(nTextureUniform[V], 2);
    g_Texture2D[0]=nTexture2D[0];
    g_Texture2D[1]=nTexture2D[1];
    g_Texture2D[2]=nTexture2D[2];
    glUseProgram(NULL);
}
EMSCRIPTEN_KEEPALIVE
GLuint initShaderProgram()
{
     ///< 顶点着色器相关操作
    GLuint nVertexShader   = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* pVS  = g_pGLVS;
    GLint nVSLen       = static_cast<GLint>(strlen(g_pGLVS));
    glShaderSource(nVertexShader, 1, (const GLchar**)&pVS, &nVSLen);
    GLint nCompileRet;
    glCompileShader(nVertexShader);
    glGetShaderiv(nVertexShader, GL_COMPILE_STATUS, &nCompileRet);
    if(0 == nCompileRet)
    {
        return SR_FAIL;
    }
	///< 片段着色器相关操作
	GLuint nFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* pFS = g_pGLFS;
    GLint nFSLen = static_cast<GLint>(strlen(g_pGLFS));
    glShaderSource(nFragmentShader, 1, (const GLchar**)&pFS, &nFSLen);
    glCompileShader(nFragmentShader);
    glGetShaderiv(nFragmentShader, GL_COMPILE_STATUS, &nCompileRet);
    if(0 == nCompileRet)
    {
        return SR_FAIL;
    }
	
	///<program相关
    GLuint nShaderProgram = glCreateProgram();
    glAttachShader(nShaderProgram, nVertexShader);
    glAttachShader(nShaderProgram, nFragmentShader);
    glLinkProgram(nShaderProgram);
    GLint nLinkRet;
    glGetProgramiv(nShaderProgram, GL_LINK_STATUS, &nLinkRet);
    if(0 == nLinkRet)
    {
        return SR_FAIL;
    }
    glDeleteShader(nVertexShader);
    glDeleteShader(nFragmentShader);
    return nShaderProgram;
 
}
EMSCRIPTEN_KEEPALIVE
void initContextGL(const char* hWindow)
{
    // double dpr = emscripten_get_device_pixel_ratio();
    // emscripten_set_element_css_size(hWindow, nWmdWidth / dpr, nWndHeight / dpr);
    // emscripten_set_canvas_element_size(hWindow, nWmdWidth, nWndHeight);
    // printf("create size success\n");
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
#if MAX_WEBGL_VERSION >= 2
    attrs.majorVersion = 2;
#endif
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE glContext = emscripten_webgl_create_context(hWindow, &attrs);
    assert(glContext);
    EMSCRIPTEN_RESULT res=emscripten_webgl_make_context_current(glContext);
    assert(res == EMSCRIPTEN_RESULT_SUCCESS);
    assert(emscripten_webgl_get_current_context() == glContext);
    printf("create context success 1730\n");
}
EMSCRIPTEN_KEEPALIVE
int init(const char* hWindow)
{ 
    //初始化Context
    initContextGL(hWindow);
    //初始化着色器程序
    g_ShaderProgram=initShaderProgram();
    //初始化buffer
    initBuffers();
    //初始化纹理
    initTexture();
    return 1;
}
EMSCRIPTEN_KEEPALIVE
int RenderFrame( unsigned char* pFrameData, int nFrameWidth, int nFrameHeight)
{
    if(pFrameData==NULL||nFrameWidth<=0||nFrameHeight<=0)
    {
        return -1;
    }
    glClearColor(0.8f,0.8f,1.0f,1.0);
    glClear(GL_COLOR_BUFFER_BIT);
 
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_Texture2D[Y]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, nFrameWidth, nFrameHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pFrameData);
    
	glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,g_Texture2D[U] );
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, nFrameWidth / 2, nFrameHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pFrameData + nFrameWidth * nFrameHeight * 5 / 4);
    
	glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_Texture2D[V]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, nFrameWidth / 2, nFrameHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pFrameData + nFrameWidth * nFrameHeight);
    
    glUseProgram(g_ShaderProgram);
    //将所有顶点数据上传至顶点着色器的顶点缓存
#ifdef DRAW_FROM_CLIENT_MEMORY
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, pos_and_color);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 20, (void*)(pos_and_color+2));
 
  GLint nPosLoc = glGetAttribLocation(g_ShaderProgram, "position");
  glVertexAttribPointer(nPosLoc, 2, GL_FLOAT, GL_FALSE, 0, g_Vertices);
  GLint nTexcoordLoc = glGetAttribLocation(g_ShaderProgram, "texCoord");
  glVertexAttribPointer(nTexcoordLoc, 2, GL_FLOAT, GL_FALSE, 0, g_TexCoord);
#else
  glBindBuffer(GL_ARRAY_BUFFER, g_vertexPosBuffer);
  GLint nPosLoc = glGetAttribLocation(g_ShaderProgram, "position");
  glEnableVertexAttribArray(nPosLoc);
  glVertexAttribPointer(nPosLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glBindBuffer(GL_ARRAY_BUFFER, NULL);
 
  glBindBuffer(GL_ARRAY_BUFFER, g_texturePosBuffer);
  GLint nTexcoordLoc = glGetAttribLocation(g_ShaderProgram, "texCoord");
  glEnableVertexAttribArray(nTexcoordLoc);
  glVertexAttribPointer(nTexcoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glBindBuffer(GL_ARRAY_BUFFER, NULL);
#endif
 
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(nPosLoc);
    glDisableVertexAttribArray(nTexcoordLoc);
 
#ifdef EXPLICIT_SWAP
  emscripten_webgl_commit_frame();
#endif
 
#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
    glUseProgram(NULL);
    return SR_OK;
}
#ifdef __cplusplus
}
#endif
