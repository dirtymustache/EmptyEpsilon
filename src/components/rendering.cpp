#include "mesh.h"
#include "textureManager.h"
#include "rendering.h"
#include <ecs/query.h>

Mesh* MeshRenderComponent::getMesh()
{
    if (!mesh.ptr && !mesh.name.empty())
        mesh.ptr = Mesh::getMesh(mesh.name);
    return mesh.ptr;
}

sp::Texture* MeshRenderComponent::getTexture()
{
    if (!texture.ptr && !texture.name.empty())
        texture.ptr = textureManager.getTexture(texture.name);
    return texture.ptr;
}

sp::Texture* MeshRenderComponent::getSpecularTexture()
{
    if (!specular_texture.ptr && !specular_texture.name.empty())
        specular_texture.ptr = textureManager.getTextureOrNull(specular_texture.name);
    return specular_texture.ptr;
}

sp::Texture* MeshRenderComponent::getIlluminationTexture()
{
    if (!illumination_texture.ptr && !illumination_texture.name.empty())
        illumination_texture.ptr = textureManager.getTextureOrNull(illumination_texture.name);
    return illumination_texture.ptr;
}

sp::Texture* MeshRenderComponent::getNormalTexture()
{
    if (!normal_texture.ptr && !normal_texture.name.empty())
        normal_texture.ptr = textureManager.getTextureOrNull(normal_texture.name);
    return normal_texture.ptr;
}

void MeshRenderComponent::resetAllTexturePtrs()
{
    for (auto [entity, mrc] : sp::ecs::Query<MeshRenderComponent>())
    {
        mrc.mesh.ptr = nullptr;
        mrc.texture.ptr = nullptr;
        mrc.specular_texture.ptr = nullptr;
        mrc.illumination_texture.ptr = nullptr;
        mrc.normal_texture.ptr = nullptr;
    }

    for (auto [entity, nr] : sp::ecs::Query<NebulaRenderer>())
    {
        for (auto& cloud : nr.clouds)
            cloud.texture.ptr = nullptr;
    }
}
