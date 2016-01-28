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

#include "renderer/CCRenderer.h"

#include <algorithm>

#include "renderer/CCTrianglesCommand.h"
#include "renderer/CCQuadCommand.h"
#include "renderer/CCArbitraryVertexCommand.h"
#include "renderer/CCBatchCommand.h"
#include "renderer/CCCustomCommand.h"
#include "renderer/CCGroupCommand.h"
#include "renderer/CCPrimitiveCommand.h"
#include "renderer/CCMeshCommand.h"
#include "renderer/CCGLProgramCache.h"
#include "renderer/CCMaterial.h"
#include "renderer/CCTechnique.h"
#include "renderer/CCPass.h"
#include "renderer/CCRenderState.h"
#include "renderer/ccGLStateCache.h"

#include "base/CCConfiguration.h"
#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventListenerCustom.h"
#include "base/CCEventType.h"
#include "2d/CCCamera.h"
#include "2d/CCScene.h"

NS_CC_BEGIN

// helper
static bool compareRenderCommand(RenderCommand* a, RenderCommand* b)
{
	return a->getGlobalOrder() < b->getGlobalOrder();
}

static bool compare3DCommand(RenderCommand* a, RenderCommand* b)
{
	return  a->getDepth() > b->getDepth();
}

// QueueCommand

class QueueCommand : public RenderCommand {
public:
	static const int QUEUE_COMMAND = 0xFF;

	QueueCommand() {
		_type = (RenderCommand::Type)(QUEUE_COMMAND);
	}

	void (Renderer::*func)();
};

// new delegates
static CustomCommand* newCustomCommand() {
	return new CustomCommand();
}
static ArbitraryVertexCommand* newArbitraryVertexCommand() {
	return new ArbitraryVertexCommand();
}

// queue
RenderQueue::RenderQueue()
{

}

void RenderQueue::push_back(RenderCommand* command)
{
	float z = command->getGlobalOrder();
	if (z < 0)
	{
		_commands[QUEUE_GROUP::GLOBALZ_NEG].push_back(command);
	}
	else if (z > 0)
	{
		_commands[QUEUE_GROUP::GLOBALZ_POS].push_back(command);
	}
	else
	{
		if (command->is3D())
		{
			if (command->isTransparent())
			{
				_commands[QUEUE_GROUP::TRANSPARENT_3D].push_back(command);
			}
			else
			{
				_commands[QUEUE_GROUP::OPAQUE_3D].push_back(command);
			}
		}
		else
		{
			_commands[QUEUE_GROUP::GLOBALZ_ZERO].push_back(command);
		}
	}
}

ssize_t RenderQueue::size() const
{
	ssize_t result(0);
	for (int index = 0; index < QUEUE_GROUP::QUEUE_COUNT; ++index)
	{
		result += _commands[index].size();
	}

	return result;
}

void RenderQueue::sort()
{
	// Don't sort _queue0, it already comes sorted
	std::sort(std::begin(_commands[QUEUE_GROUP::TRANSPARENT_3D]), std::end(_commands[QUEUE_GROUP::TRANSPARENT_3D]), compare3DCommand);
	std::sort(std::begin(_commands[QUEUE_GROUP::GLOBALZ_NEG]), std::end(_commands[QUEUE_GROUP::GLOBALZ_NEG]), compareRenderCommand);
	std::sort(std::begin(_commands[QUEUE_GROUP::GLOBALZ_POS]), std::end(_commands[QUEUE_GROUP::GLOBALZ_POS]), compareRenderCommand);
}

RenderCommand* RenderQueue::operator[](ssize_t index) const
{
	for (int queIndex = 0; queIndex < QUEUE_GROUP::QUEUE_COUNT; ++queIndex)
	{
		if (index < static_cast<ssize_t>(_commands[queIndex].size()))
			return _commands[queIndex][index];
		else
		{
			index -= _commands[queIndex].size();
		}
	}

	CCASSERT(false, "invalid index");
	return nullptr;


}

void RenderQueue::clear()
{
	for (int i = 0; i < QUEUE_COUNT; ++i)
	{
		_commands[i].clear();
	}
}

void RenderQueue::realloc(size_t reserveSize)
{
	for (int i = 0; i < QUEUE_COUNT; ++i)
	{
		_commands[i] = std::vector<RenderCommand*>();
		_commands[i].reserve(reserveSize);
	}
}

void RenderQueue::saveRenderState()
{
	_isDepthEnabled = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE;
	_isCullEnabled = glIsEnabled(GL_CULL_FACE) != GL_FALSE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &_isDepthWrite);

	CHECK_GL_ERROR_DEBUG();
}

void RenderQueue::restoreRenderState()
{
	if (_isCullEnabled)
	{
		glEnable(GL_CULL_FACE);
		RenderState::StateBlock::_defaultState->setCullFace(true);
	}
	else
	{
		glDisable(GL_CULL_FACE);
		RenderState::StateBlock::_defaultState->setCullFace(false);
	}


	if (_isDepthEnabled)
	{
		glEnable(GL_DEPTH_TEST);
		RenderState::StateBlock::_defaultState->setDepthTest(true);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
		RenderState::StateBlock::_defaultState->setDepthTest(false);
	}

	glDepthMask(_isDepthWrite);
	RenderState::StateBlock::_defaultState->setDepthWrite(_isDepthEnabled);

	CHECK_GL_ERROR_DEBUG();
}

