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
#include "UtilsGL.h"
#include "ProgramOptions.h"

#include <sstream>
#include <regex>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bf = boost::filesystem;

namespace fb = toa::frame_bender;

fb::gl::utils::GLInfo fb::gl::utils::get_info() {

    GLint major = 0;
    GLint minor = 0;

    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);


    const GLubyte* ver_str = glGetString(GL_VERSION);
    const GLubyte* vendor_str = glGetString(GL_VENDOR);
    const GLubyte* renderer_str = glGetString(GL_RENDERER);

    GLInfo answer;
    answer.major = static_cast<int32_t>(major);
    answer.minor = static_cast<int32_t>(minor);

    if (ver_str != nullptr)
        answer.version_string = std::string(reinterpret_cast<const char*>(ver_str));

    if (vendor_str != nullptr)
        answer.vendor = std::string(reinterpret_cast<const char*>(vendor_str));

    if (renderer_str != nullptr)
        answer.renderer = std::string(reinterpret_cast<const char*>(renderer_str));

    return answer;

}

std::string fb::gl::utils::read_glsl_source_file(
    const std::string& shader_source_location,
    std::set<std::string> already_included_files)
{

    // TODO-C++11: replace with raw-string literals?
    static const std::regex include_pattern("^(\\s*)#fb_include <(.+)>\\s*$");

    auto res = already_included_files.insert(shader_source_location);
    FB_ASSERT(res.second);

    bf::path glsl_shader_folder = bf::path(
        ProgramOptions::global().glsl_folder());

    if (!bf::exists(glsl_shader_folder))
        throw std::invalid_argument(
            "Path '" + glsl_shader_folder.string() +"' does not exist.");

    if (!bf::is_directory(glsl_shader_folder))
        throw std::invalid_argument(
            "Path '" + glsl_shader_folder.string() +"' is not a folder.");    

    bf::path shader_source_file = 
        glsl_shader_folder / bf::path(shader_source_location);

    if (!bf::exists(shader_source_file)) {

        FB_LOG_ERROR << "Shader file '" << shader_source_file.string() << "' does not exist.";

        throw std::invalid_argument(
            "Shader file '" + shader_source_file.string() +"' does not exist.");
    }

    bf::ifstream file_reader(
        shader_source_file,
        std::ios::in);

    std::ostringstream oss;
    std::string line;
    while ( file_reader.good() )
    {
        std::getline(file_reader,line);

        std::smatch include_matches;
        // Check if we have got an include line
        if (std::regex_match(line, include_matches, include_pattern)) {

            if (include_matches.size() != 3)
                throw std::runtime_error("Unexpected regex match... ");

            std::string indentation = include_matches[1].str();
            std::string file_include = include_matches[2].str();

            if (already_included_files.find(file_include) != std::end(already_included_files)) {
                FB_LOG_ERROR << "Including file '" << file_include << "' in shader file '" << shader_source_location << "' would result in a circular dependency. Fix your code.";
                throw std::runtime_error("Circular shader include.");
            }

            FB_LOG_DEBUG << "Shader file '" << shader_source_location << "' includes file '" << file_include << "'.";

            std::string include_source = read_glsl_source_file(file_include, already_included_files);

            std::istringstream oss_include_sources(include_source);

            oss << indentation << "// [BEGIN] of include '" << file_include << "' \n";
            while (oss_include_sources.good()) {
                std::string include_line;
                std::getline(oss_include_sources, include_line);
                include_line = indentation + include_line;
                oss << include_line << "\n";
            }
            oss << indentation << "// [END] of include '" << file_include << "' \n";

        } else {

            oss << line << "\n";

        }
    }

    return oss.str();
}

GLuint fb::gl::utils::compile_shader(
    const std::string& shader_source_location,
    GLenum shader_type,
    const std::vector<std::string>& preprocessor_macros) 
{

    GLuint shader_id = glCreateShader(shader_type);

    std::string shader_source = read_glsl_source_file(shader_source_location);

    bf::path file_out_path("fb_" + shader_source_location);
    bf::path out_path = bf::temp_directory_path() / file_out_path;
    bf::ofstream out(out_path, std::ios::out);
    out << shader_source;
    out.close();

    FB_LOG_DEBUG << "Wrote '" << out_path.string() << "'.";

    const GLchar* shader_source_c_str = static_cast<const GLchar*>(shader_source.c_str());
    const GLint shader_source_c_str_length = static_cast<GLint>(shader_source.length());

    std::vector<const GLchar*> sources;
    std::vector<GLint> source_lengths;

    for (const std::string& s : preprocessor_macros) {
        sources.push_back(static_cast<const GLchar*>(s.c_str()));
        source_lengths.push_back(static_cast<GLint>(s.length()));
    }

    static const std::string line_zero = "#line 0 \n";

	if (!sources.empty()) {
		sources.push_back(static_cast<const GLchar*>(line_zero.c_str()));
		source_lengths.push_back(static_cast<GLint>(line_zero.length()));
	}

    sources.push_back(shader_source_c_str);
    source_lengths.push_back(shader_source_c_str_length);

    glShaderSource(
        shader_id, 
        static_cast<GLsizei>(sources.size()), 
        sources.data(), 
        source_lengths.data());

    glCompileShader(shader_id);

    GLint result = GL_FALSE;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);

    if (result != GL_TRUE) {

        std::string compiler_error_string = "<unknown>";
        GLint info_log_length = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
        if(info_log_length > 0)
        {
            std::vector<char> buffer(info_log_length);
            glGetShaderInfoLog(shader_id, info_log_length, NULL, buffer.data());
            compiler_error_string = std::string(buffer.data(), buffer.size());
        }

        FB_LOG_ERROR 
            << "Failed to compile GLSL source '" 
            << shader_source_location 
            << "', reason: '"
            << compiler_error_string
            << "'.";

        throw std::runtime_error("Compiling a shader failed.");

    } else {
        FB_LOG_INFO 
            << "Compiled GLSL source '" 
            << shader_source_location << "' successfully.";
    }

    return shader_id;
}

