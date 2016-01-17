#pragma once

#include "renderer\CCRenderCommand.h"
#include "renderer\Material2D.h"

typedef unsigned char byte;

#define DRAW_INFO_AUTO_FILL -1

NS_CC_BEGIN
class CC_DLL ArbitraryVertexCommand : public RenderCommand {
public:
	struct Data {
		byte* vertexData;
		ssize_t vertexCount;
		int sizeOfVertex;
		unsigned short* indexData;
		ssize_t indexCount;
	};

	struct DrawInfo{
		int vertexCount;
		int indexCount;

		DrawInfo() {
			this->vertexCount = DRAW_INFO_AUTO_FILL;
			this->indexCount = DRAW_INFO_AUTO_FILL;
		}

		// put DRAW_INFO_AUTO_FILL as a parameter if these values should be auto filled
		DrawInfo(int vertexCount, int indexCount) {
			this->vertexCount = vertexCount;
			this->indexCount = indexCount;
		}
	};

	ArbitraryVertexCommand();
	/**Destructor.*/
	~ArbitraryVertexCommand();

	/** Initializes the command.
	@param globalOrder GlobalZOrder of the command.
	@param material2d The material that should be used for the given vertices.
	@param data The vertices and indices that should be drawn.
	@param mv ModelView matrix for the command.
	@param drawInfo Info used for rendering.
	@param flags to indicate that the command is using 3D rendering or not.
	*/
	void init(float globalOrder,
		Material2D* material2d,
		ArbitraryVertexCommand::Data& data,
		const Mat4& mv,
		bool transformOnCpu = true,
		DrawInfo drawInfo = DrawInfo(),
		uint32_t flags = 0);

	inline Material2D* getMaterial() const { return _material2d; }
	inline const Data& getData() const { return _data; }
	inline ssize_t getVertexCount() const { return _data.vertexCount; }
	inline ssize_t getIndexCount() const { return _data.indexCount; }
	inline ssize_t getVertexDataSize() const { return _data.vertexCount * _data.sizeOfVertex; }
	/**Get the vertex data pointer.*/
	inline const byte* getVertexData() const { return _data.vertexData; }
	/**Get the index data pointer.*/
	inline const unsigned short* getIndices() const { return _data.indexData; }
	/**Get the model view matrix.*/
	inline const Mat4& getModelView() const { return _mv; }

	inline bool isTransformedOnCpu() const { return _transformOnCpu; }

protected:

	friend Renderer;

	bool _transformOnCpu;
	Data _data;
	DrawInfo _info;
	Material2D* _material2d;
	/**Model view matrix when rendering the triangles.*/
	Mat4 _mv;
};
NS_CC_END