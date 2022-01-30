#ifndef GI_MAPPER_H
#define GI_MAPPER_H

#include "gi_mapper_camera.h"
#include "../../math/bounding_box.h"
#include "../renderable.h"

#include <memory>
#include <array>
#include <utility>

namespace hyperion {
class Shader;
class ComputeShader;

class GIMapper : public Renderable {
public:
    GIMapper(const BoundingBox &bounds);
    ~GIMapper();

    void SetOrigin(const Vector3 &origin);

    inline GIMapperCamera *GetCamera(int index) { return m_cameras[index]; }
    inline const GIMapperCamera *GetCamera(int index) const { return m_cameras[index]; }
    inline constexpr size_t NumCameras() const { return m_cameras.size(); }

    void Bind(Shader *shader);

    virtual void Render(Renderer *renderer, Camera *cam) override;

    void UpdateRenderTick(double dt);

private:
    double m_render_tick;
    std::array<GIMapperCamera*, 6> m_cameras;
    std::array<std::pair<Vector3, Vector3>, 6> m_directions;
    BoundingBox m_bounds;
    BoundingBox m_last_bounds;
};
} // namespace apex

#endif