// TODO-DEF: could rename vertex_shader_source_preprocessor_macros to
// something like "additional" sources.
GLuint fb::gl::utils::create_glsl_program_with_fs_and_vs(
    const std::string& vertex_shader_source_location, 
    const std::string& fragment_shader_source_location, 
    const std::vector<std::string>& vertex_shader_source_preprocessor_macros,
    const std::vector<std::string>& fragment_shader_source_preprocessor_macros)
{

    GLuint vertex_shader_id = compile_shader(
        vertex_shader_source_location, 
        GL_VERTEX_SHADER,
        vertex_shader_source_preprocessor_macros);

    GLuint fragment_shader_id = compile_shader(
        fragment_shader_source_location, 
        GL_FRAGMENT_SHADER,
        fragment_shader_source_preprocessor_macros);

    GLuint program_id = glCreateProgram();
    glProgramParameteri(program_id, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    GLint result = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);

    if (result != GL_TRUE) {

        std::string linker_error_string = "<unknown>";
        GLint info_log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
        if(info_log_length > 0)
        {
            std::vector<char> buffer(info_log_length);
            glGetProgramInfoLog(program_id, info_log_length, NULL, buffer.data());
            linker_error_string = std::string(buffer.data(), buffer.size());
        }

        FB_LOG_ERROR 
            << "Failed to compile GLSL program with vertex shader '" 
            << vertex_shader_source_location
            << "' and fragment shader '" 
            << fragment_shader_source_location
            << "', reason: '"
            << linker_error_string
            << "'.";

        throw std::runtime_error("Linking a program failed.");

    }

    // Shader objects won't actually be deleted until they
    // are detached from all programs.

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;

}

// TODO: actually, this should be unified with above with a more generic
// shader compilation routine.
GLuint fb::gl::utils::create_glsl_program_with_cs(
    const std::string& compute_shader_source_location, 
    const std::vector<std::string>& compute_shader_source_location_preprocessor_macros)
{

    GLuint compute_shader_id = compile_shader(
        compute_shader_source_location, 
        GL_COMPUTE_SHADER,
        compute_shader_source_location_preprocessor_macros);

    GLuint program_id = glCreateProgram();
    glProgramParameteri(program_id, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glAttachShader(program_id, compute_shader_id);
    glLinkProgram(program_id);

    GLint result = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);

    if (result != GL_TRUE) {

        std::string linker_error_string = "<unknown>";
        GLint info_log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
        if(info_log_length > 0)
        {
            std::vector<char> buffer(info_log_length);
            glGetProgramInfoLog(program_id, info_log_length, NULL, buffer.data());
            linker_error_string = std::string(buffer.data(), buffer.size());
        }

        FB_LOG_ERROR 
            << "Failed to compile GLSL program with compute shader '" 
            << compute_shader_source_location
            << "', reason: '"
            << linker_error_string
            << "'.";

        throw std::runtime_error("Linking a program failed.");

    }

    // Shader objects won't actually be deleted until they
    // are detached from all programs.

    glDeleteShader(compute_shader_id);

    return program_id;

}


std::vector<GLuint> fb::gl::utils::create_and_initialize_pbos(
    size_t num,
    size_t byte_size,
    GLenum target,
    GLenum usage)
{

    if (num == 0) 
        throw std::invalid_argument("Can't create zero PBOs.");

    if (byte_size == 0)
        throw std::invalid_argument("Can't create zero-sized PBOs.");

    std::vector<GLuint> pbo_ids = std::vector<GLuint>(num, 0);

    glGenBuffers(
        static_cast<GLsizei>(pbo_ids.size()), 
        pbo_ids.data());


    for (const auto& id : pbo_ids) {

        glBindBuffer(target, id);

        glBufferData(
            target, 
            static_cast<GLsizeiptr>(byte_size),
            nullptr,
            usage
            );

        glBindBuffer(target, 0);

    }

    return pbo_ids;

}

uint8_t* fb::gl::utils::map_pbo(
    GLuint pbo_id, 
    size_t buffer_size, 
    GLenum target, 
    GLbitfield access)
{

    glBindBuffer(target, pbo_id);

    GLvoid* data = glMapBufferRange(
        target, 
        0,
        static_cast<GLsizeiptr>(buffer_size),
        access);
    
    if (data == nullptr) {
        FB_LOG_CRITICAL 
            << "PBO buffer '" 
            << pbo_id << "' mapped to nullptr.";
    }
    
    glBindBuffer(target, 0);

    return static_cast<uint8_t*>(data);

}

bool fb::gl::utils::unmap_pbo(GLuint pbo_id, GLenum target)
{

    glBindBuffer(target, pbo_id);

    GLboolean success = glUnmapBuffer(target);
    if (!success) {
        FB_LOG_CRITICAL << "Unmapping of OpenGL pixel pack buffer failed, this might result in corrupted video frames.";
    }

    glBindBuffer(target, 0);

    return static_cast<bool>(success == GL_TRUE);

}

std::ostream& fb::gl::utils::operator<< (std::ostream& out, const fb::gl::utils::GLInfo& v)
{

    out << "OpenGL Context Ver. " << v.major << "." << v.minor 
        << ", version string = '" << v.version_string << "', Vendor = '" 
        << v.vendor << "', renderer = '" << v.renderer << "'.";

    return out;
}
