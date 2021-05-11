 /**
  * @file
  * video decoding with libavcodec API example
  *
  * decode_video.c
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void(*VideoCallback)(unsigned char* data_y, unsigned char* data_u, unsigned char* data_v, int line1, int line2, int line3, int width, int height, long pts);

#include <libavcodec/avcodec.h>
#include <GLES2/gl2.h>
// #include <EGL/egl.h>
// #include <EGL/eglext.h>
#include <emscripten.h>
// #include <emscripten/bind.h>
// #include <emscripten/val.h>
#include <emscripten/html5.h>
#include <emscripten/websocket.h>
 
// #include  <X11/Xlib.h>
// #include  <X11/Xatom.h>
// #include  <X11/Xutil.h>
 
//#include <iostream>
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
    GLint nVSLen       = (GLint)(strlen(g_pGLVS));
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
    GLint nFSLen = (GLint)(strlen(g_pGLFS));
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
int RenderFrame( unsigned char* pFrameData,unsigned char *u, unsigned char *v, int nFrameWidth, int nFrameHeight)
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, nFrameWidth / 2, nFrameHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, u);
    
	glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_Texture2D[V]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, nFrameWidth / 2, nFrameHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, v);
    
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

#define INBUF_SIZE 4096

typedef enum ErrorCode {
	kErrorCode_Success = 0,
	kErrorCode_Invalid_Param,
	kErrorCode_Invalid_State,
	kErrorCode_Invalid_Data,
	kErrorCode_Invalid_Format,
	kErrorCode_NULL_Pointer,
	kErrorCode_Open_File_Error,
	kErrorCode_Eof,
	kErrorCode_FFmpeg_Error
}ErrorCode;

typedef enum LogLevel {
	kLogLevel_None, //Not logging.
	kLogLevel_Core, //Only logging core module(without ffmpeg).
	kLogLevel_All   //Logging all, with ffmpeg.
}LogLevel;

typedef enum DecoderType {
	kDecoderType_H264,
	kDecoderType_H265
}DecoderType;


int logLevel = kLogLevel_None;
int decoderType = kDecoderType_H265;

void simpleLog(const char* format, ...) {
	if (logLevel == kLogLevel_None) {
		return;
	}

	char szBuffer[1024] = { 0 };
	char szTime[32] = { 0 };
	char* p = NULL;
	int prefixLength = 0;
	const char* tag = "Core";

	prefixLength = sprintf(szBuffer, "[%s][%s][DT] ", szTime, tag);
	p = szBuffer + prefixLength;

	if (1) {
		va_list ap;
		va_start(ap, format);
		vsnprintf(p, 1024 - prefixLength, format, ap);
		va_end(ap);
	}

	printf("%s\n", szBuffer);
}

void ffmpegLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
	static int printPrefix = 1;
	static int count = 0;
	static char prev[1024] = { 0 };
	char line[1024] = { 0 };
	static int is_atty;
	AVClass* avc = ptr ? *(AVClass**)ptr : NULL;
	if (level > AV_LOG_DEBUG) {
		return;
	}

	line[0] = 0;

	if (printPrefix && avc) {
		if (avc->parent_log_context_offset) {
			AVClass** parent = *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
			if (parent && *parent) {
				snprintf(line, sizeof(line), "[%s @ %p] ", (*parent)->item_name(parent), parent);
			}
		}
		snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
	}

	vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);
	line[strlen(line) + 1] = 0;
	simpleLog("%s", line);
}

VideoCallback videoCallback = NULL;

int copyFrameData(AVFrame* src, AVFrame* dst, long ptslist[]) {
	int ret = kErrorCode_Success;
	memcpy(dst->data, src->data, sizeof(src->data));
	dst->linesize[0] = src->linesize[0];
	dst->linesize[1] = src->linesize[1];
	dst->linesize[2] = src->linesize[2];
	dst->width = src->width;
	dst->height = src->height;
	long pts = LONG_MAX;
	int index = -1;
	for (int i = 0; i < 10; i++) {
		if (ptslist[i] < pts) {
			pts = ptslist[i];
			index = i;
		}
	}
	if (index > -1) {
		ptslist[index] = LONG_MAX;
	}
	dst->pts = pts;
	return ret;
}

unsigned char* yuvBuffer;
//int videoSize = 0;
//int initBuffer(width, height) {
//	videoSize = avpicture_get_size(AV_PIX_FMT_YUV420P, width, height);
//	int bufferSize = 3 * videoSize;
//	yuvBuffer = (unsigned char*)av_mallocz(bufferSize);
//}

static int decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, AVFrame* outFrame, long ptslist[])
{
	int res = kErrorCode_Success;
	char buf[1024];
	int ret;

	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		simpleLog("Error sending a packet for decoding\n");
		res = kErrorCode_FFmpeg_Error;
	}
	else {
		while (ret >= 0) {
			ret = avcodec_receive_frame(dec_ctx, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}
			else if (ret < 0) {
				simpleLog("Error during decoding\n");
				res = kErrorCode_FFmpeg_Error;
				break;
			}

			res = copyFrameData(frame, outFrame, ptslist);
			if (res != kErrorCode_Success) {
				break;
			}

			simpleLog("callback one times!\n");
			RenderFrame(outFrame->data[0], outFrame->data[1], outFrame->data[2], outFrame->width, outFrame->height);
			//videoCallback(outFrame->data[0], outFrame->data[1], outFrame->data[2], outFrame->linesize[0], outFrame->linesize[1], outFrame->linesize[2], outFrame->width, outFrame->height, outFrame->pts);

		}
	}
	return res;
}

int isInit = 0;
const AVCodec* codec;
AVCodecParserContext* parser;
AVCodecContext* c = NULL;
AVPacket* pkt;
AVFrame* frame;
AVFrame* outFrame;
long ptslist[10];

static int openDecoder(int codecType, int logLv) {
	int ret = kErrorCode_Success;
do {
		simpleLog("Initialize decoder.");

		if (isInit != 0) {
			break;
		}

		decoderType = codecType;
		logLevel = logLv;

		if (logLevel == kLogLevel_All) {
			av_log_set_callback(ffmpegLogCallback);
		}

		/* find the video decoder */
		//if (decoderType == kDecoderType_H264) {
			codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		//} else {
		//	codec = avcodec_find_decoder(AV_CODEC_ID_H265);
		//}

		if (!codec) {
			simpleLog("Codec not found\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		parser = av_parser_init(codec->id);
		if (!parser) {
			simpleLog("parser not found\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		c = avcodec_alloc_context3(codec);
		if (!c) {
			simpleLog("Could not allocate video codec context\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		if (avcodec_open2(c, codec, NULL) < 0) {
			simpleLog("Could not open codec\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		frame = av_frame_alloc();
		if (!frame) {
			simpleLog("Could not allocate video frame\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		outFrame = av_frame_alloc();
		if (!outFrame) {
			simpleLog("Could not allocate video frame\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		pkt = av_packet_alloc();
		if (!pkt) {
			simpleLog("Could not allocate video packet\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}

		for (int i = 0; i < 10; i++) {
			ptslist[i] = LONG_MAX;
		}

		//videoCallback = (VideoCallback)callback;

	} while (0);
	simpleLog("Decoder initialized %d.", ret);
	return ret;
}

int decodeData(unsigned char* data, size_t data_size, long pts) {
	int ret = kErrorCode_Success;

	for (int i = 0; i < 10; i++) {
		if (ptslist[i] == LONG_MAX) {
			ptslist[i] = pts;
			break;
		}
	}

	while (data_size > 0) {
		int size = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
			data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		if (size < 0) {
			simpleLog("Error while parsing\n");
			ret = kErrorCode_FFmpeg_Error;
			break;
		}
		data += size;
		data_size -= size;

		if (pkt->size) {
			ret = decode(c, frame, pkt, outFrame, ptslist);
			if (ret != kErrorCode_Success) {
                break;
            }
		}
	}
	return ret;
}

int flushDecoder() {
	/* flush the decoder */
	return decode(c, frame, NULL, outFrame, ptslist);
}

int closeDecoder() {
	int ret = kErrorCode_Success;

	do {
		if (parser != NULL) {
			av_parser_close(parser);
			simpleLog("Video codec context closed.");
		}
		if (c != NULL) {
			avcodec_free_context(&c);
			simpleLog("Video codec context closed.");
		}
		if (frame != NULL) {
			av_frame_free(&frame);
		}
		if (pkt != NULL) {
			av_packet_free(&pkt);
		}
		if (yuvBuffer != NULL) {
			av_freep(&yuvBuffer);
		}
		if (outFrame != NULL) {
			av_frame_free(&outFrame);
		}
		simpleLog("All buffer released.");
	} while (0);

	return ret;
}

int callbackIndex = 0;
void vcb_frame(unsigned char* data_y, unsigned char* data_u, unsigned char* data_v, int line1, int line2, int line3, int width, int height, long pts) {
	simpleLog("[%d]In video call back, size = %d * %d, pts = %ld\n", ++callbackIndex, width, height, pts);
	int i = 0;
	unsigned char* src = NULL;
	FILE* dst = fopen("vcb.yuv", "a");
	for (i = 0; i < height; i++) {
		src = data_y + i * line1;
		fwrite(src, width, 1, dst);
	}

	for (i = 0; i < height / 2; i++) {
		src = data_u + i * line2;
		fwrite(src, width/2, 1, dst);
	}

	for (i = 0; i < height / 2; i++) {
		src = data_v + i * line3;
		fwrite(src, width/2, 1, dst);
	}

	fclose(dst);
}
EMSCRIPTEN_KEEPALIVE
int start()
{
		openDecoder(0, kLogLevel_Core);

		const char* filename = "Forrest_Gump_IMAX.h264";
		//const char* filename = "FourPeople_1280x720_60_1M.265";

		FILE* f;

		//   uint8_t *data;
		size_t   data_size;

		//uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
		///* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
		//memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

		unsigned char* buffer;
		buffer = (unsigned char*)malloc(sizeof(unsigned char) * (INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE));
		if (buffer == NULL) {
			simpleLog("Memory error");
			exit(2);
		}

		f = fopen(filename, "rb");
		if (!f) {
			simpleLog("Could not open %s\n", filename);
			exit(1);
		}

		while (!feof(f)) {
			/* read raw data from the input file */
			//data_size = fread(inbuf, 1, INBUF_SIZE, f);
			data_size = fread(buffer, 1, INBUF_SIZE, f);

			if (!data_size)
				break;

			/* use the parser to split the data into frames */
			//data = inbuf;
			decodeData(buffer, data_size, 0);
                        emscripten_sleep(30);
		}
		fclose(f);
		free(buffer);

		flushDecoder();
		closeDecoder();

	return 0;
}

uint8_t *oneframe = (uint8_t *)malloc(1000000);
uint32_t num=0;
int start1 = 0;

EM_BOOL parse(const EmscriptenWebSocketMessageEvent *websocketEvent)
{
	if((*(websocketEvent->data + 12) == 0x00) &&
	   (*(websocketEvent->data + 13) == 0x00) &&
	   (*(websocketEvent->data + 14) == 0x01) &&
	   (*(websocketEvent->data + 15) == 0xba))
	{
		if(start != 0)
		{
			
		}
		start1 = 1;
		num =0;
		memset(oneframe, 0, 1000000);
		for(int i =0; i< websocketEvent->numBytes; i++)
		{
			if((*(websocketEvent->data + 12 +i) == 0x00) &&
			   (*(websocketEvent->data + 13+i) == 0x00) &&
			   (*(websocketEvent->data + 14+i) == 0x00) &&
			   (*(websocketEvent->data + 15+i) == 0x01) &&
			   (*(websocketEvent->data + 15+i) == 0x09))
			{
				memcpy(oneframe + num, websocketEvent->data + 12 +i, websocketEvent->numBytes-12-i);
				num = num + websocketEvent->numBytes-12-i;
			}
		}
	}
	else
	{
		memcpy(oneframe + num, websocketEvent->data + 12, websocketEvent->numBytes-12);
		num = num + websocketEvent->numBytes-12;
	}
	
	return EM_TRUE;
}

EM_BOOL onopen(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	puts("onopen");
	EMSCRIPTEN_RESULT result;
	result = emscripten_websocket_send_utf8_text(websocketEvent->socket, "hello");
	if(result)
	{
		printf("fail to send %d\n", result);
	}
	return EM_TRUE;
}

EM_BOOL onerror(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	puts("onerror");
	return EM_TRUE;
}

EM_BOOL onclose(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	puts("onclose");
	return EM_TRUE;
}

EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	puts("onmessage");
	parse(websocketEvent);
	return EM_TRUE;
}

int main()
{
	memset(oneframe, 0, 1000000);
	if(!emscripten_websocket_is_supported())
	{
		return 0;
	}
	EmscriptenWebSocketCreateAttributes ws_attrs = 
	{
		"ws://127.0.0.1:5080",
		NULL,
		EM_TRUE
	};
	
	EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&ws_attrs);
	emscripten_websocket_set_onopen_callback(ws, NULL, onopen);
	emscripten_websocket_set_onerror_callback(ws, NULL, onerror);
	emscripten_websocket_set_onclose_callback(ws, NULL, onclose);
	emscripten_websocket_set_onmessage_callback(ws, NULL, onmessage);
	return 0;	
}















