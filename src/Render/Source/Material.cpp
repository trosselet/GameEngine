#include "pch.h"
#include "Header/Material.h"

#include "Header/Render.h"
#include "Header/RenderResources.h"
#include "Header/Texture.h"

Material::Material(Render* pRender) :
	m_pRender(pRender),
	m_uploadBuffer(pRender->GetRenderResources()->GetDevice(), 1, 1),
	m_pTexture(nullptr)
{
	m_uploadBuffer.GetResource()->SetName(L"MaterialUBuffer");
}

Material::~Material()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}
}

UploadBuffer<ObjectData>* Material::GetUploadBuffer()
{
	return &m_uploadBuffer;
}

void Material::UpdateWorldConstantBuffer(DirectX::XMMATRIX const& matrix)
{
	ObjectData dataCb = {};
	DirectX::XMStoreFloat4x4(&dataCb.world, DirectX::XMMatrixTranspose(matrix));
	m_uploadBuffer.CopyData(0, dataCb);
}

void Material::SetTexture(Texture* pTexture)
{
	m_pTexture = pTexture;
}

bool Material::UpdateTexture(int16 position)
{
	if (m_pTexture != nullptr)
	{
		m_pRender->GetRenderResources()->GetCommandList()->SetGraphicsRootDescriptorTable(position, m_pTexture->GetTextureAddress());
	}
	return true;
}

Texture* Material::GetTexture()
{
	return m_pTexture;
}
