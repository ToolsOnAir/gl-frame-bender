/* Copyright (c) 2015 Heinrich Fink <hfink@toolsonair.com>
 * Copyright (c) 2014 ToolsOnAir Broadcast Engineering GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "Precompile.h"

#include "Quad.h"
#include "Semantics.h"
#include "UtilsGL.h"

namespace fb = toa::frame_bender;

fb::gl::Quad::Quad(UV_ORIGIN uv_origin) :
    uv_origin_(uv_origin),
    vertex_vbo_id_(0),
    element_vbo_id_(0),
    vertex_array_id_(0)
{

    // TODO-DEF: is there any advantage of using the OGL 4.3 
    // ARB_vertex_attrib_binding?

    // TODO-C++11: When VS2012 catches up with initialization_lists you 
    // should be able to directly assign, I think.

    // Filling the vertex with VTX and UV coords
    std::array<glm::vec2, 8> vertex_data_uv_upper_left = {{
        glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 1.0f),
        glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f),
        glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f)
    }};

    std::array<glm::vec2, 8> vertex_data_uv_lower_left = {{
        glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 0.0f),
        glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 0.0f),
        glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 1.0f),
        glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
    }};

    std::array<GLushort, 6> element_data = {{
        0, 1, 2, 
        2, 3, 0
    }};

    // Create element buffer ("indices")
    glGenBuffers(1, &element_vbo_id_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_vbo_id_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, 
        element_data.size()*sizeof(GLushort), 
        element_data.data(), 
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Create vertex buffer
    glGenBuffers(1, &vertex_vbo_id_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_vbo_id_);
    if (uv_origin_ == UV_ORIGIN::LOWER_LEFT) {

        glBufferData(
            GL_ARRAY_BUFFER, 
            vertex_data_uv_lower_left.size()*sizeof(glm::vec2), 
            vertex_data_uv_lower_left.data(), 
            GL_STATIC_DRAW);

    } else if (uv_origin_ == UV_ORIGIN::UPPER_LEFT) {

        glBufferData(
            GL_ARRAY_BUFFER, 
            vertex_data_uv_upper_left.size()*sizeof(glm::vec2), 
            vertex_data_uv_upper_left.data(), 
            GL_STATIC_DRAW);

    } else {

        FB_LOG_ERROR << "Unknow UV origin definition: " << static_cast<int32_t>(uv_origin_);
        throw std::runtime_error("Unknown origin definition.");

    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &vertex_array_id_);
    glBindVertexArray(vertex_array_id_);
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertex_vbo_id_);

        glVertexAttribPointer(
            static_cast<GLuint>(semantics::VertexAttributes::POSITION),
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::vec2) * 2, // TODO-DEF: maybe create a more generic vertex wrapper?
            FB_GL_BUFFER_OFFSET(0));

        glVertexAttribPointer(
            static_cast<GLuint>(semantics::VertexAttributes::TEXCOORD0),
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::vec2) * 2, // TODO-DEF: maybe create a more generic vertex wrapper?
            FB_GL_BUFFER_OFFSET(sizeof(glm::vec2)));

        // TODO-DEF: maybe create a templated enum cast helper function?
        glEnableVertexAttribArray(static_cast<GLuint>(semantics::VertexAttributes::POSITION));
        glEnableVertexAttribArray(static_cast<GLuint>(semantics::VertexAttributes::TEXCOORD0));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);

}

fb::gl::Quad::~Quad() {

    glDeleteVertexArrays(1, &vertex_array_id_);
    vertex_array_id_ = 0;

    glDeleteBuffers(1, &vertex_vbo_id_);
    vertex_vbo_id_ = 0;

    glDeleteBuffers(1, &element_vbo_id_);
    element_vbo_id_ = 0;

}

void fb::gl::Quad::draw() const {

    // Bind the VAO
    glBindVertexArray(vertex_array_id_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_vbo_id_);

    glDrawElementsInstancedBaseVertexBaseInstance(
        GL_TRIANGLES, 
        6, // TODO: necessary to generalize? 
        GL_UNSIGNED_SHORT, 
        NULL, 
        1, 
        0, 
        0);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

}
