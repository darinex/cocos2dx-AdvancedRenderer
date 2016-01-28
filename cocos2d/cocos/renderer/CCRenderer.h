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


#ifndef __CC_RENDERER_H_
#define __CC_RENDERER_H_

#include <vector>
#include <stack>

#include "platform/CCPlatformMacros.h"
#include "renderer/CCRenderCommand.h"
#include "renderer/CCGLProgram.h"
#include "platform/CCGL.h"

#include "FastVector.h"
#include "FastPool.h"
#include "Material2D.h"

 /**
  * @addtogroup renderer
  * @{
  */

NS_CC_BEGIN

class EventListenerCustom;
class QuadCommand;
class TrianglesCommand;
class MeshCommand;
class ArbitraryVertexCommand;
class CustomCommand;

/** Class that knows how to sort `RenderCommand` objects.
 Since the commands that have `z == 0` are "pushed back" in
 the correct order, the only `RenderCommand` objects that need to be sorted,
 are the ones that have `z < 0` and `z > 0`.
*/
class RenderQueue {
public:
	/**
	RenderCommand will be divided into Queue Groups.
	*/
	enum QUEUE_GROUP
	{
		/**Objects with globalZ smaller than 0.*/
		GLOBALZ_NEG = 0,
		/**Opaque 3D objects with 0 globalZ.*/
		OPAQUE_3D = 1,
		/**Transparent 3D objects with 0 globalZ.*/
		TRANSPARENT_3D = 2,
		/**2D objects with 0 globalZ.*/
		GLOBALZ_ZERO = 3,
		/**Objects with globalZ bigger than 0.*/
		GLOBALZ_POS = 4,
		QUEUE_COUNT = 5,
	};

public:
	/**Constructor.*/
	RenderQueue();
	/**Push a renderCommand into current renderqueue.*/
	void push_back(RenderCommand* command);
	/**Return the number of render commands.*/
	ssize_t size() const;
	/**Sort the render commands.*/
	void sort();
	/**Treat sorted commands as an array, access them one by one.*/
	RenderCommand* operator[](ssize_t index) const;
	/**Clear all rendered commands.*/
	void clear();
	/**Realloc command queues and reserve with given size. Note: this clears any existing commands.*/
	void realloc(size_t reserveSize);
	/**Get a sub group of the render queue.*/
	inline std::vector<RenderCommand*>& getSubQueue(QUEUE_GROUP group) { return _commands[group]; }
	/**Get the number of render commands contained in a subqueue.*/
	inline ssize_t getSubQueueSize(QUEUE_GROUP group) const { return _commands[group].size(); }

	/**Save the current DepthState, CullState, DepthWriteState render state.*/
	void saveRenderState();
	/**Restore the saved DepthState, CullState, DepthWriteState render state.*/
	void restoreRenderState();

protected:
	/**The commands in the render queue.*/
	std::vector<RenderCommand*> _commands[QUEUE_COUNT];

	/**Cull state.*/
	bool _isCullEnabled;
	/**Depth test enable state.*/
	bool _isDepthEnabled;
	/**Depth buffer write state.*/
	GLboolean _isDepthWrite;
};

//the struct is not used outside.
struct RenderStackElement
{
	int renderQueueID;
	ssize_t currentIndex;
};

class GroupCommandManager;

#ifndef ARBITARY_VBO_SIZE
#undef ARBITARY_INDEX_VBO_SIZE

#define ARBITRARY_VBO_SIZE 65536 * sizeof(V3F_C4B_T2F)
#define ARBITRARY_INDEX_VBO_SIZE 100000
#endif

typedef unsigned char byte;

struct VertexBatch {
	int startingRCIndex; // index indicating at which position this attribs should be used
	int endRCIndex; // index indicating at which position the next RenderCommandsVertexAttribInfo should be used
	ssize_t vertexBufferOffset; // an offset into the vbo
	ssize_t vertexBufferUsageStart;
	ssize_t vertexBufferUsageEnd;
	GLuint vertexBufferHandle;
	ssize_t indexBufferOffset;
	ssize_t indexBufferUsageStart;
	ssize_t indexBufferUsageEnd;
	GLuint indexBufferHandle;

	bool indexed;

	Material2D* material;
};

struct VertexIndexBO {
	GLuint buffers[2];
};

class QueueCommand;

/* Class responsible for the rendering in.

Whenever possible prefer to use `QuadCommand` objects since the renderer will automatically batch them.
 */
class CC_DLL Renderer
{
public:
	/**The max number of vertices in a vertex buffer object.*/
	static const int VBO_SIZE = 65536;
	/**The max numer of indices in a index buffer.*/
	static const int INDEX_VBO_SIZE = VBO_SIZE * 6 / 4;
	/**The rendercommands which can be batched will be saved into a list, this is the reversed size of this list.*/
	static const int BATCH_QUADCOMMAND_RESEVER_SIZE = 64;
	/**Reserved for material id, which means that the command could not be batched.*/
	static const int MATERIAL_ID_DO_NOT_BATCH = 0;

	/**Constructor.*/
	Renderer();
	/**Destructor.*/
	~Renderer();

	//TODO: manage GLView inside Render itself
	void initGLView();

	/** Adds a `RenderComamnd` into the renderer */
	void addCommand(RenderCommand* command);

	/** Adds a `RenderComamnd` into the renderer specifying a particular render queue ID */
	void addCommand(RenderCommand* command, int renderQueue);

	/** Pushes a group into the render queue */
	void pushGroup(int renderQueueID);

	/** Pops a group from the render queue */
	void popGroup();

	/** Creates a render queue and returns its Id */
	int createRenderQueue();

	/** Renders into the GLView all the queued `RenderCommand` objects */
	void render();

