// Author: Imanol Munoz-Pandiella 2023 based on Marc Comino 2020

#include <glwidget.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>

#include "./mesh_io.h"
#include "./triangle_mesh.h"

#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp>

namespace {

const double kFieldOfView = 60;
const double kZNear = 0.0001;
const double kZFar = 20;

const std::vector<std::vector<std::string>> kShaderFiles = {
    {"../shaders/phong.vert",        "../shaders/phong.frag"},
    {"../shaders/texMap.vert",       "../shaders/texMap.frag"},
    {"../shaders/reflection.vert",   "../shaders/reflection.frag"},
    {"../shaders/pbs.vert",          "../shaders/pbs.frag"},
    {"../shaders/ibl-pbs.vert",      "../shaders/ibl-pbs.frag"},
    {"../shaders/sky.vert",          "../shaders/sky.frag"},
    {"../shaders/ao-IBL.vert",       "../shaders/ao-IBL.frag"},
    {"../shaders/ao-compute.vert",   "../shaders/ao-compute.frag"},
    {"../shaders/ao-norm-alb.vert",  "../shaders/ao-norm-alb.frag"},
    {"../shaders/ao-depth.vert",     "../shaders/ao-depth.frag"},
    {"../shaders/ao-texWrite.vert",  "../shaders/ao-texWrite.frag"},
    {"../shaders/ao-show.vert",      "../shaders/ao-show.frag"},
    {"../shaders/ao-filter.vert",    "../shaders/ao-filter.frag"}};

const int kVertexAttributeIdx = 0;
const int kNormalAttributeIdx = 1;
const int kTexCoordAttributeIdx = 2;


bool ReadFile(const std::string filename, std::string *shader_source) {
    std::ifstream infile(filename.c_str());

    if (!infile.is_open() || !infile.good()) {
        std::cerr << "Error " + filename + " not found." << std::endl;
        return false;
    }

    std::stringstream stream;
    stream << infile.rdbuf();
    infile.close();

    *shader_source = stream.str();
    return true;
}

bool LoadImage(const std::string &path, GLuint cube_map_pos, GLint mipLevel = 0) {
    QImage image;
    bool res = image.load(path.c_str());
    if (res) {
        QImage gl_image = image.mirrored();
        glTexImage2D(cube_map_pos, mipLevel, GL_RGBA, image.width(), image.height(), 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
    }
    return res;
}

bool LoadCubeMap(const QString &dir) {
    std::string path = dir.toUtf8().constData();
    bool res = LoadImage(path + "/right.png", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    res = res && LoadImage(path + "/left.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    res = res && LoadImage(path + "/top.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    res = res && LoadImage(path + "/bottom.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    res = res && LoadImage(path + "/back.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    res = res && LoadImage(path + "/front.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    if (res) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    return res;
}

bool LoadProgram(const std::string &vertex, const std::string &fragment,
                 QOpenGLShaderProgram *program) {
    std::string vertex_shader, fragment_shader;
    bool res =
        ReadFile(vertex, &vertex_shader) && ReadFile(fragment, &fragment_shader);

    if (res) {
        program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                         vertex_shader.c_str());
        program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                         fragment_shader.c_str());
        program->bindAttributeLocation("vertex", kVertexAttributeIdx);
        program->bindAttributeLocation("normal", kNormalAttributeIdx);
        program->bindAttributeLocation("texCoord", kTexCoordAttributeIdx);
        program->link();
    }

    return res;
}

}  // namespace

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
    initialized_(false),
    width_(0.0),
    height_(0.0),
    currentShader_(0),
    fresnel_(0.05, 0.05, 0.05),
    currentTexture_(0),
    ao_currentTexture_(0),
    ao_samples_(5),
    ao_dirs_(5),
    ao_radius(0.05),
    skyVisible_(true),
    debugView_(false),
    aoComponent_(false),
    avaliable_color_(0),
    metalness_(0),
    roughness_(0)
{
    setFocusPolicy(Qt::StrongFocus);
}

GLWidget::~GLWidget() {
    if (initialized_) {
        glDeleteTextures(1, &specular_map_);
        glDeleteTextures(1, &diffuse_map_);
    }
}

bool GLWidget::LoadModel(const QString &filename) {
    std::string file = filename.toUtf8().constData();
    size_t pos = file.find_last_of(".");
    std::string type = file.substr(pos + 1);

    std::unique_ptr<data_representation::TriangleMesh> mesh =
        std::make_unique<data_representation::TriangleMesh>();

    bool res = false;
    if (type.compare("ply") == 0) {
        res = data_representation::ReadFromPly(file, mesh.get());
    } else if (type.compare("obj") == 0) {
        res = data_representation::ReadFromObj(file, mesh.get());
    } else if(type.compare("null") == 0) {
        res = data_representation::CreateSphere(mesh.get());
    }

    if (res) {
        mesh_.reset();
        mesh_.reset(mesh.release());
        camera_.UpdateModel(mesh_->min_, mesh_->max_);

        if (this->isValid()) {
            makeCurrent();
        }

        // Unbind everything first
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // Delete old buffers
        if (initialized_) {
            // For new models compute the normals
            mesh_->computeNormals();

            glDeleteBuffers(1, &VBO_v);
            glDeleteBuffers(1, &VBO_n);
            glDeleteBuffers(1, &VBO_tc);
            glDeleteBuffers(1, &VBO_i);
            glDeleteVertexArrays(1, &VAO);

            glDeleteBuffers(1, &VBO_v_sky);
            glDeleteBuffers(1, &VBO_i_sky);
            glDeleteVertexArrays(1, &VAO_sky);

            glDeleteBuffers(1, &VBO_v_quad);
            glDeleteBuffers(1, &VBO_i_quad);
            glDeleteVertexArrays(1, &VAO_quad);

            // Reset buffer IDs
            VAO = VBO_v = VBO_n = VBO_tc = VBO_i = 0;
            VAO_sky = VBO_v_sky = VBO_i_sky = 0;
            VAO_quad = VBO_v_quad = VBO_i_quad = 0;

            // Force synchronization
            glFinish();
        }

        // TODO(students): Create / Initialize buffers.
        // MESH: You need to create 1 VAO and 4 VBO
        // mesh_->vertices -> attrib location 0
        // mesh_->normals -> attrib location 1
        // mesh_->texCoords -> attrib location 2
        // mesh_->faces -> elements

        glGenVertexArrays(1,&VAO);
        glGenBuffers(1,&VBO_v);
        glGenBuffers(1,&VBO_n);
        glGenBuffers(1,&VBO_tc);
        glGenBuffers(1,&VBO_i);

        glBindVertexArray(VAO);
        // Vertices VBO data initialization
        glBindBuffer(GL_ARRAY_BUFFER,VBO_v);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*mesh_->vertices_.size(),&mesh_->vertices_[0],GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
        glEnableVertexAttribArray(0);
        // Normals VBO data initialization
        glBindBuffer(GL_ARRAY_BUFFER,VBO_n);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*mesh_->normals_.size(),&mesh_->normals_[0],GL_STATIC_DRAW);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,0);
        glEnableVertexAttribArray(1);
        // TextureCoords VBO data initialization
        glBindBuffer(GL_ARRAY_BUFFER,VBO_tc);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*mesh_->texCoords_.size(),&mesh_->texCoords_[0],GL_STATIC_DRAW);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,0,0);
        glEnableVertexAttribArray(2);
        // Faces VBO data initialization
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,VBO_i);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(int)*mesh_->faces_.size(),&mesh_->faces_[0],GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // SKY BOX: You need to create 1 VAO and 2 VBO:
        // vertices -> attrib location 0
        // faces -> elements

        /*
         *          4           5
         *      6           7
         *
         *
         *          0           1
         *      2           3
         */

        skyVertices_ = {
            -1.0f, -1.0f,  1.0f,    // 0
            1.0f, -1.0f,  1.0f,    // 1
            -1.0f, -1.0f, -1.0f,    // 2
            1.0f, -1.0f, -1.0f,    // 3
            -1.0f,  1.0f,  1.0f,    // 4
            1.0f,  1.0f,  1.0f,    // 5
            -1.0f,  1.0f, -1.0f,    // 6
            1.0f,  1.0f, -1.0f     // 7
        };

        skyFaces_ = {
            0,1,2,
            1,2,3,
            4,5,6,
            5,6,7,
            0,2,4,
            2,4,6,
            1,3,5,
            3,5,7,
            0,1,4,
            1,4,5,
            2,3,6,
            3,6,7
        };

        glGenVertexArrays(1,&VAO_sky);
        glGenBuffers(1,&VBO_v_sky);
        glGenBuffers(1,&VBO_i_sky);

        glBindVertexArray(VAO_sky);
        // Vertices VBO data initialization
        glBindBuffer(GL_ARRAY_BUFFER,VBO_v_sky);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*skyVertices_.size(),&skyVertices_[0],GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
        glEnableVertexAttribArray(0);
        // Faces VBO data initialization
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,VBO_i_sky);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(int)*skyFaces_.size(),&skyFaces_[0],GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // Add screen wide quad
        quadVertices_ = {
            -1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            -1.0, 1.0f, 0.0f,
            1.0f, -1.0f, 0.0f
        };

        quadFaces_ = {
            0,1,2,
            0,3,1
        };

        glGenVertexArrays(1,&VAO_quad);
        glGenBuffers(1,&VBO_v_quad);
        glGenBuffers(1,&VBO_i_quad);

        glBindVertexArray(VAO_quad);
        // Vertices VBO data initialization
        glBindBuffer(GL_ARRAY_BUFFER,VBO_v_quad);
        glBufferData(GL_ARRAY_BUFFER,sizeof(float)*quadVertices_.size(),&quadVertices_[0],GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
        glEnableVertexAttribArray(0);
        // Faces VBO data initialization
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,VBO_i_quad);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(int)*quadFaces_.size(),&quadFaces_[0],GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        initialized_ = true;

        // TODO END.

        emit SetFaces(QString(std::to_string(mesh_->faces_.size() / 3).c_str()));
        emit SetVertices(
            QString(std::to_string(mesh_->vertices_.size() / 3).c_str()));
        return true;
    }

    return false;
}