//
//
//
static const int DEFAULT_RENDER_QUEUE = 0;

//
// constructors, destructors, init
//
Renderer::Renderer()
	: _lastBatchedMeshCommand(nullptr)
	, _filledVertex(0)
	, _filledIndex(0)
	, _glViewAssigned(false)
	, _isRendering(false)
	, _isDepthTestFor2D(false)
#if CC_ENABLE_CACHE_TEXTURE_DATA
	, _cacheTextureListener(nullptr)
#endif
{
	_groupCommandManager = new (std::nothrow) GroupCommandManager();

	_commandGroupStack.push(DEFAULT_RENDER_QUEUE);

	RenderQueue defaultRenderQueue;
	_renderGroups.push_back(defaultRenderQueue);

	// default clear color
	_clearColor = Color4F::BLACK;

#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32 && CC_TARGET_PLATFORM != CC_PLATFORM_WIN32 && CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
	_isBufferSlicing = true;
#else
	_isBufferSlicing = true;
#endif

	_renderCommands = new FastVector<RenderCommand*>();
	_vertexBatches = new FastVector<VertexBatch>();

	// init all pools
	_avcPool1 = new FastPool<ArbitraryVertexCommand*>(&newArbitraryVertexCommand);
	_avcPool2 = new FastPool<ArbitraryVertexCommand*>(&newArbitraryVertexCommand);

	_customCommandPool1 = new FastPool<CustomCommand*>(&newCustomCommand);
	_customCommandPool2 = new FastPool<CustomCommand*>(&newCustomCommand);

	// init queueCommands
	_beginQueue2dCommand = new QueueCommand();
	_beginQueue2dCommand->func = &Renderer::beginQueue2d;
	_beginQueueOpaqueCommand = new QueueCommand();
	_beginQueueOpaqueCommand->func = &Renderer::beginQueueOpaque;
	_beginQueueTransparentCommand = new QueueCommand();
	_beginQueueTransparentCommand->func = &Renderer::beginQueueTransparent;

	// other

	_vboIndex = 0;

	// create vertex layouts

	// init all vbo related stuff
	// TODO read the following values from a file
	_vboByteSlice = 100000;
	_vboCountMultiplier = 1.3f;

	// this data is renderer computed

	if (_isBufferSlicing) {
		_vboCount = (int)ceilf(ARBITRARY_VBO_SIZE / (float)_vboByteSlice);
	}
	else {
		_vboCount = 10;
	}

	_aBufferVBOs = new VertexIndexBO[_vboCount];
}

Renderer::~Renderer()
{
	_renderGroups.clear();
	_groupCommandManager->release();

	delete _renderCommands;
	delete _vertexBatches;

	// delete all pools
	delete _avcPool1;
	delete _avcPool2;

	delete _customCommandPool1;
	delete _customCommandPool2;

	delete[] _triangleCommandVAIL.infos;

	for (unsigned int i = 0; i < _vboCount; i++) {
		glDeleteBuffers(2, &_aBufferVBOs[i].buffers[0]);
	}

	delete[] _aBufferVBOs;

#if CC_ENABLE_CACHE_TEXTURE_DATA
	Director::getInstance()->getEventDispatcher()->removeEventListener(_cacheTextureListener);
#endif
}

void Renderer::initGLView()
{
#if CC_ENABLE_CACHE_TEXTURE_DATA
	_cacheTextureListener = EventListenerCustom::create(EVENT_RENDERER_RECREATED, [this](EventCustom* event) {
		/** listen the event that renderer was recreated on Android/WP8 */
		this->setupBuffer();
	});

	Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_cacheTextureListener, -1);
#endif

	//setup index data for quads

	for (int i = 0; i < VBO_SIZE / 4; i++)
	{
		_quadIndices[i * 6 + 0] = (GLushort)(i * 4 + 0);
		_quadIndices[i * 6 + 1] = (GLushort)(i * 4 + 1);
		_quadIndices[i * 6 + 2] = (GLushort)(i * 4 + 2);
		_quadIndices[i * 6 + 3] = (GLushort)(i * 4 + 3);
		_quadIndices[i * 6 + 4] = (GLushort)(i * 4 + 2);
		_quadIndices[i * 6 + 5] = (GLushort)(i * 4 + 1);
	}

	QuadCommand::setStaticIndices(_quadIndices);

	setupBuffer();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM != CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
	// only use glMapBuffer on desktop platforms as it is used with buffer orphaning, and buffer orphaning could be slow with really big data on mobile devices
	_useMapBuffer = Configuration::getInstance()->checkForGLExtension("map_buffer");
	_useMapBuffer = false;
#endif

	_glViewAssigned = true;
}

void Renderer::setupBuffer()
{
	setupVBO();
}

