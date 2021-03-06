#include "Material2D.h"
#include "renderer\CCTexture2D.h"
#include "renderer\CCGLProgramState.h"
#include "renderer\CCGLProgram.h"
#include "renderer\CCRenderer.h"

#include "base/ccMacros.h"

#include "renderer\ccGLStateCache.h"

#include "xxhash.h"

NS_CC_BEGIN

int generateVertexAttribInfoId(VertexStreamAttribute info) {
	int data[5] = { (int)info._semantic, (int)info._size, (int)info._type, (int)info._normalize, (int)info._offset };
	return XXH32(data, sizeof(data), 0);
}

void VertexStreamAttributes::generateID() {
	int* data = new int[count + 1];
	int j = 0;
	for (auto i = infos; i < infos + count; i++, j++) {
		data[j] = generateVertexAttribInfoId(*i);
	}
	data[count] = stride;
	id = XXH32(data, (count + 1) * sizeof(int), 0);
	delete[] data;
}

void VertexStreamAttributes::apply(void* bufferOffset)
{
	int indices = 0;

	for (auto i = infos; i < infos + count; i++) {
		indices |= (1 << i->_semantic);
	}

	GL::enableVertexAttribs(indices);

	for (auto i = infos; i < infos + count; i++) {
		glVertexAttribPointer(i->_semantic, i->_size, i->_type, i->_normalize, stride, (GLvoid*)((GLuint)bufferOffset + i->_offset));
	}
}

VertexStreamAttributes::VertexStreamAttributes() {
	id = 0;
	count = 0;
	infos = nullptr;
}
VertexStreamAttributes::~VertexStreamAttributes() {
}

Material2D::Material2D()
{
}

Material2D::~Material2D()
{
}

void Material2D::init(GLProgramState * program, Texture2D** textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType)
{
	CCASSERT(program, "Invalid program");
	CCASSERT(texturesCount <= MAX_TEXTURES_PER_MATERIAL2D, "texturesCount must be lower or equal to MAX_TEXTURES_PER_MATERIAL2D");

	_glProgramState = program;
	memset(&_textureNames[0], 0, sizeof(GLuint) * MAX_TEXTURES_PER_MATERIAL2D);
	for (int i = 0; i < texturesCount; i++) {
		_textureNames[i] = textures[i]->getName();
	}
	_textureCount = texturesCount;
	_blendFunc = blendFunc;
	_vertexStreamAttributes = format;
	_primitiveType = primitiveType;

	_skipBatching = false;

	generateMaterialId();
}

void Material2D::init(GLProgramState * program, GLuint * textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType)
{
	CCASSERT(program, "Invalid program");
	CCASSERT(texturesCount <= MAX_TEXTURES_PER_MATERIAL2D, "texturesCount must be lower or equal to MAX_TEXTURES_PER_MATERIAL2D");

	_glProgramState = program;
	memset(&_textureNames[0], 0, sizeof(GLuint) * MAX_TEXTURES_PER_MATERIAL2D);
	memcpy(_textureNames, textures, texturesCount * sizeof(GLuint));
	_textureCount = texturesCount;
	_blendFunc = blendFunc;
	_vertexStreamAttributes = format;
	_primitiveType = primitiveType;

	_skipBatching = false;

	generateMaterialId();
}

void Material2D::apply(const Mat4& modelView)
{
	int j = 0;
	for (auto i = _textureNames; i < _textureNames + _textureCount; i++, j++) {
		GL::bindTexture2DN(j, *i);
	}

	GL::blendFunc(_blendFunc.src, _blendFunc.dst);

	_glProgramState->applyGLProgram(modelView);
	_glProgramState->applyUniforms();
}

void Material2D::generateMaterialId()
{
	if (_glProgramState->getUniformCount() > 0) {
		_id = Renderer::MATERIAL_ID_DO_NOT_BATCH;
	}
	else {
		if (_vertexStreamAttributes.id == 0) { // 0 could be a valid id too but its used as a non-initialize indicator
			_vertexStreamAttributes.generateID();
		}
		int formatId = _vertexStreamAttributes.id;
		int glProgram = (int)_glProgramState->getGLProgram()->getProgram();

		static const int size = 7 + MAX_TEXTURES_PER_MATERIAL2D;

		int intArray[size] = { glProgram, formatId, (int)_blendFunc.src ,(int)_blendFunc.dst, (int)_primitiveType, (int)_skipBatching };

		int j = 0;
		for (int i = 7; i < size; i++) {
			intArray[i] += _textureNames[i - 7];
		}
		_id = XXH32(intArray, sizeof(intArray), 0);
	}
}

NS_CC_END
