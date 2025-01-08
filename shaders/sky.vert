#version 450

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec3 o_position;
layout(set = 1, binding = 0) uniform t_view
{
    mat4 u_view;
};
layout(set = 1, binding = 1) uniform t_proj
{
    mat4 u_proj; 
};

void main()
{
    o_position = i_position;
    mat4 rotation = u_view;
    rotation[3][0] = 0.0;
    rotation[3][1] = 0.0;
    rotation[3][2] = 0.0;
    gl_Position = u_proj * rotation * vec4(i_position, 1.0);
}