void Renderer::setupVBOAndVAO()
{
}

void Renderer::setupVBO()
{
	int vboSize = ARBITRARY_VBO_SIZE / _vboCount;
	int iboSize = ARBITRARY_INDEX_VBO_SIZE / _vboCount;
	for (unsigned int i = 0; i < _vboCount; i++) {
		GLuint* buffers = &_aBufferVBOs[i].buffers[0];
		glGenBuffers(2, buffers);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, vboSize, _arbitraryVertexBuffer, GL_STREAM_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, iboSize, _arbitraryIndexBuffer, GL_STREAM_DRAW);
	}

	CHECK_GL_ERROR_DEBUG();
}

void Renderer::mapBuffers()
{
}

void Renderer::addCommand(RenderCommand* command)
{
	int renderQueue = _commandGroupStack.top();
	addCommand(command, renderQueue);
}

void Renderer::addCommand(RenderCommand* command, int renderQueue)
{
	CCASSERT(!_isRendering, "Cannot add command while rendering");
	CCASSERT(renderQueue >= 0, "Invalid render queue");
	CCASSERT(command->getType() != RenderCommand::Type::UNKNOWN_COMMAND, "Invalid Command Type");

	_renderGroups[renderQueue].push_back(command);
}

void Renderer::pushGroup(int renderQueueID)
{
	CCASSERT(!_isRendering, "Cannot change render queue while rendering");
	_commandGroupStack.push(renderQueueID);
}

void Renderer::popGroup()
{
	CCASSERT(!_isRendering, "Cannot change render queue while rendering");
	_commandGroupStack.pop();
}

int Renderer::createRenderQueue()
{
	RenderQueue newRenderQueue;
	_renderGroups.push_back(newRenderQueue);
	return (int)_renderGroups.size() - 1;
}

void Renderer::nextVertexBatch() {
	if (_vertexBatches->at(_currentVertexBatchIndex).endRCIndex == 0) {
		// vertex batch not used yet -> return
		return;
	}
	_vertexBatches->push_back_resize(VertexBatch());
	_previousVertexBatch = _vertexBatches->pointerAt(_currentVertexBatchIndex);
	_currentVertexBatchIndex++;
	_currentVertexBatch = _vertexBatches->pointerAt(_currentVertexBatchIndex);
	_currentVertexBatch->endRCIndex = 0;
}

inline bool matrixEqual(Mat4* mat1, Mat4* mat2) {
	float* m1 = mat1->m;
	float* m2 = mat2->m;
	float* me = m1 + 16;
	do {
		if (*(m1++) != *(m2++)) {
			return false;
		}
	} while (m1 < me);
	return true;
}

