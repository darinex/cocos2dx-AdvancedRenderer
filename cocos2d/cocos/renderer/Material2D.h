#pragma once

#include "CCPlatformMacros.h"
#include "platform\CCGL.h"

#include "renderer\CCGLProgramState.h"

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

struct VertexAttribInfo {
	GLuint index;
	GLuint size;
	GLenum type;
	GLboolean normalized;
	GLuint stride;
	GLuint offset;
};

struct CC_DLL VertexAttribInfoFormat {
	VertexAttribInfo* infos;
	unsigned int count;
	uint32_t id;

	VertexAttribInfoFormat();
	~VertexAttribInfoFormat();

	void generateID();
	void apply(void* bufferOffset);
};

// The name is not really accurate as this material class is not really a material and it can be used for non 2d objects too. I just needed a name :)
class CC_DLL Material2D {
public:
	Material2D();
	~Material2D();

	// @vertexStride - is the size in bytes of one vertex. use -1 as value
	// @textures - an array of Texture2D*
	// @texturesCount - the number of elements in textures
	void init(GLProgramState* program, Texture2D** textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType, GLint vertexStride = -1);

	void init(GLProgramState* program, GLuint* textures, int texturesCount, BlendFunc blendFunc, VertexAttribInfoFormat format, MaterialPrimitiveType primitiveType, GLint vertexStride = -1);

	void apply(const Mat4& modelView);

	inline uint32_t getMaterialId() {
		return _id;
	}

	inline int getVertexAttribInfoFormat() {
		return _vertexAttribFormat.id;
	}

	inline int getVertexStride() const { return _vertexStride; }

	inline GLProgramState* getProgramState() const { return _glProgramState; }

protected:
	friend Renderer;

	void generateMaterialId();

	uint32_t _id;

	GLProgramState* _glProgramState;
	GLuint _textureNames[MAX_TEXTURES_PER_MATERIAL2D];
	BlendFunc _blendFunc;
	VertexAttribInfoFormat _vertexAttribFormat;
	MaterialPrimitiveType _primitiveType;
	GLint _vertexStride;
	bool _skipBatching;
	int _textureCount;
};

NS_CC_END