bool GLWidget::LoadSpecularMap(const QString &dir) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);

    // First load the base mip level (level 0)
    bool res = LoadCubeMap(dir);
    if (!res) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return false;
    }

    // Load the precomputed mip levels (1-4)
    for (int mipLevel = 1; mipLevel < 5; mipLevel++) {
        QString mipDir = dir + "/mip_" + QString::number(mipLevel);

        // Load each face of the cubemap for this mip level
        res = LoadImage((mipDir + "/right.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_POSITIVE_X, mipLevel);
        res = res && LoadImage((mipDir + "/left.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_NEGATIVE_X, mipLevel);
        res = res && LoadImage((mipDir + "/top.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_POSITIVE_Y, mipLevel);
        res = res && LoadImage((mipDir + "/bottom.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, mipLevel);
        res = res && LoadImage((mipDir + "/back.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_POSITIVE_Z, mipLevel);
        res = res && LoadImage((mipDir + "/front.png").toUtf8().constData(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, mipLevel);
    }

    // Set texture parameters for the mip chain
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 4);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    update();
    return res;
}

bool GLWidget::LoadDiffuseMap(const QString &dir) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
    bool res = LoadCubeMap(dir);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    update();
    return res;
}

bool GLWidget::LoadColorMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier color_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D).
    //Remember to configure the texture parameters.
    std::string path = filename.toUtf8().constData();
    glBindTexture(GL_TEXTURE_2D, color_map_);
    bool res = LoadImage(path, GL_TEXTURE_2D);

    if (res) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        avaliable_color_ = 1;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;

}

bool GLWidget::LoadRoughnessMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier roughness_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D)
    //Remember to configure the texture parameters.
    std::string path = filename.toUtf8().constData();
    glBindTexture(GL_TEXTURE_2D, roughness_map_);
    bool res = LoadImage(path, GL_TEXTURE_2D);

    if (res) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;
}

bool GLWidget::LoadMetalnessMap(const QString &filename)
{
    //TODO Students
    //Configure the texture with identifier metalness_map_. Take advantage of LoadImage("path", GL_TEXTURE_2D)
    //Remember to configure the texture parameters.
    std::string path = filename.toUtf8().constData();
    glBindTexture(GL_TEXTURE_2D, metalness_map_);
    bool res = LoadImage(path, GL_TEXTURE_2D);

    if (res) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    //TODO END
    update();
    return res;
}

void GLWidget::initializeGL ()
{
    // Cal inicialitzar l'ús de les funcions d'OpenGL
    initializeOpenGLFunctions();
    // Get current widget dimensions
    width_ = this->width();
    height_ = this->height();

    //initializing opengl state
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    //generating needed textures
    glGenTextures(1, &specular_map_);
    glGenTextures(1, &diffuse_map_);
    glGenTextures(1, &color_map_);
    glGenTextures(1, &roughness_map_);
    glGenTextures(1, &metalness_map_);

    // New textures for AO (albedo,normal,depth)
    glGenFramebuffers(1, &def_FrameBuffer);
    glGenTextures(1, &def_albedo_);
    glGenTextures(1, &def_normal_);
    glGenTextures(1, &def_material_);
    glGenTextures(1, &def_depth_);

    glBindFramebuffer(GL_FRAMEBUFFER, def_FrameBuffer);

    glBindTexture(GL_TEXTURE_2D, def_albedo_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, def_albedo_, 0);

    glBindTexture(GL_TEXTURE_2D, def_normal_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, def_normal_, 0);

    // For the metalness and roughness
    glBindTexture(GL_TEXTURE_2D, def_material_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width_, height_, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, def_material_, 0);

    glBindTexture(GL_TEXTURE_2D, def_depth_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, def_depth_, 0);

    // Define which color attachments to draw to
    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    // Check framebuffer completeness
    /*
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "Error: Framebuffer is not complete! Status: " << status;
    }
    */

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Precompute noise texture for AO directions
    glGenTextures(1, &noise_text_);

    std::vector<float> randomValues(64); // For a 8x8 texture
    for (int i = 0; i < 64; i++) {
        randomValues[i] = (float)rand() / (float)RAND_MAX;
    }

    glBindTexture(GL_TEXTURE_2D, noise_text_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 8, 8, 0, GL_RED, GL_FLOAT, randomValues.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &ao_filter_FrameBuffer);
    glGenTextures(1, &ao_filter_text_);

    glBindFramebuffer(GL_FRAMEBUFFER, ao_filter_FrameBuffer);
    glBindTexture(GL_TEXTURE_2D, ao_filter_text_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao_filter_text_, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &ao_FrameBuffer);
    glGenTextures(1, &ao_text_);

    glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);
    glBindTexture(GL_TEXTURE_2D, ao_text_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao_text_, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //create shader programs
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//phong
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//texture mapping
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//reflection
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//simple pbs
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//ibl pbs
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//sky
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOIBL
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOcompute
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOdebugNormalsAlbedo
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOdebugDepth
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOdebugwriteTex
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOdebugshowAO
    programs_.push_back(std::make_unique<QOpenGLShaderProgram>());//AOfilter

    //load vertex and fragment shader files
    bool res =   LoadProgram(kShaderFiles[0][0],   kShaderFiles[0][1],    programs_[0].get());
    res = res && LoadProgram(kShaderFiles[1][0],   kShaderFiles[1][1],    programs_[1].get());
    res = res && LoadProgram(kShaderFiles[2][0],   kShaderFiles[2][1],    programs_[2].get());
    res = res && LoadProgram(kShaderFiles[3][0],   kShaderFiles[3][1],    programs_[3].get());
    res = res && LoadProgram(kShaderFiles[4][0],   kShaderFiles[4][1],    programs_[4].get());
    res = res && LoadProgram(kShaderFiles[5][0],   kShaderFiles[5][1],    programs_[5].get());
    res = res && LoadProgram(kShaderFiles[6][0],   kShaderFiles[6][1],    programs_[6].get());
    res = res && LoadProgram(kShaderFiles[7][0],   kShaderFiles[7][1],    programs_[7].get());
    res = res && LoadProgram(kShaderFiles[8][0],   kShaderFiles[8][1],    programs_[8].get());
    res = res && LoadProgram(kShaderFiles[9][0],   kShaderFiles[9][1],    programs_[9].get());
    res = res && LoadProgram(kShaderFiles[10][0],   kShaderFiles[10][1],    programs_[10].get());
    res = res && LoadProgram(kShaderFiles[11][0],   kShaderFiles[11][1],    programs_[11].get());
    res = res && LoadProgram(kShaderFiles[12][0],   kShaderFiles[12][1],    programs_[12].get());

    if (!res) exit(0);

    LoadModel(".null"); //create sphere

    initialized_ = true;
}

void GLWidget::resizeGL(int w, int h)
{
    if (h == 0) h = 1;
    width_ = w;
    height_ = h;

    camera_.SetViewport(0, 0, w, h);
    camera_.SetProjection(kFieldOfView, kZNear, kZFar);

    // Resize AO textures
    if (initialized_) {
        glBindFramebuffer(GL_FRAMEBUFFER, def_FrameBuffer);

        // Resize albedo texture
        glBindTexture(GL_TEXTURE_2D, def_albedo_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, def_albedo_, 0);

        // Resize normal texture
        glBindTexture(GL_TEXTURE_2D, def_normal_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, def_normal_, 0);

        // Resize material texture
        glBindTexture(GL_TEXTURE_2D, def_material_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width_, height_, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, def_material_, 0);

        // Resize depth texture
        glBindTexture(GL_TEXTURE_2D, def_depth_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, def_depth_, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, ao_filter_FrameBuffer);

        glBindTexture(GL_TEXTURE_2D, ao_filter_text_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao_filter_text_, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);

        glBindTexture(GL_TEXTURE_2D, ao_text_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao_text_, 0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void GLWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        camera_.StartRotating(event->x(), event->y());
    }
    if (event->button() == Qt::RightButton) {
        camera_.StartZooming(event->x(), event->y());
    }
    update();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event) {
    camera_.SetRotationX(event->y());
    camera_.SetRotationY(event->x());
    camera_.SafeZoom(event->y());
    update();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        camera_.StopRotating(event->x(), event->y());
    }
    if (event->button() == Qt::RightButton) {
        camera_.StopZooming(event->x(), event->y());
    }
    update();
}

void GLWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Up) camera_.Zoom(-1);
    if (event->key() == Qt::Key_Down) camera_.Zoom(1);

    if (event->key() == Qt::Key_Left) camera_.Rotate(-1);
    if (event->key() == Qt::Key_Right) camera_.Rotate(1);

    if (event->key() == Qt::Key_W) camera_.Zoom(-1);
    if (event->key() == Qt::Key_S) camera_.Zoom(1);

    if (event->key() == Qt::Key_A) camera_.Rotate(-1);
    if (event->key() == Qt::Key_D) camera_.Rotate(1);

    if (event->key() == Qt::Key_R) {
        for(auto i = 0; i < programs_.size(); ++i) {
            programs_[i].reset();
            programs_[i] = std::make_unique<QOpenGLShaderProgram>();
            LoadProgram(kShaderFiles[i][0], kShaderFiles[i][1], programs_[i].get());
        }
    }

    update();
}


void GLWidget::paintGL ()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (initialized_) {
        camera_.SetViewport();

        glm::mat4x4 projection = camera_.SetProjection();
        glm::mat4x4 view = camera_.SetView();
        glm::mat4x4 model = camera_.SetModel();

        //compute normal matrix
        glm::mat4x4 t = view * model;
        glm::mat3x3 normal;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                normal[i][j] = t[i][j];
        normal = glm::transpose(glm::inverse(normal));

        // Compute inverse of view matrix as a uniform
        glm::mat4x4 iview = glm::inverse(view);

        if (mesh_ != nullptr) {
            GLint projection_location, view_location, inv_view_location, model_location,
                normal_matrix_location, specular_map_location, diffuse_map_location,
                fresnel_location, color_map_location, roughness_map_location, metalness_map_location,
                current_text_location, light_location, roughness_location, metalness_location, usePBStex_location, useIBLdirl_location;

            if (!debugView_){

                if (aoComponent_){
                    GLint def_albedo_location, def_normal_location, def_depth_location, defaultFramebuffer, near_location, far_location,
                        fov_location, aspect_ratio_location, num_samples_location, num_dirs_location, radius_location, width_location,
                        height_location, noise_tex_location, av_color_location, ao_filtered_location;

                    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFramebuffer);

                    // First pass -------------------------------------------------------------------------------------------
                    programs_[programs_.size()-3]->bind();
                    glBindFramebuffer(GL_FRAMEBUFFER,def_FrameBuffer);

                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    projection_location     = programs_[programs_.size()-3]->uniformLocation("projection");
                    view_location           = programs_[programs_.size()-3]->uniformLocation("view");
                    model_location          = programs_[programs_.size()-3]->uniformLocation("model");
                    normal_matrix_location  = programs_[programs_.size()-3]->uniformLocation("normal_matrix");
                    color_map_location      = programs_[programs_.size()-3]->uniformLocation("color_map");
                    roughness_map_location  = programs_[programs_.size()-3]->uniformLocation("roughness_map");
                    metalness_map_location  = programs_[programs_.size()-3]->uniformLocation("metalness_map");
                    av_color_location       = programs_[programs_.size()-3]->uniformLocation("av_color");

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, color_map_);
                    glUniform1i(color_map_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, roughness_map_);
                    glUniform1i(roughness_map_location, 1);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, metalness_map_);
                    glUniform1i(metalness_map_location, 2);
                    glUniform1i(av_color_location, avaliable_color_);

                    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                    glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                    glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                    glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                    glBindVertexArray(VAO);
                    glDrawElements(GL_TRIANGLES,mesh_->faces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    // Write the sky to the albedo
                    unsigned int attachmentsSky[1] = {GL_COLOR_ATTACHMENT0};
                    glDrawBuffers(1, attachmentsSky);

                    // Ignore camera translation
                    view = glm::mat4(glm::mat3(camera_.SetView()));

                    programs_[programs_.size()-8]->bind();

                    projection_location     = programs_[programs_.size()-8]->uniformLocation("projection");
                    view_location           = programs_[programs_.size()-8]->uniformLocation("view");
                    model_location          = programs_[programs_.size()-8]->uniformLocation("model");
                    normal_matrix_location  = programs_[programs_.size()-8]->uniformLocation("normal_matrix");
                    specular_map_location   = programs_[programs_.size()-8]->uniformLocation("specular_map");

                    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                    glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                    glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                    glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
                    glUniform1i(specular_map_location, 0);

                    // TODO(students): implement the draw call of the sky box
                    glDepthFunc(GL_LEQUAL);
                    glBindVertexArray(VAO_sky);
                    glDrawElements(GL_TRIANGLES,skyFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);
                    glDepthFunc(GL_LESS);
                    // TODO END.

                    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
                    glDrawBuffers(3, attachments);
                    view = camera_.SetView();

                    // Second pass ----------------------------------------------------------------------------------------

                    glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-6]->bind();

                    def_normal_location     = programs_[programs_.size()-6]->uniformLocation("def_normal");
                    def_depth_location      = programs_[programs_.size()-6]->uniformLocation("def_depth");
                    noise_tex_location      = programs_[programs_.size()-6]->uniformLocation("noise_tex");
                    near_location           = programs_[programs_.size()-6]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-6]->uniformLocation("far");
                    fov_location            = programs_[programs_.size()-6]->uniformLocation("fov");
                    aspect_ratio_location   = programs_[programs_.size()-6]->uniformLocation("a_ratio");
                    num_samples_location    = programs_[programs_.size()-6]->uniformLocation("num_samples");
                    num_dirs_location       = programs_[programs_.size()-6]->uniformLocation("num_directions");
                    radius_location         = programs_[programs_.size()-6]->uniformLocation("radius");
                    width_location          = programs_[programs_.size()-6]->uniformLocation("vp_width");
                    height_location         = programs_[programs_.size()-6]->uniformLocation("vp_height");

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, def_normal_);
                    glUniform1i(def_normal_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 1);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, noise_text_);
                    glUniform1i(noise_tex_location, 2);

                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glUniform1f(fov_location, (float)kFieldOfView * (glm::pi<float>()/180));
                    glUniform1f(aspect_ratio_location, (float)(width_/height_));
                    glUniform1f(radius_location, ao_radius);
                    glUniform1f(width_location, (float)(width_));
                    glUniform1f(height_location, (float)(height_));

                    glUniform1i(num_samples_location, ao_samples_);
                    glUniform1i(num_dirs_location, ao_dirs_);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    // Third pass ----------------------------------------------------------------------------------------

                    GLint ao_tex_location, texel_size_location, direction_location, def_material_location;

                    glBindFramebuffer(GL_FRAMEBUFFER, ao_filter_FrameBuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-1]->bind();

                    near_location           = programs_[programs_.size()-1]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-1]->uniformLocation("far");
                    def_depth_location      = programs_[programs_.size()-1]->uniformLocation("def_depth");
                    ao_tex_location         = programs_[programs_.size()-1]->uniformLocation("ao_tex");
                    texel_size_location     = programs_[programs_.size()-1]->uniformLocation("texelSize");
                    direction_location      = programs_[programs_.size()-1]->uniformLocation("direction");

                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, ao_text_);
                    glUniform1i(ao_tex_location, 1);
                    glUniform2f(texel_size_location, (float)(1.0/width_), (float)(1.0/height_));
                    glUniform1i(direction_location, 0);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-1]->bind();

                    near_location           = programs_[programs_.size()-1]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-1]->uniformLocation("far");
                    def_depth_location      = programs_[programs_.size()-1]->uniformLocation("def_depth");
                    ao_tex_location         = programs_[programs_.size()-1]->uniformLocation("ao_tex");
                    texel_size_location     = programs_[programs_.size()-1]->uniformLocation("texelSize");
                    direction_location      = programs_[programs_.size()-1]->uniformLocation("direction");

                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, ao_filter_text_);
                    glUniform1i(ao_tex_location, 1);
                    glUniform2f(texel_size_location, (float)(1.0/width_), (float)(1.0/height_));
                    glUniform1i(direction_location, 1);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    // IBL shader + AO contribution + SkyBox

                    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-7]->bind();

                    view_location           = programs_[programs_.size()-7]->uniformLocation("view");
                    inv_view_location       = programs_[programs_.size()-7]->uniformLocation("inv_view");
                    ao_filtered_location    = programs_[programs_.size()-7]->uniformLocation("ao_texture");
                    def_albedo_location     = programs_[programs_.size()-7]->uniformLocation("def_albedo");
                    def_normal_location     = programs_[programs_.size()-7]->uniformLocation("def_normal");
                    def_depth_location      = programs_[programs_.size()-7]->uniformLocation("def_depth");
                    def_material_location   = programs_[programs_.size()-7]->uniformLocation("def_material");
                    fresnel_location        = programs_[programs_.size()-7]->uniformLocation("fresnel");
                    light_location          = programs_[programs_.size()-7]->uniformLocation("light");
                    roughness_location      = programs_[programs_.size()-7]->uniformLocation("roughness");
                    metalness_location      = programs_[programs_.size()-7]->uniformLocation("metalness");
                    usePBStex_location      = programs_[programs_.size()-7]->uniformLocation("pbstex_use");
                    useIBLdirl_location     = programs_[programs_.size()-7]->uniformLocation("direct_light");
                    near_location           = programs_[programs_.size()-7]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-7]->uniformLocation("far");
                    fov_location            = programs_[programs_.size()-7]->uniformLocation("fov");
                    aspect_ratio_location   = programs_[programs_.size()-7]->uniformLocation("a_ratio");
                    specular_map_location   = programs_[programs_.size()-7]->uniformLocation("specular_map");
                    diffuse_map_location    = programs_[programs_.size()-7]->uniformLocation("diffuse_map");

                    glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                    glUniformMatrix4fv(inv_view_location, 1, GL_FALSE, &iview[0][0]);
                    glUniform1i(usePBStex_location, usePBStex_);
                    glUniform1i(useIBLdirl_location, useIBLdirl_);
                    glUniform3f(fresnel_location, fresnel_[0], fresnel_[1], fresnel_[2]);
                    glUniform3f(light_location, 0.5f, 0.5f, 0.5f);
                    glUniform1f(roughness_location, roughness_);
                    glUniform1f(metalness_location, metalness_);
                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glUniform1f(fov_location, (float)kFieldOfView * (glm::pi<float>()/180));
                    glUniform1f(aspect_ratio_location, (float)(width_/height_));

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, ao_text_);
                    glUniform1i(ao_filtered_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, def_albedo_);
                    glUniform1i(def_albedo_location, 1);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, def_normal_);
                    glUniform1i(def_normal_location, 2);
                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 3);
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
                    glUniform1i(specular_map_location, 4);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
                    glUniform1i(diffuse_map_location, 5);
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, def_material_);
                    glUniform1i(def_material_location, 6);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);
                }
                else{
                    //MESH-----------------------------------------------------------------------------------------
                    //general shader setting

                    programs_[currentShader_]->bind();

                    projection_location       = programs_[currentShader_]->uniformLocation("projection");
                    view_location             = programs_[currentShader_]->uniformLocation("view");
                    inv_view_location         = programs_[currentShader_]->uniformLocation("inv_view");
                    model_location            = programs_[currentShader_]->uniformLocation("model");
                    normal_matrix_location    = programs_[currentShader_]->uniformLocation("normal_matrix");
                    specular_map_location     = programs_[currentShader_]->uniformLocation("specular_map");
                    diffuse_map_location      = programs_[currentShader_]->uniformLocation("diffuse_map");
                    color_map_location        = programs_[currentShader_]->uniformLocation("color_map");
                    roughness_map_location    = programs_[currentShader_]->uniformLocation("roughness_map");
                    metalness_map_location    = programs_[currentShader_]->uniformLocation("metalness_map");
                    current_text_location     = programs_[currentShader_]->uniformLocation("current_texture");
                    fresnel_location          = programs_[currentShader_]->uniformLocation("fresnel");
                    light_location            = programs_[currentShader_]->uniformLocation("light");
                    roughness_location        = programs_[currentShader_]->uniformLocation("roughness");
                    metalness_location        = programs_[currentShader_]->uniformLocation("metalness");
                    usePBStex_location        = programs_[currentShader_]->uniformLocation("pbstex_use");
                    useIBLdirl_location       = programs_[currentShader_]->uniformLocation("direct_light");

                    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                    glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                    glUniformMatrix4fv(inv_view_location, 1, GL_FALSE, &iview[0][0]);
                    glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                    glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
                    glUniform1i(specular_map_location, 0);

                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
                    glUniform1i(diffuse_map_location, 1);

                    //TODO(students): active texture location for the following textures:
                    //Texture unit 3 color_map_
                    //Texture unit 4 roughness_map_
                    //Texture unit 5 metalness_map_

                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D, color_map_);
                    glUniform1i(color_map_location, 3);

                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, roughness_map_);
                    glUniform1i(roughness_map_location, 4);

                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, metalness_map_);
                    glUniform1i(metalness_map_location, 5);

                    //TODO END
                    glUniform1i(current_text_location, currentTexture_ + 3);
                    glUniform1i(usePBStex_location, usePBStex_);
                    glUniform1i(useIBLdirl_location, useIBLdirl_);
                    glUniform3f(fresnel_location, fresnel_[0], fresnel_[1], fresnel_[2]);
                    glUniform3f(light_location, 0.5f, 0.5f, 0.5f);
                    glUniform1f(roughness_location, roughness_);
                    glUniform1f(metalness_location, metalness_);

                    // TODO(students): Implement draw call of the mesh
                    glBindVertexArray(VAO);
                    glDrawElements(GL_TRIANGLES,mesh_->faces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);
                    // TODO END.
                }
            }

            else { // Debug view code for AO
                GLint def_albedo_location, def_normal_location, def_depth_location, defaultFramebuffer, near_location, far_location,
                      fov_location, aspect_ratio_location, num_samples_location, num_dirs_location, radius_location, width_location,
                      height_location, noise_tex_location, av_color_location, ao_filtered_location;
                glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFramebuffer);

                // First pass -------------------------------------------------------------------------------------------
                programs_[programs_.size()-3]->bind();
                glBindFramebuffer(GL_FRAMEBUFFER,def_FrameBuffer);

                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                projection_location     = programs_[programs_.size()-3]->uniformLocation("projection");
                view_location           = programs_[programs_.size()-3]->uniformLocation("view");
                model_location          = programs_[programs_.size()-3]->uniformLocation("model");
                normal_matrix_location  = programs_[programs_.size()-3]->uniformLocation("normal_matrix");
                color_map_location      = programs_[programs_.size()-3]->uniformLocation("color_map");
                av_color_location       = programs_[programs_.size()-3]->uniformLocation("av_color");

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, color_map_);
                glUniform1i(color_map_location, 1);
                glUniform1i(av_color_location, avaliable_color_);

                glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                glBindVertexArray(VAO);
                glDrawElements(GL_TRIANGLES,mesh_->faces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                glBindVertexArray(0);

                // Second pass ----------------------------------------------------------------------------------------

                int pId = 6;
                // Select between Albedo,Normal,Depth
                if (ao_currentTexture_ == 0 || ao_currentTexture_ == 1){
                    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
                    pId = 5;
                }
                else if (ao_currentTexture_ == 2){
                    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
                    pId = 4;
                }
                else if (ao_currentTexture_ == 3){
                    glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);
                    pId = 6;
                }

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                programs_[programs_.size()-pId]->bind();

                projection_location     = programs_[programs_.size()-pId]->uniformLocation("projection");
                view_location           = programs_[programs_.size()-pId]->uniformLocation("view");
                model_location          = programs_[programs_.size()-pId]->uniformLocation("model");
                normal_matrix_location  = programs_[programs_.size()-pId]->uniformLocation("normal_matrix");
                def_albedo_location     = programs_[programs_.size()-pId]->uniformLocation("def_albedo");
                def_normal_location     = programs_[programs_.size()-pId]->uniformLocation("def_normal");
                def_depth_location      = programs_[programs_.size()-pId]->uniformLocation("def_depth");
                noise_tex_location      = programs_[programs_.size()-pId]->uniformLocation("noise_tex");
                current_text_location   = programs_[programs_.size()-pId]->uniformLocation("current_texture");
                near_location           = programs_[programs_.size()-pId]->uniformLocation("near");
                far_location            = programs_[programs_.size()-pId]->uniformLocation("far");
                fov_location            = programs_[programs_.size()-pId]->uniformLocation("fov");
                aspect_ratio_location   = programs_[programs_.size()-pId]->uniformLocation("a_ratio");
                num_samples_location    = programs_[programs_.size()-pId]->uniformLocation("num_samples");
                num_dirs_location       = programs_[programs_.size()-pId]->uniformLocation("num_directions");
                radius_location         = programs_[programs_.size()-pId]->uniformLocation("radius");
                width_location          = programs_[programs_.size()-pId]->uniformLocation("vp_width");
                height_location         = programs_[programs_.size()-pId]->uniformLocation("vp_height");

                glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, def_albedo_);
                glUniform1i(def_albedo_location, 0);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, def_normal_);
                glUniform1i(def_normal_location, 1);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, def_depth_);
                glUniform1i(def_depth_location, 2);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, noise_text_);
                glUniform1i(noise_tex_location, 3);
                glUniform1i(current_text_location, ao_currentTexture_);

                glUniform1f(near_location, (float)kZNear);
                glUniform1f(far_location, (float)kZFar);
                glUniform1f(fov_location, (float)kFieldOfView * (glm::pi<float>()/180));
                glUniform1f(aspect_ratio_location, (float)(width_/height_));
                glUniform1f(radius_location, ao_radius);
                glUniform1f(width_location, (float)(width_));
                glUniform1f(height_location, (float)(height_));

                glUniform1i(num_samples_location, ao_samples_);
                glUniform1i(num_dirs_location, ao_dirs_);

                glBindVertexArray(VAO_quad);
                glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                glBindVertexArray(0);

                // Third pass ----------------------------------------------------------------------------------------
                if (ao_currentTexture_ == 3) {
                    GLint ao_tex_location, texel_size_location, direction_location;

                    glBindFramebuffer(GL_FRAMEBUFFER, ao_filter_FrameBuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-1]->bind();

                    near_location           = programs_[programs_.size()-1]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-1]->uniformLocation("far");
                    def_depth_location      = programs_[programs_.size()-1]->uniformLocation("def_depth");
                    ao_tex_location         = programs_[programs_.size()-1]->uniformLocation("ao_tex");
                    texel_size_location     = programs_[programs_.size()-1]->uniformLocation("texelSize");
                    direction_location      = programs_[programs_.size()-1]->uniformLocation("direction");

                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, ao_text_);
                    glUniform1i(ao_tex_location, 1);
                    glUniform2f(texel_size_location, (float)(1.0/width_), (float)(1.0/height_));
                    glUniform1i(direction_location, 0);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    glBindFramebuffer(GL_FRAMEBUFFER, ao_FrameBuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-1]->bind();

                    near_location           = programs_[programs_.size()-1]->uniformLocation("near");
                    far_location            = programs_[programs_.size()-1]->uniformLocation("far");
                    def_depth_location      = programs_[programs_.size()-1]->uniformLocation("def_depth");
                    ao_tex_location         = programs_[programs_.size()-1]->uniformLocation("ao_tex");
                    texel_size_location     = programs_[programs_.size()-1]->uniformLocation("texelSize");
                    direction_location      = programs_[programs_.size()-1]->uniformLocation("direction");

                    glUniform1f(near_location, (float)kZNear);
                    glUniform1f(far_location, (float)kZFar);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, def_depth_);
                    glUniform1i(def_depth_location, 0);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, ao_filter_text_);
                    glUniform1i(ao_tex_location, 1);
                    glUniform2f(texel_size_location, (float)(1.0/width_), (float)(1.0/height_));
                    glUniform1i(direction_location, 1);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);

                    // Filtered AO (two directions)

                    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    programs_[programs_.size()-2]->bind();

                    ao_filtered_location    = programs_[programs_.size()-2]->uniformLocation("ao_texture");

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, ao_text_);
                    glUniform1i(ao_filtered_location, 0);

                    glBindVertexArray(VAO_quad);
                    glDrawElements(GL_TRIANGLES,quadFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                    glBindVertexArray(0);
                }

            }

            //SKY-----------------------------------------------------------------------------------------
            if(skyVisible_ && !debugView_ && !aoComponent_) {
                //model = camera_.SetIdentity();

                // Ignore camera translation
                view = glm::mat4(glm::mat3(camera_.SetView()));

                programs_[programs_.size()-8]->bind();

                projection_location     = programs_[programs_.size()-8]->uniformLocation("projection");
                view_location           = programs_[programs_.size()-8]->uniformLocation("view");
                model_location          = programs_[programs_.size()-8]->uniformLocation("model");
                normal_matrix_location  = programs_[programs_.size()-8]->uniformLocation("normal_matrix");
                specular_map_location   = programs_[programs_.size()-8]->uniformLocation("specular_map");

                glUniformMatrix4fv(projection_location, 1, GL_FALSE, &projection[0][0]);
                glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
                glUniformMatrix4fv(model_location, 1, GL_FALSE, &model[0][0]);
                glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, &normal[0][0]);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
                glUniform1i(specular_map_location, 0);

                // TODO(students): implement the draw call of the sky box
                glDepthFunc(GL_LEQUAL);
                glBindVertexArray(VAO_sky);
                glDrawElements(GL_TRIANGLES,skyFaces_.size(),GL_UNSIGNED_INT,(GLvoid*)0);
                glBindVertexArray(0);
                glDepthFunc(GL_LESS);
                // TODO END.
            }
        }
    }
}