void Renderer::makeSingleRenderCommandList(std::vector<RenderCommand*> commands) {
	int j = 0;
	// dont use with push_back_resize: some weird realloc error occurs
	//_renderCommands->reserveElements(commands.size());

	for (auto i = commands.cbegin(); i < commands.cend(); i++, j++) {
		auto type = (*i)->getType();

		if (type == RenderCommand::Type::ARBITRARY_VERTEX_COMMAND) {
			ArbitraryVertexCommand* avc = (ArbitraryVertexCommand*)(*i);

			bool newCommand = _lastWasFlushCommand;

			Material2D* currMaterial = avc->_material2d;
			bool transformOnCpu = avc->_transformOnCpu;
			ArbitraryVertexCommand::Data data = avc->_data;
			Mat4 modelView = avc->_mv;
			ssize_t vertexDataSize = avc->getVertexDataSize();

			_lastWasFlushCommand = false;

			// process batching

			// check if buffer limit is exceeded
			if (_currentVertexBufferOffset + vertexDataSize > ARBITRARY_VBO_SIZE ||
				_currentIndexBufferOffset + data.indexCount > ARBITRARY_INDEX_VBO_SIZE) {
				CCASSERT(false, "Exceeding the index or vertex buffer size");
			}

			if (_firstAVC) {
				_vertexBatches->push_back_resize(VertexBatch());
				_currentVertexBatch->material = currMaterial;
				_currentVertexBatch->indexed = avc->_isIndexed;
				_lastMaterial_skipBatching = currMaterial->_skipBatching && currMaterial->_id == MATERIAL_ID_DO_NOT_BATCH;
				newCommand = true;
				_firstAVC = false;
			}
			else {

				bool needsFilledVertexReset = _filledVertex + data.vertexCount > 0xFFFF; // meaning no index(short) could adress it anymore

				if (_isBufferSlicing) {
					bool vboFull = ((_currentVertexBufferOffset + vertexDataSize) - _lastVertexBufferSlicePos) > _vboByteSlice;
					needsFilledVertexReset |= vboFull;

					if (vboFull) {
						CCASSERT(vertexDataSize < _vboByteSlice, "commands vertex data is too big for slicing");
						_lastVertexBufferSlicePos = _currentVertexBufferOffset;
					}
				}

				bool currMaterial_skipBatching = currMaterial->_skipBatching || currMaterial->_id == MATERIAL_ID_DO_NOT_BATCH;
				bool needFlushDueToDifferentMatrix = false;

				bool indexedStateDiffers = avc->_isIndexed != _lastCommandWasIndexed;

				needsFilledVertexReset |= indexedStateDiffers;

				// check if there need to be new batch due to different transform mode:
				// last command was cpu-transform and new one isnt -> new batch
				// last command was non-cpu-transform and new one is -> new batch
				// last command and new command are cpu-transformed, but dont share the same modelview -> new batch
				if (_lastAVC_was_NCT) {
					do {
						if (transformOnCpu) {
							needFlushDueToDifferentMatrix = true;
							break;
						}
						if (!matrixEqual(&_lastAVC_NCT_Matrix, &modelView)) {
							needFlushDueToDifferentMatrix = true;
							_lastAVC_NCT_Matrix = modelView;
						}
					} while (0);
				}
				else if (!transformOnCpu) {
					needFlushDueToDifferentMatrix = true;
					_lastAVC_NCT_Matrix = modelView;
				}

				// check if:
				// curr material id differs from previous?
				// either curr or prev materials skipped batching?
				// there needs to be a _filledVertex reset
				// the above check returned new batch
				if (currMaterial->_id != _currentMaterial2dId ||
					currMaterial_skipBatching ||
					_lastMaterial_skipBatching ||
					needsFilledVertexReset ||
					needFlushDueToDifferentMatrix)
				{
					// set the previous vertex batch end render command index
					_currentVertexBatch->endRCIndex = _currentAVCommandCount;
					// go to next vertex batch
					nextVertexBatch();
					// set material and starting render command index
					_currentVertexBatch->material = currMaterial;
					_currentVertexBatch->indexed = avc->_isIndexed;
					_currentVertexBatch->indexBufferHandle = 0;
					_currentVertexBatch->vertexBufferHandle = 0;
					_currentVertexBatch->startingRCIndex = _currentAVCommandCount;
					if (needsFilledVertexReset || _lastArbitraryCommand->_material2d->_vertexStreamAttributes.id != currMaterial->_vertexStreamAttributes.id) {
						// if needsFilledVertexReset is set or the vertex attrib format from the previous material is different from the current use new vertex offset
						_filledVertex = 0;
						_currentVertexBatch->indexBufferOffset = _currentIndexBufferOffset;
						_currentVertexBatch->vertexBufferOffset = _currentVertexBufferOffset;
					}
					else {
						// use the offsets from the previous one
						_currentVertexBatch->indexBufferOffset = _previousVertexBatch->indexBufferOffset;
						_currentVertexBatch->vertexBufferOffset = _previousVertexBatch->vertexBufferOffset;
					}
					_previousVertexBatch->indexBufferUsageEnd = _currentVertexBatch->indexBufferUsageStart = _currentIndexBufferOffset;
					_previousVertexBatch->vertexBufferUsageEnd = _currentVertexBatch->vertexBufferUsageStart = _currentVertexBufferOffset;
					newCommand = true;
				}
			}
			_lastAVC_was_NCT = !transformOnCpu;
			_lastCommandWasIndexed = avc->_isIndexed;
			_currentMaterial2dId = currMaterial->_id;

			// data copying logic
			memcpy(_currentVertexBuffer, data.vertexData, vertexDataSize);
			if (transformOnCpu) {
				// treat the first 12 byte (3 floats) as a Vec3 and transform it using the modelView
				byte* ptr = _currentVertexBuffer;
				byte* endPtr = ptr + vertexDataSize;
				int stride = currMaterial->_vertexStreamAttributes.stride;
				while (ptr < endPtr) {
					Vec3* vec = reinterpret_cast<Vec3*>(ptr);
					modelView.transformPoint(vec);
					ptr += stride;
				}
			}
			if (data.indexCount != 0) {
				// copy index data
				if (_filledVertex == 0) {
					// special case when the vertex buffer offset is 0
					memcpy(_currentIndexBuffer, data.indexData, sizeof(short) * data.indexCount);
				}
				else {
					GLushort* ptr = _currentIndexBuffer;
					GLushort* endPtr = ptr + data.indexCount;

					GLushort* srcPtr = (GLushort*)data.indexData;

					while (ptr < endPtr) {
						*(ptr++) = *(srcPtr++) + _filledVertex;
					}
				}
			}

			// adjust buffers and offset
			_currentIndexBuffer += data.indexCount;
			_currentVertexBuffer += vertexDataSize;

			_currentVertexBufferOffset += vertexDataSize;
			_currentIndexBufferOffset += data.indexCount;

			_filledVertex += data.vertexCount;

			// if newCommand is set create a new avc and init it
			if (newCommand) {
				ArbitraryVertexCommand* avc = _avcPool1->pop();

				// the data value doesnt really matters here
				avc->init(0, currMaterial, data, modelView, transformOnCpu, 0);

				_currentAVCommandCount++;
				_lastArbitraryCommand = avc;
				_renderCommands->push_back_resize(avc);

				_avcPool2->push(avc);
			}
			else {
				// do nothing
			}
			_lastArbitraryCommand = avc;
		}
		else {
			_lastWasFlushCommand = true;
			if (type == RenderCommand::Type::GROUP_COMMAND) {
				makeSingleRenderCommandList(_renderGroups[reinterpret_cast<GroupCommand*>(*i)->getRenderQueueID()]);
				//_renderCommands->reserveElements(commands.size() - j);
				continue;
			}
			_renderCommands->push_back_resize(*i);
			continue;
		}
	}
}

