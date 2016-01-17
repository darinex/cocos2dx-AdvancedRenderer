#include "Material2D.h"
#include "renderer\CCTexture2D.h"
#include "renderer\CCGLProgramState.h"
#include "renderer\CCGLProgram.h"
#include "renderer\CCRenderer.h"

#include "base/ccMacros.h"

#include "renderer\ccGLStateCache.h"

#include "xxhash.h"

NS_CC_BEGIN

int generateVertexAttribInfoId(VertexAttribInfo info) {
	int data[6] = { (int)info.index, (int)info.size, (int)info.type, (int)info.normalized, (int)info.stride, (int)info.offset };
	return XXH32(data, sizeof(data), 0);
}

void VertexAttribInfoFormat::generateID() {
	int* data = new int[count];
	int j = 0;
	for (auto i = infos; i < infos + count; i++, j++) {
		data[j] = generateVertexAttribInfoId(*i);
	}
	id = XXH32(data, count * sizeof(int), 0);
	delete[] data;
}

void VertexAttribInfoFormat::apply(void* bufferOffset)
{
	int indices = 0;

	for (auto i = infos; i < infos + count; i++) {
		indices |= (1 << i->index);
	}

	GL::enableVertexAttribs(indices);

	for (auto i = infos; i < infos + count; i++) {
		glVertexAttribPointer(i->index, i->size, i->type, i->normalized, i->stride, (GLvoid*)((GLuint)bufferOffset + i->offset));
	}
}

Material2D::Material2D()
{
}

Material2D::~Material2D()
{
}

void Material2D::init(GLProgramState * program, Texture2D** textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType, GLint vertexStride)
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
	_vertexAttribFormat = format;
	_primitiveType = primitiveType;
	// -1 means auto
	if (vertexStride == -1) {
		_vertexStride = format.infos[0].stride;
	}
	else {
		_vertexStride = vertexStride;
	}

	_skipBatching = false;

	generateMaterialId();
}

void Material2D::init(GLProgramState * program, GLuint * textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType, GLint vertexStride)
{
	CCASSERT(program, "Invalid program");
	CCASSERT(texturesCount <= MAX_TEXTURES_PER_MATERIAL2D, "texturesCount must be lower or equal to MAX_TEXTURES_PER_MATERIAL2D");

	_glProgramState = program;
	memset(&_textureNames[0], 0, sizeof(GLuint) * MAX_TEXTURES_PER_MATERIAL2D);
	memcpy(_textureNames, textures, texturesCount * sizeof(GLuint));
	_textureCount = texturesCount;
	_blendFunc = blendFunc;
	_vertexAttribFormat = format;
	_primitiveType = primitiveType;
	// -1 means auto
	if (vertexStride == -1) {
		_vertexStride = format.infos[0].stride;
	}
	else {
		_vertexStride = vertexStride;
	}

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
		_vertexAttribFormat.generateID();
		int formatId = _vertexAttribFormat.id;
		int glProgram = (int)_glProgramState->getGLProgram()->getProgram();

		static const int size = 7 + MAX_TEXTURES_PER_MATERIAL2D;

		int intArray[size] = { glProgram, (int)_vertexAttribFormat.id, (int)_blendFunc.src ,(int)_blendFunc.dst, (int)_primitiveType ,(int)_vertexStride, (int)_skipBatching };

		int j = 0;
		for (int i = 7; i < size; i++) {
			intArray[i] += _textureNames[i- 7];
		}
		_id = XXH32(intArray, sizeof(intArray), 0);
	}
}

NS_CC_END
