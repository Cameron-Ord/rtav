#version 330 core
out vec4 frag_colour;

in vec3 frag_pos;
in vec3 normval;

uniform vec3 view_pos;
uniform vec3 light_pos;
uniform vec3 light_colour;
uniform vec3 object_colour;

void main()
{
  //ambient
    float ambient_str = 0.5;
    vec3 ambience = ambient_str * light_colour;

  //diffuse
    vec3 norm = normalize(normval);
    vec3 dir = normalize(light_pos - frag_pos);
    float diff = max(dot(norm, dir), 0.0);
    vec3 diffuse = diff * light_colour;

  //specular
    float spec_str = 0.5;
    vec3 view_dir = normalize(view_pos - frag_pos);
    vec3 reflect_dir = reflect(-dir, normval);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    vec3 specular = spec_str * spec * light_colour;
    
    vec3 result = (ambience + diffuse + specular) * object_colour;
    frag_colour = vec4(result, 1.0);
}
