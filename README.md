## What is cocos2dx-AdvancedRenderer?

cocos2dx-AdvancedRenderer is an extended version of the default Renderer class used
by cocos2d-x. It adresses some flaws with the original Renderer (listed [here](http://discuss.cocos2d-x.org/t/flaws-with-renderer-glprogram-and-gl-state-management/26239)), and adds the possibility of sending arbitrary vertex data to the Renderer.

Features are:

* draw arbitrary vertex data with all possible primitve types(TRIANGLES, LINES, POINTS, etc.), without even touching OpenGL using the ArbitraryVertexCommand
* enable or disable transformation on cpu for ArbitraryVertexCommand
* ArbitraryVertexCommands are automatically batched
* even ArbitraryVertexCommands with non-cpu-transformation who share the same model view matrix, are batched
* completely compatible with TrianglesCommand and QuadCommand (they are automatically converted to ArbitraryVertexCommands)
* all vertices are gathered before drawing so that the vertex and index vbos are only updated once per Renderer::render() call, which improves performance in some situations
* performance on mobile plattform is significantly better with the cocos2dx-AdvancedRenderer, if you use vertex data bigger than ~100kb

**Note**: Batching only works for TRIANGLES, LINES and POINTS. If you use other formats, enable skipBatching for your Material2D. If you don't do it, results may be not what you're expecting.

#### Works under:
* Android (tested)
* Win32 (tested)
* Mac and iOS (not tested)
* Windows 8.1 Universal (not tested)
* Win10 (not tested)
* Linux (not tested)

## Usage

To use an ArbitraryVertexCommand you first have to create a Material2D:

```
Texture2D* texture = ...;
GLProgramState* state = ....;
VertexStreamAttributes format = ...;
Material2D* material = new Material2D();
material->init(state, &texture, 1, BlendFunc::ADDITIVE, format, MaterialPrimitiveType::TRIANGLE); 
```

VertexAttribInfoFormats are created like this:

```
VertexStreamAttributes format;
format.count = 2;
format.stride = size_of_vertex_in_bytes;
format.infos = new VertexStreamAttribute[format.count];
format.infos[0] = VertexStreamAttribute(offset_of_vertex_attrib_in_vertex_data_in_bytes, attrib_location_in_shader, element_type, element_count, normalized);
...
// Example:  format.infos[0] = VertexStreamAttribute(0, 0, GL_FLOAT, 3, false);
```

Finally, create the ArbitraryVertexCommand and add it to the renderer:

```
ArbitraryVertexCommand* command = new ArbitraryVertexCommand();
command->init(global_z_order, material, draw_data, model_view_matrix, transform_on_cpu);
/*
Example:
ArbitraryVertexCommand::Data data;
// if index_count is 0, non-indexed drawing will be used
data.indexCount = index_count;
data.indexData = index_data;
data.vertexCount = vertex_count;
data.vertexData = vertex_data_as_byte_ptr;
command->init(0, material, data, Mat4::IDENTITY, true);
*/
renderer->addCommand(command);
```
## Installation

Drop all files from the cocos2d/cocos/renderer directory into the your_project/cocos2d/cocos/renderer dir. Overwrite if necessary.

####On Win32 under Visual Studio:
1. Goto the libcocos2dx project
2. Right-Click on renderer filter->Add Existing Files
3. Select all new files that you just dropped into the your_project/cocos2d/cocos/renderer dir
4. Click Add
5. Recompile

####On Android
1. Drop the cocos2d/cocos/Android.mk file into your_project/cocos2d/cocos/ dir
2. Recompile

##Other

For more info, statistics and etc. visit [this](http://discuss.cocos2d-x.org/t/flaws-with-renderer-glprogram-and-gl-state-management/26239) site.