void Renderer::makeSingleRenderCommandList(RenderQueue& queue) {
	//_renderCommands->reserveElements(7);

	CustomCommand* begin = _customCommandPool1->pop();
	CustomCommand* end = _customCommandPool1->pop();

	begin->func = CC_CALLBACK_0(RenderQueue::saveRenderState, &queue);
	_renderCommands->push_back_resize(begin);

	std::vector<RenderCommand*> queueEntrys = queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_NEG);
	if (queueEntrys.size() > 0) {
		_renderCommands->push_back_resize(_beginQueue2dCommand);
		_lastWasFlushCommand = true;
		makeSingleRenderCommandList(queueEntrys);
	}
	queueEntrys = queue.getSubQueue(RenderQueue::QUEUE_GROUP::OPAQUE_3D);
	if (queueEntrys.size() > 0) {
		_renderCommands->push_back_resize(_beginQueueOpaqueCommand);
		_lastWasFlushCommand = true;
		makeSingleRenderCommandList(queueEntrys);
	}
	queueEntrys = queue.getSubQueue(RenderQueue::QUEUE_GROUP::TRANSPARENT_3D);
	if (queueEntrys.size() > 0) {
		_renderCommands->push_back_resize(_beginQueueTransparentCommand);
		_lastWasFlushCommand = true;
		makeSingleRenderCommandList(queueEntrys);
	}
	queueEntrys = queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_ZERO);
	if (queueEntrys.size() > 0) {
		_renderCommands->push_back_resize(_beginQueue2dCommand);
		_lastWasFlushCommand = true;
		makeSingleRenderCommandList(queueEntrys);
	}
	queueEntrys = queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_POS);
	if (queueEntrys.size() > 0) {
		_renderCommands->push_back_resize(_beginQueue2dCommand);
		_lastWasFlushCommand = true;
		makeSingleRenderCommandList(queueEntrys);
	}

	end->func = CC_CALLBACK_0(RenderQueue::restoreRenderState, &queue);
	_renderCommands->push_back_resize(end);

	_customCommandPool2->push(begin);
	_customCommandPool2->push(end);
}

#define SWAP(a, b, t, n) t n = a; \
					a = b; \
					b = n

void Renderer::initVertexGathering() {
	_currentVertexBatchIndex = 0;

	_currentMaterial2dId = 0;
	_lastMaterial_skipBatching = false;
	_firstAVC = true;
	_lastWasFlushCommand = false;
	_lastCommandWasIndexed = false;

	_filledVertex = 0;
	_filledIndex = 0;

	memset(_vertexBatches->pointerAt(0), 0, sizeof(VertexBatch));
	_previousVertexBatch = _vertexBatches->pointerAt(0);
	_currentVertexBatch = _previousVertexBatch;

	_currentIndexBuffer = _arbitraryIndexBuffer;
	_currentVertexBuffer = _arbitraryVertexBuffer;
	_currentIndexBufferOffset = 0;
	_currentVertexBufferOffset = 0;

	_lastVertexBufferSlicePos = 0;

	_currentAVCommandCount = 0;

	_lastArbitraryCommand = nullptr;

	_currentVBOIsWritten = false;

	_lastAVC_was_NCT = false;

	// swap the pools
	if (_customCommandPool1->getElementCount() < _customCommandPool2->getElementCount()) {
		SWAP(_customCommandPool1, _customCommandPool2, FastPool<CustomCommand*>*, temp1);
	}
	if (_avcPool1->getElementCount() < _avcPool2->getElementCount()) {
		SWAP(_avcPool1, _avcPool2, FastPool<ArbitraryVertexCommand*>*, temp3);
	}

	// init draw stuff

	_startDrawIndex = 0;
	_currentDrawnRenderCommands = 0;
	_currentDrawnVertexBatches = 0;
}

// queue command functions

void Renderer::beginQueueTransparent() {
	glEnable(GL_DEPTH_TEST);
	glDepthMask(false);
	glEnable(GL_BLEND);

	RenderState::StateBlock::_defaultState->setDepthTest(true);
	RenderState::StateBlock::_defaultState->setDepthWrite(false);
	RenderState::StateBlock::_defaultState->setBlend(true);
}
void Renderer::beginQueue2d() {
	if (_isDepthTestFor2D)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(true);
		glEnable(GL_BLEND);
		RenderState::StateBlock::_defaultState->setDepthTest(true);
		RenderState::StateBlock::_defaultState->setDepthWrite(true);
		RenderState::StateBlock::_defaultState->setBlend(true);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(false);
		glEnable(GL_BLEND);
		RenderState::StateBlock::_defaultState->setDepthTest(false);
		RenderState::StateBlock::_defaultState->setDepthWrite(false);
		RenderState::StateBlock::_defaultState->setBlend(true);
	}
}
void Renderer::beginQueueOpaque() {
	//Clear depth to achieve layered rendering
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
	glDisable(GL_BLEND);
	RenderState::StateBlock::_defaultState->setDepthTest(true);
	RenderState::StateBlock::_defaultState->setDepthWrite(true);
	RenderState::StateBlock::_defaultState->setBlend(false);
}

