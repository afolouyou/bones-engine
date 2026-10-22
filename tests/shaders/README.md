# Renderer contract test shaders

These shaders feed the `Renderer Contract` unit tests. The `.spv` files are
checked in so a normal build and CI do not need `glslangValidator`.

Regenerate them after editing a `.vert` or `.frag` file:

```sh
glslangValidator -V -o tests/shaders/triangle.vert.spv tests/shaders/triangle.vert
glslangValidator -V -o tests/shaders/triangle.frag.spv tests/shaders/triangle.frag
glslangValidator -V -o tests/shaders/quad.vert.spv tests/shaders/quad.vert
glslangValidator -V -o tests/shaders/quad.frag.spv tests/shaders/quad.frag
```

The resource shaders use `#version 450` with explicit `layout(binding = N)`
qualifiers. Those binding numbers must match the `descriptorBindings` in the
test pipeline on both OpenGL and Vulkan.
