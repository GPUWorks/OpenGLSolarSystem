#include "index.h"

void set_shaders(Program *program) {
    const GLchar *vertex_shader = R"(
		#version 150 core

        in vec3 position;
        in vec2 texcoord;
        in vec3 normal;

        uniform mat4 distort;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 proj;
        uniform mat4 lightMVP;

        out vec2 Texcoord;
        out vec3 fragPosition;
        out vec3 fragNormal;
        out vec4 fragPosLightSpace;
        out vec3 cloudPosition;

		void main() {
			gl_Position = distort * proj * view * model * vec4(position, 1.0);
            fragPosition = vec3(model * vec4(position, 1.0));
            fragNormal = transpose(inverse(mat3(model))) * normal;
            Texcoord = texcoord;
            fragPosLightSpace = lightMVP * vec4(position, 1.0);
            cloudPosition = position;
		}
	)";

    const GLchar *fragment_shader = R"(
		#version 150 core

        in vec2 Texcoord;
        in vec3 fragPosition;
        in vec3 fragNormal;
        in vec4 fragPosLightSpace;
        in vec3 cloudPosition;

        uniform mat4 model;
        uniform mat4 view;

        uniform sampler2D tex;
        uniform sampler2D shadowMap;

        uniform vec3 lightPosition;
        uniform vec3 cameraPosition;

        // a flag to show whether should enable shading
        uniform int enableShading;
        // whether there is cloud
        uniform int cloud;

        uniform mat4 cloudModel;

        out vec4 outColor;

        vec3 hash(vec3 p) {
	        p = vec3(
                    dot(p, vec3(127.1, 311.7, 74.7)), 
                    dot(p, vec3(269.5, 183.3, 246.1)), 
                    dot(p, vec3(113.5, 271.9, 124.6))
                );

	        return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
        }

        float perlin(vec3 p) {
            vec3 i = floor(p);
	        vec3 f = fract(p);
            vec3 u = smoothstep(0, 1, f);

            float s000 = dot(hash(i + vec3(0, 0, 0)), f - vec3(0, 0, 0));
            float s100 = dot(hash(i + vec3(1, 0, 0)), f - vec3(1, 0, 0));
            float s110 = dot(hash(i + vec3(1, 1, 0)), f - vec3(1, 1, 0));
            float s010 = dot(hash(i + vec3(0, 1, 0)), f - vec3(0, 1, 0));
            float a = mix(mix(s000, s100, u.x), mix(s010, s110, u.x), u.y);

            float s001 = dot(hash(i + vec3(0, 0, 1)), f - vec3(0, 0, 1));
            float s101 = dot(hash(i + vec3(1, 0, 1)), f - vec3(1, 0, 1));
            float s111 = dot(hash(i + vec3(1, 1, 1)), f - vec3(1, 1, 1));
            float s011 = dot(hash(i + vec3(0, 1, 1)), f - vec3(0, 1, 1));
            float b = mix(mix(s001, s101, u.x), mix(s011, s111, u.x), u.y);

            return mix(a, b, u.z);
        }

        vec3 noise(vec3 position) {
            vec3 p = position * 8;
            float f = perlin(p);
            f += 0.5000 * perlin(2 * p);
            f += 0.2500 * perlin(4 * p);
            f += 0.1250 * perlin(8 * p);
            return vec3(0.5 * f + 0.5);
        }

		void main() {
            vec3 color = texture(tex, Texcoord).rgb;

            if (cloud == 1) {
                vec3 cloudColor = noise(vec3(view * cloudModel * model * vec4(cloudPosition, 1.0)));
                color = color + cloudColor;
            }
            
            if (enableShading == 1) {
                vec3 normal = normalize(fragNormal);
                vec3 lightColor = vec3(1.0, 1.0, 1.0);

                // ambient
                vec3 ambient = 0.15 * color;

                // Diffuse
                vec3 lightDir = normalize(lightPosition - fragPosition);
                float diff = max(dot(lightDir, normal), 0.0);
                vec3 diffuse = diff * lightColor;

                // Specular
                vec3 viewDir = normalize(cameraPosition - fragPosition);
                vec3 reflectDir = reflect(-lightDir, normal);
                float spec = 0.0;
                vec3 halfwayDir = normalize(lightDir + viewDir);  
                spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
                vec3 specular = spec * lightColor;

                // calculate shadow
                vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
                projCoords = projCoords * 0.5 + 0.5;
                float closestDepth = texture(shadowMap, projCoords.xy).r;
                float currentDepth = projCoords.z;

                float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
                float shadow = 0.0;
                vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
                for (int x = -1; x <= 1; ++x) {
                    for (int y = -1; y <= 1; ++y) {
                        float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
                        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
                    }    
                }
                shadow /= 9.0;

                vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

                outColor = vec4(lighting, 1.0);
            } else {
                outColor = vec4(color, 1.0);
            }
		}
	)";

    // Compile the two shaders and upload the binary to the GPU
    // Note that we have to explicitly specify that the output "slot" called outColor
    // is the one that we want in the fragment buffer (and thus on screen)
    program->init(vertex_shader, fragment_shader, "outColor");
}

void set_shaders_shadow_map(Program *program) {
    const GLchar *vertex_shader = R"(
		#version 150 core

        in vec3 position;

        uniform mat4 mvp;

		void main() {
			gl_Position = mvp * vec4(position, 1.0f);
		}
	)";

    const GLchar *fragment_shader = R"(
		#version 150 core

        out vec4 outColor;

		void main() {
            gl_FragDepth = gl_FragCoord.z;
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
		}
	)";

    program->init(vertex_shader, fragment_shader, "outColor");
}