// processRenderCommand

void Renderer::processRenderCommand(RenderCommand* command)
{
	auto commandType = command->getType();
	if (RenderCommand::Type::ARBITRARY_VERTEX_COMMAND == commandType) {
		// TODO use FastVector
		_batchedArbitaryCommands.push_back(command);
	}
	else if (RenderCommand::Type::MESH_COMMAND == commandType)
	{
		flush2D();
		auto cmd = static_cast<MeshCommand*>(command);

		if (cmd->isSkipBatching() || _lastBatchedMeshCommand == nullptr || _lastBatchedMeshCommand->getMaterialID() != cmd->getMaterialID())
		{
			flush3D();

			if (cmd->isSkipBatching())
			{
				// XXX: execute() will call bind() and unbind()
				// but unbind() shouldn't be call if the next command is a MESH_COMMAND with Material.
				// Once most of cocos2d-x moves to Pass/StateBlock, only bind() should be used.
				cmd->execute();
			}
			else
			{
				cmd->preBatchDraw();
				cmd->batchDraw();
				_lastBatchedMeshCommand = cmd;
			}
		}
		else
		{
			cmd->batchDraw();
		}
	}
	else if (RenderCommand::Type::CUSTOM_COMMAND == commandType)
	{
		flush();
		auto cmd = static_cast<CustomCommand*>(command);
		cmd->execute();
	}
	else if (RenderCommand::Type::BATCH_COMMAND == commandType)
	{
		flush();
		auto cmd = static_cast<BatchCommand*>(command);
		cmd->execute();
	}
	else if (RenderCommand::Type::PRIMITIVE_COMMAND == commandType)
	{
		flush();
		auto cmd = static_cast<PrimitiveCommand*>(command);
		cmd->execute();
	}
	else if ((RenderCommand::Type)QueueCommand::QUEUE_COMMAND == commandType)
	{
		flush();
		auto cmd = static_cast<QueueCommand*>(command);
		(this->*cmd->func)();
	}
	else
	{
		CCLOGERROR("Unknown commands in renderQueue");
	}
}

void Renderer::visitRenderQueue(RenderQueue& queue)
{
	// unused delete
}

void Renderer::render()
{
	//Uncomment this once everything is rendered by new renderer
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//TODO: setup camera or MVP
	_isRendering = true;

	if (_glViewAssigned)
	{
		//Process render commands
		//1. Sort render commands based on ID
		for (auto &renderqueue : _renderGroups)
		{
			renderqueue.sort();
		}
		//2. 
		//	1. convert all render queues into one giant list of render command
		//	2. convert all TrianglesCommands and QuadCommands to ArbitraryVertexCommand
		//	3. create batching data
		initVertexGathering();
		makeSingleRenderCommandList(_renderGroups[0]);
		_vertexBatches->pointerAt(_currentVertexBatchIndex)->endRCIndex = _currentAVCommandCount;
		_vertexBatches->pointerAt(_currentVertexBatchIndex)->indexBufferUsageEnd = _currentIndexBufferOffset;
		_vertexBatches->pointerAt(_currentVertexBatchIndex)->vertexBufferUsageEnd = _currentVertexBufferOffset;
		//3. map buffers
		mapArbitraryBuffers();
		//4. process render commands
		RenderCommand** commandPtr = const_cast<RenderCommand**>(_renderCommands->cbegin());
		RenderCommand** endPtr = const_cast<RenderCommand**>(_renderCommands->cend());

		while (commandPtr < endPtr) {
			processRenderCommand(*(commandPtr++)); // cast away the const
		}
	}
	clean();
	_isRendering = false;
}

void Renderer::clean()
{
	// Clear render group
	for (size_t j = 0; j < _renderGroups.size(); j++)
	{
		//commands are owned by nodes
		// for (const auto &cmd : _renderGroups[j])
		// {
		//     cmd->releaseToCommandPool();
		// }
		_renderGroups[j].clear();
	}

	_vertexBatches->clear();
	_renderCommands->clear();

	// Clear batch commands
	_batchedArbitaryCommands.clear();
	_filledVertex = 0;
	_filledIndex = 0;
	_lastBatchedMeshCommand = nullptr;
}

void Renderer::clear()
{
	//Enable Depth mask to make sure glClear clear the depth buffer correctly
	glDepthMask(true);
	glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask(false);

	RenderState::StateBlock::_defaultState->setDepthWrite(false);
}

