#include "RealTimeRenderer.h"
#include "libavutil/pixfmt.h"
#include <QOpenGLPixelTransferOptions>

#define VSHCODE                                                                                                        \
    R"(
attribute highp vec3 qt_Vertex;
attribute highp vec2 texCoord;

uniform mat4 u_modelMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectMatrix;

varying vec2 v_texCoord;
void main(void)
{
    gl_Position = u_projectMatrix * u_viewMatrix * u_modelMatrix * vec4(qt_Vertex, 1.0f);
    v_texCoord = texCoord;
}

)"

#define FSHCPDE                                                                                                        \
    R"(
varying vec2 v_texCoord;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
uniform int pixFmt;
void main(void)
{
    vec3 yuv;
    vec3 rgb;
    if (pixFmt == 0 || pixFmt == 12) {
        //yuv420p
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, v_texCoord).r - 0.5;
        rgb = mat3( 1.0,       1.0,         1.0,
                    0.0,       -0.3455,  1.779,
                    1.4075, -0.7169,  0.0) * yuv;
    } else if( pixFmt == 23 ){
        // NV12
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_u, v_texCoord).a - 0.5;
        rgb = mat3( 1.0,       1.0,         1.0,
                    0.0,       -0.3455,  1.779,
                    1.4075, -0.7169,  0.0) * yuv;

    } else {
        //YUV444P
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, v_texCoord).r - 0.5;

        rgb.x = clamp( yuv.x + 1.402 *yuv.z, 0.0, 1.0);
        rgb.y = clamp( yuv.x - 0.34414 * yuv.y - 0.71414 * yuv.z, 0.0, 1.0);
        rgb.z = clamp( yuv.x + 1.772 * yuv.y, 0.0, 1.0);
    }
    gl_FragColor = vec4(rgb, 1.0);
}

)"

static void safeDeleteTexture(QOpenGLTexture *texture) {
    if (texture) {
        if (texture->isBound()) {
            texture->release();
        }
        if (texture->isCreated()) {
            texture->destroy();
        }
        delete texture;
        texture = nullptr;
    }
}

RealTimeRenderer::RealTimeRenderer() {
    qWarning() << __FUNCTION__;
}

RealTimeRenderer::~RealTimeRenderer() {
    qWarning() << __FUNCTION__;
    safeDeleteTexture(mTexY);
    safeDeleteTexture(mTexU);
    safeDeleteTexture(mTexV);
}

void RealTimeRenderer::init() {
    qWarning() << __FUNCTION__;
    initializeOpenGLFunctions();
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    initShader();
    initGeometry();
}
void RealTimeRenderer::resize(int width, int height) {

    m_itemWidth = width;
    m_itemHeight = height;
    glViewport(0, 0, width, height);
    float bottom = -1.0f;
    float top = 1.0f;
    float n = 1.0f;
    float f = 100.0f;
    mProjectionMatrix.setToIdentity();
    mProjectionMatrix.frustum(-1.0, 1.0, bottom, top, n, f);
}
void RealTimeRenderer::initShader() {
    if (!mProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, VSHCODE)) {
        qWarning() << " add vertex shader file failed.";
        return;
    }
    if (!mProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, FSHCPDE)) {
        qWarning() << " add fragment shader file failed.";
        return;
    }
    mProgram.bindAttributeLocation("qt_Vertex", 0);
    mProgram.bindAttributeLocation("texCoord", 1);
    mProgram.link();
    mProgram.bind();
}
void RealTimeRenderer::initTexture() {
    // yuv420p
    mTexY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mTexY->setFormat(QOpenGLTexture::LuminanceFormat);
    //    mTexY->setFixedSamplePositions(false);
    mTexY->setMinificationFilter(QOpenGLTexture::Nearest);
    mTexY->setMagnificationFilter(QOpenGLTexture::Nearest);
    mTexY->setWrapMode(QOpenGLTexture::ClampToEdge);

    mTexU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mTexU->setFormat(mPixFmt == AV_PIX_FMT_NV12?QOpenGLTexture::LuminanceAlphaFormat:QOpenGLTexture::LuminanceFormat);
    //    mTexU->setFixedSamplePositions(false);
    mTexU->setMinificationFilter(QOpenGLTexture::Nearest);
    mTexU->setMagnificationFilter(QOpenGLTexture::Nearest);
    mTexU->setWrapMode(QOpenGLTexture::ClampToEdge);

    mTexV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mTexV->setFormat(QOpenGLTexture::LuminanceFormat);
    //    mTexV->setFixedSamplePositions(false);
    mTexV->setMinificationFilter(QOpenGLTexture::Nearest);
    mTexV->setMagnificationFilter(QOpenGLTexture::Nearest);
    mTexV->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void RealTimeRenderer::initGeometry() {
    mVertices << QVector3D(-1, 1, 0.0f) << QVector3D(1, 1, 0.0f) << QVector3D(1, -1, 0.0f) << QVector3D(-1, -1, 0.0f);
    mTexcoords << QVector2D(0, 1) << QVector2D(1, 1) << QVector2D(1, 0) << QVector2D(0, 0);

    mViewMatrix.setToIdentity();
    mViewMatrix.lookAt(QVector3D(0.0f, 0.0f, 1.001f), QVector3D(0.0f, 0.0f, -5.0f), QVector3D(0.0f, 1.0f, 0.0f));
    mModelMatrix.setToIdentity();
}
void RealTimeRenderer::updateTextureInfo(int width, int height, int format) {
    mPixFmt = format;
    if(!inited) {
        inited = true;
        initTexture();
    }
    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        // yuv420p
        mTexY->setSize(width, height);
        mTexY->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);

        mTexU->setSize(width / 2, height / 2);
        mTexU->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);

        mTexV->setSize(width / 2, height / 2);
        mTexV->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);
    } else if (format == AV_PIX_FMT_NV12) {
        mTexY->setSize(width, height);
        mTexY->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);

        mTexU->setSize(width / 2, height / 2);
        mTexU->allocateStorage(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8);

        // NV12 not use for v
        mTexV->setSize(2, 2);
        mTexV->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);
    } else {
        // 先按yuv444p处理
        mTexY->setSize(width, height);
        mTexY->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);

        mTexU->setSize(width, height);
        mTexU->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);

        mTexV->setSize(width, height);
        mTexV->allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8);
    }
    mTextureAlloced = true;
}

