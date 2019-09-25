//
// Created by gerw on 9/25/19.
//

#ifndef INFINITAM_GLMODEL_H
#define INFINITAM_GLMODEL_H

#include <glad/glad.h>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Geometry/VectorT.hh>
#include <glm/glm.hpp>

struct MyTraits : public OpenMesh::DefaultTraits {
    VertexAttributes(OpenMesh::Attributes::Normal | OpenMesh::Attributes::TexCoord2D);
};

typedef OpenMesh::TriMesh_ArrayKernelT<MyTraits> MyMesh;

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoords;
    glm::vec3 normal;
};

class GLModel {
public:
    explicit GLModel(const std::string &meshPath);

    MyMesh mesh;

    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;

    unsigned vao{}, vbo{}, ebo{};

    void draw();
};


#endif //INFINITAM_GLMODEL_H
