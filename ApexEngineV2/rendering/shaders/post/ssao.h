#ifndef SSAO_H
#define SSAO_H

#include "../post_shader.h"

namespace hyperion {

class SSAOShader : public PostShader {
public:
    SSAOShader(const ShaderProperties &properties);
    virtual ~SSAOShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};

} // namespace hyperion

#endif
