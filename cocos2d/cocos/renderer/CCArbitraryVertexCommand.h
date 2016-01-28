#pragma once

#include "renderer\CCRenderCommand.h"
#include "renderer\Material2D.h"

#include "renderer\CCVertexIndexBuffer.h"

typedef unsigned char byte;

#define DRAW_INFO_AUTO_FILL -1

// this struct should not be used outside of the renderer


NS_CC_BEGIN
class CC_DLL ArbitraryVertexCommand : public RenderCommand {
public:
	struct Data {
		byte* vertexData;
		ssize_t vertexCount;
		unsigned short* indexData;
		ssize_t indexCount;
	};

	ArbitraryVertexCommand();
	/**Destructor.*/
	~ArbitraryVertexCommand();

	/** Initializes the command.
	This function will init the command as a dynamic command
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
		uint32_t flags = 0);

	inline Material2D* getMaterial() const { return _material2d; }
	/**Get the model view matrix.*/
	inline const Mat4& getModelView() const { return _mv; }

	// dynamic buffer command

	// Gets the dynamic command's data.
	inline const Data& getData() const { return _data; }
	// Gets the dynamic command's vertex count.
	inline ssize_t getVertexCount() const { return _data.vertexCount; }
	// Gets the dynamic command's index count.
	inline ssize_t getIndexCount() const { return _data.indexCount; }
	// Gets the dynamic command's vertex size.
	inline ssize_t getVertexDataSize() const { return _data.vertexCount * _material2d->getVertexSize(); }
	/**Get the dynamic command's vertex data pointer.*/
	inline const byte* getVertexData() const { return _data.vertexData; }
	/**Get the dynamic command's index data pointer.*/
	inline const unsigned short* getIndices() const { return _data.indexData; }
	
	/*Get the dynamic command's transform on cpu flag. Do not use this function if the command is a buffer command*/
	inline bool isTransformedOnCpu() const { return _transformOnCpu; }

protected:

	friend Renderer;

	bool _transformOnCpu;
	Data _data;

	Material2D* _material2d;
	bool _isIndexed;
	/**Model view matrix when rendering the triangles.*/
	Mat4 _mv;
};
NS_CC_END