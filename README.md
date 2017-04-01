# vulkan-cpp
Vulkan abstraction library using C++11 for memory, resource management, type and thread safety as well as system independency.
The goal is to be able to quickly write Vulkan code that is type safe, readable and clearly states its purpose, instead of being overwhelmed by pointer arithmetics, memory alignment and the "Vulkan/OpenGL" black screen of death.
### Resource management
Buffer content is typed, referenced and revision counted. Push and specialization constants are typed. The library synchronizes your buffers between client and host lazily,
layouts data according to `std140`, `std430`, `interleaved` or `linear` depending on your need.
```C++
vec3_array vertices;
vec3_array normals;
vec2_array texcoords;

buffer_type vertex_buffer(create<interleaved_std140>(...,
  std::ref(vertices), std::ref(texcoords), std::ref(normals)));
```
The above `vertices`, `normals` and `texcoords` can be read or written to at any time, using the following syntax:
```C++
readable_vec3_array r = read(vertices);
// read r[0]

writable_vec3_array m = write(vertices);
m[0] = 1.f;
```
Submitting a command buffer which depend on the buffer will cause a flush of said buffer.
```C++
queue::submit(queue, {}, command_buffers, {});
```
Locking the array with the `writable_*_array` types will increase its reference count when it goes out of scope. Locking the array with `readable_*_array` makes sure no concurrent read/write occurs.
The `writable_*_array` provides a full `std::vector` like interface where `readable_*_array` provides a read-only `const std::vector` like interface.

Custom types are supported too. The following GLSL definition
```GLSL
struct light_type {
  vec4 position;
  vec3 attenuation;
  vec3 spot_direction;
  float spot_cos_cutoff;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  float spot_exponent;
};

layout(std140, binding = 1) uniform lights {
        light_type lights[max_lights];
} lightsbuf;
```

can be matched with the following host/C++ code:

```C++
struct light_type {
  vec4 position;
  vec3 attenuation;
  vec3 spot_direction;
  float spot_cos_cutoff;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  float spot_exponent;

  VCC_STRUCT_SERIALIZABLE(position, attenuation, spot_direction,
    spot_cos_cutoff, ambient, diffuse, specular, spot_exponent);
};
t_array<light_type> lights(max_lights);

auto light_uniform_buffer(input_buffer::create<linear_std140>(
  std::ref(device), 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  VK_SHARING_MODE_EXCLUSIVE, {}, std::ref(lights)));
```
See ```sample/lighting``` for an example.

*Because the data is strongly typed, there are a lot of opportunities for type checking, especially against SPIR-V. This is in the works!*
The library includes the spirv-reflection project. It parses SPIR-V and extracts uniforms, inputs and outputs. The library will be used to do runtime validation on SPIR-V assembly against the C++ declared arrays. This is currently being worked on.
### Memory management
All Vulkan objects are encapsulated and hidden in C++ classes. These are movable only and destroy the underlying Vulkan objects when they go out of scope.

This makes memory management as easy as is expected with C++, simply move your object to a safe place and use `std::ref` whenever another object
needs to keep a reference, or, move your object into a `std::shared_ptr` for reference counting. All functions that take a `supplier<T>` in this library
will keep a reference to the object, where functions taking a reference will use the argument only for the scope of the function. `supplier<T>` has overloads for rvalue references (takes ownership), `std::shared_ptr` `std::unique_ptr`, `std::reference_wrapper` (`std::ref`) and `function<T&()>`.
### Multithreading
The library is thread-safe as required by the Vulkan specification, `2.5 Threading Behavior`, `Externally Synchronized Parameters`, `Externally Synchronized Parameter Lists`.
Notice that `Implicit Externally Synchronized Parameters` is not included.
### OpenVR
Samples include an OpenVR example. This simple demo renders the models of the connected devices like trackers and controllers. It supports lazy loading and recompiles the command buffers when any new devices are added or removed.
## Install
### Linux/XCB
`cmake .` downloads all the dependencies needed. `cmake --build .` compiles the libraries and samples. 
### Android
Install `Android Studio` and the `NDK`. `SDK 25` is required. Import the root project directory. Initial building and synchronizing will take a very long time, as it will download the dependency projects needed.

**Note:** Textures are copied to the `assets/` and `res/` folders of the respective projects. However, `*.spv` compiled shaders are not generated. These must be copied to the `assets/` folder. 
### Visual Studio 2015
Only 2015 is supported. The C++11 support in previous versions is not sufficient.
`cmake -DVULKAN_SDK_DIR:PATH=<path-to-vulkan-sdk> .` downloads all the dependencies and sets up the projects.
Either use  `cmake --build .` to compile or open the generated  `.sln`.
## Acknowledgements
* This library optionally uses [OpenGL Mathematics, glm.](http://glm.g-truc.net/0.9.7/index.html)
* vcc-image uses [libpng](http://www.libpng.org/) for loading VK_IMAGE_TILING_LINEAR images.
* vcc-image uses [OpenGL Image, gli](http://gli.g-truc.net/) for loading VK_IMAGE_TILING_OPTIMAL images.
* The demos contain image resources by [Emil Persson, aka Humus](http://www.humus.name).
* The demos contain image resources by [Julian Herzog](https://commons.wikimedia.org/wiki/File:Normal_map_example_with_scene_and_result.png).

This is not an official Google product.
This is purely a project made by a Google employee.
