/****************************************************************************
 Copyright (c) 2013-2014 Chukong Technologies Inc.

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "renderer/CCTrianglesCommand.h"
#include "renderer/ccGLStateCache.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCGLProgramState.h"
#include "xxhash.h"
#include "renderer/CCRenderer.h"

NS_CC_BEGIN

TrianglesCommand::TrianglesCommand()
	:_materialID(0)
	, _textureID(0)
	, _glProgramState(nullptr)
	, _blendType(BlendFunc::DISABLE)
	, _tmpMaterial(nullptr)
{
	_type = RenderCommand::Type::ARBITRARY_VERTEX_COMMAND;
}

static VertexStreamAttributes* s_triangleAttributes = nullptr;

static VertexStreamAttributes* getTriangleAttributes() {
	if (s_triangleAttributes == nullptr) {
		s_triangleAttributes = new VertexStreamAttributes();
		s_triangleAttributes->infos = new VertexStreamAttribute[3];
		s_triangleAttributes->infos[0] = VertexStreamAttribute(0, GLProgram::VERTEX_ATTRIB_POSITION, GL_FLOAT, 3, false);
		s_triangleAttributes->infos[1] = VertexStreamAttribute(12, GLProgram::VERTEX_ATTRIB_COLOR, GL_UNSIGNED_BYTE, 4, true);
		s_triangleAttributes->infos[2] = VertexStreamAttribute(16, GLProgram::VERTEX_ATTRIB_TEX_COORD, GL_FLOAT, 2, false);
		s_triangleAttributes->stride = 24;
		s_triangleAttributes->count = 3;
		s_triangleAttributes->generateID();
	}
	return s_triangleAttributes;
}

void TrianglesCommand::init(float globalOrder, GLuint textureID, GLProgramState* glProgramState, BlendFunc blendType, const Triangles& triangles, const Mat4& mv, uint32_t flags)
{
	CCASSERT(glProgramState, "Invalid GLProgramState");
	CCASSERT(glProgramState->getVertexAttribsFlags() == 0, "No custom attributes are supported in QuadCommand");

	if (_tmpMaterial == nullptr) {
		_tmpMaterial = new Material2D();
	}

	if (_textureID != textureID || _blendType.src != blendType.src || _blendType.dst != blendType.dst || _glProgramState != glProgramState) {

		_textureID = textureID;
		_blendType = blendType;
		_glProgramState = glProgramState;

		_tmpMaterial->init(_glProgramState, &textureID, 1, blendType, *getTriangleAttributes(), MaterialPrimitiveType::TRIANGLE);
	}

	ArbitraryVertexCommand::Data data;
	data.indexCount = triangles.indexCount;
	data.indexData = triangles.indices;
	data.vertexCount = triangles.vertCount;
	data.vertexData = (unsigned char*)triangles.verts;

	ArbitraryVertexCommand::init(globalOrder, _tmpMaterial, data, mv, true, flags);

	_triangles = triangles;
	if (_triangles.indexCount % 3 != 0)
	{
		ssize_t count = _triangles.indexCount;
		_triangles.indexCount = count / 3 * 3;
		CCLOGERROR("Resize indexCount from %zd to %zd, size must be multiple times of 3", count, _triangles.indexCount);
	}
	_mv = mv;
}

void TrianglesCommand::init(float globalOrder, GLuint textureID, GLProgramState* glProgramState, BlendFunc blendType, const Triangles& triangles, const Mat4& mv)
{
	init(globalOrder, textureID, glProgramState, blendType, triangles, mv, 0);
}

TrianglesCommand::~TrianglesCommand()
{
	if (_tmpMaterial != nullptr) {
		delete _tmpMaterial;
	}
}

void TrianglesCommand::generateMaterialID()
{
}

void TrianglesCommand::useMaterial() const
{
	//Set texture
	GL::bindTexture2D(_textureID);

	//set blend mode
	GL::blendFunc(_blendType.src, _blendType.dst);

	_glProgramState->apply(_mv);
}

NS_CC_END