void GLWidget::SetReflection(bool set) {
    if(set) currentShader_ = 2;
    update();
}

void GLWidget::SetPBS(bool set) {
    if(set) currentShader_ = 3;
    update();
}

void GLWidget::SetIBLPBS(bool set) {
    if(set) currentShader_ = 4;
    update();
}

void GLWidget::SetPhong(bool set)
{
    if(set) currentShader_ = 0;
    update();
}

void GLWidget::SetTexMap(bool set)
{
    if(set) currentShader_ = 1;
    update();
}

void GLWidget::SetFresnelR(double r) {
    fresnel_[0] = r;
    update();
}

void GLWidget::SetFresnelG(double g) {
    fresnel_[1] = g;
    update();
}

void GLWidget::SetCurrentTexture(int i)
{
    currentTexture_ = i;
    update();
}

void GLWidget::SetCurrentTextureAO(int i)
{
    ao_currentTexture_ = i;
    update();
}

void GLWidget::SetNumSamplesAO(int s)
{
    ao_samples_ = s;
    update();
}

void GLWidget::SetNumDirsAO(int d)
{
    ao_dirs_ = d;
    update();
}

void GLWidget::SetRadiusAO(double r)
{
    ao_radius = r;
    update();
}

void GLWidget::SetSkyVisible(bool set)
{
    skyVisible_ = set;
    update();
}

void GLWidget::SetDebugView(bool set)
{
    debugView_ = set;
    update();
}

void GLWidget::SetAOContribution(bool set)
{
    aoComponent_ = set;
    update();
}

void GLWidget::SetPBSTexture(bool set)
{
    set ? usePBStex_ = 1 : usePBStex_ = 0;
    update();
}

void GLWidget::SetIBLDirectLight(bool set)
{
    set ? useIBLdirl_ = 1 : useIBLdirl_ = 0;
    update();
}

void GLWidget::SetFresnelB(double b) {
    fresnel_[2] = b;
    update();
}

void GLWidget::SetMetalness(double d) {
    metalness_ = d;
    update();
}

void GLWidget::SetRoughness(double d) {
    roughness_ = d;
    update();
}
