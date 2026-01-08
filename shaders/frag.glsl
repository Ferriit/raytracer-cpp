#version 430 core

in vec2 vUV;
out vec4 FragColor;

uniform vec2 resolution; 
uniform float fov;       
uniform vec3 camerapos;
uniform vec3 camerarotation;

uniform int framecount;


struct material {
    vec4 color;         // x, y, z are rgb color, w is emission
    float specularcoefficient;
    float _pad0, _pad1;
};


struct sphere {
    vec4 pos;       // x, y, z = x, y, z position, w = radius
    material mat;
};


struct rayinfo {
    material mat;
    vec3 position;
    vec3 direction;
    vec3 normalvec;
    float distancetraveled;
};


layout(std430, binding = 0) buffer SphereBlock {
    sphere spheres[];
};


// Rotation matrices
mat3 rotX(float a) {
    float s = sin(a), c = cos(a);
    return mat3(
        1, 0, 0,
        0, c,-s,
        0, s, c
    );
}

mat3 rotY(float a) {
    float s = sin(a), c = cos(a);
    return mat3(
         c, 0, s,
         0, 1, 0,
        -s, 0, c
    );
}

mat3 rotZ(float a) {
    float s = sin(a), c = cos(a);
    return mat3(
        c,-s, 0,
        s, c, 0,
        0, 0, 1
    );
}


mat3 getCameraRotation(vec3 rotDeg) {
    vec3 r = radians(rotDeg);
    return rotY(r.y) * rotX(r.x) * rotZ(r.z);
}


// Signed Distance Fields / functions
float circleSDF(vec3 pos, float size) {
    vec3 spherePos = vec3(0.0, 0.0, 5.0);
    return length(pos - spherePos) - size;
}

vec3 sphereNormal(vec3 p, vec3 center) {
    return normalize(p - center);
}

float sceneSDF(vec3 p) {
    float minDist = 1000.0;

    for (int i = 0; i < 16; i++) {
        float d = length(p - spheres[i].pos.xyz) - spheres[i].pos.w;
        minDist = min(minDist, d);
    }
    return minDist;
}

uint hash_u32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint rng_state;

float rand() {
    rng_state = rng_state * 747796405u + 2891336453u;
    uint x = ((rng_state >> ((rng_state >> 28) + 4)) ^ rng_state) * 277803737u;
    x = (x >> 22) ^ x;
    return float(x) * (1.0 / 4294967296.0);
}


vec3 randomhemispherevector(vec3 normal) {
    float u1 = rand();
    float u2 = rand();

    float r = sqrt(u1);
    float theta = 6.28318530718 * u2;

    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - u1);

    vec3 tangent = normalize(
        abs(normal.x) < 0.999 ? cross(normal, vec3(1,0,0))
                              : cross(normal, vec3(0,1,0))
    );
    vec3 bitangent = cross(normal, tangent);

    return normalize(x * tangent + y * bitangent + z * normal);
}


vec3 getnormal(vec3 p) {
    const float eps = 0.001;

    float dx = sceneSDF(p + vec3(eps, 0, 0)) - sceneSDF(p - vec3(eps, 0, 0));
    float dy = sceneSDF(p + vec3(0, eps, 0)) - sceneSDF(p - vec3(0, eps, 0));
    float dz = sceneSDF(p + vec3(0, 0, eps)) - sceneSDF(p - vec3(0, 0, eps));

    return normalize(vec3(dx, dy, dz));
}


material SDFmaterial(vec3 p) {
    float minDist = 1000.0;

    float smallest = 100000000;
    int smallestIDX = -1;

    for (int i = 0; i < 16; i++) {
        float d = length(p - spheres[i].pos.xyz) - spheres[i].pos.w;
        if (d < smallest) {
            smallest = d;
            smallestIDX = i;
        }
    }

    if (smallestIDX == -1) {
        material m;
        m.color = vec4(0.0);
        m.specularcoefficient = 0.0;
        m._pad0 = 0.0;
        m._pad1 = 0.0;
        return m;
    }
    return spheres[smallestIDX].mat;
}


vec3 getpixelvector() {
    float aspect = resolution.x / resolution.y;
    float fovRad = radians(fov);

    vec2 p = vUV * 2.0 - 1.0;
    p.x *= aspect;
    p *= tan(fovRad * 0.5);

    vec3 rayDir = normalize(vec3(p, 1.0));

    mat3 camRot = getCameraRotation(camerarotation);
    return camRot * rayDir;
}

