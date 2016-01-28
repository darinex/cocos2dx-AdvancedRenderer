#include "CCArbitraryVertexCommand.h"

NS_CC_BEGIN

ArbitraryVertexCommand::ArbitraryVertexCommand() : _material2d(nullptr)
{
	_type = RenderCommand::Type::ARBITRARY_VERTEX_COMMAND;
}

ArbitraryVertexCommand::~ArbitraryVertexCommand()
{
}

void ArbitraryVertexCommand::init(float globalOrder,
	Material2D * material2d,
	ArbitraryVertexCommand::Data& data,
	const Mat4 & mv,
	bool transformOnCpu,
	uint32_t flags)
{
	CCASSERT(material2d, "Invalid Material2D");
	if (transformOnCpu) {
		if (material2d->getVertexSize() < 12) {
			CCASSERT(false, "To be transformed on the cpu the vertex stride of the given vertex data must be greater than 12");
		}
	}

	if (material2d->getProgramState()->getVertexAttribsFlags() != 0) {
		cocos2d::log("Custom attributes of this material's shader are ignored.\nIf you want to set custom attributes, do that through the VertexAttribFormat parameter in Material2Ds initialization");
	}

	RenderCommand::init(globalOrder, mv, flags);

	CCASSERT(data.vertexCount > 0, "Vertex count and index count must be greater than 0");

	_isIndexed = data.indexCount > 0;
	_mv = mv;

	_data = data;
	_transformOnCpu = transformOnCpu;

	_material2d = material2d;
}

NS_CC_END
