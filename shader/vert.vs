#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;

out vec3 frag_pos;
out vec3 normval;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    frag_pos = vec3(model * vec4(pos, 1.0));
    normval = mat3(transpose(inverse(model))) * normal;
    gl_Position = projection * view * model * vec4(pos, 1.0f);
}