rayinfo raymarch(int maxsteps, vec3 pixelvector, vec3 rayorigin) {
    vec3 pos = rayorigin;

    for (int i = 0; i < maxsteps; i++) {
        float SDF = sceneSDF(pos);
        if (SDF < 0.001) {
            float dist = length(pos - camerapos);

            // Inverse square law with smoothing
            float brightness = 1 / max(0.001, 1.0 + 0.09 * dist + 0.032 * dist * dist);

            return rayinfo(SDFmaterial(pos), pos, pixelvector, getnormal(pos), dist);
        }
        
        pos += pixelvector * SDF;
    }

    rayinfo data;
    data.mat = material(vec4(0.0), 0.0, 0.0, 0.0);
    data.position = vec3(0.0);
    data.direction = vec3(0.0);
    data.distancetraveled = length(pos - camerapos);
    data.normalvec = vec3(0.0);

    return data;
}


bool raysphereintersect(vec3 origin, vec3 direction, vec3 center, float radius, out float t) {
    vec3 oc = origin - center;
    float b = dot(oc, direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b*b - c;

    if (discriminant < 0.0) {
        return false; // no intersection
    }

    float sqrtdisc = sqrt(discriminant);
    float t0 = -b - sqrtdisc;
    float t1 = -b + sqrtdisc;

    // pick the smallest positive t
    t = (t0 > 0.0) ? t0 : ((t1 > 0.0) ? t1 : -1.0);
    return t > 0.0;
}


rayinfo raytracesceneonce(vec3 rayorigin, vec3 raydirection) {
    float smallestT = 1e20;
    float T = 0.0;

    rayinfo data;
    data.mat.color = vec4(0.0);
    data.position = vec3(0.0);
    data.direction = vec3(0.0);
    data.distancetraveled = 0.0;
    data.normalvec = vec3(0.0);

    for (int i = 0; i < 16; i++) {
        sphere Sphere = spheres[i];
        if (raysphereintersect(rayorigin, raydirection, Sphere.pos.xyz, Sphere.pos.w, T) && T < smallestT) {
            vec3 pos = rayorigin + T * raydirection;
            smallestT = T;

            data.mat = Sphere.mat;
            data.position = pos;
            data.direction = raydirection;
            data.normalvec = sphereNormal(pos, Sphere.pos.xyz);
            data.distancetraveled = T;
        }
    }

    return data;
}


rayinfo raytrace(vec3 rayorigin, vec3 raydirection, int maxbounces, int samples) {
    rayinfo data;
    data.mat.color = vec4(0.0); 
    data.position = vec3(0.0);
    data.direction = vec3(0.0);
    data.distancetraveled = 0.0;
    data.normalvec = vec3(0.0);

    for (int s = 0; s < samples; s++) {
        rng_state = hash_u32(
            uint(gl_FragCoord.x)
          ^ uint(gl_FragCoord.y) * 4099u
          ^ uint(framecount) * 131071u
          ^ uint(s) * 8191u
        );

        vec3 origin = rayorigin;
        vec3 direction = raydirection;

        vec3 throughput = vec3(1.0); 
        vec3 radiance = vec3(0.0);

        for (int bounce = 0; bounce < maxbounces; bounce++) {
            rayinfo hit = raytracesceneonce(origin, direction);

            if (hit.distancetraveled <= 0.0 || length(hit.mat.color.xyz) == 0.0) break;

            vec3 normal = hit.normalvec;
            radiance += throughput * hit.mat.color.w;
            throughput *= hit.mat.color.xyz;
            origin = hit.position + normal * 0.0005;

            vec3 diffuseDir = randomhemispherevector(normal);
            vec3 specularDir = normalize(direction - 2.0 * dot(direction, normal) * normal);

            float choose = rand();
            direction = (choose < hit.mat.specularcoefficient) ? specularDir : diffuseDir;
        }

        data.mat.color.xyz += radiance / float(samples);
    }

    return data;
}


void main() {
    vec3 raydir = getpixelvector();

    rayinfo ray = raytrace(camerapos, raydir, 5, 1024);

    FragColor = vec4(ray.mat.color.xyz, 1.0);
}