void Renderer::setDepthTest(bool enable)
{
	if (enable)
	{
		glClearDepth(1.0f);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		RenderState::StateBlock::_defaultState->setDepthTest(true);
		RenderState::StateBlock::_defaultState->setDepthFunction(RenderState::DEPTH_LEQUAL);

		//        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);

		RenderState::StateBlock::_defaultState->setDepthTest(false);
	}

	_isDepthTestFor2D = enable;
	CHECK_GL_ERROR_DEBUG();
}

void Renderer::fillVerticesAndIndices(const TrianglesCommand* cmd)
{
}

void Renderer::fillQuads(const QuadCommand *cmd)
{
}

void Renderer::mapArbitraryBuffers() {
	if (!_isBufferSlicing) {
		if (_currentVertexBufferOffset > 0) {
			glBindBuffer(GL_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[0]);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[1]);

			if (_useMapBuffer) {
				glBufferData(GL_ARRAY_BUFFER, _currentVertexBufferOffset, nullptr, GL_STREAM_DRAW);
				void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
				memcpy(ptr, _arbitraryVertexBuffer, _currentVertexBufferOffset);
				glUnmapBuffer(GL_ARRAY_BUFFER);

				glBufferData(GL_ELEMENT_ARRAY_BUFFER, _currentIndexBufferOffset * sizeof(short), nullptr, GL_STREAM_DRAW);
				ptr = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
				memcpy(ptr, _arbitraryIndexBuffer, _currentIndexBufferOffset * sizeof(short));
				glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
			}
			else {
				glBufferData(GL_ARRAY_BUFFER, _currentVertexBufferOffset, _arbitraryVertexBuffer, GL_STREAM_DRAW);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, _currentIndexBufferOffset * sizeof(short), _arbitraryIndexBuffer, GL_STREAM_DRAW);
			}

			for (auto i = _vertexBatches->cbegin(); i < _vertexBatches->cend(); i++) {
				VertexBatch* batch = const_cast<VertexBatch*>(i);
				batch->vertexBufferHandle = _aBufferVBOs[_vboIndex].buffers[0];
				batch->indexBufferHandle = _aBufferVBOs[_vboIndex].buffers[1];
			}
			nextVBO();
		}
	}
	else {
		if (_currentVertexBufferOffset > 0) {
			// The following logic is used for buffer splitting. its pretty messy but gets the job done ;)

			// Note that all offset with vertex in it are an offset in bytes
			// Note that all offsets with index in it are an offset in shorts
			// TODO maybe change the offset handling so indexOffset are in bytes

			// TODO this part could need some name refactoring as the names doesnt really tell much about the usage of the var

			ssize_t currentVertexBufferOffset = 0;
			ssize_t currentIndexBufferOffset = 0;
			ssize_t nextBufferOffset = _vboByteSlice;

			ssize_t vertexSize = 0;
			ssize_t indexSize = 0;

			VertexBatch* batch = nullptr;

			if (_currentVertexBufferOffset > _vboByteSlice) {
				vertexSize = 0;
			}

			for (auto i = _vertexBatches->cbegin(); i < _vertexBatches->cend(); i++) {
				batch = const_cast<VertexBatch*>(i);
				ssize_t endOffset = batch->vertexBufferUsageEnd;

				if (endOffset - currentVertexBufferOffset > _vboByteSlice) {
					glBindBuffer(GL_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[0]);
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[1]);
					glBufferData(GL_ARRAY_BUFFER, vertexSize, _arbitraryVertexBuffer + currentVertexBufferOffset, GL_STREAM_DRAW);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize * sizeof(short), (_arbitraryIndexBuffer + currentIndexBufferOffset), GL_STREAM_DRAW);

					currentVertexBufferOffset = batch->vertexBufferOffset;
					currentIndexBufferOffset = batch->indexBufferOffset;

					vertexSize = 0;
					indexSize = 0;

					nextVBO();
				}
				batch->indexBufferHandle = _aBufferVBOs[_vboIndex].buffers[1];
				batch->vertexBufferHandle = _aBufferVBOs[_vboIndex].buffers[0];

				vertexSize += batch->vertexBufferUsageEnd - batch->vertexBufferUsageStart;
				indexSize += batch->indexBufferUsageEnd - batch->indexBufferUsageStart;

				batch->vertexBufferOffset -= currentVertexBufferOffset;
				batch->indexBufferOffset -= currentIndexBufferOffset;
				batch->vertexBufferUsageStart -= currentVertexBufferOffset;
				batch->indexBufferUsageStart -= currentIndexBufferOffset;
				batch->vertexBufferUsageEnd -= currentVertexBufferOffset;
				batch->indexBufferUsageEnd -= currentIndexBufferOffset;
			}

			// submit remaining data
			glBindBuffer(GL_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[0]);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _aBufferVBOs[_vboIndex].buffers[1]);
			glBufferData(GL_ARRAY_BUFFER, vertexSize, _arbitraryVertexBuffer + currentVertexBufferOffset, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize * sizeof(short), (_arbitraryIndexBuffer + currentIndexBufferOffset), GL_STREAM_DRAW);
			nextVBO();
		}
	}
}