	/** Cleans all `RenderCommand`s in the queue */
	void clean();

	/** Clear GL buffer and screen */
	void clear();

	/** set color for clear screen */
	void setClearColor(const Color4F& clearColor);
	/* returns the number of drawn batches in the last frame */
	ssize_t getDrawnBatches() const { return _drawnBatches; }
	/* RenderCommands (except) QuadCommand should update this value */
	void addDrawnBatches(ssize_t number) { _drawnBatches += number; };
	/* returns the number of drawn triangles in the last frame */
	ssize_t getDrawnVertices() const { return _drawnVertices; }
	/* RenderCommands (except) QuadCommand should update this value */
	void addDrawnVertices(ssize_t number) { _drawnVertices += number; };
	/* clear draw stats */
	void clearDrawStats() { _drawnBatches = _drawnVertices = 0; }

	/**
	 * Enable/Disable depth test
	 * For 3D object depth test is enabled by default and can not be changed
	 * For 2D object depth test is disabled by default
	 */
	void setDepthTest(bool enable);

	//This will not be used outside.
	inline GroupCommandManager* getGroupCommandManager() const { return _groupCommandManager; };

	/** returns whether or not a rectangle is visible or not */
	bool checkVisibility(const Mat4& transform, const Size& size);

protected:

	//Setup VBO or VAO based on OpenGL extensions
	void setupBuffer();
	void setupVBOAndVAO();
	void setupVBO();
	void mapBuffers();
	void drawBatchedArbitaryVertices();

	//Draw the previews queued quads and flush previous context
	void flush();

	void flush2D();

	void flush3D();

	void flushArbitaryVertices();

	void initVertexGathering();

	void processRenderCommand(RenderCommand* command);
	void visitRenderQueue(RenderQueue& queue);

	void fillVerticesAndIndices(const TrianglesCommand* cmd);
	void fillQuads(const QuadCommand* cmd);

	void makeSingleRenderCommandList(RenderQueue& queue);
	void makeSingleRenderCommandList(std::vector<RenderCommand*> commands);

	void mapArbitraryBuffers();

	inline void nextVertexBatch();

	// queue begin functions

	void beginQueueTransparent();
	void beginQueue2d();
	void beginQueueOpaque();

	inline int nextVBO() { return _vboIndex = (_vboIndex + 1) % _vboCount; }

	bool _isBufferSlicing;
	bool _currentVBOIsWritten;

	// pool and vector stuff

	FastPool<ArbitraryVertexCommand*>* _avcPool1;
	FastPool<ArbitraryVertexCommand*>* _avcPool2;

	FastPool<CustomCommand*>* _customCommandPool1;
	FastPool<CustomCommand*>* _customCommandPool2;

	QueueCommand* _beginQueueTransparentCommand;
	QueueCommand* _beginQueueOpaqueCommand;
	QueueCommand* _beginQueue2dCommand;

	FastVector<RenderCommand*>* _renderCommands;

	// arbitraryVertexCommand batching stuff
	ArbitraryVertexCommand* _lastArbitraryCommand;

	VertexBatch* _currentVertexBatch;
	VertexBatch* _previousVertexBatch;

	ssize_t _lastVertexBufferSlicePos;

	int _currentVertexBatchIndex;

	// vbo data
	// this value is used for a loose round-robin approach, may not be less than 1
	float _vboCountMultiplier;
	ssize_t _vboByteSlice;
	unsigned int _vboCount;

	// buffer data info
	byte* _currentVertexBuffer;
	GLushort* _currentIndexBuffer;
	ssize_t _currentVertexBufferOffset;
	ssize_t _currentIndexBufferOffset;

	// batching info
	bool _lastMaterial_skipBatching = false;
	bool _lastCommandWasIndexed;
	bool _lastWasFlushCommand;
	bool _firstAVC = false;
	uint32_t _currentMaterial2dId;

	bool _lastAVC_was_NCT; // short version for : last ArbitaryVertexCommand was Non Cpu Transform
	Mat4 _lastAVC_NCT_Matrix;

	// map buffer
	bool _useMapBuffer;

	/* clear color set outside be used in setGLDefaultValues() */
	Color4F _clearColor;

	std::stack<int> _commandGroupStack;

	std::vector<RenderQueue> _renderGroups;

	MeshCommand*              _lastBatchedMeshCommand;
	std::vector<RenderCommand*> _batchedArbitaryCommands;

	// for arbitaryDrawing
	VertexIndexBO* _aBufferVBOs;
	int _vboIndex;

	byte _arbitraryVertexBuffer[ARBITRARY_VBO_SIZE];
	unsigned short _arbitraryIndexBuffer[ARBITRARY_INDEX_VBO_SIZE];
	FastVector<VertexBatch>* _vertexBatches;

	int _currentAVCommandCount;

	VertexAttribInfoFormat _triangleCommandVAIL;

	int _currentDrawnVertexBatches;
	int _currentDrawnRenderCommands;

	int _startDrawIndex;

	//for TrianglesCommand
	int _filledVertex;
	int _filledIndex;

	//for QuadCommand
	GLushort _quadIndices[INDEX_VBO_SIZE];

	bool _glViewAssigned;

	// stats
	ssize_t _drawnBatches;
	ssize_t _drawnVertices;
	//the flag for checking whether renderer is rendering
	bool _isRendering;

	bool _isDepthTestFor2D;

	GroupCommandManager* _groupCommandManager;

#if CC_ENABLE_CACHE_TEXTURE_DATA
	EventListenerCustom* _cacheTextureListener;
#endif
};

NS_CC_END

/**
 end of support group
 @}
 */
#endif //__CC_RENDERER_H_
