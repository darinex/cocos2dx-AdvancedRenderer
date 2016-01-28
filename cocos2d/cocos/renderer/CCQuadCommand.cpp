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


#include "renderer/CCQuadCommand.h"

#include "renderer/ccGLStateCache.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCMaterial.h"
#include "renderer/CCTechnique.h"
#include "renderer/CCRenderer.h"
#include "renderer/CCPass.h"

#include "xxhash.h"

NS_CC_BEGIN


QuadCommand::QuadCommand()
	:_materialID(0)
	, _textureID(0)
	, _glProgramState(nullptr)
	, _blendType(BlendFunc::DISABLE)
	, _quads(nullptr)
	, _quadsCount(0)
	, _tmpMaterial(nullptr)
{
	_type = RenderCommand::Type::ARBITRARY_VERTEX_COMMAND;
}

static VertexStreamAttributes* s_triangleAttributes = nullptr;

static unsigned short* s_indices = nullptr;

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

void QuadCommand::setStaticIndices(unsigned short* indices) {
	s_indices = indices;
}

void QuadCommand::init(float globalOrder, GLuint textureID, GLProgramState* shader, const BlendFunc& blendType, V3F_C4B_T2F_Quad* quads, ssize_t quadCount,
	const Mat4& mv, uint32_t flags)
{
	CCASSERT(shader, "Invalid GLProgramState");
	CCASSERT(shader->getVertexAttribsFlags() == 0, "No custom attributes are supported in QuadCommand");

	if (_tmpMaterial == nullptr) {
		_tmpMaterial = new Material2D();
	}

	if (_textureID != textureID || _blendType.src != blendType.src || _blendType.dst != blendType.dst || _glProgramState != shader) {

		_textureID = textureID;
		_blendType = blendType;
		_glProgramState = shader;

		_tmpMaterial->init(_glProgramState, &textureID, 1, blendType, *getTriangleAttributes(), MaterialPrimitiveType::TRIANGLE);
	}

	ArbitraryVertexCommand::Data data;
	data.indexCount = quadCount * 6;
	data.indexData = s_indices;
	data.vertexCount = quadCount * 4;
	data.vertexData = (unsigned char*)quads;

	ArbitraryVertexCommand::init(globalOrder, _tmpMaterial, data, mv, true, flags);

	_quadsCount = quadCount;
	_quads = quads;

	_mv = mv;
}

void QuadCommand::init(float globalOrder, GLuint textureID, GLProgramState* shader, const BlendFunc& blendType, V3F_C4B_T2F_Quad* quads, ssize_t quadCount, const Mat4 &mv)
{
	init(globalOrder, textureID, shader, blendType, quads, quadCount, mv, 0);
}

QuadCommand::~QuadCommand()
{
	if (_tmpMaterial != nullptr) {
		delete _tmpMaterial;
	}
}

void QuadCommand::generateMaterialID()
{
}

void QuadCommand::useMaterial() const
{
	//Set texture
	GL::bindTexture2D(_textureID);

	//set blend mode
	GL::blendFunc(_blendType.src, _blendType.dst);

	_glProgramState->applyGLProgram(_mv);
	_glProgramState->applyUniforms();
}

NS_CC_END