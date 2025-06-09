#version 330 core
out vec4 frag_colour;

in vec3 normval;

uniform vec3 light_dir;
uniform vec3 light_colour;
uniform vec3 object_colour;

void main()
{
    float ambient_str = 0.125;
    vec3 ambience = ambient_str * light_colour;

    vec3 norm = normalize(normval);
    vec3 dir = normalize(-light_dir);

    float diff = max(dot(norm, dir), 0.0);
    vec3 diffuse = diff * light_colour;
    
    vec3 result = (ambience + diffuse) * object_colour;
    frag_colour = vec4(result, 1.0);
}