void RealTimeRenderer::updateTextureData(const std::shared_ptr<AVFrame> &data) {
    double frameWidth = m_itemWidth;
    double frameHeight = m_itemHeight;
    if (m_itemWidth * (1.0 * data->height / data->width) < m_itemHeight) {
        frameHeight = frameWidth * (1.0 * data->height / data->width);
    } else {
        frameWidth = frameHeight * (1.0 * data->width / data->height);
    }
    double x = (m_itemWidth - frameWidth) / 2;
    double y = (m_itemHeight - frameHeight) / 2;
    // GL顶点坐标转换
    auto x1 = (float)(-1 + 2.0 / m_itemWidth * x);
    auto y1 = (float)(1 - 2.0 / m_itemHeight * y);
    auto x2 = (float)(2.0 / m_itemWidth * frameWidth + x1);
    auto y2 = (float)(y1 - 2.0 / m_itemHeight * frameHeight);

    mVertices.clear();
    mVertices << QVector3D(x1, y1, 0.0f) << QVector3D(x2, y1, 0.0f) << QVector3D(x2, y2, 0.0f)
              << QVector3D(x1, y2, 0.0f);

    QOpenGLPixelTransferOptions options;
    if (data->linesize[0]) {
        options.setRowLength(data->linesize[0]);
        options.setImageHeight(data->height);
        mTexY->setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, data->data[0], &options);
    }
    if (data->linesize[1]) {
        if (data->format == AV_PIX_FMT_NV12) {
            options.setRowLength(data->linesize[1] / 2);
            options.setImageHeight(data->height / 2);
            mTexU->setData(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8, data->data[1], &options);
        } else {
            options.setRowLength(data->linesize[1]);
            options.setImageHeight(data->height);
            mTexU->setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, data->data[1], &options);
        }
    }
    if (data->linesize[2]) {
        options.setRowLength(data->linesize[2]);
        options.setImageHeight(data->height);
        mTexV->setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, data->data[2], &options);
    }
}
void RealTimeRenderer::paint() {
    glDepthMask(true);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!mTextureAlloced) {
        return;
    }
    if (mNeedClear) {
        mNeedClear = false;
        return;
    }
    mProgram.bind();

    mModelMatHandle = mProgram.uniformLocation("u_modelMatrix");
    mViewMatHandle = mProgram.uniformLocation("u_viewMatrix");
    mProjectMatHandle = mProgram.uniformLocation("u_projectMatrix");
    mVerticesHandle = mProgram.attributeLocation("qt_Vertex");
    mTexCoordHandle = mProgram.attributeLocation("texCoord");
    // 顶点
    mProgram.enableAttributeArray(mVerticesHandle);
    mProgram.setAttributeArray(mVerticesHandle, mVertices.constData());

    // 纹理坐标
    mProgram.enableAttributeArray(mTexCoordHandle);
    mProgram.setAttributeArray(mTexCoordHandle, mTexcoords.constData());

    // MVP矩阵
    mProgram.setUniformValue(mModelMatHandle, mModelMatrix);
    mProgram.setUniformValue(mViewMatHandle, mViewMatrix);
    mProgram.setUniformValue(mProjectMatHandle, mProjectionMatrix);

    // pixFmt
    mProgram.setUniformValue("pixFmt", mPixFmt);

    // 纹理
    //  Y
    glActiveTexture(GL_TEXTURE0);
    mTexY->bind();

    // U
    glActiveTexture(GL_TEXTURE1);
    mTexU->bind();

    // V
    glActiveTexture(GL_TEXTURE2);
    mTexV->bind();

    mProgram.setUniformValue("tex_y", 0);
    mProgram.setUniformValue("tex_u", 1);
    mProgram.setUniformValue("tex_v", 2);

    glDrawArrays(GL_TRIANGLE_FAN, 0, mVertices.size());

    mProgram.disableAttributeArray(mVerticesHandle);
    mProgram.disableAttributeArray(mTexCoordHandle);
    mProgram.release();
}

void RealTimeRenderer::clear() {
    mNeedClear = true;
}
