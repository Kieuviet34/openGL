#include "sphere.hpp"
#include <cmath>

struct _RawMesh {
    std::vector<float> verts;      // x,y,z, nx,ny,nz
    std::vector<unsigned int> idx;
};

static _RawMesh _makeRawSphere(unsigned int sectors, unsigned int stacks) {
    const float PI = 3.14159265359f;
    _RawMesh m;
    // vertices
    for (unsigned int i = 0; i <= stacks; ++i) {
        float st = PI/2 - i * PI/stacks;
        float xy = cos(st), z = sin(st);
        for (unsigned int j = 0; j <= sectors; ++j) {
            float se = j * 2*PI/sectors;
            float x = xy*cos(se), y = xy*sin(se);
            // pos
            m.verts.push_back(x);
            m.verts.push_back(y);
            m.verts.push_back(z);
            // normal (same as pos for unit sphere)
            m.verts.push_back(x);
            m.verts.push_back(y);
            m.verts.push_back(z);
        }
    }
    // indices
    for (unsigned int i = 0; i < stacks; ++i) {
        for (unsigned int j = 0; j < sectors; ++j) {
            unsigned int a = i*(sectors+1) + j;
            unsigned int b = a + sectors + 1;
            m.idx.push_back(a);
            m.idx.push_back(b);
            m.idx.push_back(a+1);
            m.idx.push_back(b);
            m.idx.push_back(b+1);
            m.idx.push_back(a+1);
        }
    }
    return m;
}

void Sphere::build(unsigned int sectorCount, unsigned int stackCount) {
    auto raw = _makeRawSphere(sectorCount, stackCount);
    indexCount = raw.idx.size();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      glBufferData(GL_ARRAY_BUFFER,
                   raw.verts.size()*sizeof(float),
                   raw.verts.data(),
                   GL_STATIC_DRAW);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   raw.idx.size()*sizeof(unsigned int),
                   raw.idx.data(),
                   GL_STATIC_DRAW);

      // pos
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                            6*sizeof(float), (void*)0);
      // normal
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                            6*sizeof(float),
                            (void*)(3*sizeof(float)));
    glBindVertexArray(0);
}