void Renderer::drawBatchedArbitaryVertices() {
	int endDrawnRenderCommands = _currentDrawnRenderCommands + _batchedArbitaryCommands.size();

	int indexToDraw = 0;
	int vertexCount = 0;

	VertexBatch* batch = _vertexBatches->pointerAt(_currentDrawnVertexBatches);

	bool bindMaterial = true;
	bool applyVertexAttribFormat = true;
	bool bindBuffer = true;

	auto avcPtr = _batchedArbitaryCommands.begin();

	while (_currentDrawnRenderCommands < endDrawnRenderCommands) {
		CCASSERT(_currentDrawnRenderCommands < _currentAVCommandCount, "Something went really wrong");
		ArbitraryVertexCommand* avc = reinterpret_cast<ArbitraryVertexCommand*>(*avcPtr);
		if (applyVertexAttribFormat) {
			if (bindBuffer) {
				glBindBuffer(GL_ARRAY_BUFFER, batch->vertexBufferHandle);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indexBufferHandle);
			}
			batch->material->_vertexStreamAttributes.apply((GLvoid*)batch->vertexBufferOffset);
		}
		if (bindMaterial) {
			batch->material->apply(avc->_mv);
		}

		bindMaterial = applyVertexAttribFormat = bindBuffer = false;
		_currentDrawnRenderCommands++;
		avcPtr++;
		if (_currentDrawnRenderCommands >= batch->endRCIndex) {
			if (batch->indexed) {
				indexToDraw = batch->indexBufferUsageEnd - batch->indexBufferUsageStart;
				glDrawElements(
					(GLenum)batch->material->_primitiveType,
					(GLsizei)(indexToDraw),
					GL_UNSIGNED_SHORT,
					(GLvoid*)(batch->indexBufferUsageStart*sizeof(_arbitraryIndexBuffer[0])));
				_drawnBatches++;
				_drawnVertices += indexToDraw;
			}
			else {
				indexToDraw = (batch->vertexBufferUsageEnd - batch->vertexBufferUsageStart) / batch->material->_vertexStreamAttributes.stride;
				glDrawArrays((GLenum)batch->material->_primitiveType, 0, indexToDraw);
				_drawnBatches++;
				_drawnVertices += indexToDraw;
			}
			_currentDrawnVertexBatches++;

			if (_currentDrawnRenderCommands >= _currentAVCommandCount) { // _currentAVCommandCount is equal to the max avc command count at this point
				break;
			}

			VertexBatch* newBatch = _vertexBatches->pointerAt(_currentDrawnVertexBatches);

			if (newBatch->vertexBufferOffset != batch->vertexBufferOffset) {
				// this means that the vertex attrib format must be changed
				applyVertexAttribFormat = true;
			}
			if (newBatch->vertexBufferHandle != batch->vertexBufferHandle) {
				// vbo changed means that the vertex attrib must be rebind
				bindBuffer = applyVertexAttribFormat = true;
				_startDrawIndex = 0;
			}
			batch = newBatch;
			bindMaterial = true;
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Renderer::flush()
{
	flush2D();
	flush3D();
}

void Renderer::flush2D()
{
	flushArbitaryVertices();
}

void Renderer::flush3D()
{
	if (_lastBatchedMeshCommand)
	{
		_lastBatchedMeshCommand->postBatchDraw();
		_lastBatchedMeshCommand = nullptr;
	}
}

void Renderer::flushArbitaryVertices() {
	if (_batchedArbitaryCommands.size() > 0)
	{
		drawBatchedArbitaryVertices();
		_batchedArbitaryCommands.clear();
	}
}

// helpers
bool Renderer::checkVisibility(const Mat4 &transform, const Size &size)
{
	auto scene = Director::getInstance()->getRunningScene();

	//If draw to Rendertexture, return true directly.
	// only cull the default camera. The culling algorithm is valid for default camera.
	if (!scene || (scene && scene->_defaultCamera != Camera::getVisitingCamera()))
		return true;

	auto director = Director::getInstance();
	Rect visiableRect(director->getVisibleOrigin(), director->getVisibleSize());

	// transform center point to screen space
	float hSizeX = size.width / 2;
	float hSizeY = size.height / 2;
	Vec3 v3p(hSizeX, hSizeY, 0);
	transform.transformPoint(&v3p);
	Vec2 v2p = Camera::getVisitingCamera()->projectGL(v3p);

	// convert content size to world coordinates
	float wshw = std::max(fabsf(hSizeX * transform.m[0] + hSizeY * transform.m[4]), fabsf(hSizeX * transform.m[0] - hSizeY * transform.m[4]));
	float wshh = std::max(fabsf(hSizeX * transform.m[1] + hSizeY * transform.m[5]), fabsf(hSizeX * transform.m[1] - hSizeY * transform.m[5]));

	// enlarge visable rect half size in screen coord
	visiableRect.origin.x -= wshw;
	visiableRect.origin.y -= wshh;
	visiableRect.size.width += wshw * 2;
	visiableRect.size.height += wshh * 2;
	bool ret = visiableRect.containsPoint(v2p);
	return ret;
}


void Renderer::setClearColor(const Color4F &clearColor)
{
	_clearColor = clearColor;
}

NS_CC_END


