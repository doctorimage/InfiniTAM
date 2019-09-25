//
// Created by gerw on 9/25/19.
//

#include <OpenMesh/Core/IO/Options.hh>
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <iostream>
#include "GLModel.h"
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>


GLModel::GLModel(const std::string &meshPath) {
    {
        //come on, read mesh
        OpenMesh::IO::Options opt;
        opt.set(OpenMesh::IO::Options::VertexNormal | OpenMesh::IO::Options::VertexTexCoord);
        if (!OpenMesh::IO::read_mesh(mesh, meshPath, opt)) {
            std::cerr << "read mesh error " << meshPath << std::endl;
        }
    }

    vertices.resize(mesh.n_vertices());
    for (auto i = 0; i < int(mesh.n_vertices()); i++) {
        vertices[i].position = glm::make_vec3(mesh.points()[i].data());
        vertices[i].texCoords = glm::make_vec2(mesh.texcoords2D()[i].data());
        vertices[i].normal = glm::make_vec3(mesh.vertex_normals()[i].data());
    }

    {
        indices.resize(mesh.n_faces() * 3);
        int curIdx = 0;
        for (auto fit = mesh.faces_begin(); fit != mesh.faces_end(); ++fit) {
            for (auto vit = mesh.fv_begin(*fit); vit != mesh.fv_end(*fit); ++vit) {
                indices[curIdx++] = vit->idx();
            }
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.n_vertices() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, normal));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.n_faces() * sizeof(unsigned) * 3, indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void GLModel::draw() {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
