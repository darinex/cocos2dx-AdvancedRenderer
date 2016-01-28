#pragma once

#include "CCPlatformMacros.h"
#include "platform\CCGL.h"

#include "renderer\CCGLProgramState.h"
#include "renderer\CCVertexIndexData.h"

#define MAX_TEXTURES_PER_MATERIAL2D 4

NS_CC_BEGIN

class Renderer;

enum class MaterialPrimitiveType {
	TRIANGLE = GL_TRIANGLES,
	TRIANGLE_FAN = GL_TRIANGLE_FAN,
	TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
	POINT = GL_POINTS,
	LINE = GL_LINES,
	LINE_LOOP = GL_LINE_LOOP,
	LINE_STRIP = GL_LINE_STRIP,
};

struct CC_DLL VertexStreamAttributes {
	VertexStreamAttribute* infos;
	uint32_t count;
	uint32_t id;
	GLuint stride;

	VertexStreamAttributes();
	~VertexStreamAttributes();

	void generateID();
	void apply(void* bufferOffset);
};

typedef VertexStreamAttributes VertexAttribInfoFormat;

// The name is not really accurate as this material class is not really a material and it can be used for non 2d objects too. I just needed a name :)
class CC_DLL Material2D {
public:
	Material2D();
	~Material2D();

	// @vertexStride - is the size in bytes of one vertex. use -1 as value
	// @textures - an array of Texture2D*
	// @texturesCount - the number of elements in textures
	void init(GLProgramState* program, Texture2D** textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType);

	void init(GLProgramState* program, GLuint* textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType);

	void apply(const Mat4& modelView);

	inline uint32_t getMaterialId() {
		return _id;
	}

	inline int getVertexAttribInfoFormat() {
		return _vertexStreamAttributes.id;
	}

	CC_DEPRECATED() inline int getVertexStride() const { return getVertexSize(); }

	inline int getVertexSize() const { return _vertexStreamAttributes.stride; }

	inline GLProgramState* getProgramState() const { return _glProgramState; }

protected:
	friend Renderer;

	void generateMaterialId();

	uint32_t _id;

	GLProgramState* _glProgramState;
	GLuint _textureNames[MAX_TEXTURES_PER_MATERIAL2D];
	BlendFunc _blendFunc;
	VertexAttribInfoFormat _vertexStreamAttributes;
	MaterialPrimitiveType _primitiveType;
	bool _skipBatching;
	int _textureCount;
};

NS_CC_END