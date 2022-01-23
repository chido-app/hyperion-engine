#ifndef SHADERTOY_H
#define SHADERTOY_H

#include "../post_shader.h"

namespace hyperion {

class ShadertoyShader : public PostShader {
public:
    ShadertoyShader(const ShaderProperties &properties);
    virtual ~ShadertoyShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};

} // namespace hyperion

#endif
